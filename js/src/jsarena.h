/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef jsarena_h___
#define jsarena_h___
/*
 * Lifetime-based fast allocation, inspired by much prior art, including
 * "Fast Allocation and Deallocation of Memory Based on Object Lifetimes"
 * David R. Hanson, Software -- Practice and Experience, Vol. 20(1).
 *
 * Also supports LIFO allocation (JS_ARENA_MARK/JS_ARENA_RELEASE).
 */
#include <stdlib.h>
#include "jstypes.h"
#include "jscompat.h"

JS_BEGIN_EXTERN_C

typedef struct JSArena JSArena;
typedef struct JSArenaPool JSArenaPool;

struct JSArena {
    JSArena     *next;          /* next arena for this lifetime */
    jsuword     base;           /* aligned base address, follows this header */
    jsuword     limit;          /* one beyond last byte in arena */
    jsuword     avail;          /* points to next available byte */
};

#ifdef JS_ARENAMETER
typedef struct JSArenaStats JSArenaStats;

struct JSArenaStats {
    JSArenaStats *next;         /* next in arenaStats list */
    char        *name;          /* name for debugging */
    uint32      narenas;        /* number of arenas in pool */
    uint32      nallocs;        /* number of JS_ARENA_ALLOCATE() calls */
    uint32      nreclaims;      /* number of reclaims from freeArenas */
    uint32      nmallocs;       /* number of malloc() calls */
    uint32      ndeallocs;      /* number of lifetime deallocations */
    uint32      ngrows;         /* number of JS_ARENA_GROW() calls */
    uint32      ninplace;       /* number of in-place growths */
    uint32      nreleases;      /* number of JS_ARENA_RELEASE() calls */
    uint32      nfastrels;      /* number of "fast path" releases */
    size_t      nbytes;         /* total bytes allocated */
    size_t      maxalloc;       /* maximum allocation size in bytes */
    double      variance;       /* size variance accumulator */
};
#endif

struct JSArenaPool {
    JSArena     first;          /* first arena in pool list */
    JSArena     *current;       /* arena from which to allocate space */
    size_t      arenasize;      /* net exact size of a new arena */
    jsuword     mask;           /* alignment mask (power-of-2 - 1) */
#ifdef JS_ARENAMETER
    JSArenaStats stats;
#endif
};

/*
 * If the including .c file uses only one power-of-2 alignment, it may define
 * JS_ARENA_CONST_ALIGN_MASK to the alignment mask and save a few instructions
 * per ALLOCATE and GROW.
 */
#ifdef JS_ARENA_CONST_ALIGN_MASK
#define JS_ARENA_ALIGN(pool, n)	(((jsuword)(n) + JS_ARENA_CONST_ALIGN_MASK) \
				 & ~JS_ARENA_CONST_ALIGN_MASK)

#define JS_INIT_ARENA_POOL(pool, name, size) \
	JS_InitArenaPool(pool, name, size, JS_ARENA_CONST_ALIGN_MASK + 1)
#else
#define JS_ARENA_ALIGN(pool, n) (((jsuword)(n) + (pool)->mask) & ~(pool)->mask)
#endif

#define JS_ARENA_ALLOCATE(p, pool, nb)                                        \
    JS_BEGIN_MACRO                                                            \
	JSArena *_a = (pool)->current;                                        \
	size_t _nb = JS_ARENA_ALIGN(pool, nb);                                \
	jsuword _p = _a->avail;                                               \
	jsuword _q = _p + _nb;                                                \
	if (_q > _a->limit)                                                   \
	    _p = (jsuword)JS_ArenaAllocate(pool, _nb);                        \
	else                                                                  \
	    _a->avail = _q;                                                   \
	p = (void *)_p;                                                       \
	JS_ArenaCountAllocation(pool, nb);                                    \
    JS_END_MACRO

#define JS_ARENA_GROW(p, pool, size, incr)                                    \
    JS_BEGIN_MACRO                                                            \
	JSArena *_a = (pool)->current;                                        \
	size_t _incr = JS_ARENA_ALIGN(pool, incr);                            \
	jsuword _p = _a->avail;                                               \
	jsuword _q = _p + _incr;                                              \
	if (_p == (jsuword)(p) + JS_ARENA_ALIGN(pool, size) &&                \
	    _q <= _a->limit) {                                                \
	    _a->avail = _q;                                                   \
	    JS_ArenaCountInplaceGrowth(pool, size, incr);                     \
	} else {                                                              \
	    p = JS_ArenaGrow(pool, p, size, incr);                            \
	}                                                                     \
	JS_ArenaCountGrowth(pool, size, incr);                                \
    JS_END_MACRO

#define JS_ARENA_MARK(pool)     ((void *) (pool)->current->avail)
#define JS_UPTRDIFF(p,q)	((jsuword)(p) - (jsuword)(q))

