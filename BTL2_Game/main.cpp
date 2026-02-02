#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <sstream>

using namespace std;

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

void drawFilledTriangle(SDL_Renderer* renderer,
                        SDL_Point p1, SDL_Point p2, SDL_Point p3) {
    auto drawLine = [&](SDL_Point a, SDL_Point b) {
        SDL_RenderDrawLine(renderer, a.x, a.y, b.x, b.y);
    };

    // Sort by y
    if (p2.y < p1.y) swap(p1, p2);
    if (p3.y < p1.y) swap(p1, p3);
    if (p3.y < p2.y) swap(p2, p3);

    auto interp = [](SDL_Point a, SDL_Point b, float t) {
        return SDL_Point{
            (int)(a.x + (b.x - a.x) * t),
            (int)(a.y + (b.y - a.y) * t)
        };
    };

    for (int y = p1.y; y <= p3.y; y++) {
        float t1 = (p3.y == p1.y) ? 0 : (float)(y - p1.y) / (p3.y - p1.y);
        SDL_Point A = interp(p1, p3, t1);

        SDL_Point B;
        if (y < p2.y) {
            float t2 = (p2.y == p1.y) ? 0 : (float)(y - p1.y) / (p2.y - p1.y);
            B = interp(p1, p2, t2);
        } else {
            float t2 = (p3.y == p2.y) ? 0 : (float)(y - p2.y) / (p3.y - p2.y);
            B = interp(p2, p3, t2);
        }

        drawLine(A, B);
    }
}


// ===================================================
// Draw filled circle
void drawFilledCircle(SDL_Renderer* renderer, int cx, int cy, int r) {
    for (int w = -r; w <= r; w++) {
        for (int h = -r; h <= r; h++) {
            if (w*w + h*h <= r*r) {
                SDL_RenderDrawPoint(renderer, cx + w, cy + h);
            }
        }
    }
}
// ===================================================

// ===================== PLAYER ======================
class Player {
public:
    int x, y;
    int radius = 20;
    SDL_Color color;
    bool active = false;
    
    // Direction tracking
    float dirX = 1.0f; // Default facing right
    float dirY = 0.0f;

    Player(int x, int y, SDL_Color c) : x(x), y(y), color(c) {}

    void updateDirection(int dx, int dy) {
        // Update direction based on input (even if not moving due to wall)
        if (dx != 0 || dy != 0) {
            float len = sqrt(dx*dx + dy*dy);
            dirX = dx / len;
            dirY = dy / len;
        }
    }
    
    void move(int dx, int dy) {
        updateDirection(dx, dy);
        
        x += dx;
        y += dy;

        // keep in screen
        if (x < radius) x = radius;
        if (x > SCREEN_WIDTH - radius) x = SCREEN_WIDTH - radius;
        if (y < radius) y = radius;
        if (y > SCREEN_HEIGHT - radius) y = SCREEN_HEIGHT - radius;
    }
    
    void drawArrow(SDL_Renderer* renderer) {
        int arrowStartDist = radius + 20;
        int arrowLength = 28;

        int sx = x + dirX * arrowStartDist;
        int sy = y + dirY * arrowStartDist;
        int ex = x + dirX * (arrowStartDist + arrowLength);
        int ey = y + dirY * (arrowStartDist + arrowLength);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

        //SDL_RenderDrawLine(renderer, sx, sy, ex, ey);

        float angle = atan2(dirY, dirX);
        float headLength = 14.0f;
        float headWidth  = 10.0f;

        SDL_Point tip = { ex, ey };
        SDL_Point left = {
            (int)(ex - headLength * cos(angle) + headWidth * sin(angle)),
            (int)(ey - headLength * sin(angle) - headWidth * cos(angle))
        };
        SDL_Point right = {
            (int)(ex - headLength * cos(angle) - headWidth * sin(angle)),
            (int)(ey - headLength * sin(angle) + headWidth * cos(angle))
        };

        drawFilledTriangle(renderer, tip, left, right);
    }

    void draw(SDL_Renderer* renderer) {
        // highlight active player
        if (active) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            drawFilledCircle(renderer, x, y, radius + 3);
        }

        // draw player
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
        drawFilledCircle(renderer, x, y, radius);

        // draw arrow
        if (active) {
            drawArrow(renderer);
        }
    }
};
// ===================================================

