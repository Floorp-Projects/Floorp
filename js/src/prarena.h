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

#ifndef prarena_h___
#define prarena_h___
/*
 * Lifetime-based fast allocation, inspired by much prior art, including
 * "Fast Allocation and Deallocation of Memory Based on Object Lifetimes"
 * David R. Hanson, Software -- Practice and Experience, Vol. 20(1).
 *
 * Also supports LIFO allocation (PR_ARENA_MARK/PR_ARENA_RELEASE).
 */
#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#include "jscompat.h"

#undef PR_ARENA_ALIGN
#undef PR_ARENA_ALLOCATE
#undef PR_ARENA_GROW
#undef PR_ARENA_MARK
#undef PR_CLEAR_UNUSED
#undef PR_CLEAR_ARENA
#undef PR_ARENA_RELEASE
#undef PR_COUNT_ARENA
#undef PR_ARENA_DESTROY
#undef PR_ArenaCountAllocation
#undef PR_ArenaCountInplaceGrowth
#undef PR_ArenaCountGrowth
#undef PR_ArenaCountRelease
#undef PR_ArenaCountRetract

PR_BEGIN_EXTERN_C

#undef PRArena
typedef struct PRArena PRArena;
struct PRArena {
    PRArena     *next;          /* next arena for this lifetime */
    pruword     base;           /* aligned base address, follows this header */
    pruword     limit;          /* one beyond last byte in arena */
    pruword     avail;          /* points to next available byte */
};

#ifdef PR_ARENAMETER
typedef struct PRArenaStats PRArenaStats;

struct PRArenaStats {
    PRArenaStats *next;         /* next in arenaStats list */
    char        *name;          /* name for debugging */
    uint32      narenas;        /* number of arenas in pool */
    uint32      nallocs;        /* number of PR_ARENA_ALLOCATE() calls */
    uint32      nreclaims;      /* number of reclaims from freeArenas */
    uint32      nmallocs;       /* number of malloc() calls */
    uint32      ndeallocs;      /* number of lifetime deallocations */
    uint32      ngrows;         /* number of PR_ARENA_GROW() calls */
    uint32      ninplace;       /* number of in-place growths */
    uint32      nreleases;      /* number of PR_ARENA_RELEASE() calls */
    uint32      nfastrels;      /* number of "fast path" releases */
    size_t      nbytes;         /* total bytes allocated */
    size_t      maxalloc;       /* maximum allocation size in bytes */
    double      variance;       /* size variance accumulator */
};
#endif

#undef PRArenaPool
typedef struct PRArenaPool {
    PRArena     first;          /* first arena in pool list */
    PRArena     *current;       /* arena from which to allocate space */
    size_t      arenasize;      /* net exact size of a new arena */
    pruword     mask;           /* alignment mask (power-of-2 - 1) */
#ifdef PR_ARENAMETER
    PRArenaStats stats;
#endif
} PRArenaPool;

/*
 * If the including .c file uses only one power-of-2 alignment, it may define
 * PR_ARENA_CONST_ALIGN_MASK to the alignment mask and save a few instructions
 * per ALLOCATE and GROW.
 */
#ifdef PR_ARENA_CONST_ALIGN_MASK
#define PR_ARENA_ALIGN(pool, n)	(((pruword)(n) + PR_ARENA_CONST_ALIGN_MASK) \
				 & ~PR_ARENA_CONST_ALIGN_MASK)

#define PR_INIT_ARENA_POOL(pool, name, size) \
	PR_InitArenaPool(pool, name, size, PR_ARENA_CONST_ALIGN_MASK + 1)
#else
#define PR_ARENA_ALIGN(pool, n) (((pruword)(n) + (pool)->mask) & ~(pool)->mask)
#endif

#define PR_ARENA_ALLOCATE(p, pool, nb)                                        \
    PR_BEGIN_MACRO                                                            \
	PRArena *_a = (pool)->current;                                        \
	size_t _nb = PR_ARENA_ALIGN(pool, nb);                                \
	pruword _p = _a->avail;                                               \
	pruword _q = _p + _nb;                                                \
	if (_q > _a->limit)                                                   \
	    _p = (pruword)PR_ArenaAllocate(pool, _nb);                        \
	else                                                                  \
	    _a->avail = _q;                                                   \
	p = (void *)_p;                                                       \
	PR_ArenaCountAllocation(pool, nb);                                    \
    PR_END_MACRO

#define PR_ARENA_GROW(p, pool, size, incr)                                    \
    PR_BEGIN_MACRO                                                            \
	PRArena *_a = (pool)->current;                                        \
	size_t _incr = PR_ARENA_ALIGN(pool, incr);                            \
	pruword _p = _a->avail;                                               \
	pruword _q = _p + _incr;                                              \
	if (_p == (pruword)(p) + PR_ARENA_ALIGN(pool, size) &&                \
	    _q <= _a->limit) {                                                \
	    _a->avail = _q;                                                   \
	    PR_ArenaCountInplaceGrowth(pool, size, incr);                     \
	} else {                                                              \
	    p = PR_ArenaGrow(pool, p, size, incr);                            \
	}                                                                     \
	PR_ArenaCountGrowth(pool, size, incr);                                \
    PR_END_MACRO

