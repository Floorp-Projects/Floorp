/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Compartment_h
#define vm_Compartment_h

#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Tuple.h"
#include "mozilla/Variant.h"

#include <stddef.h>

#include "gc/Barrier.h"
#include "gc/NurseryAwareHashMap.h"
#include "gc/Zone.h"
#include "js/UniquePtr.h"

namespace js {

namespace gc {
template <typename Node, typename Derived> class ComponentFinder;
} // namespace gc

class CrossCompartmentKey
{
  public:
    enum DebuggerObjectKind : uint8_t { DebuggerSource, DebuggerEnvironment, DebuggerObject,
                                        DebuggerWasmScript, DebuggerWasmSource };
    using DebuggerAndObject = mozilla::Tuple<NativeObject*, JSObject*, DebuggerObjectKind>;
    using DebuggerAndScript = mozilla::Tuple<NativeObject*, JSScript*>;
    using WrappedType = mozilla::Variant<
        JSObject*,
        JSString*,
        DebuggerAndScript,
        DebuggerAndObject>;

    explicit CrossCompartmentKey(JSObject* obj) : wrapped(obj) { MOZ_RELEASE_ASSERT(obj); }
    explicit CrossCompartmentKey(JSString* str) : wrapped(str) { MOZ_RELEASE_ASSERT(str); }
    explicit CrossCompartmentKey(const JS::Value& v)
      : wrapped(v.isString() ? WrappedType(v.toString()) : WrappedType(&v.toObject()))
    {}
    explicit CrossCompartmentKey(NativeObject* debugger, JSObject* obj, DebuggerObjectKind kind)
      : wrapped(DebuggerAndObject(debugger, obj, kind))
    {
        MOZ_RELEASE_ASSERT(debugger);
        MOZ_RELEASE_ASSERT(obj);
    }
    explicit CrossCompartmentKey(NativeObject* debugger, JSScript* script)
      : wrapped(DebuggerAndScript(debugger, script))
    {
        MOZ_RELEASE_ASSERT(debugger);
        MOZ_RELEASE_ASSERT(script);
    }

    bool operator==(const CrossCompartmentKey& other) const { return wrapped == other.wrapped; }
    bool operator!=(const CrossCompartmentKey& other) const { return wrapped != other.wrapped; }

    template <typename T> bool is() const { return wrapped.is<T>(); }
    template <typename T> const T& as() const { return wrapped.as<T>(); }

    template <typename F>
    auto applyToWrapped(F f) -> decltype(f(static_cast<JSObject**>(nullptr))) {
        using ReturnType = decltype(f(static_cast<JSObject**>(nullptr)));
        struct WrappedMatcher {
            F f_;
            explicit WrappedMatcher(F f) : f_(f) {}
            ReturnType match(JSObject*& obj) { return f_(&obj); }
            ReturnType match(JSString*& str) { return f_(&str); }
            ReturnType match(DebuggerAndScript& tpl) { return f_(&mozilla::Get<1>(tpl)); }
            ReturnType match(DebuggerAndObject& tpl) { return f_(&mozilla::Get<1>(tpl)); }
        } matcher(f);
        return wrapped.match(matcher);
    }

    template <typename F>
    auto applyToDebugger(F f) -> decltype(f(static_cast<NativeObject**>(nullptr))) {
        using ReturnType = decltype(f(static_cast<NativeObject**>(nullptr)));
        struct DebuggerMatcher {
            F f_;
            explicit DebuggerMatcher(F f) : f_(f) {}
            ReturnType match(JSObject*& obj) { return ReturnType(); }
            ReturnType match(JSString*& str) { return ReturnType(); }
            ReturnType match(DebuggerAndScript& tpl) { return f_(&mozilla::Get<0>(tpl)); }
            ReturnType match(DebuggerAndObject& tpl) { return f_(&mozilla::Get<0>(tpl)); }
        } matcher(f);
        return wrapped.match(matcher);
    }

    JS::Compartment* compartment() {
        struct GetCompartmentFunctor {
            JS::Compartment* operator()(JSObject** tp) const { return (*tp)->compartment(); }
            JS::Compartment* operator()(JSScript** tp) const { return (*tp)->compartment(); }
            JS::Compartment* operator()(JSString** tp) const { return nullptr; }
        };
        return applyToWrapped(GetCompartmentFunctor());
    }

