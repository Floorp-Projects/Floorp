/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jscompartment_h___
#define jscompartment_h___

#include "mozilla/Attributes.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/Util.h"

#include "jscntxt.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsobj.h"

#include "gc/FindSCCs.h"
#include "vm/GlobalObject.h"
#include "vm/RegExpObject.h"
#include "vm/Shape.h"

namespace js {

namespace ion {
class IonCompartment;
}

struct NativeIterator;

/*
 * A single-entry cache for some base-10 double-to-string conversions. This
 * helps date-format-xparb.js.  It also avoids skewing the results for
 * v8-splay.js when measured by the SunSpider harness, where the splay tree
 * initialization (which includes many repeated double-to-string conversions)
 * is erroneously included in the measurement; see bug 562553.
 */
class DtoaCache {
    double       d;
    int          base;
    JSFlatString *s;      // if s==NULL, d and base are not valid

  public:
    DtoaCache() : s(NULL) {}
    void purge() { s = NULL; }

    JSFlatString *lookup(int base, double d) {
        return this->s && base == this->base && d == this->d ? this->s : NULL;
    }

    void cache(int base, double d, JSFlatString *s) {
        this->base = base;
        this->d = d;
        this->s = s;
    }
};

/* If HashNumber grows, need to change WrapperHasher. */
JS_STATIC_ASSERT(sizeof(HashNumber) == 4);

struct CrossCompartmentKey
{
    enum Kind {
        ObjectWrapper,
        StringWrapper,
        DebuggerScript,
        DebuggerObject,
        DebuggerEnvironment
    };

    Kind kind;
    JSObject *debugger;
    js::gc::Cell *wrapped;

    CrossCompartmentKey()
      : kind(ObjectWrapper), debugger(NULL), wrapped(NULL) {}
    CrossCompartmentKey(JSObject *wrapped)
      : kind(ObjectWrapper), debugger(NULL), wrapped(wrapped) {}
    CrossCompartmentKey(JSString *wrapped)
      : kind(StringWrapper), debugger(NULL), wrapped(wrapped) {}
    CrossCompartmentKey(Value wrapped)
      : kind(wrapped.isString() ? StringWrapper : ObjectWrapper),
        debugger(NULL),
        wrapped((js::gc::Cell *)wrapped.toGCThing()) {}
    CrossCompartmentKey(const RootedValue &wrapped)
      : kind(wrapped.get().isString() ? StringWrapper : ObjectWrapper),
        debugger(NULL),
        wrapped((js::gc::Cell *)wrapped.get().toGCThing()) {}
    CrossCompartmentKey(Kind kind, JSObject *dbg, js::gc::Cell *wrapped)
      : kind(kind), debugger(dbg), wrapped(wrapped) {}
};

struct WrapperHasher
{
    typedef CrossCompartmentKey Lookup;

    static HashNumber hash(const CrossCompartmentKey &key) {
        JS_ASSERT(!IsPoisonedPtr(key.wrapped));
        return uint32_t(uintptr_t(key.wrapped)) | uint32_t(key.kind);
    }

    static bool match(const CrossCompartmentKey &l, const CrossCompartmentKey &k) {
        return l.kind == k.kind && l.debugger == k.debugger && l.wrapped == k.wrapped;
    }
};

typedef HashMap<CrossCompartmentKey, ReadBarrieredValue,
                WrapperHasher, SystemAllocPolicy> WrapperMap;

} /* namespace js */

namespace JS {
struct TypeInferenceSizes;
}

namespace js {
class AutoDebugModeGC;
class DebugScopes;
}

namespace js {

/*
 * Encapsulates the data needed to perform allocation.  Typically
 * there is precisely one of these per compartment
 * (|compartment.allocator|).  However, in parallel execution mode,
 * there will be one per worker thread.  In general, if a piece of
 * code must perform execution and should work safely either in
 * parallel or sequential mode, you should make it take an
 * |Allocator*| rather than a |JSContext*|.
 */
class Allocator : public MallocProvider<Allocator>
{
    JS::Zone *zone;

  public:
    explicit Allocator(JS::Zone *zone);

    js::gc::ArenaLists arenas;

