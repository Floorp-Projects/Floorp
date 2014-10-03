/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jscompartment_h
#define jscompartment_h

#include "mozilla/MemoryReporting.h"

#include "builtin/RegExp.h"
#include "gc/Zone.h"
#include "vm/GlobalObject.h"
#include "vm/PIC.h"
#include "vm/SavedStacks.h"

namespace js {

namespace jit {
class JitCompartment;
}

namespace gc {
template<class Node> class ComponentFinder;
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
    JSFlatString *s;      // if s==nullptr, d and base are not valid

  public:
    DtoaCache() : s(nullptr) {}
    void purge() { s = nullptr; }

    JSFlatString *lookup(int base, double d) {
        return this->s && base == this->base && d == this->d ? this->s : nullptr;
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
        DebuggerSource,
        DebuggerObject,
        DebuggerEnvironment
    };

    Kind kind;
    JSObject *debugger;
    js::gc::Cell *wrapped;

    CrossCompartmentKey()
      : kind(ObjectWrapper), debugger(nullptr), wrapped(nullptr) {}
    explicit CrossCompartmentKey(JSObject *wrapped)
      : kind(ObjectWrapper), debugger(nullptr), wrapped(wrapped) {}
    explicit CrossCompartmentKey(JSString *wrapped)
      : kind(StringWrapper), debugger(nullptr), wrapped(wrapped) {}
    explicit CrossCompartmentKey(Value wrapped)
      : kind(wrapped.isString() ? StringWrapper : ObjectWrapper),
        debugger(nullptr),
        wrapped((js::gc::Cell *)wrapped.toGCThing()) {}
    explicit CrossCompartmentKey(const RootedValue &wrapped)
      : kind(wrapped.get().isString() ? StringWrapper : ObjectWrapper),
        debugger(nullptr),
        wrapped((js::gc::Cell *)wrapped.get().toGCThing()) {}
    CrossCompartmentKey(Kind kind, JSObject *dbg, js::gc::Cell *wrapped)
      : kind(kind), debugger(dbg), wrapped(wrapped) {}
};

struct WrapperHasher : public DefaultHasher<CrossCompartmentKey>
{
    static HashNumber hash(const CrossCompartmentKey &key) {
        MOZ_ASSERT(!IsPoisonedPtr(key.wrapped));
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
class AutoDebugModeInvalidation;
class DebugScopes;
class WeakMapBase;
}

struct JSCompartment
{
    JS::CompartmentOptions       options_;

  private:
    JS::Zone                     *zone_;
    JSRuntime                    *runtime_;

  public:
    JSPrincipals                 *principals;
    bool                         isSystem;
    bool                         isSelfHosting;
    bool                         marked;

    // A null add-on ID means that the compartment is not associated with an
    // add-on.
    JSAddonId                    *addonId;

#ifdef DEBUG
    bool                         firedOnNewGlobalObject;
#endif

    void mark() { marked = true; }

  private:
    friend struct JSRuntime;
    friend struct JSContext;
    friend class js::ExclusiveContext;
    js::ReadBarrieredGlobalObject global_;

    unsigned                     enterCompartmentDepth;

  public:
    void enter() { enterCompartmentDepth++; }
    void leave() { enterCompartmentDepth--; }
    bool hasBeenEntered() { return !!enterCompartmentDepth; }

    JS::Zone *zone() { return zone_; }
    const JS::Zone *zone() const { return zone_; }
    JS::CompartmentOptions &options() { return options_; }
    const JS::CompartmentOptions &options() const { return options_; }

    JSRuntime *runtimeFromMainThread() {
        MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime_));
        return runtime_;
    }

    // Note: Unrestricted access to the zone's runtime from an arbitrary
    // thread can easily lead to races. Use this method very carefully.
    JSRuntime *runtimeFromAnyThread() const {
        return runtime_;
    }

    /*
     * Nb: global_ might be nullptr, if (a) it's the atoms compartment, or
     * (b) the compartment's global has been collected.  The latter can happen
     * if e.g. a string in a compartment is rooted but no object is, and thus
     * the global isn't rooted, and thus the global can be finalized while the
     * compartment lives on.
     *
     * In contrast, JSObject::global() is infallible because marking a JSObject
     * always marks its global as well.
     * TODO: add infallible JSScript::global()
     */
    inline js::GlobalObject *maybeGlobal() const;