// ====================== BALL =======================
class Ball {
public:
    float x, y;
    float vx = 4, vy = 3;
    int radius = 5;
    
    // Possession system
    Player* possessedBy = nullptr;
    bool isCharging = false;
    Uint32 chargeStartTime = 0;
    const float MAX_SHOT_POWER = 20.0f;
    const float MIN_SHOT_POWER = 5.0f;
    const Uint32 MAX_CHARGE_TIME = 2000; // 2 seconds max charge

    Ball(int x, int y) : x(x), y(y) {}

    void update() {
        if (possessedBy) {
            // Ball positioned outside player in the direction they're facing
            float distance = possessedBy->radius + radius + 5; // 5 pixels gap
            x = possessedBy->x + possessedBy->dirX * distance;
            y = possessedBy->y + possessedBy->dirY * distance;
        } else {
            // Ball moves freely
            x += vx;
            y += vy;
        }
    }

    void wallCollision() {
        if (possessedBy) return; // No wall collision when possessed
        
        if (x - radius <= 0 || x + radius >= SCREEN_WIDTH)
            vx = -vx;
        if (y - radius <= 0 || y + radius >= SCREEN_HEIGHT)
            vy = -vy;
    }
    
    void attachToPlayer(Player* player) {
        possessedBy = player;
        vx = 0;
        vy = 0;
    }
    
    void startCharging() {
        if (possessedBy) {
            isCharging = true;
            chargeStartTime = SDL_GetTicks();
        }
    }
    
    void shoot() {
        if (!possessedBy) return;

        Player* shooter = possessedBy;

        // Use player's direction (arrow direction)
        float dx = shooter->dirX;
        float dy = shooter->dirY;

        float chargePower = min(1.0f,
            (float)(SDL_GetTicks() - chargeStartTime) / MAX_CHARGE_TIME);

        float shotPower = MIN_SHOT_POWER +
            (MAX_SHOT_POWER - MIN_SHOT_POWER) * chargePower;

        // Set ball velocity in arrow direction
        vx = dx * shotPower;
        vy = dy * shotPower;

        // Push ball outside player radius in arrow direction
        x = shooter->x + dx * (shooter->radius + radius + 2);
        y = shooter->y + dy * (shooter->radius + radius + 2);

        possessedBy = nullptr;
        isCharging = false;
    }


    void draw(SDL_Renderer* renderer) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        drawFilledCircle(renderer, (int)x, (int)y, radius);
        
        // Draw charge indicator
        if (isCharging && possessedBy) {
            Uint32 chargeTime = SDL_GetTicks() - chargeStartTime;
            float chargePower = min(1.0f, (float)chargeTime / MAX_CHARGE_TIME);
            
            // Draw power bar
            int barWidth = 60;
            int barHeight = 8;
            int barX = (int)x - barWidth/2;
            int barY = (int)y - radius - 20;
            
            // Background
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_Rect bgRect = {barX, barY, barWidth, barHeight};
            SDL_RenderFillRect(renderer, &bgRect);
            
            // Charge fill (green to red gradient based on power)
            int fillWidth = (int)(barWidth * chargePower);
            int r = (int)(255 * chargePower);
            int g = (int)(255 * (1.0f - chargePower));
            SDL_SetRenderDrawColor(renderer, r, g, 0, 255);
            SDL_Rect fillRect = {barX, barY, fillWidth, barHeight};
            SDL_RenderFillRect(renderer, &fillRect);
        }
    }
};
// ===================================================

// ====================== GOAL =======================
class Goal {
public:
    SDL_Rect rect;
    int teamId; // 1 for left goal (team2 scores), 2 for right goal (team1 scores)
    
    Goal(int x, int y, int w, int h, int team) : teamId(team) {
        rect = {x, y, w, h};
    }
    
    bool checkBallInside(Ball& ball) {
        return ball.x >= rect.x && ball.x <= rect.x + rect.w &&
               ball.y >= rect.y && ball.y <= rect.y + rect.h;
    }
    
    void draw(SDL_Renderer* renderer) {
        // Draw goal zone with semi-transparent color
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        if (teamId == 1) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 100); // Red for left goal
        } else {
            SDL_SetRenderDrawColor(renderer, 0, 0, 255, 100); // Blue for right goal
        }
        SDL_RenderFillRect(renderer, &rect);
        
        // Draw border
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &rect);
    }
};
// ===================================================