    inline void *parallelNewGCThing(gc::AllocKind thingKind, size_t thingSize);

    inline void *onOutOfMemory(void *p, size_t nbytes);
    inline void updateMallocCounter(size_t nbytes);
    inline void reportAllocationOverflow();
};

} /* namespace js */

struct JSCompartment : private JS::shadow::Zone, public js::gc::GraphNodeBase<JSCompartment>
{
    JSRuntime                    *rt;
    JSPrincipals                 *principals;

  private:
    friend struct JSRuntime;
    friend struct JSContext;
    js::ReadBarriered<js::GlobalObject> global_;

    unsigned                     enterCompartmentDepth;

  public:
    void enter() { enterCompartmentDepth++; }
    void leave() { enterCompartmentDepth--; }

    /*
     * Nb: global_ might be NULL, if (a) it's the atoms compartment, or (b) the
     * compartment's global has been collected.  The latter can happen if e.g.
     * a string in a compartment is rooted but no object is, and thus the global
     * isn't rooted, and thus the global can be finalized while the compartment
     * lives on.
     *
     * In contrast, JSObject::global() is infallible because marking a JSObject
     * always marks its global as well.
     * TODO: add infallible JSScript::global()
     */
    inline js::GlobalObject *maybeGlobal() const;

    void initGlobal(js::GlobalObject &global) {
        JS_ASSERT(global.compartment() == this);
        JS_ASSERT(!global_);
        global_ = &global;
    }

  public:
    js::Allocator                    allocator;

    /*
     * Moves all data from the allocator |workerAllocator|, which was
     * in use by a parallel worker, into the compartment's main
     * allocator.  This is used at the end of a parallel section.
     */
    void adoptWorkerAllocator(js::Allocator *workerAllocator);

  private:
    bool                         ionUsingBarriers_;
  public:

    JS::Zone *zone() {
        return this;
    }

    const JS::Zone *zone() const {
        return this;
    }

    bool needsBarrier() const {
        return needsBarrier_;
    }

    bool compileBarriers(bool needsBarrier) const {
        return needsBarrier || rt->gcZeal() == js::gc::ZealVerifierPreValue;
    }

    bool compileBarriers() const {
        return compileBarriers(needsBarrier());
    }

    enum ShouldUpdateIon {
        DontUpdateIon,
        UpdateIon
    };

    void setNeedsBarrier(bool needs, ShouldUpdateIon updateIon);

    static size_t OffsetOfNeedsBarrier() {
        return offsetof(JSCompartment, needsBarrier_);
    }

    js::GCMarker *barrierTracer() {
        JS_ASSERT(needsBarrier_);
        return &rt->gcMarker;
    }

  public:
    enum CompartmentGCState {
        NoGC,
        Mark,
        MarkGray,
        Sweep,
        Finished
    };

  private:
    bool                         gcScheduled;
    CompartmentGCState           gcState;
    bool                         gcPreserveCode;

  public:
    bool isCollecting() const {
        if (rt->isHeapCollecting())
            return gcState != NoGC;
        else
            return needsBarrier();
    }

    bool isPreservingCode() const {
        return gcPreserveCode;
    }

    /*
     * If this returns true, all object tracing must be done with a GC marking
     * tracer.
     */
    bool requireGCTracer() const {
        return rt->isHeapCollecting() && gcState != NoGC;
    }

    void setGCState(CompartmentGCState state) {
        JS_ASSERT(rt->isHeapBusy());
        gcState = state;
    }

    void scheduleGC() {
        JS_ASSERT(!rt->isHeapBusy());
        gcScheduled = true;
    }

    void unscheduleGC() {
        gcScheduled = false;
    }

    bool isGCScheduled() const {
        return gcScheduled;
    }

    void setPreservingCode(bool preserving) {
        gcPreserveCode = preserving;
    }

    bool wasGCStarted() const {
        return gcState != NoGC;
    }

    bool isGCMarking() {
        if (rt->isHeapCollecting())
            return gcState == Mark || gcState == MarkGray;
        else
            return needsBarrier();
    }

    bool isGCMarkingBlack() {
        return gcState == Mark;
    }

