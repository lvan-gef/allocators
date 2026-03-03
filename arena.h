#ifndef ARENA_H
#define ARENA_H

#ifdef C_ARENA_IMPLEMENTATION
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct arena_block arena_block;
struct arena_block {
    uint64_t pos;
    uint64_t cap;
    arena_block *next;
    arena_block *prev;
};

typedef struct {
    uint64_t align;
    uint64_t block_size;
    arena_block *first;
    arena_block *current;
} arena;

typedef struct {
    arena_block *block;
    uint64_t pos;
} arena_mark;

#ifndef ARENA_SIZE
#define ARENA_SIZE UINT64_C(4096)
#endif // !ARENA_SIZE

static arena_block *new_block_(uint64_t cap);
static bool is_power_of_two_(uintptr_t x);

arena *arena_alloc(uint64_t cap) {
    assert(cap > 0 && "cap must be > 0");

    arena_block *block = new_block_(cap);
    if (!block) {
        return NULL;
    }

    arena *arena = malloc(sizeof(*arena));
    if (!arena) {
        free(block);
        return NULL;
    }

    arena->first = block;
    arena->current = block;
    arena->align = 0;
    arena->block_size = cap;

    return arena;
}

void arena_release(arena *arena) {
    assert(arena && "arena can not be NULL");

    arena_block *block = arena->first;

    while (block) {
        arena_block *next = block->next;
        free(block);
        block = next;
    }

    free(arena);
}

void arena_set_auto_align(arena *arena, uint64_t align) {
    assert(arena && "arena can not be NULL");

    if (!align) {
        arena->align = 0;
        return;
    }

    assert(is_power_of_two_((uintptr_t)align) && "align must be a power of 2");
    arena->align = align;
    if (align > arena->block_size) {
        arena->block_size = align;
    }
}

arena_mark arena_marker(arena *arena) {
    assert(arena && "arena can not be NULL");

    arena_mark mark = {.block = arena->current, .pos = arena->current->pos};
    assert(mark.block && "arena block can not be NULL");

    return mark;
}

void *arena_push_no_zero(arena *arena, uint64_t size) {
    assert(arena && "arena can not be NULL");
    assert(size > 0 && "size must be > 0");

    uint64_t align_pos = arena->current->pos;

    uint64_t modulo = align_pos & (size - 1);
    if (modulo) {
        align_pos += size - modulo;
    }

    if (arena->align > 0) {
        uint64_t remainder = align_pos % arena->align;
        if (remainder) {
            align_pos += arena->align - remainder;
        }
    }

    if (align_pos + size > arena->current->cap) {
        uint64_t cap = arena->block_size;
        if (size > cap) {
            cap = size;
        }

        arena_block *block = new_block_(cap);
        if (!block) {
            return NULL;
        }

        block->prev = arena->current;
        arena->current->next = block;
        arena->current = block;
        align_pos = 0;
    }

    unsigned char *base = (unsigned char *)(arena->current + 1);
    void *ptr = base + align_pos;
    arena->current->pos = align_pos + size;

    assert(ptr && "ptr can not be NULL");
    return ptr;
}

void *ArenaPush(arena *arena, uint64_t size) {
    assert(arena && "arena can not be NULL");
    assert(size > 0 && "size must be > 0");

    void *ptr = arena_push_no_zero(arena, size);
    if (!ptr) {
        return NULL;
    }

    memset(ptr, 0, size);
    return ptr;
}

void arena_pop_to_mark(arena *arena, arena_mark mark) {
    assert(arena && "arena can not be NULL");
    assert(arena->current && "arena->current can not be NULL");
    assert(arena->current == mark.block && "arena is in a different block then mark");

    arena_block *block = arena->current;

    block->pos = mark.pos;
    arena->current = block;
}

void ArenaClear(arena *arena) {
    assert(arena && "arena can not be NULL");

    arena_block *block = arena->first->next;
    while (block) {
        arena_block *next = block->next;
        free(block);
        block = next;
    }

    arena->first->next = NULL;
    arena->first->prev = NULL;
    arena->first->pos = 0;
    arena->current = arena->first;
}

static bool is_power_of_two_(uintptr_t x) {
	return (x & (x - 1)) == 0;
}

static arena_block *new_block_(uint64_t cap) {
    assert(cap > 0 && "cap must be > 0");

    arena_block *block = malloc(sizeof(*block) + cap);
    if (!block) {
        return NULL;
    }

    block->next = NULL;
    block->prev = NULL;
    block->pos = 0;
    block->cap = cap;

    return block;
}
#endif // !C_ARENA_IMPLEMENTATION

#endif // !ARENA_H
