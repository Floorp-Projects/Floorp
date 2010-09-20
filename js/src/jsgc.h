/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include <setjmp.h>

#include "jstypes.h"
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsdhash.h"
#include "jsbit.h"
#include "jsgcchunk.h"
#include "jsutil.h"
#include "jsvector.h"
#include "jsversion.h"
#include "jsobj.h"
#include "jsfun.h"
#include "jsgcstats.h"

#define JSTRACE_XML         2

/*
 * One past the maximum trace kind.
 */
#define JSTRACE_LIMIT       3

/*
 * Lower limit after which we limit the heap growth
 */
const size_t GC_ARENA_ALLOCATION_TRIGGER = 25 * js::GC_CHUNK_SIZE;

/*
 * A GC is triggered once the number of newly allocated arenas 
 * is 1.5 times the number of live arenas after the last GC.
 * (Starting after the lower limit of GC_ARENA_ALLOCATION_TRIGGER)
 */
const float GC_HEAP_GROWTH_FACTOR = 1.5;

const uintN JS_EXTERNAL_STRING_LIMIT = 8;

/*
 * Get the type of the external string or -1 if the string was not created
 * with JS_NewExternalString.
 */
extern intN
js_GetExternalStringGCType(JSString *str);

extern JS_FRIEND_API(uint32)
js_GetGCThingTraceKind(void *thing);

extern size_t
ThingsPerArena(size_t thingSize);

/*
 * The sole purpose of the function is to preserve public API compatibility
 * in JS_GetStringBytes which takes only single JSString* argument.
 */
JSRuntime *
js_GetGCThingRuntime(void *thing);

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
js_AddRoot(JSContext *cx, js::Value *vp, const char *name);

extern JSBool
js_AddGCThingRoot(JSContext *cx, void **rp, const char *name);

#ifdef DEBUG
extern void
js_DumpNamedRoots(JSRuntime *rt,
                  void (*dump)(const char *name, void *rp, JSGCRootType type, void *data),
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

#ifdef JS_TRACER
extern JSBool
js_ReserveObjects(JSContext *cx, size_t nobjects);
#endif

extern JSBool
js_LockGCThingRT(JSRuntime *rt, void *thing);

extern void
js_UnlockGCThingRT(JSRuntime *rt, void *thing);

extern JS_FRIEND_API(bool)
js_IsAboutToBeFinalized(void *thing);

extern JS_FRIEND_API(bool)
js_GCThingIsMarked(void *thing, uint32 color);

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

extern void
js_TraceStackFrame(JSTracer *trc, JSStackFrame *fp);

namespace js {

extern JS_REQUIRES_STACK void
MarkRuntime(JSTracer *trc);

extern void
TraceRuntime(JSTracer *trc);

extern JS_REQUIRES_STACK JS_FRIEND_API(void)
MarkContext(JSTracer *trc, JSContext *acx);

/* Must be called with GC lock taken. */
extern void
TriggerGC(JSRuntime *rt);

} /* namespace js */

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
     */
    GC_LOCK_HELD        = 0x10
} JSGCInvocationKind;

extern void
js_GC(JSContext *cx, JSGCInvocationKind gckind);

#ifdef JS_THREADSAFE
/*
 * This is a helper for code at can potentially run outside JS request to
 * ensure that the GC is not running when the function returns.
 *
 * This function must be called with the GC lock held.
 */
extern void
js_WaitForGC(JSRuntime *rt);

#else /* !JS_THREADSAFE */

# define js_WaitForGC(rt)    ((void) 0)

#endif

/*
 * The kind of GC thing with a finalizer. The external strings follow the
 * ordinary string to simplify js_GetExternalStringGCType.
 */
enum JSFinalizeGCThingKind {
    FINALIZE_OBJECT0,
    FINALIZE_OBJECT2,
    FINALIZE_OBJECT4,
    FINALIZE_OBJECT8,
    FINALIZE_OBJECT12,
    FINALIZE_OBJECT16,
    FINALIZE_OBJECT_LAST = FINALIZE_OBJECT16,
    FINALIZE_FUNCTION,
#if JS_HAS_XML_SUPPORT
    FINALIZE_XML,
#endif
    FINALIZE_SHORT_STRING,
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
    return unsigned(FINALIZE_SHORT_STRING) <= thingKind &&
           thingKind <= unsigned(FINALIZE_EXTERNAL_STRING_LAST);
}

/*
 * Allocates a new GC thing. After a successful allocation the caller must
 * fully initialize the thing before calling any function that can potentially
 * trigger GC. This will ensure that GC tracing never sees junk values stored
 * in the partially initialized thing.
 */
extern void *
js_NewFinalizableGCThing(JSContext *cx, JSFinalizeGCThingKind thingKind);