    bool isGCMarkingGray() {
        return gcState == MarkGray;
    }

    bool isGCSweeping() {
        return gcState == Sweep;
    }

    bool isGCFinished() {
        return gcState == Finished;
    }

    volatile size_t              gcBytes;
    size_t                       gcTriggerBytes;
    size_t                       gcMaxMallocBytes;
    double                       gcHeapGrowthFactor;

    bool                         hold;
    bool                         isSystem;

    int64_t                      lastCodeRelease;

    /* Pools for analysis and type information in this compartment. */
    static const size_t ANALYSIS_LIFO_ALLOC_PRIMARY_CHUNK_SIZE = 32 * 1024;
    static const size_t TYPE_LIFO_ALLOC_PRIMARY_CHUNK_SIZE = 8 * 1024;
    js::LifoAlloc                analysisLifoAlloc;
    js::LifoAlloc                typeLifoAlloc;

    bool                         activeAnalysis;

    /* Type information about the scripts and objects in this compartment. */
    js::types::TypeCompartment   types;

    void                         *data;
    bool                         active;  // GC flag, whether there are active frames

  private:
    js::WrapperMap               crossCompartmentWrappers;

  public:
    /*
     * These flags help us to discover if a compartment that shouldn't be alive
     * manages to outlive a GC.
     */
    bool                         scheduledForDestruction;
    bool                         maybeAlive;

    /* Last time at which an animation was played for a global in this compartment. */
    int64_t                      lastAnimationTime;

    js::RegExpCompartment        regExps;

  private:
    void sizeOfTypeInferenceData(JS::TypeInferenceSizes *stats, JSMallocSizeOfFun mallocSizeOf);

