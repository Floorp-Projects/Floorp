/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jscompartment_h___
#define jscompartment_h___

#include "mozilla/Attributes.h"

#include "jscntxt.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsobj.h"
#include "jsscope.h"

#include "gc/StoreBuffer.h"
#include "vm/GlobalObject.h"
#include "vm/RegExpObject.h"

namespace js {

namespace ion {
    class IonCompartment;
}

/*
 * A single-entry cache for some base-10 double-to-string conversions. This
 * helps date-format-xparb.js.  It also avoids skewing the results for
 * v8-splay.js when measured by the SunSpider harness, where the splay tree
 * initialization (which includes many repeated double-to-string conversions)
 * is erroneously included in the measurement; see bug 562553.
 */
class DtoaCache {
    double        d;
    int         base;
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
}

struct JSCompartment
{
    JSRuntime                    *rt;
    JSPrincipals                 *principals;

  private:
    friend struct JSContext;
    js::GlobalObject             *global_;
  public:
    // Nb: global_ might be NULL, if (a) it's the atoms compartment, or (b) the
    // compartment's global has been collected.  The latter can happen if e.g.
    // a string in a compartment is rooted but no object is, and thus the
    // global isn't rooted, and thus the global can be finalized while the
    // compartment lives on.
    //
    // In contrast, JSObject::global() is infallible because marking a JSObject
    // always marks its global as well.
    // TODO: add infallible JSScript::global()
    //
    js::GlobalObject *maybeGlobal() const {
        JS_ASSERT_IF(global_, global_->compartment() == this);
        return global_;
    }

    void initGlobal(js::GlobalObject &global) {
        JS_ASSERT(global.compartment() == this);
        JS_ASSERT(!global_);
        global_ = &global;
    }

  public:
    js::gc::ArenaLists           arenas;

#ifdef JSGC_GENERATIONAL
    js::gc::Nursery              gcNursery;
    js::gc::StoreBuffer          gcStoreBuffer;
#endif

  private:
    bool                         needsBarrier_;
  public:

    bool needsBarrier() const {
        return needsBarrier_;
    }

    bool compileBarriers(bool needsBarrier) const {
        return needsBarrier || rt->gcZeal() == js::gc::ZealVerifierPreValue;
    }

    bool compileBarriers() const {
        return compileBarriers(needsBarrier());
    }

    void setNeedsBarrier(bool needs);

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
        Sweep
    };

  private:
    bool                         gcScheduled;
    CompartmentGCState           gcState;
    bool                         gcPreserveCode;

  public:
    bool isCollecting() const {
        if (rt->isHeapCollecting()) {
            return gcState != NoGC;
        } else {
            return needsBarrier();
        }
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
        return gcState == Mark;
    }

    bool isGCSweeping() {
        return gcState == Sweep;
    }

    size_t                       gcBytes;
    size_t                       gcTriggerBytes;
    size_t                       gcMaxMallocBytes;
    double                       gcHeapGrowthFactor;
    JSCompartment                *gcNextCompartment;

    bool                         hold;
    bool                         isSystemCompartment;

    int64_t                      lastCodeRelease;

    /* Pools for analysis and type information in this compartment. */
    static const size_t LIFO_ALLOC_PRIMARY_CHUNK_SIZE = 128 * 1024;
    js::LifoAlloc                analysisLifoAlloc;
    js::LifoAlloc                typeLifoAlloc;

    bool                         activeAnalysis;
    bool                         activeInference;

    /* Type information about the scripts and objects in this compartment. */
    js::types::TypeCompartment   types;

    void                         *data;
    bool                         active;  // GC flag, whether there are active frames
    js::WrapperMap               crossCompartmentWrappers;

    /* Last time at which an animation was played for a global in this compartment. */
    int64_t                      lastAnimationTime;

    js::RegExpCompartment        regExps;

