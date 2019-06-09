#include <stdio.h>
#include <time.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>

#define SCREEN_W                 640
#define SCREEN_H                 480
#define SPRITE_CELL_W            32
#define SPRITE_CELL_H            20
#define PLAYER_SPEED             4
#define PLAYER_SHOOT_DELAY       30
#define PLAYER_MAX_LIFES         4
#define PLAYER_INVINCIBLE_TIME   96
#define PLAYER_EXPLOSION_TIME    96
#define ALIENS_INITIAL_SPEED     3
#define ALIENS_MOVE_STEP         8
#define ALIENS_PADDING_LEFT      42
#define ALIENS_PADDING_TOP       60
#define ALIENS_IN_ROW            6
#define ALIENS_COL_SPACING       25
#define ALIENS_ROW_SPACING       20
#define ALIENS_DESCEND_STEP      25
#define ALIEN_EXPLOSION_TIME     32
#define BULLET_W                 4
#define BULLET_H                 18
#define MAX_BULLETS              50
#define BULLET_SPEED             8

// enums
enum e_movingDirections
{
    NOT_MOVING,
    MOVING_LEFT,
    MOVING_RIGHT
};

enum e_entites
{
    PLAYER_ENTITY,
    PLAYER_LIFE_ENTITY,
    ALIEN_ENTITY,
    BULLET_ENTITY,
    ENTITY_COUNT
};

enum e_aliens
{
    ALIEN_1,
    ALIEN_2,
    ALIEN_3,
    ALIEN_4,
    ALIEN_5,
    ALIENS_COUNT
};

// typedef
typedef char           t_i8;
typedef short          t_i16;
typedef int            t_i32;
typedef long           t_i64;

typedef unsigned char  t_u8;
typedef unsigned short t_u16;
typedef unsigned int   t_u32;
typedef unsigned long  t_u64;

typedef struct 
{
    SDL_Window *pWindow;
    SDL_Renderer *pRenderer;
} t_SDLData;

typedef struct 
{
    t_u32 ticks;
    bool isRunning;
    t_u8 lastBullet;
} t_game;

typedef struct 
{
    SDL_Texture *pTexture;
    SDL_Rect playerClip[1];
    SDL_Rect ufoClip[1];
    SDL_Rect alien1Clip[2];
    SDL_Rect alien2Clip[2];
    SDL_Rect alien3Clip[2];
    SDL_Rect alien4Clip[2];
    SDL_Rect explosionClip[4];
    SDL_Rect bulletClip[1];
} t_spritesData;

typedef struct
{
    Mix_Chunk *playerShot;
    Mix_Chunk *playerExplosion;
    Mix_Chunk *alienExplosion;
} t_audioData;

typedef struct
{
    t_i16 x, y;
} t_position;

typedef struct 
{
    t_position position;
    e_movingDirections movement;
    bool isShooting;
    t_u32 lastShot;
    t_u32 lastHit;
    t_u8 lifes;
    t_u32 deathTime;
} t_playerData;

typedef struct
{
    e_movingDirections moving;
    t_position position;
    t_u8 speed;
    // edges can be negative after recalculation
    // so use signed integer type
    t_i16 leftEdge;
    t_i16 rightEdge;
    t_i16 bottomEdge;
} t_aliensData;

typedef struct
{
    t_u8 entity;
    t_u32 deathTime;
    bool isHidden;
} t_alienData;

typedef struct 
{
    t_position position;
    e_entites entity;
    bool isVisible;
} t_bulletData;

// function prototypes
bool initSDL();
void close();
bool initGame();
void startGame();
bool initSpritesData(char *path);
bool initAudioData();
void handleEvents();
void handleKeyStates();
void update();
void updatePlayer();
void updateAliens();
void updateAliensEdges();
void updateBullets();
void render();
void renderPlayer();
void renderLifeBar();
void renderAliens();
void renderAlien();
void renderBullets();
void shoot(e_entites entity, t_u8 col, t_u8 row);
void killAlien(t_u8 col, t_u8 row);
void hitPlayer();

// globals
t_SDLData g_SDLData;
t_game g_game;
t_spritesData g_spritesData;
t_audioData g_audioData;
t_playerData g_playerData;
t_aliensData g_aliensData;

SDL_Rect g_alienClips[ALIENS_COUNT][2];
SDL_Rect g_renderRects[ENTITY_COUNT];