    struct Hasher : public DefaultHasher<CrossCompartmentKey>
    {
        struct HashFunctor {
            HashNumber match(JSObject* obj) { return DefaultHasher<JSObject*>::hash(obj); }
            HashNumber match(JSString* str) { return DefaultHasher<JSString*>::hash(str); }
            HashNumber match(const DebuggerAndScript& tpl) {
                return DefaultHasher<NativeObject*>::hash(mozilla::Get<0>(tpl)) ^
                       DefaultHasher<JSScript*>::hash(mozilla::Get<1>(tpl));
            }
            HashNumber match(const DebuggerAndObject& tpl) {
                return DefaultHasher<NativeObject*>::hash(mozilla::Get<0>(tpl)) ^
                       DefaultHasher<JSObject*>::hash(mozilla::Get<1>(tpl)) ^
                       (mozilla::Get<2>(tpl) << 5);
            }
        };
        static HashNumber hash(const CrossCompartmentKey& key) {
            return key.wrapped.match(HashFunctor());
        }

        static bool match(const CrossCompartmentKey& l, const CrossCompartmentKey& k) {
            return l.wrapped == k.wrapped;
        }
    };

    bool isTenured() const {
        struct IsTenuredFunctor {
            using ReturnType = bool;
            ReturnType operator()(JSObject** tp) { return !IsInsideNursery(*tp); }
            ReturnType operator()(JSScript** tp) { return true; }
            ReturnType operator()(JSString** tp) { return !IsInsideNursery(*tp); }
        };
        return const_cast<CrossCompartmentKey*>(this)->applyToWrapped(IsTenuredFunctor());
    }

    void trace(JSTracer* trc);
    bool needsSweep();

  private:
    CrossCompartmentKey() = delete;
    WrappedType wrapped;
};

// The data structure for storing CCWs, which has a map per target compartment
// so we can access them easily. Note string CCWs are stored separately from the
// others because they have target compartment nullptr.
class WrapperMap
{
    static const size_t InitialInnerMapSize = 4;

    using InnerMap = NurseryAwareHashMap<CrossCompartmentKey,
                                         JS::Value,
                                         CrossCompartmentKey::Hasher,
                                         SystemAllocPolicy>;
    using OuterMap = GCHashMap<JS::Compartment*,
                               InnerMap,
                               DefaultHasher<JS::Compartment*>,
                               SystemAllocPolicy>;

    OuterMap map;

  public:
    class Enum
    {
      public:
        enum SkipStrings : bool {
            WithStrings = false,
            WithoutStrings = true
        };

      private:
        Enum(const Enum&) = delete;
        void operator=(const Enum&) = delete;

        void goToNext() {
            if (outer.isNothing())
                return;
            for (; !outer->empty(); outer->popFront()) {
                JS::Compartment* c = outer->front().key();
                // Need to skip string at first, because the filter may not be
                // happy with a nullptr.
                if (!c && skipStrings)
                    continue;
                if (filter && !filter->match(c))
                    continue;
                InnerMap& m = outer->front().value();
                if (!m.empty()) {
                    if (inner.isSome())
                        inner.reset();
                    inner.emplace(m);
                    outer->popFront();
                    return;
                }
            }
        }

        mozilla::Maybe<OuterMap::Enum> outer;
        mozilla::Maybe<InnerMap::Enum> inner;
        const CompartmentFilter* filter;
        SkipStrings skipStrings;

      public:
        explicit Enum(WrapperMap& m, SkipStrings s = WithStrings) :
                filter(nullptr), skipStrings(s) {
            outer.emplace(m.map);
            goToNext();
        }

        Enum(WrapperMap& m, const CompartmentFilter& f, SkipStrings s = WithStrings) :
                filter(&f), skipStrings(s) {
            outer.emplace(m.map);
            goToNext();
        }

        Enum(WrapperMap& m, JS::Compartment* target) {
            // Leave the outer map as nothing and only iterate the inner map we
            // find here.
            auto p = m.map.lookup(target);
            if (p)
                inner.emplace(p->value());
        }