    size_t sizeOfShapeTable(JSMallocSizeOfFun mallocSizeOf);
    void sizeOfTypeInferenceData(JS::TypeInferenceSizes *stats, JSMallocSizeOfFun mallocSizeOf);

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

    js::types::TypeObject *getNewType(JSContext *cx, js::TaggedProto proto,
                                      JSFunction *fun = NULL, bool isDOM = false);

    js::types::TypeObject *getLazyType(JSContext *cx, js::Handle<js::TaggedProto> proto);

    /*
     * Keeps track of the total number of malloc bytes connected to a
     * compartment's GC things. This counter should be used in preference to
     * gcMallocBytes. These counters affect collection in the same way as
     * gcBytes and gcTriggerBytes.
     */
    size_t                       gcMallocAndFreeBytes;
    size_t                       gcTriggerMallocAndFreeBytes;

    /* During GC, stores the index of this compartment in rt->compartments. */
    unsigned                     index;

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

    bool wrap(JSContext *cx, js::Value *vp);
    bool wrap(JSContext *cx, JSString **strp);
    bool wrap(JSContext *cx, js::HeapPtrString *strp);
    bool wrap(JSContext *cx, JSObject **objp);
    bool wrapId(JSContext *cx, jsid *idp);
    bool wrap(JSContext *cx, js::PropertyOp *op);
    bool wrap(JSContext *cx, js::StrictPropertyOp *op);
    bool wrap(JSContext *cx, js::PropertyDescriptor *desc);
    bool wrap(JSContext *cx, js::AutoIdVector &props);

    void mark(JSTracer *trc);
    void markTypes(JSTracer *trc);
    void discardJitCode(js::FreeOp *fop, bool discardConstraints);
    bool isDiscardingJitCode(JSTracer *trc);
    void sweep(js::FreeOp *fop, bool releaseTypes);
    void sweepCrossCompartmentWrappers();
    void purge();

    void setGCLastBytes(size_t lastBytes, size_t lastMallocBytes, js::JSGCInvocationKind gckind);
    void reduceGCTriggerBytes(size_t amount);

    void resetGCMallocBytes();
    void setGCMaxMallocBytes(size_t value);
    void updateMallocCounter(size_t nbytes) {
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

    void mallocInCompartment(size_t nbytes) {
        gcMallocAndFreeBytes += nbytes;
    }

    void freeInCompartment(size_t nbytes) {
        JS_ASSERT(gcMallocAndFreeBytes >= nbytes);
        gcMallocAndFreeBytes -= nbytes;
    }

    js::DtoaCache dtoaCache;

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
    void removeDebuggee(js::FreeOp *fop, js::GlobalObject *global,
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

    void scheduleGC(JSCompartment *compartment) {
        JS_ASSERT(!rt->isHeapBusy());
        PrepareCompartmentForGC(compartment);
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
    return js::Handle<js::GlobalObject*>::fromMarkedLocation(&compartment->global_);
}

namespace js {

class AssertCompartmentUnchanged {
  protected:
    JSContext * const cx;
    JSCompartment * const oldCompartment;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
  public:
     AssertCompartmentUnchanged(JSContext *cx JS_GUARD_OBJECT_NOTIFIER_PARAM)
     : cx(cx), oldCompartment(cx->compartment) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~AssertCompartmentUnchanged() {
        JS_ASSERT(cx->compartment == oldCompartment);
    }
};

class AutoCompartment
{
    JSContext * const cx_;
    JSCompartment * const origin_;

  public:
    AutoCompartment(JSContext *cx, JSObject *target)
      : cx_(cx),
        origin_(cx->compartment)
    {
        cx_->enterCompartment(target->compartment());
    }

    ~AutoCompartment() {
        cx_->leaveCompartment(origin_);
    }

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
    Maybe<AutoCompartment> &ac;
    RootedObject scope;

  public:
    ErrorCopier(Maybe<AutoCompartment> &ac, JSObject *scope)
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

} /* namespace js */

#endif /* jscompartment_h___ */