    /* An unbarriered getter for use while tracing. */
    inline js::GlobalObject *unsafeUnbarrieredMaybeGlobal() const;

    inline void initGlobal(js::GlobalObject &global);

  public:
    /*
     * Moves all data from the allocator |workerAllocator|, which was
     * in use by a parallel worker, into the compartment's main
     * allocator.  This is used at the end of a parallel section.
     */
    void adoptWorkerAllocator(js::Allocator *workerAllocator);

    bool                         activeAnalysis;

    /* Type information about the scripts and objects in this compartment. */
    js::types::TypeCompartment   types;

    void                         *data;

  private:
    js::ObjectMetadataCallback   objectMetadataCallback;

    js::SavedStacks              savedStacks_;

    js::WrapperMap               crossCompartmentWrappers;

  public:
    /* Last time at which an animation was played for a global in this compartment. */
    int64_t                      lastAnimationTime;

    js::RegExpCompartment        regExps;

    /*
     * For generational GC, record whether a write barrier has added this
     * compartment's global to the store buffer since the last minor GC.
     *
     * This is used to avoid adding it to the store buffer on every write, which
     * can quickly fill the buffer and also cause performance problems.
     */
    bool                         globalWriteBarriered;

  public:
    void addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                                size_t *tiAllocationSiteTables,
                                size_t *tiArrayTypeTables,
                                size_t *tiObjectTypeTables,
                                size_t *compartmentObject,
                                size_t *compartmentTables,
                                size_t *innerViews,
                                size_t *crossCompartmentWrappers,
                                size_t *regexpCompartment,
                                size_t *savedStacksSet);

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
    js::types::TypeObjectWithNewScriptSet newTypeObjects;
    js::types::TypeObjectWithNewScriptSet lazyTypeObjects;
    void sweepNewTypeObjectTable(js::types::TypeObjectWithNewScriptSet &table);
#ifdef JSGC_HASH_TABLE_CHECKS
    void checkTypeObjectTablesAfterMovingGC();
    void checkTypeObjectTableAfterMovingGC(js::types::TypeObjectWithNewScriptSet &table);
    void checkInitialShapesTableAfterMovingGC();
    void checkWrapperMapAfterMovingGC();
#endif

    /*
     * Hash table of all manually call site-cloned functions from within
     * self-hosted code. Cloning according to call site provides extra
     * sensitivity for type specialization and inlining.
     */
    js::CallsiteCloneTable callsiteClones;
    void sweepCallsiteClones();

    /*
     * Lazily initialized script source object to use for scripts cloned
     * from the self-hosting global.
     */
    js::ReadBarrieredScriptSourceObject selfHostingScriptSource;

    // Information mapping objects which own their own storage to other objects
    // sharing that storage.
    js::InnerViewTable innerViews;

    /* During GC, stores the index of this compartment in rt->compartments. */
    unsigned                     gcIndex;

    /*
     * During GC, stores the head of a list of incoming pointers from gray cells.
     *
     * The objects in the list are either cross-compartment wrappers, or
     * debugger wrapper objects.  The list link is either in the second extra
     * slot for the former, or a special slot for the latter.
     */
    JSObject                     *gcIncomingGrayPointers;

    /* Linked list of live weakmaps in this compartment. */
    js::WeakMapBase              *gcWeakMapList;

  private:
    /* Whether to preserve JIT code on non-shrinking GCs. */
    bool                         gcPreserveJitCode;

    enum {
        DebugMode = 1 << 0,
        DebugNeedDelazification = 1 << 1
    };

    unsigned                     debugModeBits;

  public:
    JSCompartment(JS::Zone *zone, const JS::CompartmentOptions &options);
    ~JSCompartment();

    bool init(JSContext *cx);

    /* Mark cross-compartment wrappers. */
    void markCrossCompartmentWrappers(JSTracer *trc);

    inline bool wrap(JSContext *cx, JS::MutableHandleValue vp,
                     JS::HandleObject existing = js::NullPtr());

    bool wrap(JSContext *cx, js::MutableHandleString strp);
    bool wrap(JSContext *cx, JS::MutableHandleObject obj,
              JS::HandleObject existingArg = js::NullPtr());
    bool wrap(JSContext *cx, JS::MutableHandle<js::PropertyDescriptor> desc);
    bool wrap(JSContext *cx, JS::MutableHandle<js::PropDesc> desc);

    template<typename T> bool wrap(JSContext *cx, JS::AutoVectorRooter<T> &vec) {
        for (size_t i = 0; i < vec.length(); ++i) {
            if (!wrap(cx, vec[i]))
                return false;
        }
        return true;
    };

    bool putWrapper(JSContext *cx, const js::CrossCompartmentKey& wrapped, const js::Value& wrapper);

    js::WrapperMap::Ptr lookupWrapper(const js::Value& wrapped) {
        return crossCompartmentWrappers.lookup(js::CrossCompartmentKey(wrapped));
    }

    void removeWrapper(js::WrapperMap::Ptr p) {
        crossCompartmentWrappers.remove(p);
    }

    struct WrapperEnum : public js::WrapperMap::Enum {
        explicit WrapperEnum(JSCompartment *c) : js::WrapperMap::Enum(c->crossCompartmentWrappers) {}
    };

    void trace(JSTracer *trc);
    void markRoots(JSTracer *trc);
    bool preserveJitCode() { return gcPreserveJitCode; }

    void sweepInnerViews();
    void sweepCrossCompartmentWrappers();
    void sweepTypeObjectTables();
    void sweepSavedStacks();
    void sweepGlobalObject(js::FreeOp *fop);
    void sweepSelfHostingScriptSource();
    void sweepJitCompartment(js::FreeOp *fop);
    void sweepRegExps();
    void sweepDebugScopes();
    void sweepWeakMaps();
    void sweepNativeIterators();

    void purge();
    void clearTables();