#define PR_ARENA_MARK(pool)     ((void *) (pool)->current->avail)
#define PR_UPTRDIFF(p,q)	((pruword)(p) - (pruword)(q))

#ifdef DEBUG
#define PR_FREE_PATTERN         0xDA
#define PR_CLEAR_UNUSED(a)	(PR_ASSERT((a)->avail <= (a)->limit),         \
				 memset((void*)(a)->avail, PR_FREE_PATTERN,   \
					(a)->limit - (a)->avail))
#define PR_CLEAR_ARENA(a)       memset((void*)(a), PR_FREE_PATTERN,           \
				       (a)->limit - (pruword)(a))
#else
#define PR_CLEAR_UNUSED(a)	/* nothing */
#define PR_CLEAR_ARENA(a)       /* nothing */
#endif

#define PR_ARENA_RELEASE(pool, mark)                                          \
    PR_BEGIN_MACRO                                                            \
	char *_m = (char *)(mark);                                            \
	PRArena *_a = (pool)->current;                                        \
	if (PR_UPTRDIFF(_m, _a) <= PR_UPTRDIFF(_a->avail, _a)) {              \
	    _a->avail = (pruword)PR_ARENA_ALIGN(pool, _m);                    \
	    PR_CLEAR_UNUSED(_a);                                              \
	    PR_ArenaCountRetract(pool, _m);                                   \
	} else {                                                              \
	    PR_ArenaRelease(pool, _m);                                        \
	}                                                                     \
	PR_ArenaCountRelease(pool, _m);                                       \
    PR_END_MACRO

#ifdef PR_ARENAMETER
#define PR_COUNT_ARENA(pool,op) ((pool)->stats.narenas op)
#else
#define PR_COUNT_ARENA(pool,op)
#endif

#define PR_ARENA_DESTROY(pool, a, pnext)                                      \
    PR_BEGIN_MACRO                                                            \
	PR_COUNT_ARENA(pool,--);                                              \
	if ((pool)->current == (a)) (pool)->current = &(pool)->first;         \
	*(pnext) = (a)->next;                                                 \
	PR_CLEAR_ARENA(a);                                                    \
	free(a);                                                              \
	(a) = NULL;                                                           \
    PR_END_MACRO

/*
 * Initialize an arena pool with the given name for debugging and metering,
 * with a minimum size per arena of size bytes.
 */
extern PR_PUBLIC_API(void)
PR_InitArenaPool(PRArenaPool *pool, const char *name, size_t size,
		 size_t align);

/*
 * Free the arenas in pool.  The user may continue to allocate from pool
 * after calling this function.  There is no need to call PR_InitArenaPool()
 * again unless PR_FinishArenaPool(pool) has been called.
 */
extern PR_PUBLIC_API(void)
PR_FreeArenaPool(PRArenaPool *pool);

/*
 * Free the arenas in pool and finish using it altogether.
 */
extern PR_PUBLIC_API(void)
PR_FinishArenaPool(PRArenaPool *pool);

/*
 * Compact all of the arenas in a pool so that no space is wasted.
 */
extern PR_PUBLIC_API(void)
PR_CompactArenaPool(PRArenaPool *pool);

/*
 * Finish using arenas, freeing all memory associated with them.
 */
extern PR_PUBLIC_API(void)
PR_ArenaFinish(void);

/*
 * Friend functions used by the PR_ARENA_*() macros.
 */
extern PR_PUBLIC_API(void *)
PR_ArenaAllocate(PRArenaPool *pool, size_t nb);

extern PR_PUBLIC_API(void *)
PR_ArenaGrow(PRArenaPool *pool, void *p, size_t size, size_t incr);

extern PR_PUBLIC_API(void)
PR_ArenaRelease(PRArenaPool *pool, char *mark);

#ifdef PR_ARENAMETER

#include <stdio.h>

extern PR_PUBLIC_API(void)
PR_ArenaCountAllocation(PRArenaPool *pool, size_t nb);

extern PR_PUBLIC_API(void)
PR_ArenaCountInplaceGrowth(PRArenaPool *pool, size_t size, size_t incr);

extern PR_PUBLIC_API(void)
PR_ArenaCountGrowth(PRArenaPool *pool, size_t size, size_t incr);

extern PR_PUBLIC_API(void)
PR_ArenaCountRelease(PRArenaPool *pool, char *mark);

extern PR_PUBLIC_API(void)
PR_ArenaCountRetract(PRArenaPool *pool, char *mark);

extern PR_PUBLIC_API(void)
PR_DumpArenaStats(FILE *fp);

#else  /* !PR_ARENAMETER */

#define PR_ArenaCountAllocation(ap, nb)                 /* nothing */
#define PR_ArenaCountInplaceGrowth(ap, size, incr)      /* nothing */
#define PR_ArenaCountGrowth(ap, size, incr)             /* nothing */
#define PR_ArenaCountRelease(ap, mark)                  /* nothing */
#define PR_ArenaCountRetract(ap, mark)                  /* nothing */

#endif /* !PR_ARENAMETER */

PR_END_EXTERN_C

#endif /* prarena_h___ */
