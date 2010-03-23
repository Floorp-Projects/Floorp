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
#include "jsbit.h"
#include "jsutil.h"
#include "jstask.h"
#include "jsversion.h"

JS_BEGIN_EXTERN_C

#define JSTRACE_XML         3

/*
 * One past the maximum trace kind.
 */
#define JSTRACE_LIMIT       4

const uintN JS_EXTERNAL_STRING_LIMIT = 8;

/*
 * Get the type of the external string or -1 if the string was not created
 * with JS_NewExternalString.
 */
extern intN
js_GetExternalStringGCType(JSString *str);

extern JS_FRIEND_API(uint32)
js_GetGCThingTraceKind(void *thing);

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
 * Allocate a new double jsval and store the result in *vp. vp must be a root.
 * The function does not copy the result into any weak root.
 */
extern JSBool
js_NewDoubleInRootedValue(JSContext *cx, jsdouble d, jsval *vp);

/*
 * Return a pointer to a new GC-allocated and weakly-rooted jsdouble number,
 * or null when the allocation fails.
 */
extern jsdouble *
js_NewWeaklyRootedDouble(JSContext *cx, jsdouble d);

#ifdef JS_TRACER
extern JSBool
js_ReserveObjects(JSContext *cx, size_t nobjects);
#endif

extern JSBool
js_LockGCThingRT(JSRuntime *rt, void *thing);

extern void
js_UnlockGCThingRT(JSRuntime *rt, void *thing);

extern bool
js_IsAboutToBeFinalized(void *thing);

/*
 * Macro to test if a traversal is the marking phase of GC to avoid exposing
 * ScriptFilenameEntry to traversal implementations.
 */
#define IS_GC_MARKING_TRACER(trc) ((trc)->callback == NULL)

#if JS_HAS_XML_SUPPORT
# define JS_IS_VALID_TRACE_KIND(kind) ((uint32)(kind) < JSTRACE_LIMIT)
#else
# define JS_IS_VALID_TRACE_KIND(kind) ((uint32)(kind) <= JSTRACE_STRING)
#endif

/*
 * Trace jsval when JSVAL_IS_OBJECT(v) can be a GC thing pointer tagged as a
 * jsval. NB: punning an arbitrary JSString * as an untagged (object-tagged)
 * jsval no longer works due to static int and unit strings!
 */
extern void
js_CallValueTracerIfGCThing(JSTracer *trc, jsval v);

extern void
js_TraceStackFrame(JSTracer *trc, JSStackFrame *fp);

extern JS_REQUIRES_STACK void
js_TraceRuntime(JSTracer *trc, JSBool allAtoms);

extern JS_REQUIRES_STACK JS_FRIEND_API(void)
js_TraceContext(JSTracer *trc, JSContext *acx);

/*
 * Schedule the GC call at a later safe point.
 */
#ifndef JS_THREADSAFE
# define js_TriggerGC(cx, gcLocked)    js_TriggerGC (cx)
#endif

extern void
js_TriggerGC(JSContext *cx, JSBool gcLocked);

/*
 * Kinds of js_GC invocation.
 */
typedef enum JSGCInvocationKind {
    /* Normal invocation. */
    GC_NORMAL           = 0,

    /*
     * Called from js_DestroyContext for last JSContext in a JSRuntime, when
     * it is imperative that rt->gcPoke gets cleared early in js_GC.
     */
    GC_LAST_CONTEXT     = 1,

    /*
     * Flag bit telling js_GC that the caller has already acquired rt->gcLock.
     * Currently, this flag is set for the invocation kinds that also preserve
     * atoms and weak roots, so we don't need another bit for GC_KEEP_ATOMS.
     */
    GC_LOCK_HELD        = 0x10,
    GC_KEEP_ATOMS       = GC_LOCK_HELD,

    /*
     * Called from js_SetProtoOrParent with a request to set an object's proto
     * or parent slot inserted on rt->setSlotRequests.
     */
    GC_SET_SLOT_REQUEST = GC_LOCK_HELD | 1,

    /*
     * Called from js_NewGCThing as a last-ditch GC attempt. See comments in
     * jsgc.c just before js_GC's definition for details.
     */
    GC_LAST_DITCH       = GC_LOCK_HELD | 2
} JSGCInvocationKind;