        bool empty() const {
            return (outer.isNothing() || outer->empty()) &&
                   (inner.isNothing() || inner->empty());
        }

        InnerMap::Entry& front() const {
            MOZ_ASSERT(inner.isSome() && !inner->empty());
            return inner->front();
        }

        void popFront() {
            MOZ_ASSERT(!empty());
            if (!inner->empty()) {
                inner->popFront();
                if (!inner->empty())
                    return;
            }
            goToNext();
        }

        void removeFront() {
            MOZ_ASSERT(inner.isSome());
            inner->removeFront();
        }
    };

    class Ptr : public InnerMap::Ptr
    {
        friend class WrapperMap;

        InnerMap* map;

        Ptr() : InnerMap::Ptr(), map(nullptr) {}
        Ptr(const InnerMap::Ptr& p, InnerMap& m) : InnerMap::Ptr(p), map(&m) {}
    };

    MOZ_MUST_USE bool init(uint32_t len) { return map.init(len); }

    bool empty() {
        if (map.empty())
            return true;
        for (OuterMap::Enum e(map); !e.empty(); e.popFront()) {
            if (!e.front().value().empty())
                return false;
        }
        return true;
    }

    Ptr lookup(const CrossCompartmentKey& k) const {
        auto op = map.lookup(const_cast<CrossCompartmentKey&>(k).compartment());
        if (op) {
            auto ip = op->value().lookup(k);
            if (ip)
                return Ptr(ip, op->value());
        }
        return Ptr();
    }

    void remove(Ptr p) {
        if (p)
            p.map->remove(p);
    }

    MOZ_MUST_USE bool put(const CrossCompartmentKey& k, const JS::Value& v) {
        JS::Compartment* c = const_cast<CrossCompartmentKey&>(k).compartment();
        MOZ_ASSERT(k.is<JSString*>() == !c);
        auto p = map.lookupForAdd(c);
        if (!p) {
            InnerMap m;
            if (!m.init(InitialInnerMapSize) || !map.add(p, c, std::move(m)))
                return false;
        }
        return p->value().put(k, v);
    }

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) {
        size_t size = map.sizeOfExcludingThis(mallocSizeOf);
        for (OuterMap::Enum e(map); !e.empty(); e.popFront())
            size += e.front().value().sizeOfExcludingThis(mallocSizeOf);
        return size;
    }
    size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) {
        size_t size = map.sizeOfIncludingThis(mallocSizeOf);
        for (OuterMap::Enum e(map); !e.empty(); e.popFront())
            size += e.front().value().sizeOfIncludingThis(mallocSizeOf);
        return size;
    }

    bool hasNurseryAllocatedWrapperEntries(const CompartmentFilter& f) {
        for (OuterMap::Enum e(map); !e.empty(); e.popFront()) {
            JS::Compartment* c = e.front().key();
            if (c && !f.match(c))
                continue;
            InnerMap& m = e.front().value();
            if (m.hasNurseryEntries())
                return true;
        }
        return false;
    }

    void sweepAfterMinorGC(JSTracer* trc) {
        for (OuterMap::Enum e(map); !e.empty(); e.popFront()) {
            InnerMap& m = e.front().value();
            m.sweepAfterMinorGC(trc);
            if (m.empty())
                e.removeFront();
        }
    }

    void sweep() {
        for (OuterMap::Enum e(map); !e.empty(); e.popFront()) {
            InnerMap& m = e.front().value();
            m.sweep();
            if (m.empty())
                e.removeFront();
        }
    }
};

} // namespace js

class JS::Compartment
{
    JS::Zone*                    zone_;
    JSRuntime*                   runtime_;

    js::WrapperMap crossCompartmentWrappers;

    using RealmVector = js::Vector<JS::Realm*, 1, js::SystemAllocPolicy>;
    RealmVector realms_;

  public:
    /*
     * During GC, stores the head of a list of incoming pointers from gray cells.
     *
     * The objects in the list are either cross-compartment wrappers, or
     * debugger wrapper objects.  The list link is either in the second extra
     * slot for the former, or a special slot for the latter.
     */
    JSObject* gcIncomingGrayPointers = nullptr;

    void* data = nullptr;

