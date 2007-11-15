/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jsgc_h___
#define jsgc_h___
/*
 * JS Garbage Collector.
 */
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsdhash.h"
#include "jsutil.h"

JS_BEGIN_EXTERN_C

/* GC thing type indexes. */
#define GCX_OBJECT              0               /* JSObject */
#define GCX_STRING              1               /* JSString */
#define GCX_DOUBLE              2               /* jsdouble */
#define GCX_FUNCTION            3               /* JSFunction */
#define GCX_NAMESPACE           4               /* JSXMLNamespace */
#define GCX_QNAME               5               /* JSXMLQName */
#define GCX_XML                 6               /* JSXML */
#define GCX_EXTERNAL_STRING     8               /* JSString w/ external chars */

#define GCX_NTYPES_LOG2         4               /* type index bits */
#define GCX_NTYPES              JS_BIT(GCX_NTYPES_LOG2)

/* GC flag definitions, must fit in 8 bits (type index goes in the low bits). */
#define GCF_TYPEMASK    JS_BITMASK(GCX_NTYPES_LOG2)
#define GCF_MARK        JS_BIT(GCX_NTYPES_LOG2)
#define GCF_FINAL       JS_BIT(GCX_NTYPES_LOG2 + 1)
#define GCF_LOCKSHIFT   (GCX_NTYPES_LOG2 + 2)   /* lock bit shift */
#define GCF_LOCK        JS_BIT(GCF_LOCKSHIFT)   /* lock request bit in API */

extern JS_FRIEND_API(uint8 *)
js_GetGCThingFlags(void *thing);

/*
 * The sole purpose of the function is to preserve public API compatibility
 * in JS_GetStringBytes which takes only single JSString* argument.
 */
JSRuntime*
js_GetGCStringRuntime(JSString *str);

#if 1
/*
 * Since we're forcing a GC from JS_GC anyway, don't bother wasting cycles
 * loading oldval.  XXX remove implied force, fix jsinterp.c's "second arg
 * ignored", etc.
 */
#define GC_POKE(cx, oldval) ((cx)->runtime->gcPoke = JS_TRUE)
#else
#define GC_POKE(cx, oldval) ((cx)->runtime->gcPoke = JSVAL_IS_GCTHING(oldval))
#endif

extern JSBool
js_InitGC(JSRuntime *rt, uint32 maxbytes);

extern void
js_FinishGC(JSRuntime *rt);

extern intN
js_ChangeExternalStringFinalizer(JSStringFinalizeOp oldop,
                                 JSStringFinalizeOp newop);

extern JSBool
js_AddRoot(JSContext *cx, void *rp, const char *name);

extern JSBool
js_AddRootRT(JSRuntime *rt, void *rp, const char *name);

extern JSBool
js_RemoveRoot(JSRuntime *rt, void *rp);

#ifdef DEBUG
extern void
js_DumpNamedRoots(JSRuntime *rt,
                  void (*dump)(const char *name, void *rp, void *data),
                  void *data);
#endif

extern uint32
js_MapGCRoots(JSRuntime *rt, JSGCRootMapFun map, void *data);

/* Table of pointers with count valid members. */
typedef struct JSPtrTable {
    size_t      count;
    void        **array;
} JSPtrTable;

extern JSBool
js_RegisterCloseableIterator(JSContext *cx, JSObject *obj);

/*
 * The private JSGCThing struct, which describes a gcFreeList element.
 */
struct JSGCThing {
    JSGCThing   *next;
    uint8       *flagp;
};

#define GC_NBYTES_MAX           (10 * sizeof(JSGCThing))
#define GC_NUM_FREELISTS        (GC_NBYTES_MAX / sizeof(JSGCThing))
#define GC_FREELIST_NBYTES(i)   (((i) + 1) * sizeof(JSGCThing))
#define GC_FREELIST_INDEX(n)    (((n) / sizeof(JSGCThing)) - 1)

/*
 * Allocates a new GC thing of the given size. After a successful allocation
 * the caller must fully initialize the thing before calling any function that
 * can potentially trigger GC. This will ensure that GC tracing never sees junk
 * values stored in the partially initialized thing.
 */
extern void *
js_NewGCThing(JSContext *cx, uintN flags, size_t nbytes);

extern JSBool
js_LockGCThing(JSContext *cx, void *thing);

extern JSBool
js_LockGCThingRT(JSRuntime *rt, void *thing);

extern JSBool
js_UnlockGCThingRT(JSRuntime *rt, void *thing);

extern JSBool
js_IsAboutToBeFinalized(JSContext *cx, void *thing);

/*
 * Macro to test if a traversal is the marking phase of GC to avoid exposing
 * ScriptFilenameEntry to traversal implementations.
 */
#define IS_GC_MARKING_TRACER(trc) ((trc)->callback == NULL)

JS_STATIC_ASSERT(JSTRACE_STRING == 2);

#define JSTRACE_FUNCTION    3
#define JSTRACE_NAMESPACE   4
#define JSTRACE_QNAME       5
#define JSTRACE_XML         6

#if JS_HAS_XML_SUPPORT
# define JS_IS_VALID_TRACE_KIND(kind) ((uint32)(kind) <= JSTRACE_XML)
#else
# define JS_IS_VALID_TRACE_KIND(kind) ((uint32)(kind) <= JSTRACE_FUNCTION)
#endif

/*
 * JS_IS_VALID_TRACE_KIND assumes that JSTRACE_FUNCTION is the last non-xml
 * trace kind when JS_HAS_XML_SUPPORT is false.
 */