extern void
js_GC(JSContext *cx, JSGCInvocationKind gckind);

extern void
js_CallGCMarker(JSTracer *trc, void *thing, uint32 kind);

/*
 * The kind of GC thing with a finalizer. The external strings follow the
 * ordinary string to simplify js_GetExternalStringGCType.
 */
enum JSFinalizeGCThingKind {
    FINALIZE_OBJECT,
    FINALIZE_FUNCTION,
#if JS_HAS_XML_SUPPORT
    FINALIZE_XML,
#endif
    FINALIZE_STRING,
    FINALIZE_EXTERNAL_STRING0,
    FINALIZE_EXTERNAL_STRING1,
    FINALIZE_EXTERNAL_STRING2,
    FINALIZE_EXTERNAL_STRING3,
    FINALIZE_EXTERNAL_STRING4,
    FINALIZE_EXTERNAL_STRING5,
    FINALIZE_EXTERNAL_STRING6,
    FINALIZE_EXTERNAL_STRING7,
    FINALIZE_EXTERNAL_STRING_LAST = FINALIZE_EXTERNAL_STRING7,
    FINALIZE_LIMIT
};

static inline bool
IsFinalizableStringKind(unsigned thingKind)
{
    return unsigned(FINALIZE_STRING) <= thingKind &&
           thingKind <= unsigned(FINALIZE_EXTERNAL_STRING_LAST);
}

/*
 * Allocates a new GC thing. After a successful allocation the caller must
 * fully initialize the thing before calling any function that can potentially
 * trigger GC. This will ensure that GC tracing never sees junk values stored
 * in the partially initialized thing.
 */
extern void *
js_NewFinalizableGCThing(JSContext *cx, unsigned thingKind);

static inline JSObject *
js_NewGCObject(JSContext *cx)
{
    return (JSObject *) js_NewFinalizableGCThing(cx, FINALIZE_OBJECT);
}

static inline JSString *
js_NewGCString(JSContext *cx)
{
    return (JSString *) js_NewFinalizableGCThing(cx, FINALIZE_STRING);
}

static inline JSString *
js_NewGCExternalString(JSContext *cx, uintN type)
{
    JS_ASSERT(type < JS_EXTERNAL_STRING_LIMIT);
    type += FINALIZE_EXTERNAL_STRING0;
    return (JSString *) js_NewFinalizableGCThing(cx, type);
}

static inline JSFunction*
js_NewGCFunction(JSContext *cx)
{
    return (JSFunction *) js_NewFinalizableGCThing(cx, FINALIZE_FUNCTION);
}

#if JS_HAS_XML_SUPPORT
static inline JSXML *
js_NewGCXML(JSContext *cx)
{
    return (JSXML *) js_NewFinalizableGCThing(cx, FINALIZE_XML);
}
#endif

struct JSGCArena;
struct JSGCChunkInfo;

struct JSGCArenaList {
    JSGCArena       *head;          /* list start */
    JSGCArena       *cursor;        /* arena with free things */
    uint32          thingKind;      /* one of JSFinalizeGCThingKind */
    uint32          thingSize;      /* size of things to allocate on this list
                                     */
};

struct JSGCDoubleArenaList {
    JSGCArena       *head;          /* list start */
    JSGCArena       *cursor;        /* next arena with free cells */
};

struct JSGCFreeLists {
    JSGCThing       *doubles;
    JSGCThing       *finalizables[FINALIZE_LIMIT];

    void purge();
    void moveTo(JSGCFreeLists * another);

#ifdef DEBUG
    bool isEmpty() const {
        if (doubles)
            return false;
        for (size_t i = 0; i != JS_ARRAY_LENGTH(finalizables); ++i) {
            if (finalizables[i])
                return false;
        }
        return true;
    }
#endif
};