    // Fields set and used by the GC. Be careful, may be stale after we return
    // to the mutator.
    struct {
        // These flags help us to discover if a compartment that shouldn't be
        // alive manages to outlive a GC. Note that these flags have to be on
        // the compartment, not the realm, because same-compartment realms can
        // have cross-realm pointers without wrappers.
        bool scheduledForDestruction = false;
        bool maybeAlive = true;

        // During GC, we may set this to |true| if we entered a realm in this
        // compartment. Note that (without a stack walk) we don't know exactly
        // *which* realms, because Realm::enterRealmDepthIgnoringJit_ does not
        // account for cross-Realm calls in JIT code updating cx->realm_. See
        // also the enterRealmDepthIgnoringJit_ comment.
        bool hasEnteredRealm = false;
    } gcState;

    JS::Zone* zone() { return zone_; }
    const JS::Zone* zone() const { return zone_; }

    JSRuntime* runtimeFromMainThread() const {
        MOZ_ASSERT(js::CurrentThreadCanAccessRuntime(runtime_));
        return runtime_;
    }

    // Note: Unrestricted access to the zone's runtime from an arbitrary
    // thread can easily lead to races. Use this method very carefully.
    JSRuntime* runtimeFromAnyThread() const {
        return runtime_;
    }

    RealmVector& realms() {
        return realms_;
    }

    void assertNoCrossCompartmentWrappers() {
        MOZ_ASSERT(crossCompartmentWrappers.empty());
    }

    void addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                                size_t* compartmentObjects,
                                size_t* crossCompartmentWrappersTables,
                                size_t* compartmentsPrivateData);

#ifdef JSGC_HASH_TABLE_CHECKS
    void checkWrapperMapAfterMovingGC();
#endif

  private:
    bool getNonWrapperObjectForCurrentCompartment(JSContext* cx, js::MutableHandleObject obj);
    bool getOrCreateWrapper(JSContext* cx, js::HandleObject existing, js::MutableHandleObject obj);

  public:
    explicit Compartment(JS::Zone* zone);

    MOZ_MUST_USE bool init(JSContext* cx);
    void destroy(js::FreeOp* fop);

    MOZ_MUST_USE inline bool wrap(JSContext* cx, JS::MutableHandleValue vp);

    MOZ_MUST_USE bool wrap(JSContext* cx, js::MutableHandleString strp);
#ifdef ENABLE_BIGINT
    MOZ_MUST_USE bool wrap(JSContext* cx, js::MutableHandle<JS::BigInt*> bi);
#endif
    MOZ_MUST_USE bool wrap(JSContext* cx, JS::MutableHandleObject obj);
    MOZ_MUST_USE bool wrap(JSContext* cx, JS::MutableHandle<js::PropertyDescriptor> desc);
    MOZ_MUST_USE bool wrap(JSContext* cx, JS::MutableHandle<JS::GCVector<JS::Value>> vec);
    MOZ_MUST_USE bool rewrap(JSContext* cx, JS::MutableHandleObject obj, JS::HandleObject existing);

    MOZ_MUST_USE bool putWrapper(JSContext* cx, const js::CrossCompartmentKey& wrapped,
                                 const js::Value& wrapper);

    js::WrapperMap::Ptr lookupWrapper(const js::Value& wrapped) const {
        return crossCompartmentWrappers.lookup(js::CrossCompartmentKey(wrapped));
    }

    js::WrapperMap::Ptr lookupWrapper(JSObject* obj) const {
        return crossCompartmentWrappers.lookup(js::CrossCompartmentKey(obj));
    }

    void removeWrapper(js::WrapperMap::Ptr p) {
        crossCompartmentWrappers.remove(p);
    }

    bool hasNurseryAllocatedWrapperEntries(const js::CompartmentFilter& f) {
        return crossCompartmentWrappers.hasNurseryAllocatedWrapperEntries(f);
    }

    struct WrapperEnum : public js::WrapperMap::Enum {
        explicit WrapperEnum(JS::Compartment* c)
          : js::WrapperMap::Enum(c->crossCompartmentWrappers)
        {}
    };

    struct NonStringWrapperEnum : public js::WrapperMap::Enum {
        explicit NonStringWrapperEnum(JS::Compartment* c)
          : js::WrapperMap::Enum(c->crossCompartmentWrappers, WithoutStrings)
        {}
        explicit NonStringWrapperEnum(JS::Compartment* c, const js::CompartmentFilter& f)
          : js::WrapperMap::Enum(c->crossCompartmentWrappers, f, WithoutStrings)
        {}
        explicit NonStringWrapperEnum(JS::Compartment* c, JS::Compartment* target)
          : js::WrapperMap::Enum(c->crossCompartmentWrappers, target)
        {
            MOZ_ASSERT(target);
        }
    };

    struct StringWrapperEnum : public js::WrapperMap::Enum {
        explicit StringWrapperEnum(JS::Compartment* c)
          : js::WrapperMap::Enum(c->crossCompartmentWrappers, nullptr)
        {}
    };

    /*
     * These methods mark pointers that cross compartment boundaries. They are
     * called in per-zone GCs to prevent the wrappers' outgoing edges from
     * dangling (full GCs naturally follow pointers across compartments) and
     * when compacting to update cross-compartment pointers.
     */
    void traceOutgoingCrossCompartmentWrappers(JSTracer* trc);
    static void traceIncomingCrossCompartmentEdgesForZoneGC(JSTracer* trc);

    void sweepRealms(js::FreeOp* fop, bool keepAtleastOne, bool destroyingRuntime);
    void sweepAfterMinorGC(JSTracer* trc);
    void sweepCrossCompartmentWrappers();

    static void fixupCrossCompartmentWrappersAfterMovingGC(JSTracer* trc);
    void fixupAfterMovingGC();

    void findOutgoingEdges(js::gc::ZoneComponentFinder& finder);
};