JS_STATIC_ASSERT(JSTRACE_FUNCTION + 1 == JSTRACE_NAMESPACE);

/*
 * Trace jsval when JSVAL_IS_OBJECT(v) can be an arbitrary GC thing casted as
 * JSVAL_OBJECT and js_GetGCThingFlags has to be used to find the real type
 * behind v.
 */
extern void
js_CallValueTracerIfGCThing(JSTracer *trc, jsval v);

extern void
js_TraceStackFrame(JSTracer *trc, JSStackFrame *fp);

extern void
js_TraceRuntime(JSTracer *trc, JSBool allAtoms);

extern JS_FRIEND_API(void)
js_TraceContext(JSTracer *trc, JSContext *acx);

/*
 * Kinds of js_GC invocation.
 */
typedef enum JSGCInvocationKind {
    /* Normal invocation. */
    GC_NORMAL,

    /*
     * Called from js_DestroyContext for last JSContext in a JSRuntime, when
     * it is imperative that rt->gcPoke gets cleared early in js_GC.
     */
    GC_LAST_CONTEXT,

    /*
     * Called from js_NewGCThing as a last-ditch GC attempt. See comments
     * before js_GC definition for details.
     */
    GC_LAST_DITCH
} JSGCInvocationKind;

extern void
js_GC(JSContext *cx, JSGCInvocationKind gckind);

/* Call this after succesful malloc of memory for GC-related things. */
extern void
js_UpdateMallocCounter(JSContext *cx, size_t nbytes);

#ifdef DEBUG_notme
#define JS_GCMETER 1
#endif

#ifdef JS_GCMETER

typedef struct JSGCStats {
#ifdef JS_THREADSAFE
    uint32  localalloc; /* number of succeeded allocations from local lists */
#endif
    uint32  alloc;      /* number of allocation attempts */
    uint32  retry;      /* allocation attempt retries after running the GC */
    uint32  retryhalt;  /* allocation retries halted by the branch callback */
    uint32  fail;       /* allocation failures */
    uint32  finalfail;  /* finalizer calls allocator failures */
    uint32  lockborn;   /* things born locked */
    uint32  lock;       /* valid lock calls */
    uint32  unlock;     /* valid unlock calls */
    uint32  depth;      /* mark tail recursion depth */
    uint32  maxdepth;   /* maximum mark tail recursion depth */
    uint32  cdepth;     /* mark recursion depth of C functions */
    uint32  maxcdepth;  /* maximum mark recursion depth of C functions */
    uint32  untraced;   /* number of times tracing of GC thing's children were
                           delayed due to a low C stack */
#ifdef DEBUG
    uint32  maxuntraced;/* maximum number of things with children to trace
                           later */
#endif
    uint32  maxlevel;   /* maximum GC nesting (indirect recursion) level */
    uint32  poke;       /* number of potentially useful GC calls */
    uint32  afree;      /* thing arenas freed so far */
    uint32  stackseg;   /* total extraordinary stack segments scanned */
    uint32  segslots;   /* total stack segment jsval slots scanned */
    uint32  nclose;     /* number of objects with close hooks */
    uint32  maxnclose;  /* max number of objects with close hooks */
    uint32  closelater; /* number of close hooks scheduled to run */
    uint32  maxcloselater; /* max number of close hooks scheduled to run */
} JSGCStats;

extern JS_FRIEND_API(void)
js_DumpGCStats(JSRuntime *rt, FILE *fp);

#endif /* JS_GCMETER */

typedef struct JSGCArenaInfo JSGCArenaInfo;
typedef struct JSGCArenaList JSGCArenaList;
typedef struct JSGCChunkInfo JSGCChunkInfo;

#ifdef JS_GCMETER
typedef struct JSGCArenaStats JSGCArenaStats;

struct JSGCArenaStats {
    uint32  narenas;        /* number of arena in list */
    uint32  maxarenas;      /* maximun number of allocated arenas */
    uint32  nthings;        /* number of allocates JSGCThing */
    uint32  maxthings;      /* maximum number number of allocates JSGCThing */
    uint32  totalnew;       /* number of succeeded calls to js_NewGCThing */
    uint32  freelen;        /* freeList lengths */
    uint32  recycle;        /* number of things recycled through freeList */
    uint32  totalarenas;    /* total number of arenas with live things that
                               GC scanned so far */
    uint32  totalfreelen;   /* total number of things that GC put to free
                               list so far */
};
#endif

struct JSGCArenaList {
    JSGCArenaInfo   *last;          /* last allocated GC arena */
    uint16          lastCount;      /* number of allocated things in the last
                                       arena */
    uint16          thingSize;      /* size of things to allocate on this list
                                     */
    JSGCThing       *freeList;      /* list of free GC things */
#ifdef JS_GCMETER
    JSGCArenaStats  stats;
#endif
};

struct JSWeakRoots {
    /* Most recently created things by type, members of the GC's root set. */
    void            *newborn[GCX_NTYPES];

    /* Atom root for the last-looked-up atom on this context. */
    jsval           lastAtom;

    /* Root for the result of the most recent js_InternalInvoke call. */
    jsval           lastInternalResult;
};

JS_STATIC_ASSERT(JSVAL_NULL == 0);
#define JS_CLEAR_WEAK_ROOTS(wr) (memset((wr), 0, sizeof(JSWeakRoots)))

JS_END_EXTERN_C

#endif /* jsgc_h___ */