// ====================== TEAM =======================
class Team {
public:
    vector<Player> players;
    int score = 0;
    int activeIndex = 0;

    void draw(SDL_Renderer* renderer) {
        for (auto& p : players)
            p.draw(renderer);
    }

    void deactivateAll() {
        for (auto& p : players) p.active = false;
    }

    void activateNext() {
        deactivateAll();
        activeIndex = (activeIndex + 1) % players.size();
        players[activeIndex].active = true;
    }
};
// ===================================================

// =================== COLLISION =====================
bool checkCollision(Player& p, Ball& b) {
    float dx = p.x - b.x;
    float dy = p.y - b.y;
    float dist = sqrt(dx*dx + dy*dy);
    return dist <= p.radius + b.radius;
}
// ===================================================

// ================= DRAW DIGIT =====================
void drawDigit(SDL_Renderer* renderer, int digit, int x, int y, int size) {
    // Simple 7-segment style digit rendering
    bool segments[10][7] = {
        {1,1,1,1,1,1,0}, // 0
        {0,1,1,0,0,0,0}, // 1
        {1,1,0,1,1,0,1}, // 2
        {1,1,1,1,0,0,1}, // 3
        {0,1,1,0,0,1,1}, // 4
        {1,0,1,1,0,1,1}, // 5
        {1,0,1,1,1,1,1}, // 6
        {1,1,1,0,0,0,0}, // 7
        {1,1,1,1,1,1,1}, // 8
        {1,1,1,1,0,1,1}  // 9
    };
    
    if (digit < 0 || digit > 9) return;
    
    int w = size / 3;
    int h = size / 2;
    
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    
    // Top horizontal
    if (segments[digit][0]) {
        SDL_Rect r = {x + w/3, y, w, h/5};
        SDL_RenderFillRect(renderer, &r);
    }
    // Top right vertical
    if (segments[digit][1]) {
        SDL_Rect r = {x + w + w/3, y, w/5, h};
        SDL_RenderFillRect(renderer, &r);
    }
    // Bottom right vertical
    if (segments[digit][2]) {
        SDL_Rect r = {x + w + w/3, y + h, w/5, h};
        SDL_RenderFillRect(renderer, &r);
    }
    // Bottom horizontal
    if (segments[digit][3]) {
        SDL_Rect r = {x + w/3, y + 2*h - h/5, w, h/5};
        SDL_RenderFillRect(renderer, &r);
    }
    // Bottom left vertical
    if (segments[digit][4]) {
        SDL_Rect r = {x, y + h, w/5, h};
        SDL_RenderFillRect(renderer, &r);
    }
    // Top left vertical
    if (segments[digit][5]) {
        SDL_Rect r = {x, y, w/5, h};
        SDL_RenderFillRect(renderer, &r);
    }
    // Middle horizontal
    if (segments[digit][6]) {
        SDL_Rect r = {x + w/3, y + h - h/10, w, h/5};
        SDL_RenderFillRect(renderer, &r);
    }
}