t_alienData g_aliens[ALIENS_COUNT][ALIENS_IN_ROW];
t_bulletData *g_bullets[MAX_BULLETS];

bool initSDL()
{
    int imageFlags = IMG_INIT_PNG;

    // init SDl
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
    {
        printf("SDL could not initialize! SDL_Error: %s!\n", SDL_GetError());
        return false;
    }

    // init SDL_image
    if (!(IMG_Init(imageFlags) & imageFlags))
    {
        printf("SDL_image could not initialize! SDL_Image error: %s!\n", IMG_GetError());
        return false;
    }

    // init SDL_mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
    {
        printf("SDL_mixer could not initialize! SDL_mixer error: %s!\n", Mix_GetError());
        return false;
    }

    // init window
    g_SDLData.pWindow = SDL_CreateWindow(
        "SDL Tutorial",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        SCREEN_W, SCREEN_H,
        SDL_WINDOW_SHOWN);
    if (g_SDLData.pWindow == NULL)
    {
        printf("Window could not be created! SDL_Error: %s!\n", SDL_GetError());
        return false;
    }

    // init renderer
    g_SDLData.pRenderer = SDL_CreateRenderer(g_SDLData.pWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (g_SDLData.pRenderer == NULL)
    {
        printf("Renderer could not be created! SDL_Error: %s!\n", SDL_GetError());
        return false;
    }
    else SDL_SetRenderDrawColor(g_SDLData.pRenderer, 0x00, 0x00, 0x00, 0xFF);

    return true;
}

void close()
{
    // TODO: free sprites data 
    
    // free audio
    Mix_FreeChunk(g_audioData.playerShot);
    Mix_FreeChunk(g_audioData.playerExplosion);
    Mix_FreeChunk(g_audioData.alienExplosion);

    // destroy window
    SDL_DestroyWindow(g_SDLData.pWindow);
    SDL_DestroyRenderer(g_SDLData.pRenderer);
    g_SDLData.pWindow = NULL;
    g_SDLData.pRenderer = NULL;
    
    IMG_Quit();
    Mix_Quit();
    SDL_Quit();
}

bool initGame()
{
    srand(time(NULL));
    
    // init sprites
    g_spritesData.pTexture = NULL;
    char spritePath[] = "assets/spritesheet.png";

    if (!initSpritesData(spritePath))
    {
        printf("Sprite init error\n");
        return false;
    }

    // init audio
    g_audioData.playerShot = NULL;
    g_audioData.playerExplosion = NULL;
    g_audioData.alienExplosion = NULL;

    if (!initAudioData())
    {
        printf("Audio init error\n");
        return false;
    }

    g_game.ticks = 0;
    g_game.isRunning = true;

    return true;
}

bool initSpritesData(char *path)
{
    SDL_Surface *surface = IMG_Load(path);
    
    if (surface == NULL)
    {
        printf("Unable to load image %s! SDL_Image error: %s\n", path, IMG_GetError());
        return false;
    }

    g_spritesData.pTexture = SDL_CreateTextureFromSurface(g_SDLData.pRenderer, surface);
    if (g_spritesData.pTexture == NULL)
    {
        printf("Unable to create texture from %s! SDL_Error: %s\n", path, SDL_GetError());
        return false;
    }
    SDL_FreeSurface(surface);

    // init clip rects for all entities
    g_spritesData.playerClip[0].w = SPRITE_CELL_W;
    g_spritesData.playerClip[0].h = SPRITE_CELL_H;
    g_spritesData.playerClip[0].x = 0;
    g_spritesData.playerClip[0].y = 0;

    g_spritesData.ufoClip[0].w = SPRITE_CELL_W;
    g_spritesData.ufoClip[0].h = SPRITE_CELL_H;
    g_spritesData.ufoClip[0].x = 0;
    g_spritesData.ufoClip[0].y = SPRITE_CELL_H;

    g_alienClips[ALIEN_1][0].w = SPRITE_CELL_W;
    g_alienClips[ALIEN_1][0].h = SPRITE_CELL_H;
    g_alienClips[ALIEN_1][0].x = 0;
    g_alienClips[ALIEN_1][0].y = SPRITE_CELL_H * 2;
    g_alienClips[ALIEN_1][1].w = SPRITE_CELL_W;
    g_alienClips[ALIEN_1][1].h = SPRITE_CELL_H;
    g_alienClips[ALIEN_1][1].x = 0;
    g_alienClips[ALIEN_1][1].y = SPRITE_CELL_H * 3;

    g_alienClips[ALIEN_2][0].w = SPRITE_CELL_W;
    g_alienClips[ALIEN_2][0].h = SPRITE_CELL_H;
    g_alienClips[ALIEN_2][0].x = 0;
    g_alienClips[ALIEN_2][0].y = SPRITE_CELL_H * 4;
    g_alienClips[ALIEN_2][1].w = SPRITE_CELL_W;
    g_alienClips[ALIEN_2][1].h = SPRITE_CELL_H;
    g_alienClips[ALIEN_2][1].x = 0;
    g_alienClips[ALIEN_2][1].y = SPRITE_CELL_H * 5;

    g_alienClips[ALIEN_3][0].w = SPRITE_CELL_W;
    g_alienClips[ALIEN_3][0].h = SPRITE_CELL_H;
    g_alienClips[ALIEN_3][0].x = 0;
    g_alienClips[ALIEN_3][0].y = SPRITE_CELL_H * 6;
    g_alienClips[ALIEN_3][1].w = SPRITE_CELL_W;
    g_alienClips[ALIEN_3][1].h = SPRITE_CELL_H;
    g_alienClips[ALIEN_3][1].x = 0;
    g_alienClips[ALIEN_3][1].y = SPRITE_CELL_H * 7;

    g_alienClips[ALIEN_4][0].w = SPRITE_CELL_W;
    g_alienClips[ALIEN_4][0].h = SPRITE_CELL_H;
    g_alienClips[ALIEN_4][0].x = 0;
    g_alienClips[ALIEN_4][0].y = SPRITE_CELL_H * 8;
    g_alienClips[ALIEN_4][1].w = SPRITE_CELL_W;
    g_alienClips[ALIEN_4][1].h = SPRITE_CELL_H;
    g_alienClips[ALIEN_4][1].x = 0;
    g_alienClips[ALIEN_4][1].y = SPRITE_CELL_H * 9;

    g_alienClips[ALIEN_5][0].w = SPRITE_CELL_W;
    g_alienClips[ALIEN_5][0].h = SPRITE_CELL_H;
    g_alienClips[ALIEN_5][0].x = 0;
    g_alienClips[ALIEN_5][0].y = SPRITE_CELL_H * 10;
    g_alienClips[ALIEN_5][1].w = SPRITE_CELL_W;
    g_alienClips[ALIEN_5][1].h = SPRITE_CELL_H;
    g_alienClips[ALIEN_5][1].x = 0;
    g_alienClips[ALIEN_5][1].y = SPRITE_CELL_H * 11;

    g_spritesData.explosionClip[0].w = SPRITE_CELL_W;
    g_spritesData.explosionClip[0].h = SPRITE_CELL_H;
    g_spritesData.explosionClip[0].x = 0;
    g_spritesData.explosionClip[0].y = SPRITE_CELL_H * 12;
    g_spritesData.explosionClip[1].w = SPRITE_CELL_W;
    g_spritesData.explosionClip[1].h = SPRITE_CELL_H;
    g_spritesData.explosionClip[1].x = 0;
    g_spritesData.explosionClip[1].y = SPRITE_CELL_H * 13;
    g_spritesData.explosionClip[2].w = SPRITE_CELL_W;
    g_spritesData.explosionClip[2].h = SPRITE_CELL_H;
    g_spritesData.explosionClip[2].x = 0;
    g_spritesData.explosionClip[2].y = SPRITE_CELL_H * 14;
    g_spritesData.explosionClip[3].w = SPRITE_CELL_W;
    g_spritesData.explosionClip[3].h = SPRITE_CELL_H;
    g_spritesData.explosionClip[3].x = 0;
    g_spritesData.explosionClip[3].y = SPRITE_CELL_H * 15;

    g_spritesData.bulletClip[0].w = 4;
    g_spritesData.bulletClip[0].h = 12;
    g_spritesData.bulletClip[0].x = 0;
    g_spritesData.bulletClip[0].y = SPRITE_CELL_H * 16;

    // init render rects for all entities
    g_renderRects[PLAYER_ENTITY].w = g_spritesData.playerClip[0].w;
    g_renderRects[PLAYER_ENTITY].h = g_spritesData.playerClip[0].h;
    g_renderRects[PLAYER_ENTITY].x = 0;
    g_renderRects[PLAYER_ENTITY].y = 0;

    g_renderRects[ALIEN_ENTITY].w = g_alienClips[ALIEN_1][0].w;
    g_renderRects[ALIEN_ENTITY].h = g_alienClips[ALIEN_1][0].h;
    g_renderRects[ALIEN_ENTITY].x = 0;
    g_renderRects[ALIEN_ENTITY].y = 0;

    g_renderRects[BULLET_ENTITY].w = 4;
    g_renderRects[BULLET_ENTITY].h = 12;
    g_renderRects[BULLET_ENTITY].x = 0;
    g_renderRects[BULLET_ENTITY].y = 0;

    g_renderRects[PLAYER_LIFE_ENTITY].w = g_renderRects[PLAYER_ENTITY].w / 1.5;
    g_renderRects[PLAYER_LIFE_ENTITY].h = g_renderRects[PLAYER_ENTITY].h / 1.5;
    g_renderRects[PLAYER_LIFE_ENTITY].x = 0;
    g_renderRects[PLAYER_LIFE_ENTITY].y = 0;

    return true;
}

bool initAudioData()
{
    g_audioData.playerShot = Mix_LoadWAV("assets/audio/player-shot.wav");
    if (g_audioData.playerShot == NULL)
    {
        printf("Failed to load 'player-shot' sound effect! SDL_mixer error: %s\n", Mix_GetError());
        return false;
    }  

    g_audioData.playerExplosion = Mix_LoadWAV("assets/audio/player-explosion.wav");
    if (g_audioData.playerExplosion == NULL)
    {
        printf("Failed to load 'player-explosion' sound effect! SDL_mixer error: %s\n", Mix_GetError());
        return false;
    }  

    g_audioData.alienExplosion = Mix_LoadWAV("assets/audio/alien-explosion.wav");
    if (g_audioData.alienExplosion == NULL)
    {
        printf("Failed to load 'alien-explosion' sound effect! SDL_mixer error: %s\n", Mix_GetError());
        return false;
    }  

    return true;
}

void startGame()
{
    // init player data
    g_playerData.position.x = SCREEN_W / 2 - g_renderRects[PLAYER_ENTITY].w / 2;
    g_playerData.position.y = SCREEN_H - SPRITE_CELL_H;
    g_playerData.movement = NOT_MOVING;
    g_playerData.isShooting = false;
    g_playerData.lastShot = 0;
    g_playerData.lastHit = 0;
    g_playerData.lifes = PLAYER_MAX_LIFES;
    g_playerData.deathTime = 0;

    // init aliens data
    g_aliensData.moving = MOVING_RIGHT;
    g_aliensData.speed = ALIENS_INITIAL_SPEED;
    g_aliensData.position.x = 0;
    g_aliensData.position.y = 0;

    g_aliensData.leftEdge = 0;

    // sum of all alien widths and horizantal spacings
    g_aliensData.rightEdge = ALIENS_IN_ROW * 
        (g_renderRects[ALIEN_ENTITY].w + ALIENS_COL_SPACING) -
        ALIENS_COL_SPACING;

    // sum of all alien heights and vertical spacings
    g_aliensData.bottomEdge = ALIENS_COUNT *
        (g_renderRects[ALIEN_ENTITY].h + ALIENS_ROW_SPACING) -
        ALIENS_ROW_SPACING;

    t_u8 row, col;
    for (row = 0; row < ALIENS_COUNT; row++)
        for (col = 0; col < ALIENS_IN_ROW; col++)
        {
            g_aliens[row][col].entity = row;
            g_aliens[row][col].deathTime = 0;
            g_aliens[row][col].isHidden = false;
        }

    // init bullets data
    t_u8 i;
    for (i = 0; i < MAX_BULLETS; i++)
    {
        // free bullets of previous game
        if (g_bullets[i] != NULL)
            free(g_bullets[i]);

        g_bullets[i] = NULL;
    }
    g_game.lastBullet = 0;
}

void shoot(e_entites entity, t_u8 col, t_u8 row)
{
    t_u8 i;

    // finding first avaible empty space in bullets array
    for (i = 0; i < MAX_BULLETS; i++)
        if (g_bullets[i] == NULL)
        {
            g_bullets[i] = (t_bulletData *)malloc(sizeof(t_bulletData));
            g_bullets[i]->entity = entity;
            g_bullets[i]->isVisible = true;

            if (entity == PLAYER_ENTITY)
            {
                // placing bullet a bit above current player position
                g_bullets[i]->position.x = 
                    g_playerData.position.x + g_renderRects[PLAYER_ENTITY].w / 2 - BULLET_W / 2;

                g_bullets[i]->position.y = 
                    SCREEN_H - g_renderRects[PLAYER_ENTITY].h - g_renderRects[BULLET_ENTITY].h - 15;
            }
            else if (entity == ALIEN_ENTITY)
            {
                // placing bullet a bit below provided alien position
                g_bullets[i]->position.x = 
                    ALIENS_PADDING_LEFT + g_aliensData.position.x +
                    col * (g_renderRects[ALIEN_ENTITY].w + ALIENS_ROW_SPACING) +
                    g_renderRects[ALIEN_ENTITY].w / 2;

                g_bullets[i]->position.y = 
                    ALIENS_PADDING_TOP + g_aliensData.position.y +
                    row * (g_renderRects[ALIEN_ENTITY].h + ALIENS_COL_SPACING) + 
                    g_renderRects[ALIEN_ENTITY].h + 10;
            }

            break;       
        }

    // updating last bullet index if reached last index
    if (i == g_game.lastBullet)
        g_game.lastBullet++;

    if (entity == PLAYER_ENTITY)
        Mix_PlayChannel(-1, g_audioData.playerShot, 0); 
}

void killAlien(t_u8 col, t_u8 row)
{
    if (g_aliens[row][col].deathTime == 0)
    {
        g_aliens[row][col].deathTime = g_game.ticks;
        Mix_PlayChannel(-1, g_audioData.alienExplosion, 0); 
    }
}

void hitPlayer()
{
    if (g_playerData.lifes > 0)
    {
        g_playerData.lifes--;
        g_playerData.lastHit = g_game.ticks;
    }
}

void update()
{
    updatePlayer();
    updateAliens();
    updateBullets();

    g_game.ticks++;
}

void updatePlayer()
{
    // update player position
    if (g_playerData.movement == MOVING_LEFT && g_playerData.position.x > 0)
    {
        g_playerData.position.x -= PLAYER_SPEED;
    }
    else if (g_playerData.movement == MOVING_RIGHT 
             && g_playerData.position.x + g_renderRects[PLAYER_ENTITY].w < SCREEN_W)
    {
        g_playerData.position.x += PLAYER_SPEED;
    }

    // update player shooting
    if (g_playerData.isShooting)
    {
        if (g_playerData.lastShot == 0 ||
            g_game.ticks - g_playerData.lastShot > PLAYER_SHOOT_DELAY)
        {
            shoot(PLAYER_ENTITY, 0, 0); 
            g_playerData.lastShot = g_game.ticks;
        }
    }

    // check if player is alive
    if (g_playerData.lifes == 0 && g_playerData.deathTime == 0)
    {
        g_playerData.deathTime = g_game.ticks;
        g_playerData.movement = NOT_MOVING;
        
        Mix_PlayChannel(-1, g_audioData.playerExplosion, 0); 
    }
    // if player is dead wait some time until explosion animation will finish
    // and restart the game
    else if (g_playerData.deathTime > 0 &&
             g_game.ticks - g_playerData.deathTime > PLAYER_EXPLOSION_TIME)
    {
        startGame();
    }
}

void updateAliens()
{
    t_u8 col, row;
    
    // update movement
    if (g_game.ticks % (40 * 1 / g_aliensData.speed) == 0)
    {
        if (g_aliensData.moving == MOVING_RIGHT)
        {
            g_aliensData.position.x += ALIENS_MOVE_STEP;
        }
        else if (g_aliensData.moving == MOVING_LEFT)
        {
            g_aliensData.position.x -= ALIENS_MOVE_STEP;
        }

        // check if reached horizontal boundary
        if (ALIENS_PADDING_LEFT + g_aliensData.position.x + g_aliensData.rightEdge >=
            SCREEN_W - ALIENS_PADDING_LEFT)
        {
            // change direction
            g_aliensData.moving = MOVING_LEFT;

            // descend if there is enough space
            if (g_aliensData.position.y + g_aliensData.bottomEdge < SCREEN_H - 100)
                g_aliensData.position.y += ALIENS_DESCEND_STEP;
        }
        else if (g_aliensData.position.x <= g_aliensData.leftEdge)
        {
            // change direction
            g_aliensData.moving = MOVING_RIGHT;

            // descend if there is enough space
            if (g_aliensData.position.y + g_aliensData.bottomEdge < SCREEN_H - 100)
                g_aliensData.position.y += ALIENS_DESCEND_STEP;
        }
    }

    // generate shot by random alien
    if (g_game.ticks % 40 == 0)
    {
        col = rand() % ALIENS_IN_ROW;
        row = rand() % ALIENS_COUNT;

        // get random alien that is alive
        while (g_aliens[row][col].deathTime > 0)
        {
            col = rand() % ALIENS_IN_ROW;
            row = rand() % ALIENS_COUNT;
        }

        shoot(ALIEN_ENTITY, col, row);
    }

    bool hasDeaths = false;
    // hide dead aliens
    for (row = 0; row < ALIENS_COUNT; row++)
        for (col = 0; col < ALIENS_IN_ROW; col++)
            if (g_aliens[row][col].deathTime > 0 &&
                g_game.ticks - g_aliens[row][col].deathTime > ALIEN_EXPLOSION_TIME)
            {
                g_aliens[row][col].isHidden = true;
                hasDeaths = true;
            }

    // recalculate left and right edges for aliens
    // if any aliens have died (after explosion animation)
    if (hasDeaths)
        updateAliensEdges();
}

void updateAliensEdges()
{
    t_u8 col, row;
    t_u16 newLeftEdge, newRightEdge, newBottomEdge;
    bool stop;

    // start counting from initial values (not current ones)
    newLeftEdge = 0;

    newRightEdge = ALIENS_IN_ROW * 
        (g_renderRects[ALIEN_ENTITY].w + ALIENS_COL_SPACING) -
        ALIENS_COL_SPACING;

    newBottomEdge = ALIENS_COUNT *
        (g_renderRects[ALIEN_ENTITY].h + ALIENS_ROW_SPACING) -
        ALIENS_ROW_SPACING;

    // count left edge
    stop = false;
    for (col = 0; col < ALIENS_IN_ROW && !stop; col++)
    {
        for (row = 0; row < ALIENS_COUNT; row++)
        {
            // stop counting if current row contains alive alien
            if (!g_aliens[row][col].isHidden)
            {
                stop = true;
                break;
            }
        }
    
        if (!stop)
        {
            // reduce left edge further by one column width if whole column is dead aliens   
            // (dead means explosion animation has over from them)
            newLeftEdge -= (g_renderRects[ALIEN_ENTITY].w + ALIENS_COL_SPACING);
        }
    }

    // count right edge
    stop = false;
    for (col = ALIENS_IN_ROW - 1; col >= 0 && !stop; col--)
    {
        for (row = ALIENS_COUNT - 1; row >= 0; row--)
        {
            if (!g_aliens[row][col].isHidden)
            {
                stop = true;
                break;
            }
        }
    
        if (!stop)
        {
            // move right edge further by one column width if whole column is dead aliens   
            newRightEdge += g_renderRects[ALIEN_ENTITY].w + ALIENS_COL_SPACING;
        }
    }

    // count bottom edge
    stop = false;
    for (row = ALIENS_COUNT - 1; row >= 0; row--)
    {
        for (col = ALIENS_IN_ROW - 1; col >= 0 && !stop; col--)
        {
            if (!g_aliens[row][col].isHidden)
            {
                stop = true;
                break;
            }
        }
    
        if (!stop)
        {
            // reduce bottom edge by one row width if whole row is dead aliens   
            newBottomEdge -= (g_renderRects[ALIEN_ENTITY].h + ALIENS_ROW_SPACING);
        }
    }

    // update values only if counted values differ from current ones
    if (newLeftEdge > g_aliensData.leftEdge)
    {
        g_aliensData.leftEdge = newLeftEdge;
    }
    if (newRightEdge < g_aliensData.rightEdge)
    {
        g_aliensData.rightEdge = newRightEdge;
    }
    if (newBottomEdge < g_aliensData.bottomEdge)
    {
        g_aliensData.bottomEdge = newBottomEdge;
    }
}

void updateBullets()
{
    t_u8 i;

    for (i = 0; i < g_game.lastBullet; i++)
        if (g_bullets[i] != NULL)
        {
            if (g_bullets[i]->isVisible)
            {
                // mark bullets that are not visible anymore
                if (g_bullets[i]->position.y < g_renderRects[BULLET_ENTITY].h * -2 ||
                    g_bullets[i]->position.y > SCREEN_H + g_renderRects[BULLET_ENTITY].h * 2)
                {
                    g_bullets[i]->isVisible = false;
                }
                else if (g_bullets[i]->entity == PLAYER_ENTITY)
                {
                    g_bullets[i]->position.y -= BULLET_SPEED;
                }
                else if (g_bullets[i]->entity == ALIEN_ENTITY)
                {
                    g_bullets[i]->position.y += BULLET_SPEED;
                }

                // check player bullets collision with aliens
                if (g_bullets[i]->entity == PLAYER_ENTITY)
                {
                    t_u8 col, row; 
                    t_u16 alienX, alienY, alienRightEdge, alienBottomEdge;

                    for (row = 0; row < ALIENS_COUNT; row++)
                        for (col = 0; col < ALIENS_IN_ROW; col++)
                        {
                            if (g_aliens[row][col].deathTime == 0)
                            {
                                // getting an absolute x position in pixels
                                // that consists of padding-left value, whole group x-offset
                                // and total width (including spacing) of all aliens on the left
                                alienX = ALIENS_PADDING_LEFT + g_aliensData.position.x + 
                                    col * (g_renderRects[ALIEN_ENTITY].w + ALIENS_COL_SPACING);

                                // getting an absolute y position in pixels
                                // that consists of padding-top value, whole group y-offset
                                // and total height (including spacing) of all aliens above
                                alienY = ALIENS_PADDING_TOP + g_aliensData.position.y +
                                    row * (g_renderRects[ALIEN_ENTITY].h + ALIENS_ROW_SPACING);

                                // getting right and bottom edges
                                alienRightEdge = alienX + g_renderRects[ALIEN_ENTITY].w;
                                alienBottomEdge = alienY + g_renderRects[ALIEN_ENTITY].h;

                                // simple collision check:
                                // taking horizontal center of bullet as x-collision point
                                // and top edge (position.y) as y-collision point
                                if (g_bullets[i]->position.x + g_renderRects[BULLET_ENTITY].w / 2 > alienX &&
                                    g_bullets[i]->position.x + g_renderRects[BULLET_ENTITY].w / 2 < alienRightEdge &&
                                    g_bullets[i]->position.y > alienY &&
                                    g_bullets[i]->position.y < alienBottomEdge)
                                {
                                    killAlien(col, row);
                                    g_bullets[i]->isVisible = false;
                                }
                            }
                        }
                }
                // check aliens' bullets collision with player
                if (g_bullets[i]->entity == ALIEN_ENTITY)
                {
                    t_u16 playerRightEdge = g_playerData.position.x + g_renderRects[PLAYER_ENTITY].w;

                    if (g_bullets[i]->position.x + g_renderRects[BULLET_ENTITY].w / 2 > g_playerData.position.x &&
                        g_bullets[i]->position.x + g_renderRects[BULLET_ENTITY].w / 2 < playerRightEdge &&
                        g_bullets[i]->position.y + g_renderRects[BULLET_ENTITY].h > g_playerData.position.y)
                    {
                        // hit if player is not dead or invincible
                        if (g_playerData.deathTime == 0 &&
                               (g_playerData.lastHit == 0 ||
                                g_game.ticks - g_playerData.lastHit > PLAYER_INVINCIBLE_TIME))
                        {
                            hitPlayer();
                            g_bullets[i]->isVisible = false;
                        }
                    }
                }
            }
            else
            {
                // free invisible bullets'
                free(g_bullets[i]);
                g_bullets[i] = NULL;
            }
        }
}

void render()
{
    SDL_SetRenderDrawColor(g_SDLData.pRenderer, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear(g_SDLData.pRenderer);

    renderPlayer();
    renderLifeBar();
    renderAliens();
    renderBullets();

    SDL_RenderPresent(g_SDLData.pRenderer);
}

void renderPlayer()
{
    if (g_playerData.lastHit == 0 ||
        g_game.ticks - g_playerData.lastHit > PLAYER_INVINCIBLE_TIME ||
        (g_game.ticks - g_playerData.lastHit) / 12 % 2 == 0)
    {
        SDL_Rect renderRect;
        
        renderRect.x = g_playerData.position.x;
        renderRect.y = g_playerData.position.y;
        renderRect.w = g_renderRects[PLAYER_ENTITY].w;
        renderRect.h = g_renderRects[PLAYER_ENTITY].h;

        SDL_Rect *clip = g_playerData.deathTime > 0 
            ? &g_spritesData.explosionClip[(g_game.ticks - g_playerData.deathTime) / 8 % 4]
            : &g_spritesData.playerClip[0]; 

        SDL_RenderCopy(
            g_SDLData.pRenderer,
            g_spritesData.pTexture,
            clip,
            &renderRect);
    }
}

void renderLifeBar()
{
    t_u8 i;

    for (i = 0; i < g_playerData.lifes - 1; i++)
    {
        SDL_Rect renderRect;
        
        renderRect.x = 15 + (g_renderRects[PLAYER_LIFE_ENTITY].w + 10) * i;
        renderRect.y = 15;
        renderRect.w = g_renderRects[PLAYER_LIFE_ENTITY].w;
        renderRect.h = g_renderRects[PLAYER_LIFE_ENTITY].h;

        SDL_RenderCopy(
            g_SDLData.pRenderer,
            g_spritesData.pTexture,
            &g_spritesData.playerClip[0],
            &renderRect);
    }
}

void renderAlien(t_alienData *alien, t_i16 x, t_i16 y)
{
    SDL_Rect renderRect;
    
    renderRect.x = x;
    renderRect.y = y;
    renderRect.w = g_renderRects[ALIEN_ENTITY].w;
    renderRect.h = g_renderRects[ALIEN_ENTITY].h;

    SDL_Rect *clip = alien->deathTime > 0 
        ? &g_spritesData.explosionClip[(g_game.ticks - alien->deathTime) / (ALIEN_EXPLOSION_TIME / 4) % 4]
        : &g_alienClips[alien->entity][g_game.ticks / 40 % 2]; 

    SDL_RenderCopy(
        g_SDLData.pRenderer,
        g_spritesData.pTexture,
        clip,
        &renderRect);
}

void renderAliens()
{
    t_u8 row, col;

    for (row = 0; row < ALIENS_COUNT; row++)
        for (col = 0; col < ALIENS_IN_ROW; col++)
            if (!g_aliens[row][col].isHidden)
            {
                renderAlien(
                    // pointer to current alien struct
                    &g_aliens[row][col],
                    // x position
                    ALIENS_PADDING_LEFT + g_aliensData.position.x +
                        col * (g_renderRects[ALIEN_ENTITY].w + ALIENS_COL_SPACING),
                    // y position
                    ALIENS_PADDING_TOP + g_aliensData.position.y +
                        row * (g_renderRects[ALIEN_ENTITY].w + ALIENS_ROW_SPACING));
            }
}

void renderBullets()
{
    t_i8 i;

    for (i = 0; i < g_game.lastBullet; i++)
        if (g_bullets[i] != NULL && g_bullets[i]->isVisible)
        {
            SDL_Rect renderRect;
            renderRect.x = g_bullets[i]->position.x;
            renderRect.y = g_bullets[i]->position.y;
            renderRect.w = BULLET_W;
            renderRect.h = BULLET_H;

            SDL_RenderCopy(
                g_SDLData.pRenderer,
                g_spritesData.pTexture,
                &g_spritesData.bulletClip[0],
                &renderRect);
        }
}

void handleEvents()
{
    SDL_Event e;

    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
            g_game.isRunning = false;
        else if (e.type = SDL_KEYDOWN)
        {
            /* nothing yet */
        }
    }
}

void handleKeyStates()
{
    const Uint8 *state = SDL_GetKeyboardState(NULL);

    // handle movement
    if (g_playerData.deathTime == 0)
    {
        if (state[SDL_SCANCODE_LEFT])
            g_playerData.movement = MOVING_LEFT;
        else if (state[SDL_SCANCODE_RIGHT])
            g_playerData.movement = MOVING_RIGHT;
        else g_playerData.movement = NOT_MOVING;
    }

    // handle shooting
    if (state[SDL_SCANCODE_SPACE])
        g_playerData.isShooting = true;
    else g_playerData.isShooting = false;
}

int main()
{
    SDL_Event e;

    g_SDLData.pWindow = NULL;
    g_SDLData.pRenderer = NULL;
    g_spritesData.pTexture = NULL;
    g_game.isRunning = false;

    if (initSDL())
    {
        initGame();
        startGame();

        while (g_game.isRunning)
        {
            handleEvents();
            handleKeyStates();
            update();
            render();
        }       
    }
    else
    {
        printf("SDL init error\n");
        exit(1);
    }

    close();

    return 0;
}