  public:
    void sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf, size_t *compartmentObject,
                             JS::TypeInferenceSizes *tiSizes,
                             size_t *shapesCompartmentTables, size_t *crossCompartmentWrappers,
                             size_t *regexpCompartment, size_t *debuggeesSet);

    /*
     * Shared scope property tree, and arena-pool for allocating its nodes.
     */
    js::PropertyTree             propertyTree;

    /* Set of all unowned base shapes in the compartment. */
    js::BaseShapeSet             baseShapes;
    void sweepBaseShapeTable();

    /* Set of initial shapes in the compartment. */
    js::InitialShapeSet          initialShapes;
    void sweepInitialShapeTable();

    /* Set of default 'new' or lazy types in the compartment. */
    js::types::TypeObjectSet     newTypeObjects;
    js::types::TypeObjectSet     lazyTypeObjects;
    void sweepNewTypeObjectTable(js::types::TypeObjectSet &table);

    js::types::TypeObject *getNewType(JSContext *cx, js::Class *clasp, js::TaggedProto proto,
                                      JSFunction *fun = NULL);

    js::types::TypeObject *getLazyType(JSContext *cx, js::Class *clasp, js::TaggedProto proto);

    /*
     * Hash table of all manually call site-cloned functions from within
     * self-hosted code. Cloning according to call site provides extra
     * sensitivity for type specialization and inlining.
     */
    js::CallsiteCloneTable callsiteClones;
    void sweepCallsiteClones();

    /* During GC, stores the index of this compartment in rt->compartments. */
    unsigned                     gcIndex;

    /*
     * During GC, stores the head of a list of incoming pointers from gray cells.
     *
     * The objects in the list are either cross-compartment wrappers, or
     * debugger wrapper objects.  The list link is either in the second extra
     * slot for the former, or a special slot for the latter.
     */
    js::RawObject                gcIncomingGrayPointers;

    /* Linked list of live array buffers with >1 view. */
    JSObject                     *gcLiveArrayBuffers;

    /* Linked list of live weakmaps in this compartment. */
    js::WeakMapBase              *gcWeakMapList;

    /* This compartment's gray roots. */
    js::Vector<js::GrayRoot, 0, js::SystemAllocPolicy> gcGrayRoots;

  private:
    /*
     * Malloc counter to measure memory pressure for GC scheduling. It runs from
     * gcMaxMallocBytes down to zero. This counter should be used only when it's
     * not possible to know the size of a free.
     */
    ptrdiff_t                    gcMallocBytes;

    enum { DebugFromC = 1, DebugFromJS = 2 };

    unsigned                     debugModeBits;  // see debugMode() below

  public:
    JSCompartment(JSRuntime *rt);
    ~JSCompartment();

    bool init(JSContext *cx);

    /* Mark cross-compartment wrappers. */
    void markCrossCompartmentWrappers(JSTracer *trc);

    bool wrap(JSContext *cx, JS::MutableHandleValue vp, JS::HandleObject existing = JS::NullPtr());
    bool wrap(JSContext *cx, JSString **strp);
    bool wrap(JSContext *cx, js::HeapPtrString *strp);
    bool wrap(JSContext *cx, JSObject **objp, JSObject *existing = NULL);
    bool wrapId(JSContext *cx, jsid *idp);
    bool wrap(JSContext *cx, js::PropertyOp *op);
    bool wrap(JSContext *cx, js::StrictPropertyOp *op);
    bool wrap(JSContext *cx, js::PropertyDescriptor *desc);
    bool wrap(JSContext *cx, js::AutoIdVector &props);

    bool putWrapper(const js::CrossCompartmentKey& wrapped, const js::Value& wrapper);

    js::WrapperMap::Ptr lookupWrapper(const js::Value& wrapped) {
        return crossCompartmentWrappers.lookup(wrapped);
    }

    void removeWrapper(js::WrapperMap::Ptr p) {
        crossCompartmentWrappers.remove(p);
    }

    struct WrapperEnum : public js::WrapperMap::Enum {
        WrapperEnum(JSCompartment *c) : js::WrapperMap::Enum(c->crossCompartmentWrappers) {}
    };

    void mark(JSTracer *trc);
    void markTypes(JSTracer *trc);
    void discardJitCode(js::FreeOp *fop, bool discardConstraints);
    bool isDiscardingJitCode(JSTracer *trc);
    void sweep(js::FreeOp *fop, bool releaseTypes);
    void sweepCrossCompartmentWrappers();
    void purge();

    void findOutgoingEdgesFromCompartment(js::gc::ComponentFinder<JS::Zone> &finder);
    void findOutgoingEdges(js::gc::ComponentFinder<JS::Zone> &finder);

    void setGCLastBytes(size_t lastBytes, js::JSGCInvocationKind gckind);
    void reduceGCTriggerBytes(size_t amount);

    void resetGCMallocBytes();
    void setGCMaxMallocBytes(size_t value);
    void updateMallocCounter(size_t nbytes) {
        /*
         * Note: this code may be run from worker threads.  We
         * tolerate any thread races when updating gcMallocBytes.
         */
        ptrdiff_t oldCount = gcMallocBytes;
        ptrdiff_t newCount = oldCount - ptrdiff_t(nbytes);
        gcMallocBytes = newCount;
        if (JS_UNLIKELY(newCount <= 0 && oldCount > 0))
            onTooMuchMalloc();
    }

    bool isTooMuchMalloc() const {
        return gcMallocBytes <= 0;
     }

    void onTooMuchMalloc();

    js::DtoaCache dtoaCache;

    /* Random number generator state, used by jsmath.cpp. */
    uint64_t rngState;

  private:
    /*
     * Weak reference to each global in this compartment that is a debuggee.
     * Each global has its own list of debuggers.
     */
    js::GlobalObjectSet              debuggees;

  private:
    JSCompartment *thisForCtor() { return this; }

  public:
    /*
     * There are dueling APIs for debug mode. It can be enabled or disabled via
     * JS_SetDebugModeForCompartment. It is automatically enabled and disabled
     * by Debugger objects. Therefore debugModeBits has the DebugFromC bit set
     * if the C API wants debug mode and the DebugFromJS bit set if debuggees
     * is non-empty.
     */
    bool debugMode() const { return !!debugModeBits; }

    /* True if any scripts from this compartment are on the JS stack. */
    bool hasScriptsOnStack();

  private:
    /* This is called only when debugMode() has just toggled. */
    void updateForDebugMode(js::FreeOp *fop, js::AutoDebugModeGC &dmgc);

  public:
    js::GlobalObjectSet &getDebuggees() { return debuggees; }
    bool addDebuggee(JSContext *cx, js::GlobalObject *global);
    bool addDebuggee(JSContext *cx, js::GlobalObject *global,
                     js::AutoDebugModeGC &dmgc);
    void removeDebuggee(js::FreeOp *fop, js::GlobalObject *global,
                        js::GlobalObjectSet::Enum *debuggeesEnum = NULL);
    void removeDebuggee(js::FreeOp *fop, js::GlobalObject *global,
                        js::AutoDebugModeGC &dmgc,
                        js::GlobalObjectSet::Enum *debuggeesEnum = NULL);
    bool setDebugModeFromC(JSContext *cx, bool b, js::AutoDebugModeGC &dmgc);

    void clearBreakpointsIn(js::FreeOp *fop, js::Debugger *dbg, JSObject *handler);
    void clearTraps(js::FreeOp *fop);

  private:
    void sweepBreakpoints(js::FreeOp *fop);

  public:
    js::WatchpointMap *watchpointMap;

    js::ScriptCountsMap *scriptCountsMap;

    js::DebugScriptMap *debugScriptMap;

    /* Bookkeeping information for debug scope objects. */
    js::DebugScopes *debugScopes;

    /*
     * List of potentially active iterators that may need deleted property
     * suppression.
     */
    js::NativeIterator *enumerators;

