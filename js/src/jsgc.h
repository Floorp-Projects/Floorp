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

#ifndef jsgc_h___
#define jsgc_h___
/*
 * JS Garbage Collector.
 */
#include "jspubtd.h"

PR_BEGIN_EXTERN_C

/* GC thing type indexes. */
#define GCX_OBJECT	0			/* JSObject */
#define GCX_STRING	1			/* JSString */
#define GCX_DOUBLE	2			/* jsdouble */
#define GCX_DECIMAL     3                       /* JSDecimal */
#define GCX_NTYPES      4

/* GC flag definitions (type index goes in low bits). */
#define GCF_TYPEMASK	PR_BITMASK(2)		/* use low bits for type */
#define GCF_MARK	PR_BIT(2)		/* mark bit */
#define GCF_FINAL	PR_BIT(3)		/* in finalization bit */
#define GCF_LOCKBIT	4			/* lock bit shift and mask */
#define GCF_LOCKMASK	(PR_BITMASK(4) << GCF_LOCKBIT)
#define GCF_LOCK	PR_BIT(GCF_LOCKBIT)	/* lock request bit in API */

#if 1
/*
 * Since we're forcing a GC from JS_GC anyway, don't bother wasting cycles
 * loading oldval.  XXX remove implied force, poke in addroot/removeroot, &c
 */
#define GC_POKE(cx, oldval) ((cx)->runtime->gcPoke = JS_TRUE)
#else
#define GC_POKE(cx, oldval) ((cx)->runtime->gcPoke = JSVAL_IS_GCTHING(oldval))
#endif

extern JSBool
js_InitGC(JSRuntime *rt, uint32 maxbytes);

extern void
js_FinishGC(JSRuntime *rt);

extern JSBool
js_AddRoot(JSContext *cx, void *rp);

extern JSBool
js_AddNamedRoot(JSContext *cx, void *rp, const char *name);

extern JSBool
js_RemoveRoot(JSContext *cx, void *rp);

extern void *
js_AllocGCThing(JSContext *cx, uintN flags);

extern JSBool
js_LockGCThing(JSContext *cx, void *thing);

extern JSBool
js_UnlockGCThing(JSContext *cx, void *thing);

extern void
js_ForceGC(JSContext *cx);

extern void
js_GC(JSContext *cx);

#ifdef JS_GCMETER

typedef struct JSGCStats {
    uint32  alloc;      /* number of allocation attempts */
    uint32  freelen;    /* gcFreeList length */
    uint32  recycle;    /* number of things recycled through gcFreeList */
    uint32  retry;      /* allocation attempt retries after running the GC */
    uint32  fail;       /* allocation failures */
    uint32  lock;       /* valid lock calls */
    uint32  unlock;     /* valid unlock calls */
    uint32  stuck;      /* stuck reference counts seen by lock calls */
    uint32  unstuck;    /* unlock calls that saw a stuck lock count */
    uint32  depth;      /* mark recursion depth */
    uint32  maxdepth;   /* maximum mark recursion depth */
    uint32  maxlevel;   /* maximum GC nesting (indirect recursion) level */
    uint32  poke;       /* number of potentially useful GC calls */
    uint32  nopoke;     /* useless GC calls where js_PokeGC was not set */
    uint32  badarena;   /* thing arena corruption */
    uint32  badflag;    /* flags arena corruption */
    uint32  afree;      /* thing arenas freed so far */
    uint32  fafree;     /* flags arenas freed so far */
} JSGCStats;

extern void
js_DumpGCStats(JSRuntime *rt, FILE *fp);

#endif /* JS_GCMETER */

PR_END_EXTERN_C

#endif /* jsgc_h___ */