void drawNumber(SDL_Renderer* renderer, int number, int x, int y, int size) {
    string numStr = to_string(number);
    int spacing = size / 2;
    for (size_t i = 0; i < numStr.length(); i++) {
        int digit = numStr[i] - '0';
        drawDigit(renderer, digit, x + i * spacing, y, size);
    }
}
// ===================================================

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    SDL_Window* window = SDL_CreateWindow(
        "Football SDL Game",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT, 0
    );

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    
    // Load background texture
    SDL_Texture* backgroundTexture = IMG_LoadTexture(renderer, "Football_field.png");
    if (!backgroundTexture) {
        cerr << "Failed to load football_field.png: " << IMG_GetError() << endl;
    }

    bool running = true;
    SDL_Event event;
    const Uint8* keystate;

    int speed = 5;
    
    // ================= GAME STATE ===================
    Uint32 matchStartTime = SDL_GetTicks();
    const int MATCH_DURATION = 60000; // 60 seconds in milliseconds
    bool gameOver = false;
    
    // Create goals (small rectangles on left and right)
    int goalWidth = 20;
    int goalHeight = 150;
    Goal leftGoal(0, (SCREEN_HEIGHT - goalHeight) / 2, goalWidth, goalHeight, 1);
    Goal rightGoal(SCREEN_WIDTH - goalWidth, (SCREEN_HEIGHT - goalHeight) / 2, goalWidth, goalHeight, 2);

    // ================= CREATE TEAMS =================
    Team team1, team2;

    team1.players = {
        Player(150, 200, {255, 0, 0}),
        Player(100, 300, {255, 0, 0}),
        Player(150, 400, {255, 0, 0})
    };

    team2.players = {
        Player(650, 200, {0, 0, 255}),
        Player(700, 300, {0, 0, 255}),
        Player(650, 400, {0, 0, 255})
    };

    team1.activeIndex = 0;
    team2.activeIndex = 0;

    team1.players[team1.activeIndex].active = true;
    team2.players[team2.activeIndex].active = true;

    Ball ball(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);

    // ================= GAME LOOP ====================
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;

            // SELECT PLAYER
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    // Team 1 select (1,2,3)
                    // case SDLK_1: team1.deactivateAll(); team1.players[0].active = true; break;
                    // case SDLK_2: team1.deactivateAll(); team1.players[1].active = true; break;
                    // case SDLK_3: team1.deactivateAll(); team1.players[2].active = true; break;

                    // // Team 2 select (I,O,P)
                    // case SDLK_i: team2.deactivateAll(); team2.players[0].active = true; break;
                    // case SDLK_o: team2.deactivateAll(); team2.players[1].active = true; break;
                    // case SDLK_p: team2.deactivateAll(); team2.players[2].active = true; break;

                    case SDLK_SPACE: team1.activateNext(); break;
                    case SDLK_RIGHTBRACKET: team2.activateNext(); break;
                }
            }
        }

        keystate = SDL_GetKeyboardState(NULL);

        // TEAM 1 – WASD
        for (auto& p : team1.players) {
            if (!p.active) continue;
            int dx = 0, dy = 0;
            if (keystate[SDL_SCANCODE_W]) dy -= speed;
            if (keystate[SDL_SCANCODE_S]) dy += speed;
            if (keystate[SDL_SCANCODE_A]) dx -= speed;
            if (keystate[SDL_SCANCODE_D]) dx += speed;
            if (dx != 0 || dy != 0) {
                p.move(dx, dy);
            }
        }

        // TEAM 2 – ARROWS
        for (auto& p : team2.players) {
            if (!p.active) continue;
            int dx = 0, dy = 0;
            if (keystate[SDL_SCANCODE_UP]) dy -= speed;
            if (keystate[SDL_SCANCODE_DOWN]) dy += speed;
            if (keystate[SDL_SCANCODE_LEFT]) dx -= speed;
            if (keystate[SDL_SCANCODE_RIGHT]) dx += speed;
            if (dx != 0 || dy != 0) {
                p.move(dx, dy);
            }
        }
        
        // SHOOTING – Hold to charge, release to shoot
        // Team 1: E key
        if (keystate[SDL_SCANCODE_E]) {
            for (auto& p : team1.players) {
                if (p.active && ball.possessedBy == &p && !ball.isCharging) {
                    ball.startCharging();
                }
            }
        } else {
            // Released E key - shoot if was charging
            if (ball.isCharging && ball.possessedBy) {
                for (auto& p : team1.players) {
                    if (p.active && ball.possessedBy == &p) {
                        // Shoot in the arrow direction
                        ball.shoot();
                    }
                }
            }
        }
        
        // Team 2: Enter/Return key
        if (keystate[SDL_SCANCODE_RETURN]) {
            for (auto& p : team2.players) {
                if (p.active && ball.possessedBy == &p && !ball.isCharging) {
                    ball.startCharging();
                }
            }
        } else {
            // Released Enter key - shoot if was charging
            if (ball.isCharging && ball.possessedBy) {
                for (auto& p : team2.players) {
                    if (p.active && ball.possessedBy == &p) {
                        // Shoot in the arrow direction
                        ball.shoot();
                    }
                }
            }
        }

        // BALL
        ball.update();
        ball.wallCollision();

        // COLLISION BALL – PLAYERS (attach ball to player)
        if (!ball.possessedBy) {
            for (auto& p : team1.players) {
                if (checkCollision(p, ball)) {
                    ball.attachToPlayer(&p);
                    break;
                }
            }
            
            if (!ball.possessedBy) {
                for (auto& p : team2.players) {
                    if (checkCollision(p, ball)) {
                        ball.attachToPlayer(&p);
                        break;
                    }
                }
            }
        }
        
        // GOAL DETECTION
        if (!gameOver) {
            if (leftGoal.checkBallInside(ball)) {
                team2.score++; // Team 2 scores in left goal
                // Reset ball
                ball.x = SCREEN_WIDTH / 2;
                ball.y = SCREEN_HEIGHT / 2;
                ball.vx = 0;
                ball.vy = 0;
                ball.possessedBy = nullptr;
                ball.isCharging = false;
            } else if (rightGoal.checkBallInside(ball)) {
                team1.score++; // Team 1 scores in right goal
                // Reset ball
                ball.x = SCREEN_WIDTH / 2;
                ball.y = SCREEN_HEIGHT / 2;
                ball.vx = 0;
                ball.vy = 0;
                ball.possessedBy = nullptr;
                ball.isCharging = false;
            }
        }
        
        // CHECK TIMER
        Uint32 currentTime = SDL_GetTicks();
        Uint32 elapsedTime = currentTime - matchStartTime;
        int remainingTime = (MATCH_DURATION - elapsedTime) / 1000;
        
        if (elapsedTime >= MATCH_DURATION && !gameOver) {
            gameOver = true;
            remainingTime = 0;
        }

        // RENDER
        SDL_RenderClear(renderer);
        
        // Draw background
        if (backgroundTexture) {
            SDL_RenderCopy(renderer, backgroundTexture, NULL, NULL);
        } else {
            // Fallback to green if texture failed to load
            SDL_SetRenderDrawColor(renderer, 0, 120, 0, 255);
            SDL_RenderClear(renderer);
        }

        // Draw goals
        leftGoal.draw(renderer);
        rightGoal.draw(renderer);
        
        team1.draw(renderer);
        team2.draw(renderer);
        ball.draw(renderer);
        
        // DRAW SCOREBOARD
        // Background bar
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
        SDL_Rect scoreboardBg = {0, 0, SCREEN_WIDTH, 50};
        SDL_RenderFillRect(renderer, &scoreboardBg);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        
        // Team 1 score (left side)
        drawNumber(renderer, team1.score, 50, 10, 30);
        
        // Timer (center)
        drawNumber(renderer, remainingTime, SCREEN_WIDTH/2 - 20, 10, 30);
        
        // Team 2 score (right side)
        drawNumber(renderer, team2.score, SCREEN_WIDTH - 100, 10, 30);
        
        // GAME OVER SCREEN
        if (gameOver) {
            // Semi-transparent overlay
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
            SDL_Rect overlay = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
            SDL_RenderFillRect(renderer, &overlay);
            
            // Game Over box
            SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
            SDL_Rect gameOverBox = {SCREEN_WIDTH/2 - 200, SCREEN_HEIGHT/2 - 150, 400, 300};
            SDL_RenderFillRect(renderer, &gameOverBox);
            
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &gameOverBox);
            
            // Display final scores
            int centerX = SCREEN_WIDTH / 2;
            int centerY = SCREEN_HEIGHT / 2;
            
            // Team 1 final score
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_Rect team1Label = {centerX - 150, centerY - 80, 80, 60};
            SDL_RenderFillRect(renderer, &team1Label);
            drawNumber(renderer, team1.score, centerX - 130, centerY - 70, 40);
            
            // Team 2 final score
            SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
            SDL_Rect team2Label = {centerX + 70, centerY - 80, 80, 60};
            SDL_RenderFillRect(renderer, &team2Label);
            drawNumber(renderer, team2.score, centerX + 90, centerY - 70, 40);
            
            // Winner text (simple representation)
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            if (team1.score > team2.score) {
                // Red wins
                SDL_Rect winnerBox = {centerX - 100, centerY + 50, 200, 40};
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                SDL_RenderFillRect(renderer, &winnerBox);
            } else if (team2.score > team1.score) {
                // Blue wins
                SDL_Rect winnerBox = {centerX - 100, centerY + 50, 200, 40};
                SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
                SDL_RenderFillRect(renderer, &winnerBox);
            } else {
                // Draw
                SDL_Rect drawBox = {centerX - 100, centerY + 50, 200, 40};
                SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
                SDL_RenderFillRect(renderer, &drawBox);
            }
            
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    if (backgroundTexture) {
        SDL_DestroyTexture(backgroundTexture);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}