#ifdef JS_ION
  private:
    js::ion::IonCompartment *ionCompartment_;

  public:
    bool ensureIonCompartmentExists(JSContext *cx);
    js::ion::IonCompartment *ionCompartment() {
        return ionCompartment_;
    }
#endif
};

namespace JS {
typedef JSCompartment Zone;
} /* namespace JS */

// For use when changing the debug mode flag on one or more compartments.
// Do not run scripts in any compartment that is scheduled for GC using this
// object. See comment in updateForDebugMode.
//
class js::AutoDebugModeGC
{
    JSRuntime *rt;
    bool needGC;
  public:
    explicit AutoDebugModeGC(JSRuntime *rt) : rt(rt), needGC(false) {}

    ~AutoDebugModeGC() {
        // Under some circumstances (say, in the midst of an animation),
        // the garbage collector may try to retain JIT code and analyses.
        // The DEBUG_MODE_GC reason forces the collector to always throw
        // everything away, as required for debug mode transitions.
        if (needGC)
            GC(rt, GC_NORMAL, gcreason::DEBUG_MODE_GC);
    }

    void scheduleGC(Zone *zone) {
        JS_ASSERT(!rt->isHeapBusy());
        PrepareZoneForGC(zone);
        needGC = true;
    }
};

inline bool
JSContext::typeInferenceEnabled() const
{
    return compartment->types.inferenceEnabled;
}

inline js::Handle<js::GlobalObject*>
JSContext::global() const
{
    /*
     * It's safe to use |unsafeGet()| here because any compartment that is
     * on-stack will be marked automatically, so there's no need for a read
     * barrier on it. Once the compartment is popped, the handle is no longer
     * safe to use.
     */
    return js::Handle<js::GlobalObject*>::fromMarkedLocation(compartment->global_.unsafeGet());
}

namespace js {

class AssertCompartmentUnchanged
{
  public:
    AssertCompartmentUnchanged(JSContext *cx
                                MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx), oldCompartment(cx->compartment)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~AssertCompartmentUnchanged() {
        JS_ASSERT(cx->compartment == oldCompartment);
    }

  protected:
    JSContext * const cx;
    JSCompartment * const oldCompartment;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoCompartment
{
    JSContext * const cx_;
    JSCompartment * const origin_;

  public:
    inline AutoCompartment(JSContext *cx, JSObject *target);
    inline ~AutoCompartment();

    JSContext *context() const { return cx_; }
    JSCompartment *origin() const { return origin_; }

  private:
    AutoCompartment(const AutoCompartment &) MOZ_DELETE;
    AutoCompartment & operator=(const AutoCompartment &) MOZ_DELETE;
};

/*
 * Entering the atoms comaprtment is not possible with the AutoCompartment
 * since the atoms compartment does not have a global.
 *
 * Note: since most of the VM assumes that cx->global is non-null, only a
 * restricted set of (atom creating/destroying) operations may be used from
 * inside the atoms compartment.
 */
class AutoEnterAtomsCompartment
{
    JSContext *cx;
    JSCompartment *oldCompartment;
  public:
    AutoEnterAtomsCompartment(JSContext *cx)
      : cx(cx),
        oldCompartment(cx->compartment)
    {
        cx->setCompartment(cx->runtime->atomsCompartment);
    }