namespace js {

// We only set the maybeAlive flag for objects and scripts. It's assumed that,
// if a compartment is alive, then it will have at least some live object or
// script it in. Even if we get this wrong, the worst that will happen is that
// scheduledForDestruction will be set on the compartment, which will cause
// some extra GC activity to try to free the compartment.
template<typename T> inline void SetMaybeAliveFlag(T* thing) {}

template<> inline void
SetMaybeAliveFlag(JSObject* thing)
{
    thing->compartment()->gcState.maybeAlive = true;
}

template<> inline void
SetMaybeAliveFlag(JSScript* thing)
{
    thing->compartment()->gcState.maybeAlive = true;
}

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
     * gc/GC.cpp regarding this). If there is an incremental GC while the
     * wrapper is in use, the AutoWrapper rooter will ensure the wrapper gets
     * marked.
     */
    explicit WrapperValue(const WrapperMap::Ptr& ptr)
      : value(*ptr->value().unsafeGet())
    {}

    explicit WrapperValue(const WrapperMap::Enum& e)
      : value(*e.front().value().unsafeGet())
    {}

    Value& get() { return value; }
    Value get() const { return value; }
    operator const Value&() const { return value; }
    JSObject& toObject() const { return value.toObject(); }

  private:
    Value value;
};

class MOZ_RAII AutoWrapperVector : public JS::GCVector<WrapperValue, 8>,
                                   private JS::AutoGCRooter
{
  public:
    explicit AutoWrapperVector(JSContext* cx
                               MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : JS::GCVector<WrapperValue, 8>(cx),
        JS::AutoGCRooter(cx, JS::AutoGCRooter::Tag::WrapperVector)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    friend void AutoGCRooter::trace(JSTracer* trc);

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class MOZ_RAII AutoWrapperRooter : private JS::AutoGCRooter {
  public:
    AutoWrapperRooter(JSContext* cx, const WrapperValue& v
                      MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : JS::AutoGCRooter(cx, JS::AutoGCRooter::Tag::Wrapper), value(v)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    operator JSObject*() const {
        return value.get().toObjectOrNull();
    }

    friend void JS::AutoGCRooter::trace(JSTracer* trc);

  private:
    WrapperValue value;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} /* namespace js */

namespace JS {
template <>
struct GCPolicy<js::CrossCompartmentKey> : public StructGCPolicy<js::CrossCompartmentKey> {
    static bool isTenured(const js::CrossCompartmentKey& key) { return key.isTenured(); }
};
} // namespace JS

#endif /* vm_Compartment_h */