extern void
js_DestroyScriptsToGC(JSContext *cx, JSThreadData *data);

struct JSWeakRoots {
    /* Most recently created things by type, members of the GC's root set. */
    void            *finalizableNewborns[FINALIZE_LIMIT];
    jsdouble        *newbornDouble;

    /* Atom root for the last-looked-up atom on this context. */
    jsval           lastAtom;

    /* Root for the result of the most recent js_InternalInvoke call. */
    jsval           lastInternalResult;

    void mark(JSTracer *trc);
};

#define JS_CLEAR_WEAK_ROOTS(wr) (memset((wr), 0, sizeof(JSWeakRoots)))

#ifdef JS_THREADSAFE
class JSFreePointerListTask : public JSBackgroundTask {
    void *head;
  public:
    JSFreePointerListTask() : head(NULL) {}

    void add(void* ptr) {
        *(void**)ptr = head;
        head = ptr;
    }

    void run() {
        void *ptr = head;
        while (ptr) {
            void *next = *(void **)ptr;
            js_free(ptr);
            ptr = next;
        }
    }
};
#endif

extern void
js_FinalizeStringRT(JSRuntime *rt, JSString *str);

#if defined JS_GCMETER
const bool JS_WANT_GC_METER_PRINT = true;
#elif defined DEBUG
# define JS_GCMETER 1
const bool JS_WANT_GC_METER_PRINT = false;
#endif

#ifdef JS_GCMETER

struct JSGCArenaStats {
    uint32  alloc;          /* allocation attempts */
    uint32  localalloc;     /* allocations from local lists */
    uint32  retry;          /* allocation retries after running the GC */
    uint32  fail;           /* allocation failures */
    uint32  nthings;        /* live GC things */
    uint32  maxthings;      /* maximum of live GC cells */
    double  totalthings;    /* live GC things the GC scanned so far */
    uint32  narenas;        /* number of arena in list before the GC */
    uint32  newarenas;      /* new arenas allocated before the last GC */
    uint32  livearenas;     /* number of live arenas after the last GC */
    uint32  maxarenas;      /* maximum of allocated arenas */
    uint32  totalarenas;    /* total number of arenas with live things that
                               GC scanned so far */
};

struct JSGCStats {
    uint32  finalfail;  /* finalizer calls allocator failures */
    uint32  lockborn;   /* things born locked */
    uint32  lock;       /* valid lock calls */
    uint32  unlock;     /* valid unlock calls */
    uint32  depth;      /* mark tail recursion depth */
    uint32  maxdepth;   /* maximum mark tail recursion depth */
    uint32  cdepth;     /* mark recursion depth of C functions */
    uint32  maxcdepth;  /* maximum mark recursion depth of C functions */
    uint32  unmarked;   /* number of times marking of GC thing's children were
                           delayed due to a low C stack */
#ifdef DEBUG
    uint32  maxunmarked;/* maximum number of things with children to mark
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

    JSGCArenaStats  arenaStats[FINALIZE_LIMIT];
    JSGCArenaStats  doubleArenaStats;
};

extern JS_FRIEND_API(void)
js_DumpGCStats(JSRuntime *rt, FILE *fp);

#endif /* JS_GCMETER */

/*
 * This function is defined in jsdbgapi.cpp but is declared here to avoid
 * polluting jsdbgapi.h, a public API header, with internal functions.
 */
extern void
js_MarkTraps(JSTracer *trc);

JS_END_EXTERN_C

namespace js {

void
TraceObjectVector(JSTracer *trc, JSObject **vec, uint32 len);

inline void
TraceValues(JSTracer *trc, size_t len, jsval *vec, const char *name)
{
    for (jsval *vp = vec, *end = vp + len; vp < end; vp++) {
        jsval v = *vp;
        if (JSVAL_IS_TRACEABLE(v)) {
            JS_SET_TRACING_INDEX(trc, name, vp - vec);
            js_CallGCMarker(trc, JSVAL_TO_TRACEABLE(v), JSVAL_TRACE_KIND(v));
        }
    }
}

}

#endif /* jsgc_h___ */