#ifdef JSGC_COMPACTING
    void fixupInitialShapeTable();
    void fixupNewTypeObjectTable(js::types::TypeObjectWithNewScriptSet &table);
    void fixupAfterMovingGC();
    void fixupGlobal();
#endif

    bool hasObjectMetadataCallback() const { return objectMetadataCallback; }
    void setObjectMetadataCallback(js::ObjectMetadataCallback callback);
    void forgetObjectMetadataCallback() {
        objectMetadataCallback = nullptr;
    }
    bool callObjectMetadataCallback(JSContext *cx, JSObject **obj) const {
        return objectMetadataCallback(cx, obj);
    }

    js::SavedStacks &savedStacks() { return savedStacks_; }

    void findOutgoingEdges(js::gc::ComponentFinder<JS::Zone> &finder);

    js::DtoaCache dtoaCache;

    /* Random number generator state, used by jsmath.cpp. */
    uint64_t rngState;

  private:
    JSCompartment *thisForCtor() { return this; }

    // Only called from {enter,leave}DebugMode.
    bool updateJITForDebugMode(JSContext *maybecx, js::AutoDebugModeInvalidation &invalidate);

  public:
    // True if this compartment's global is a debuggee of some Debugger
    // object.
    bool debugMode() const {
        return !!(debugModeBits & DebugMode);
    }

    bool enterDebugMode(JSContext *cx);
    bool enterDebugMode(JSContext *cx, js::AutoDebugModeInvalidation &invalidate);
    bool leaveDebugMode(JSContext *cx);
    bool leaveDebugMode(JSContext *cx, js::AutoDebugModeInvalidation &invalidate);
    void leaveDebugModeUnderGC();

    /* True if any scripts from this compartment are on the JS stack. */
    bool hasScriptsOnStack();

    /*
     * Schedule the compartment to be delazified. Called from
     * LazyScript::Create.
     */
    void scheduleDelazificationForDebugMode() {
        debugModeBits |= DebugNeedDelazification;
    }

    /*
     * If we scheduled delazification for turning on debug mode, delazify all
     * scripts.
     */
    bool ensureDelazifyScriptsForDebugMode(JSContext *cx);

    void clearBreakpointsIn(js::FreeOp *fop, js::Debugger *dbg, JS::HandleObject handler);

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

    /* Used by memory reporters and invalid otherwise. */
    void               *compartmentStats;

    // These flags help us to discover if a compartment that shouldn't be alive
    // manages to outlive a GC.
    bool scheduledForDestruction;
    bool maybeAlive;

  private:
    js::jit::JitCompartment *jitCompartment_;

  public:
    bool ensureJitCompartmentExists(JSContext *cx);
    js::jit::JitCompartment *jitCompartment() {
        return jitCompartment_;
    }
};

inline bool
JSRuntime::isAtomsZone(JS::Zone *zone)
{
    return zone == atomsCompartment_->zone();
}