/* Get the kind which was used when making a GC thing. */
extern JSFinalizeGCThingKind
js_KindFromGCThing(const void *thing);

/* Maximum number of fixed slots for an object. */
const size_t JSOBJECT_FIXED_SLOTS_LIMIT = 16;

/* Capacity for js_GCObjectSlotsToThingKind */
const size_t SLOTS_TO_THING_KIND_LIMIT = 33;

/* Get the best kind to use when making an object with the given slot count. */
static inline JSFinalizeGCThingKind
js_GetGCObjectKind(size_t numSlots)
{
    extern JSFinalizeGCThingKind js_GCObjectSlotsToThingKind[];

    if (numSlots >= SLOTS_TO_THING_KIND_LIMIT)
        return FINALIZE_OBJECT0;
    return js_GCObjectSlotsToThingKind[numSlots];
}

/* Get the number of fixed slots and initial capacity associated with a kind. */
static inline size_t
js_GetGCKindSlots(JSFinalizeGCThingKind thingKind)
{
    /* Using a switch in hopes that thingKind will usually be a compile-time constant. */
    switch (thingKind) {
      case FINALIZE_OBJECT0:
        return 0;
      case FINALIZE_OBJECT2:
        return 2;
      case FINALIZE_OBJECT4:
        return 4;
      case FINALIZE_OBJECT8:
        return 8;
      case FINALIZE_OBJECT12:
        return 12;
      case FINALIZE_OBJECT16:
        return 16;
      default:
        JS_NOT_REACHED("Bad object finalize kind");
        return 0;
    }
}

static inline JSObject *
js_NewGCObject(JSContext *cx, JSFinalizeGCThingKind thingKind)
{
    JS_ASSERT(thingKind >= FINALIZE_OBJECT0 && thingKind <= FINALIZE_OBJECT_LAST);
    JSObject *obj = (JSObject *) js_NewFinalizableGCThing(cx, thingKind);
    if (obj)
        obj->capacity = js_GetGCKindSlots(thingKind);
    return obj;
}

static inline JSString *
js_NewGCString(JSContext *cx)
{
    return (JSString *) js_NewFinalizableGCThing(cx, FINALIZE_STRING);
}

struct JSShortString;

static inline JSShortString *
js_NewGCShortString(JSContext *cx)
{
    return (JSShortString *) js_NewFinalizableGCThing(cx, FINALIZE_SHORT_STRING);
}

static inline JSString *
js_NewGCExternalString(JSContext *cx, uintN type)
{
    JS_ASSERT(type < JS_EXTERNAL_STRING_LIMIT);
    type += FINALIZE_EXTERNAL_STRING0;
    return (JSString *) js_NewFinalizableGCThing(cx, JSFinalizeGCThingKind(type));
}

static inline JSFunction *
js_NewGCFunction(JSContext *cx)
{
    JSFunction* obj = (JSFunction *)js_NewFinalizableGCThing(cx, FINALIZE_FUNCTION);

    if (obj) {
        obj->capacity = JSObject::FUN_CLASS_RESERVED_SLOTS;
#ifdef DEBUG
        memset((uint8 *) obj + sizeof(JSObject), JS_FREE_PATTERN,
               sizeof(JSFunction) - sizeof(JSObject));
#endif
    }

    return obj;
}

#if JS_HAS_XML_SUPPORT
static inline JSXML *
js_NewGCXML(JSContext *cx)
{
    return (JSXML *) js_NewFinalizableGCThing(cx, FINALIZE_XML);
}
#endif

struct JSGCArena;

struct JSGCArenaList {
    JSGCArena       *head;          /* list start */
    JSGCArena       *cursor;        /* arena with free things */
    uint32          thingKind;      /* one of JSFinalizeGCThingKind */
    uint32          thingSize;      /* size of things to allocate on this list
                                     */
};

struct JSGCFreeLists {
    JSGCThing       *finalizables[FINALIZE_LIMIT];