    ~AutoEnterAtomsCompartment()
    {
        cx->setCompartment(oldCompartment);
    }
};

/*
 * Use this to change the behavior of an AutoCompartment slightly on error. If
 * the exception happens to be an Error object, copy it to the origin compartment
 * instead of wrapping it.
 */
class ErrorCopier
{
    mozilla::Maybe<AutoCompartment> &ac;
    RootedObject scope;

  public:
    ErrorCopier(mozilla::Maybe<AutoCompartment> &ac, JSObject *scope)
      : ac(ac), scope(ac.ref().context(), scope) {}
    ~ErrorCopier();
};

class CompartmentsIter {
  private:
    JSCompartment **it, **end;

  public:
    CompartmentsIter(JSRuntime *rt) {
        it = rt->compartments.begin();
        end = rt->compartments.end();
    }

    bool done() const { return it == end; }

    void next() {
        JS_ASSERT(!done());
        it++;
    }

    JSCompartment *get() const {
        JS_ASSERT(!done());
        return *it;
    }

    operator JSCompartment *() const { return get(); }
    JSCompartment *operator->() const { return get(); }
};

typedef CompartmentsIter ZonesIter;

/*
 * AutoWrapperVector and AutoWrapperRooter can be used to store wrappers that
 * are obtained from the cross-compartment map. However, these classes should
 * not be used if the wrapper will escape. For example, it should not be stored
 * in the heap.
 *
 * The AutoWrapper rooters are different from other autorooters because their
 * wrappers are marked on every GC slice rather than just the first one. If
 * there's some wrapper that we want to use temporarily without causing it to be
 * marked, we can use these AutoWrapper classes. If we get unlucky and a GC
 * slice runs during the code using the wrapper, the GC will mark the wrapper so
 * that it doesn't get swept out from under us. Otherwise, the wrapper needn't
 * be marked. This is useful in functions like JS_TransplantObject that
 * manipulate wrappers in compartments that may no longer be alive.
 */

/*
 * This class stores the data for AutoWrapperVector and AutoWrapperRooter. It
 * should not be used in any other situations.
 */
struct WrapperValue
{
    /*
     * We use unsafeGet() in the constructors to avoid invoking a read barrier
     * on the wrapper, which may be dead (see the comment about bug 803376 in
     * jsgc.cpp regarding this). If there is an incremental GC while the wrapper
     * is in use, the AutoWrapper rooter will ensure the wrapper gets marked.
     */
    explicit WrapperValue(const WrapperMap::Ptr &ptr)
      : value(*ptr->value.unsafeGet())
    {}

    explicit WrapperValue(const WrapperMap::Enum &e)
      : value(*e.front().value.unsafeGet())
    {}

    Value &get() { return value; }
    Value get() const { return value; }
    operator const Value &() const { return value; }
    JSObject &toObject() const { return value.toObject(); }

  private:
    Value value;
};

class AutoWrapperVector : public AutoVectorRooter<WrapperValue>
{
  public:
    explicit AutoWrapperVector(JSContext *cx
                               MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : AutoVectorRooter<WrapperValue>(cx, WRAPVECTOR)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoWrapperRooter : private AutoGCRooter {
  public:
    AutoWrapperRooter(JSContext *cx, WrapperValue v
                      MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : AutoGCRooter(cx, WRAPPER), value(v)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    operator JSObject *() const {
        return value.get().toObjectOrNull();
    }

    friend void AutoGCRooter::trace(JSTracer *trc);

  private:
    WrapperValue value;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} /* namespace js */

#endif /* jscompartment_h___ */