#ifdef DEBUG
#define JS_FREE_PATTERN         0xDA
#define JS_CLEAR_UNUSED(a)	(JS_ASSERT((a)->avail <= (a)->limit),         \
				 memset((void*)(a)->avail, JS_FREE_PATTERN,   \
					(a)->limit - (a)->avail))
#define JS_CLEAR_ARENA(a)       memset((void*)(a), JS_FREE_PATTERN,           \
				       (a)->limit - (jsuword)(a))
#else
#define JS_CLEAR_UNUSED(a)	/* nothing */
#define JS_CLEAR_ARENA(a)       /* nothing */
#endif

#define JS_ARENA_RELEASE(pool, mark)                                          \
    JS_BEGIN_MACRO                                                            \
	char *_m = (char *)(mark);                                            \
	JSArena *_a = (pool)->current;                                        \
	if (JS_UPTRDIFF(_m, _a) <= JS_UPTRDIFF(_a->avail, _a)) {              \
	    _a->avail = (jsuword)JS_ARENA_ALIGN(pool, _m);                    \
	    JS_CLEAR_UNUSED(_a);                                              \
	    JS_ArenaCountRetract(pool, _m);                                   \
	} else {                                                              \
	    JS_ArenaRelease(pool, _m);                                        \
	}                                                                     \
	JS_ArenaCountRelease(pool, _m);                                       \
    JS_END_MACRO

#ifdef JS_ARENAMETER
#define JS_COUNT_ARENA(pool,op) ((pool)->stats.narenas op)
#else
#define JS_COUNT_ARENA(pool,op)
#endif

#define JS_ARENA_DESTROY(pool, a, pnext)                                      \
    JS_BEGIN_MACRO                                                            \
	JS_COUNT_ARENA(pool,--);                                              \
	if ((pool)->current == (a)) (pool)->current = &(pool)->first;         \
	*(pnext) = (a)->next;                                                 \
	JS_CLEAR_ARENA(a);                                                    \
	free(a);                                                              \
	(a) = NULL;                                                           \
    JS_END_MACRO

/*
 * Initialize an arena pool with the given name for debugging and metering,
 * with a minimum size per arena of size bytes.
 */
JS_EXTERN_API(void)
JS_InitArenaPool(JSArenaPool *pool, const char *name, JSUint32 size,
		 JSUint32 align);

/*
 * Free the arenas in pool.  The user may continue to allocate from pool
 * after calling this function.  There is no need to call JS_InitArenaPool()
 * again unless JS_FinishArenaPool(pool) has been called.
 */
JS_EXTERN_API(void)
JS_FreeArenaPool(JSArenaPool *pool);

/*
 * Free the arenas in pool and finish using it altogether.
 */
JS_EXTERN_API(void)
JS_FinishArenaPool(JSArenaPool *pool);

/*
 * Compact all of the arenas in a pool so that no space is wasted.
 */
JS_EXTERN_API(void)
JS_CompactArenaPool(JSArenaPool *pool);

/*
 * Finish using arenas, freeing all memory associated with them.
 */
JS_EXTERN_API(void)
JS_ArenaFinish(void);

/*
 * Friend functions used by the JS_ARENA_*() macros.
 */
JS_EXTERN_API(void *)
JS_ArenaAllocate(JSArenaPool *pool, JSUint32 nb);

JS_EXTERN_API(void *)
JS_ArenaGrow(JSArenaPool *pool, void *p, JSUint32 size, JSUint32 incr);

JS_EXTERN_API(void)
JS_ArenaRelease(JSArenaPool *pool, char *mark);

#ifdef JS_ARENAMETER

#include <stdio.h>

JS_EXTERN_API(void)
JS_ArenaCountAllocation(JSArenaPool *pool, JSUint32 nb);

JS_EXTERN_API(void)
JS_ArenaCountInplaceGrowth(JSArenaPool *pool, JSUint32 size, JSUint32 incr);

JS_EXTERN_API(void)
JS_ArenaCountGrowth(JSArenaPool *pool, JSUint32 size, JSUint32incr);

JS_EXTERN_API(void)
JS_ArenaCountRelease(JSArenaPool *pool, char *mark);

JS_EXTERN_API(void)
JS_ArenaCountRetract(JSArenaPool *pool, char *mark);

JS_EXTERN_API(void)
JS_DumpArenaStats(FILE *fp);

#else  /* !JS_ARENAMETER */

#define JS_ArenaCountAllocation(ap, nb)                 /* nothing */
#define JS_ArenaCountInplaceGrowth(ap, size, incr)      /* nothing */
#define JS_ArenaCountGrowth(ap, size, incr)             /* nothing */
#define JS_ArenaCountRelease(ap, mark)                  /* nothing */
#define JS_ArenaCountRetract(ap, mark)                  /* nothing */

#endif /* !JS_ARENAMETER */

JS_END_EXTERN_C

#endif /* jsarena_h___ */