    void purge();
    void moveTo(JSGCFreeLists * another);

#ifdef DEBUG
    bool isEmpty() const {
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

namespace js {

#ifdef JS_THREADSAFE

/*
 * During the finalization we do not free immediately. Rather we add the
 * corresponding pointers to a buffer which we later release on a separated
 * thread.
 *
 * The buffer is implemented as a vector of 64K arrays of pointers, not as a
 * simple vector, to avoid realloc calls during the vector growth and to not
 * bloat the binary size of the inlined freeLater method. Any OOM during
 * buffer growth results in the pointer being freed immediately.
 */
class GCHelperThread {
    static const size_t FREE_ARRAY_SIZE = size_t(1) << 16;
    static const size_t FREE_ARRAY_LENGTH = FREE_ARRAY_SIZE / sizeof(void *);

    PRThread*         thread;
    PRCondVar*        wakeup;
    PRCondVar*        sweepingDone;
    bool              shutdown;
    bool              sweeping;

    Vector<void **, 16, js::SystemAllocPolicy> freeVector;
    void            **freeCursor;
    void            **freeCursorEnd;

    JS_FRIEND_API(void)
    replenishAndFreeLater(void *ptr);

    static void freeElementsAndArray(void **array, void **end) {
        JS_ASSERT(array <= end);
        for (void **p = array; p != end; ++p)
            js_free(*p);
        js_free(array);
    }

    static void threadMain(void* arg);

    void threadLoop(JSRuntime *rt);
    void doSweep();

  public:
    GCHelperThread()
      : thread(NULL),
        wakeup(NULL),
        sweepingDone(NULL),
        shutdown(false),
        sweeping(false),
        freeCursor(NULL),
        freeCursorEnd(NULL) { }
    
    bool init(JSRuntime *rt);
    void finish(JSRuntime *rt);
    
    /* Must be called with GC lock taken. */
    void startBackgroundSweep(JSRuntime *rt);
    
    /* Must be called outside the GC lock. */
    void waitBackgroundSweepEnd(JSRuntime *rt);
    
    void freeLater(void *ptr) {
        JS_ASSERT(!sweeping);
        js_free(ptr);
        /*
        if (freeCursor != freeCursorEnd)
            *freeCursor++ = ptr;
        else
            replenishAndFreeLater(ptr);
        */
    }
};

#endif /* JS_THREADSAFE */


struct GCChunkInfo;

struct GCChunkHasher {
    typedef jsuword Lookup;

    /*
     * Strip zeros for better distribution after multiplying by the golden
     * ratio.
     */
    static HashNumber hash(jsuword chunk) {
        JS_ASSERT(!(chunk & GC_CHUNK_MASK));
        return HashNumber(chunk >> GC_CHUNK_SHIFT);
    }

    static bool match(jsuword k, jsuword l) {
        JS_ASSERT(!(k & GC_CHUNK_MASK));
        JS_ASSERT(!(l & GC_CHUNK_MASK));
        return k == l;
    }
};

typedef HashSet<jsuword, GCChunkHasher, SystemAllocPolicy> GCChunkSet;
typedef Vector<GCChunkInfo *, 32, SystemAllocPolicy> GCChunkInfoVector;

struct ConservativeGCThreadData {

    /*
     * The GC scans conservatively between JSThreadData::nativeStackBase and
     * nativeStackTop unless the latter is NULL.
     */
    jsuword             *nativeStackTop;

    union {
        jmp_buf         jmpbuf;
        jsuword         words[JS_HOWMANY(sizeof(jmp_buf), sizeof(jsuword))];
    } registerSnapshot;

    /*
     * Cycle collector uses this to communicate that the native stack of the
     * GC thread should be scanned only if the thread have more than the given
     * threshold of requests.
     */
    unsigned requestThreshold;

    JS_NEVER_INLINE void recordStackTop();

#ifdef JS_THREADSAFE
    void updateForRequestEnd(unsigned suspendCount) {
        if (suspendCount)
            recordStackTop();
        else
            nativeStackTop = NULL;
    }
#endif

    bool hasStackToScan() const {
        return !!nativeStackTop;
    }
};

struct GCMarker : public JSTracer {
  private:
    /* The color is only applied to objects, functions and xml. */
    uint32 color;

    /* See comments before delayMarkingChildren is jsgc.cpp. */
    JSGCArena           *unmarkedArenaStackTop;
#ifdef DEBUG
    size_t              markLaterCount;
#endif

  public:
#if defined(JS_DUMP_CONSERVATIVE_GC_ROOTS) || defined(JS_GCMETER)
    ConservativeGCStats conservativeStats;
#endif

#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
    struct ConservativeRoot { void *thing; uint32 traceKind; };
    Vector<ConservativeRoot, 0, SystemAllocPolicy> conservativeRoots;
    const char *conservativeDumpFileName;

    void dumpConservativeRoots();
#endif

    js::Vector<JSObject *, 0, js::SystemAllocPolicy> arraysToSlowify;

  public:
    explicit GCMarker(JSContext *cx);
    ~GCMarker();

    uint32 getMarkColor() const {
        return color;
    }

    void setMarkColor(uint32 newColor) {
        /*
         * We must process any delayed marking here, otherwise we confuse
         * colors.
         */
        markDelayedChildren();
        color = newColor;
    }

    void delayMarkingChildren(void *thing);

    JS_FRIEND_API(void) markDelayedChildren();

    void slowifyArrays();
};

} /* namespace js */

extern void
js_FinalizeStringRT(JSRuntime *rt, JSString *str);

/*
 * This function is defined in jsdbgapi.cpp but is declared here to avoid
 * polluting jsdbgapi.h, a public API header, with internal functions.
 */
extern void
js_MarkTraps(JSTracer *trc);

namespace js {

/*
 * Set object's prototype while checking that doing so would not create
 * a cycle in the proto chain. The cycle check and proto change are done
 * only when all other requests are finished or suspended to ensure exclusive
 * access to the chain. If there is a cycle, return false without reporting
 * an error. Otherwise, set the proto and return true.
 */
extern bool
SetProtoCheckingForCycles(JSContext *cx, JSObject *obj, JSObject *proto);

/* N.B. Assumes JS_SET_TRACING_NAME/INDEX has already been called. */
void
Mark(JSTracer *trc, void *thing, uint32 kind);

static inline void
Mark(JSTracer *trc, void *thing, uint32 kind, const char *name)
{
    JS_ASSERT(thing);
    JS_SET_TRACING_NAME(trc, name);
    Mark(trc, thing, kind);
}

static inline void
MarkString(JSTracer *trc, JSString *str)
{
    JS_ASSERT(str);
    Mark(trc, str, JSTRACE_STRING);
}

static inline void
MarkString(JSTracer *trc, JSString *str, const char *name)
{
    JS_ASSERT(str);
    JS_SET_TRACING_NAME(trc, name);
    Mark(trc, str, JSTRACE_STRING);
}

static inline void
MarkAtomRange(JSTracer *trc, size_t len, JSAtom **vec, const char *name)
{
    for (uint32 i = 0; i < len; i++) {
        if (JSAtom *atom = vec[i]) {
            JS_SET_TRACING_INDEX(trc, name, i);
            Mark(trc, ATOM_TO_STRING(atom), JSTRACE_STRING);
        }
    }
}

static inline void
MarkObject(JSTracer *trc, JSObject &obj, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    Mark(trc, &obj, JSTRACE_OBJECT);
}

static inline void
MarkObjectRange(JSTracer *trc, size_t len, JSObject **vec, const char *name)
{
    for (uint32 i = 0; i < len; i++) {
        if (JSObject *obj = vec[i]) {
            JS_SET_TRACING_INDEX(trc, name, i);
            Mark(trc, obj, JSTRACE_OBJECT);
        }
    }
}

/* N.B. Assumes JS_SET_TRACING_NAME/INDEX has already been called. */
static inline void
MarkValueRaw(JSTracer *trc, const js::Value &v)
{
    if (v.isMarkable())
        return Mark(trc, v.asGCThing(), v.gcKind());
}

static inline void
MarkValue(JSTracer *trc, const js::Value &v, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkValueRaw(trc, v);
}

static inline void
MarkValueRange(JSTracer *trc, Value *beg, Value *end, const char *name)
{
    for (Value *vp = beg; vp < end; ++vp) {
        JS_SET_TRACING_INDEX(trc, name, vp - beg);
        MarkValueRaw(trc, *vp);
    }
}

static inline void
MarkValueRange(JSTracer *trc, size_t len, Value *vec, const char *name)
{
    MarkValueRange(trc, vec, vec + len, name);
}

void
MarkStackRangeConservatively(JSTracer *trc, Value *begin, Value *end);

static inline void
MarkId(JSTracer *trc, jsid id)
{
    if (JSID_IS_STRING(id))
        Mark(trc, JSID_TO_STRING(id), JSTRACE_STRING);
    else if (JS_UNLIKELY(JSID_IS_OBJECT(id)))
        Mark(trc, JSID_TO_OBJECT(id), JSTRACE_OBJECT);
}

static inline void
MarkId(JSTracer *trc, jsid id, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkId(trc, id);
}

static inline void
MarkIdRange(JSTracer *trc, jsid *beg, jsid *end, const char *name)
{
    for (jsid *idp = beg; idp != end; ++idp) {
        JS_SET_TRACING_INDEX(trc, name, (idp - beg));
        MarkId(trc, *idp);
    }
}

static inline void
MarkIdRange(JSTracer *trc, size_t len, jsid *vec, const char *name)
{
    MarkIdRange(trc, vec, vec + len, name);
}

/* N.B. Assumes JS_SET_TRACING_NAME/INDEX has already been called. */
void
MarkGCThing(JSTracer *trc, void *thing);

static inline void
MarkGCThing(JSTracer *trc, void *thing, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkGCThing(trc, thing);
}

static inline void
MarkGCThing(JSTracer *trc, void *thing, const char *name, size_t index)
{
    JS_SET_TRACING_INDEX(trc, name, index);
    MarkGCThing(trc, thing);
}

JSCompartment *
NewCompartment(JSContext *cx, JSPrincipals *principals);

} /* namespace js */

#endif /* jsgc_h___ */