// For use when changing the debug mode flag on one or more compartments.
// Invalidate and discard JIT code since debug mode breaks JIT assumptions.
//
// AutoDebugModeInvalidation has two modes: compartment or zone
// invalidation. While it is correct to always use compartment invalidation,
// if you know ahead of time you need to invalidate a whole zone, it is faster
// to invalidate the zone.
//
// Compartment invalidation only invalidates scripts belonging to that
// compartment.
//
// Zone invalidation invalidates all scripts belonging to non-special
// (i.e. those with principals) compartments of the zone.
//
// FIXME: Remove entirely once bug 716647 lands.
//
class js::AutoDebugModeInvalidation
{
    JSCompartment *comp_;
    JS::Zone *zone_;

    enum {
        NoNeed = 0,
        ToggledOn = 1,
        ToggledOff = 2
    } needInvalidation_;

  public:
    explicit AutoDebugModeInvalidation(JSCompartment *comp)
      : comp_(comp), zone_(nullptr), needInvalidation_(NoNeed)
    { }

    explicit AutoDebugModeInvalidation(JS::Zone *zone)
      : comp_(nullptr), zone_(zone), needInvalidation_(NoNeed)
    { }

    ~AutoDebugModeInvalidation();

    bool isFor(JSCompartment *comp) {
        if (comp_)
            return comp == comp_;
        return comp->zone() == zone_;
    }

    void scheduleInvalidation(bool debugMode) {
        // If we are scheduling invalidation for multiple compartments, they
        // must all agree on the toggle. This is so we can decide if we need
        // to invalidate on-stack scripts.
        MOZ_ASSERT_IF(needInvalidation_ != NoNeed,
                      needInvalidation_ == (debugMode ? ToggledOn : ToggledOff));
        needInvalidation_ = debugMode ? ToggledOn : ToggledOff;
    }
};

namespace js {

inline js::Handle<js::GlobalObject*>
ExclusiveContext::global() const
{
    /*
     * It's safe to use |unsafeGet()| here because any compartment that is
     * on-stack will be marked automatically, so there's no need for a read
     * barrier on it. Once the compartment is popped, the handle is no longer
     * safe to use.
     */
    MOZ_ASSERT(compartment_, "Caller needs to enter a compartment first");
    return Handle<GlobalObject*>::fromMarkedLocation(compartment_->global_.unsafeGet());
}

class AssertCompartmentUnchanged
{
  public:
    explicit AssertCompartmentUnchanged(JSContext *cx
                                        MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx), oldCompartment(cx->compartment())
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~AssertCompartmentUnchanged() {
        MOZ_ASSERT(cx->compartment() == oldCompartment);
    }

  protected:
    JSContext * const cx;
    JSCompartment * const oldCompartment;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoCompartment
{
    ExclusiveContext * const cx_;
    JSCompartment * const origin_;

  public:
    inline AutoCompartment(ExclusiveContext *cx, JSObject *target);
    inline AutoCompartment(ExclusiveContext *cx, JSCompartment *target);
    inline ~AutoCompartment();

    ExclusiveContext *context() const { return cx_; }
    JSCompartment *origin() const { return origin_; }

  private:
    AutoCompartment(const AutoCompartment &) MOZ_DELETE;
    AutoCompartment & operator=(const AutoCompartment &) MOZ_DELETE;
};

/*
 * Use this to change the behavior of an AutoCompartment slightly on error. If
 * the exception happens to be an Error object, copy it to the origin compartment
 * instead of wrapping it.
 */
class ErrorCopier
{
    mozilla::Maybe<AutoCompartment> &ac;

  public:
    explicit ErrorCopier(mozilla::Maybe<AutoCompartment> &ac)
      : ac(ac) {}
    ~ErrorCopier();
};

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
      : value(*ptr->value().unsafeGet())
    {}

    explicit WrapperValue(const WrapperMap::Enum &e)
      : value(*e.front().value().unsafeGet())
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

class AutoWrapperRooter : private JS::AutoGCRooter {
  public:
    AutoWrapperRooter(JSContext *cx, WrapperValue v
                      MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : JS::AutoGCRooter(cx, WRAPPER), value(v)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    operator JSObject *() const {
        return value.get().toObjectOrNull();
    }

    friend void JS::AutoGCRooter::trace(JSTracer *trc);

  private:
    WrapperValue value;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} /* namespace js */

#endif /* jscompartment_h */
