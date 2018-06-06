/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_JSCompartment_h
#define vm_JSCompartment_h

#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Tuple.h"
#include "mozilla/Variant.h"
#include "mozilla/XorShift128PlusRNG.h"

#include <stddef.h>

#include "gc/Barrier.h"
#include "gc/NurseryAwareHashMap.h"
#include "gc/Zone.h"
#include "js/UniquePtr.h"
#include "vm/ArrayBufferObject.h"
#include "vm/GlobalObject.h"
#include "vm/ReceiverGuard.h"
#include "vm/RegExpShared.h"
#include "vm/SavedStacks.h"
#include "vm/Time.h"
#include "wasm/WasmRealm.h"

namespace js {

namespace jit {
class JitRealm;
} // namespace jit

namespace gc {
template <typename Node, typename Derived> class ComponentFinder;
} // namespace gc

class AutoRestoreRealmDebugMode;
class GlobalObject;
class LexicalEnvironmentObject;
class MapObject;
class ScriptSourceObject;
class SetObject;
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
    JSFlatString* s;      // if s==nullptr, d and base are not valid

  public:
    DtoaCache() : s(nullptr) {}
    void purge() { s = nullptr; }

    JSFlatString* lookup(int base, double d) {
        return this->s && base == this->base && d == this->d ? this->s : nullptr;
    }

    void cache(int base, double d, JSFlatString* s) {
        this->base = base;
        this->d = d;
        this->s = s;
    }

#ifdef JSGC_HASH_TABLE_CHECKS
    void checkCacheAfterMovingGC();
#endif
};

// Cache to speed up the group/shape lookup in ProxyObject::create. A proxy's
// group/shape is only determined by the Class + proto, so a small cache for
// this is very effective in practice.
class NewProxyCache
{
    struct Entry {
        ObjectGroup* group;
        Shape* shape;
    };
    static const size_t NumEntries = 4;
    mozilla::UniquePtr<Entry[], JS::FreePolicy> entries_;

  public:
    MOZ_ALWAYS_INLINE bool lookup(const Class* clasp, TaggedProto proto,
                                  ObjectGroup** group, Shape** shape) const
    {
        if (!entries_)
            return false;
        for (size_t i = 0; i < NumEntries; i++) {
            const Entry& entry = entries_[i];
            if (entry.group && entry.group->clasp() == clasp && entry.group->proto() == proto) {
                *group = entry.group;
                *shape = entry.shape;
                return true;
            }
        }
        return false;
    }
    void add(ObjectGroup* group, Shape* shape) {
        MOZ_ASSERT(group && shape);
        if (!entries_) {
            entries_.reset(js_pod_calloc<Entry>(NumEntries));
            if (!entries_)
                return;
        } else {
            for (size_t i = NumEntries - 1; i > 0; i--)
                entries_[i] = entries_[i - 1];
        }
        entries_[0].group = group;
        entries_[0].shape = shape;
    }
    void purge() {
        entries_.reset();
    }
};

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

    JSCompartment* compartment() {
        struct GetCompartmentFunctor {
            JSCompartment* operator()(JSObject** tp) const { return (*tp)->compartment(); }
            JSCompartment* operator()(JSScript** tp) const { return (*tp)->compartment(); }
            JSCompartment* operator()(JSString** tp) const { return nullptr; }
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
    using OuterMap = GCHashMap<JSCompartment*,
                               InnerMap,
                               DefaultHasher<JSCompartment*>,
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
                JSCompartment* c = outer->front().key();
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

        Enum(WrapperMap& m, JSCompartment* target) {
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
        JSCompartment* c = const_cast<CrossCompartmentKey&>(k).compartment();
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
            JSCompartment* c = e.front().key();
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

// We must ensure that all newly allocated JSObjects get their metadata
// set. However, metadata builders may require the new object be in a sane
// state (eg, have its reserved slots initialized so they can get the
// sizeOfExcludingThis of the object). Therefore, for objects of certain
// JSClasses (those marked with JSCLASS_DELAY_METADATA_BUILDER), it is not safe
// for the allocation paths to call the object metadata builder
// immediately. Instead, the JSClass-specific "constructor" C++ function up the
// stack makes a promise that it will ensure that the new object has its
// metadata set after the object is initialized.
//
// To help those constructor functions keep their promise of setting metadata,
// each compartment is in one of three states at any given time:
//
// * ImmediateMetadata: Allocators should set new object metadata immediately,
//                      as usual.
//
// * DelayMetadata: Allocators should *not* set new object metadata, it will be
//                  handled after reserved slots are initialized by custom code
//                  for the object's JSClass. The newly allocated object's
//                  JSClass *must* have the JSCLASS_DELAY_METADATA_BUILDER flag
//                  set.
//
// * PendingMetadata: This object has been allocated and is still pending its
//                    metadata. This should never be the case when we begin an
//                    allocation, as a constructor function was supposed to have
//                    set the metadata of the previous object *before*
//                    allocating another object.
//
// The js::AutoSetNewObjectMetadata RAII class provides an ergonomic way for
// constructor functions to navigate state transitions, and its instances
// collectively maintain a stack of previous states. The stack is required to
// support the lazy resolution and allocation of global builtin constructors and
// prototype objects. The initial (and intuitively most common) state is
// ImmediateMetadata.
//
// Without the presence of internal errors (such as OOM), transitions between
// the states are as follows:
//
//     ImmediateMetadata                 .----- previous state on stack
//           |                           |          ^
//           | via constructor           |          |
//           |                           |          | via setting the new
//           |        via constructor    |          | object's metadata
//           |   .-----------------------'          |
//           |   |                                  |
//           V   V                                  |
//     DelayMetadata -------------------------> PendingMetadata
//                         via allocation
//
// In the presence of internal errors, we do not set the new object's metadata
// (if it was even allocated) and reset to the previous state on the stack.

struct ImmediateMetadata { };
struct DelayMetadata { };
using PendingMetadata = JSObject*;

using NewObjectMetadataState = mozilla::Variant<ImmediateMetadata,
                                                DelayMetadata,
                                                PendingMetadata>;

class MOZ_RAII AutoSetNewObjectMetadata : private JS::CustomAutoRooter
{
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER;

    JSContext* cx_;
    NewObjectMetadataState prevState_;

    AutoSetNewObjectMetadata(const AutoSetNewObjectMetadata& aOther) = delete;
    void operator=(const AutoSetNewObjectMetadata& aOther) = delete;

  protected:
    virtual void trace(JSTracer* trc) override {
        if (prevState_.is<PendingMetadata>()) {
            TraceRoot(trc,
                      &prevState_.as<PendingMetadata>(),
                      "Object pending metadata");
        }
    }

  public:
    explicit AutoSetNewObjectMetadata(JSContext* cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
    ~AutoSetNewObjectMetadata();
};

class PropertyIteratorObject;

struct IteratorHashPolicy
{
    struct Lookup {
        ReceiverGuard* guards;
        size_t numGuards;
        uint32_t key;

        Lookup(ReceiverGuard* guards, size_t numGuards, uint32_t key)
          : guards(guards), numGuards(numGuards), key(key)
        {
            MOZ_ASSERT(numGuards > 0);
        }
    };
    static HashNumber hash(const Lookup& lookup) {
        return lookup.key;
    }
    static bool match(PropertyIteratorObject* obj, const Lookup& lookup);
};

} /* namespace js */

namespace js {
class DebugEnvironments;
class ObjectWeakMap;
class WeakMapBase;
} // namespace js

struct JSCompartment
{
  private:
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

    // These flags help us to discover if a compartment that shouldn't be alive
    // manages to outlive a GC. Note that these flags have to be on the
    // compartment, not the realm, because same-compartment realms can have
    // cross-realm pointers without wrappers.
    bool scheduledForDestruction = false;
    bool maybeAlive = true;

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
    explicit JSCompartment(JS::Zone* zone);

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
        explicit WrapperEnum(JSCompartment* c) : js::WrapperMap::Enum(c->crossCompartmentWrappers) {}
    };

    struct NonStringWrapperEnum : public js::WrapperMap::Enum {
        explicit NonStringWrapperEnum(JSCompartment* c) : js::WrapperMap::Enum(c->crossCompartmentWrappers, WithoutStrings) {}
        explicit NonStringWrapperEnum(JSCompartment* c, const js::CompartmentFilter& f) : js::WrapperMap::Enum(c->crossCompartmentWrappers, f, WithoutStrings) {}
        explicit NonStringWrapperEnum(JSCompartment* c, JSCompartment* target) : js::WrapperMap::Enum(c->crossCompartmentWrappers, target) { MOZ_ASSERT(target); }
    };

    struct StringWrapperEnum : public js::WrapperMap::Enum {
        explicit StringWrapperEnum(JSCompartment* c) : js::WrapperMap::Enum(c->crossCompartmentWrappers, nullptr) {}
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

// ObjectRealm stores various tables and other state associated with particular
// objects in a realm. To make sure the correct ObjectRealm is used for an
// object, use of the ObjectRealm::get(obj) static method is required.
class ObjectRealm
{
    using NativeIteratorSentinel = js::UniquePtr<js::NativeIterator, JS::FreePolicy>;
    NativeIteratorSentinel iteratorSentinel_;

    // All non-syntactic lexical environments in the realm. These are kept in a
    // map because when loading scripts into a non-syntactic environment, we
    // need to use the same lexical environment to persist lexical bindings.
    js::UniquePtr<js::ObjectWeakMap> nonSyntacticLexicalEnvironments_;

    ObjectRealm(const ObjectRealm&) = delete;
    void operator=(const ObjectRealm&) = delete;

  public:
    // List of potentially active iterators that may need deleted property
    // suppression.
    js::NativeIterator* enumerators = nullptr;

    // Map from array buffers to views sharing that storage.
    JS::WeakCache<js::InnerViewTable> innerViews;

    // Inline transparent typed objects do not initially have an array buffer,
    // but can have that buffer created lazily if it is accessed later. This
    // table manages references from such typed objects to their buffers.
    js::UniquePtr<js::ObjectWeakMap> lazyArrayBuffers;

    // Keep track of the metadata objects which can be associated with each JS
    // object. Both keys and values are in this realm.
    js::UniquePtr<js::ObjectWeakMap> objectMetadataTable;

    using IteratorCache = js::HashSet<js::PropertyIteratorObject*,
                                      js::IteratorHashPolicy,
                                      js::SystemAllocPolicy>;
    IteratorCache iteratorCache;

    static inline ObjectRealm& get(const JSObject* obj);

    explicit ObjectRealm(JS::Zone* zone);
    ~ObjectRealm();

    MOZ_MUST_USE bool init(JSContext* cx);

    void finishRoots();
    void trace(JSTracer* trc);
    void sweepAfterMinorGC();
    void sweepNativeIterators();

    void addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf,
                                size_t* innerViewsArg,
                                size_t* lazyArrayBuffersArg,
                                size_t* objectMetadataTablesArg,
                                size_t* nonSyntacticLexicalEnvironmentsArg);

    MOZ_ALWAYS_INLINE bool objectMaybeInIteration(JSObject* obj);

    js::LexicalEnvironmentObject*
    getOrCreateNonSyntacticLexicalEnvironment(JSContext* cx, js::HandleObject enclosing);
    js::LexicalEnvironmentObject* getNonSyntacticLexicalEnvironment(JSObject* enclosing) const;
};

} // namespace js

class JS::Realm : public JS::shadow::Realm
{
    JS::Zone* zone_;
    JSRuntime* runtime_;

    const JS::RealmCreationOptions creationOptions_;
    JS::RealmBehaviors behaviors_;

    friend struct ::JSContext;
    js::ReadBarrieredGlobalObject global_;

    // Note: this is private to enforce use of ObjectRealm::get(obj).
    js::ObjectRealm objects_;
    friend js::ObjectRealm& js::ObjectRealm::get(const JSObject*);

    // Object group tables and other state in the realm. This is private to
    // enforce use of ObjectGroupRealm::get(group)/getForNewObject(cx).
    js::ObjectGroupRealm objectGroups_;
    friend js::ObjectGroupRealm& js::ObjectGroupRealm::get(js::ObjectGroup* group);
    friend js::ObjectGroupRealm& js::ObjectGroupRealm::getForNewObject(JSContext* cx);

    // The global environment record's [[VarNames]] list that contains all
    // names declared using FunctionDeclaration, GeneratorDeclaration, and
    // VariableDeclaration declarations in global code in this realm.
    // Names are only removed from this list by a |delete IdentifierReference|
    // that successfully removes that global property.
    using VarNamesSet = JS::GCHashSet<JSAtom*,
                                      js::DefaultHasher<JSAtom*>,
                                      js::SystemAllocPolicy>;
    VarNamesSet varNames_;

    friend class js::AutoSetNewObjectMetadata;
    js::NewObjectMetadataState objectMetadataState_ { js::ImmediateMetadata() };

    // Random number generator for Math.random().
    mozilla::Maybe<mozilla::non_crypto::XorShift128PlusRNG> randomNumberGenerator_;

    // Random number generator for randomHashCodeScrambler().
    mozilla::non_crypto::XorShift128PlusRNG randomKeyGenerator_;

    JSPrincipals* principals_ = nullptr;

    js::UniquePtr<js::jit::JitRealm> jitRealm_;

    // Bookkeeping information for debug scope objects.
    js::UniquePtr<js::DebugEnvironments> debugEnvs_;

    js::SavedStacks savedStacks_;

    // Used by memory reporters and invalid otherwise.
    JS::RealmStats* realmStats_ = nullptr;

    const js::AllocationMetadataBuilder* allocationMetadataBuilder_ = nullptr;
    void* realmPrivate_ = nullptr;

    // This pointer is controlled by the embedder. If it is non-null, and if
    // cx->enableAccessValidation is true, then we assert that *validAccessPtr
    // is true before running any code in this realm.
    bool* validAccessPtr_ = nullptr;

    js::ReadBarriered<js::ArgumentsObject*> mappedArgumentsTemplate_ { nullptr };
    js::ReadBarriered<js::ArgumentsObject*> unmappedArgumentsTemplate_ { nullptr };
    js::ReadBarriered<js::NativeObject*> iterResultTemplate_ { nullptr };

    unsigned enterRealmDepth_ = 0;

    enum {
        IsDebuggee = 1 << 0,
        DebuggerObservesAllExecution = 1 << 1,
        DebuggerObservesAsmJS = 1 << 2,
        DebuggerObservesCoverage = 1 << 3,
        DebuggerObservesBinarySource = 1 << 4,
        DebuggerNeedsDelazification = 1 << 5
    };
    static const unsigned DebuggerObservesMask = IsDebuggee |
                                                 DebuggerObservesAllExecution |
                                                 DebuggerObservesCoverage |
                                                 DebuggerObservesAsmJS |
                                                 DebuggerObservesBinarySource;
    unsigned debugModeBits_ = 0;
    friend class js::AutoRestoreRealmDebugMode;

    bool isSelfHostingRealm_ = false;
    bool marked_ = true;
    bool isSystem_ = false;

  public:
    // WebAssembly state for the realm.
    js::wasm::Realm wasm;

    // Aggregated output used to collect JSScript hit counts when code coverage
    // is enabled.
    js::coverage::LCovRealm lcovOutput;

    js::RegExpRealm regExps;

    js::DtoaCache dtoaCache;
    js::NewProxyCache newProxyCache;
    js::ArraySpeciesLookup arraySpeciesLookup;

    js::PerformanceGroupHolder performanceMonitoring;

    js::UniquePtr<js::ScriptCountsMap> scriptCountsMap;
    js::UniquePtr<js::ScriptNameMap> scriptNameMap;
    js::UniquePtr<js::DebugScriptMap> debugScriptMap;

    /*
     * Lazily initialized script source object to use for scripts cloned
     * from the self-hosting global.
     */
    js::ReadBarrieredScriptSourceObject selfHostingScriptSource { nullptr };

    // Last time at which an animation was played for this realm.
    int64_t lastAnimationTime = 0;

    /*
     * For generational GC, record whether a write barrier has added this
     * realm's global to the store buffer since the last minor GC.
     *
     * This is used to avoid calling into the VM every time a nursery object is
     * written to a property of the global.
     */
    uint32_t globalWriteBarriered = 0;

    uint32_t warnedAboutStringGenericsMethods = 0;
#ifdef DEBUG
    bool firedOnNewGlobalObject = false;
#endif

  private:
    void updateDebuggerObservesFlag(unsigned flag);

    Realm(const Realm&) = delete;
    void operator=(const Realm&) = delete;

  public:
    Realm(JSCompartment* comp, const JS::RealmOptions& options);
    ~Realm();

    MOZ_MUST_USE bool init(JSContext* cx);
    void destroy(js::FreeOp* fop);
    void clearTables();

    void addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                                size_t* tiAllocationSiteTables,
                                size_t* tiArrayTypeTables,
                                size_t* tiObjectTypeTables,
                                size_t* realmObject,
                                size_t* realmTables,
                                size_t* innerViews,
                                size_t* lazyArrayBuffers,
                                size_t* objectMetadataTables,
                                size_t* savedStacksSet,
                                size_t* varNamesSet,
                                size_t* nonSyntacticLexicalScopes,
                                size_t* jitRealm,
                                size_t* scriptCountsMapArg);

    JS::Zone* zone() {
        return zone_;
    }
    const JS::Zone* zone() const {
        return zone_;
    }

    JSRuntime* runtimeFromMainThread() const {
        MOZ_ASSERT(js::CurrentThreadCanAccessRuntime(runtime_));
        return runtime_;
    }

    // Note: Unrestricted access to the runtime from an arbitrary thread
    // can easily lead to races. Use this method very carefully.
    JSRuntime* runtimeFromAnyThread() const {
        return runtime_;
    }

    const JS::RealmCreationOptions& creationOptions() const { return creationOptions_; }
    JS::RealmBehaviors& behaviors() { return behaviors_; }
    const JS::RealmBehaviors& behaviors() const { return behaviors_; }

    /* Whether to preserve JIT code on non-shrinking GCs. */
    bool preserveJitCode() { return creationOptions_.preserveJitCode(); }

    bool isSelfHostingRealm() const {
        return isSelfHostingRealm_;
    }
    void setIsSelfHostingRealm() {
        isSelfHostingRealm_ = true;
    }

    /* The global object for this realm.
     *
     * Note: the global_ field is null briefly during GC, after the global
     * object is collected; but when that happens the Realm is destroyed during
     * the same GC.)
     *
     * In contrast, JSObject::global() is infallible because marking a JSObject
     * always marks its global as well.
     */
    inline js::GlobalObject* maybeGlobal() const;

    /* An unbarriered getter for use while tracing. */
    inline js::GlobalObject* unsafeUnbarrieredMaybeGlobal() const;

    /* True if a global object exists, but it's being collected. */
    inline bool globalIsAboutToBeFinalized();

    inline void initGlobal(js::GlobalObject& global);

    /*
     * This method traces data that is live iff we know that this realm's
     * global is still live.
     */
    void traceGlobal(JSTracer* trc);

    void sweepGlobalObject();
    void fixupGlobal();

    /*
     * This method traces Realm-owned GC roots that are considered live
     * regardless of whether the realm's global is still live.
     */
    void traceRoots(JSTracer* trc, js::gc::GCRuntime::TraceOrMarkRuntime traceOrMark);
    /*
     * This method clears out tables of roots in preparation for the final GC.
     */
    void finishRoots();

    void sweepAfterMinorGC();
    void sweepDebugEnvironments();
    void sweepObjectRealm();
    void sweepRegExps();
    void sweepSelfHostingScriptSource();
    void sweepTemplateObjects();

    void sweepObjectGroups() {
        objectGroups_.sweep();
    }

    void clearScriptCounts();
    void clearScriptNames();

    void purge();

    void fixupAfterMovingGC();
    void fixupScriptMapsAfterMovingGC();

#ifdef JSGC_HASH_TABLE_CHECKS
    void checkObjectGroupTablesAfterMovingGC() {
        objectGroups_.checkTablesAfterMovingGC();
    }
    void checkScriptMapsAfterMovingGC();
#endif

    // Add a name to [[VarNames]].  Reports OOM on failure.
    MOZ_MUST_USE bool addToVarNames(JSContext* cx, JS::Handle<JSAtom*> name);
    void sweepVarNames();

    void removeFromVarNames(JS::Handle<JSAtom*> name) {
        varNames_.remove(name);
    }

    // Whether the given name is in [[VarNames]].
    bool isInVarNames(JS::Handle<JSAtom*> name) {
        return varNames_.has(name);
    }

    void enter() {
        enterRealmDepth_++;
    }
    void leave() {
        enterRealmDepth_--;
    }
    bool hasBeenEntered() const {
        return enterRealmDepth_ > 0;
    }
    bool shouldTraceGlobal() const {
        return hasBeenEntered();
    }

    bool hasAllocationMetadataBuilder() const {
        return allocationMetadataBuilder_;
    }
    const js::AllocationMetadataBuilder* getAllocationMetadataBuilder() const {
        return allocationMetadataBuilder_;
    }
    const void* addressOfMetadataBuilder() const {
        return &allocationMetadataBuilder_;
    }
    void setAllocationMetadataBuilder(const js::AllocationMetadataBuilder* builder);
    void forgetAllocationMetadataBuilder();
    void setNewObjectMetadata(JSContext* cx, JS::HandleObject obj);

    bool hasObjectPendingMetadata() const {
        return objectMetadataState_.is<js::PendingMetadata>();
    }
    void setObjectPendingMetadata(JSContext* cx, JSObject* obj) {
        if (!cx->helperThread()) {
            MOZ_ASSERT(objectMetadataState_.is<js::DelayMetadata>());
            objectMetadataState_ = js::NewObjectMetadataState(js::PendingMetadata(obj));
        }
    }

    void* realmPrivate() const {
        return realmPrivate_;
    }
    void setRealmPrivate(void* p) {
        realmPrivate_ = p;
    }

    // This should only be called when it is non-null, i.e. during memory
    // reporting.
    JS::RealmStats& realmStats() {
        // We use MOZ_RELEASE_ASSERT here because in bug 1132502 there was some
        // (inconclusive) evidence that realmStats_ can be nullptr unexpectedly.
        MOZ_RELEASE_ASSERT(realmStats_);
        return *realmStats_;
    }
    void nullRealmStats() {
        MOZ_ASSERT(realmStats_);
        realmStats_ = nullptr;
    }
    void setRealmStats(JS::RealmStats* newStats) {
        MOZ_ASSERT(!realmStats_ && newStats);
        realmStats_ = newStats;
    }

    bool marked() const {
        return marked_;
    }
    void mark() {
        marked_ = true;
    }
    void unmark() {
        marked_ = false;
    }

    /*
     * The principals associated with this realm. Note that the same several
     * realms may share the same principals and that a realm may change
     * principals during its lifetime (e.g. in case of lazy parsing).
     */
    JSPrincipals* principals() {
        return principals_;
    }
    void setPrincipals(JSPrincipals* principals) {
        if (principals_ == principals)
            return;

        // If we change principals, we need to unlink immediately this
        // realm from its PerformanceGroup. For one thing, the performance data
        // we collect should not be improperly associated with a group to which
        // we do not belong anymore. For another thing, we use `principals()` as
        // part of the key to map realms to a `PerformanceGroup`, so if we do
        // not unlink now, this will be too late once we have updated
        // `principals_`.
        performanceMonitoring.unlink();
        principals_ = principals;
    }

    bool isSystem() const {
        return isSystem_;
    }
    void setIsSystem(bool isSystem) {
        if (isSystem_ == isSystem)
            return;

        // If we change `isSystem*(`, we need to unlink immediately this realm
        // from its PerformanceGroup. For one thing, the performance data we
        // collect should not be improperly associated to a group to which we
        // do not belong anymore. For another thing, we use `isSystem()` as part
        // of the key to map realms to a `PerformanceGroup`, so if we do not
        // unlink now, this will be too late once we have updated `isSystem_`.
        performanceMonitoring.unlink();
        isSystem_ = isSystem;
    }

    // Used to approximate non-content code when reporting telemetry.
    bool isProbablySystemCode() const {
        return isSystem_;
    }

    static const size_t IterResultObjectValueSlot = 0;
    static const size_t IterResultObjectDoneSlot = 1;
    js::NativeObject* getOrCreateIterResultTemplateObject(JSContext* cx);

    js::ArgumentsObject* getOrCreateArgumentsTemplateObject(JSContext* cx, bool mapped);
    js::ArgumentsObject* maybeArgumentsTemplateObject(bool mapped) const;

    //
    // The Debugger observes execution on a frame-by-frame basis. The
    // invariants of Realm's debug mode bits, JSScript::isDebuggee,
    // InterpreterFrame::isDebuggee, and BaselineFrame::isDebuggee are
    // enumerated below.
    //
    // 1. When a realm's isDebuggee() == true, relazification and lazy
    //    parsing are disabled.
    //
    //    Whether AOT wasm is disabled is togglable by the Debugger API. By
    //    default it is disabled. See debuggerObservesAsmJS below.
    //
    // 2. When a realm's debuggerObservesAllExecution() == true, all of
    //    the realm's scripts are considered debuggee scripts.
    //
    // 3. A script is considered a debuggee script either when, per above, its
    //    realm is observing all execution, or if it has breakpoints set.
    //
    // 4. A debuggee script always pushes a debuggee frame.
    //
    // 5. A debuggee frame calls all slow path Debugger hooks in the
    //    Interpreter and Baseline. A debuggee frame implies that its script's
    //    BaselineScript, if extant, has been compiled with debug hook calls.
    //
    // 6. A debuggee script or a debuggee frame (i.e., during OSR) ensures
    //    that the compiled BaselineScript is compiled with debug hook calls
    //    when attempting to enter Baseline.
    //
    // 7. A debuggee script or a debuggee frame (i.e., during OSR) does not
    //    attempt to enter Ion.
    //
    // Note that a debuggee frame may exist without its script being a
    // debuggee script. e.g., Debugger.Frame.prototype.eval only marks the
    // frame in which it is evaluating as a debuggee frame.
    //

    // True if this realm's global is a debuggee of some Debugger
    // object.
    bool isDebuggee() const { return !!(debugModeBits_ & IsDebuggee); }
    void setIsDebuggee() { debugModeBits_ |= IsDebuggee; }
    void unsetIsDebuggee();

    // True if this compartment's global is a debuggee of some Debugger
    // object with a live hook that observes all execution; e.g.,
    // onEnterFrame.
    bool debuggerObservesAllExecution() const {
        static const unsigned Mask = IsDebuggee | DebuggerObservesAllExecution;
        return (debugModeBits_ & Mask) == Mask;
    }
    void updateDebuggerObservesAllExecution() {
        updateDebuggerObservesFlag(DebuggerObservesAllExecution);
    }

    // True if this realm's global is a debuggee of some Debugger object
    // whose allowUnobservedAsmJS flag is false.
    //
    // Note that since AOT wasm functions cannot bail out, this flag really
    // means "observe wasm from this point forward". We cannot make
    // already-compiled wasm code observable to Debugger.
    bool debuggerObservesAsmJS() const {
        static const unsigned Mask = IsDebuggee | DebuggerObservesAsmJS;
        return (debugModeBits_ & Mask) == Mask;
    }
    void updateDebuggerObservesAsmJS() {
        updateDebuggerObservesFlag(DebuggerObservesAsmJS);
    }

    bool debuggerObservesBinarySource() const {
        static const unsigned Mask = IsDebuggee | DebuggerObservesBinarySource;
        return (debugModeBits_ & Mask) == Mask;
    }

    void updateDebuggerObservesBinarySource() {
        updateDebuggerObservesFlag(DebuggerObservesBinarySource);
    }

    // True if this realm's global is a debuggee of some Debugger object
    // whose collectCoverageInfo flag is true.
    bool debuggerObservesCoverage() const {
        static const unsigned Mask = DebuggerObservesCoverage;
        return (debugModeBits_ & Mask) == Mask;
    }
    void updateDebuggerObservesCoverage();

    // The code coverage can be enabled either for each realm, with the
    // Debugger API, or for the entire runtime.
    bool collectCoverage() const;
    bool collectCoverageForDebug() const;
    bool collectCoverageForPGO() const;

    bool needsDelazificationForDebugger() const {
        return debugModeBits_ & DebuggerNeedsDelazification;
    }

    // Schedule the realm to be delazified. Called from LazyScript::Create.
    void scheduleDelazificationForDebugger() {
        debugModeBits_ |= DebuggerNeedsDelazification;
    }

    // If we scheduled delazification for turning on debug mode, delazify all
    // scripts.
    bool ensureDelazifyScriptsForDebugger(JSContext* cx);

    void clearBreakpointsIn(js::FreeOp* fop, js::Debugger* dbg, JS::HandleObject handler);

    // Initializes randomNumberGenerator if needed.
    mozilla::non_crypto::XorShift128PlusRNG& getOrCreateRandomNumberGenerator();

    const void* addressOfRandomNumberGenerator() const {
        return randomNumberGenerator_.ptr();
    }

    mozilla::HashCodeScrambler randomHashCodeScrambler();

    bool isAccessValid() const {
        return validAccessPtr_ ? *validAccessPtr_ : true;
    }
    void setValidAccessPtr(bool* accessp) {
        validAccessPtr_ = accessp;
    }

    bool ensureJitRealmExists(JSContext* cx);
    void sweepJitRealm();

    js::jit::JitRealm* jitRealm() {
        return jitRealm_.get();
    }

    js::DebugEnvironments* debugEnvs() {
        return debugEnvs_.get();
    }
    js::UniquePtr<js::DebugEnvironments>& debugEnvsRef() {
        return debugEnvs_;
    }

    js::SavedStacks& savedStacks() {
        return savedStacks_;
    }

    // Recompute the probability with which this realm should record
    // profiling data (stack traces, allocations log, etc.) about each
    // allocation. We consult the probabilities requested by the Debugger
    // instances observing us, if any.
    void chooseAllocationSamplingProbability() {
        savedStacks_.chooseSamplingProbability(this);
    }

    void sweepSavedStacks();

    static constexpr size_t offsetOfCompartment() {
        return offsetof(JS::Realm, compartment_);
    }
    static constexpr size_t offsetOfRegExps() {
        return offsetof(JS::Realm, regExps);
    }
};

namespace js {

// We only set the maybeAlive flag for objects and scripts. It's assumed that,
// if a compartment is alive, then it will have at least some live object or
// script it in. Even if we get this wrong, the worst that will happen is that
// scheduledForDestruction will be set on the compartment, which will cause
// some extra GC activity to try to free the compartment.
template<typename T> inline void SetMaybeAliveFlag(T* thing) {}
template<> inline void SetMaybeAliveFlag(JSObject* thing) {thing->compartment()->maybeAlive = true;}
template<> inline void SetMaybeAliveFlag(JSScript* thing) {thing->compartment()->maybeAlive = true;}

} // namespace js

inline js::Handle<js::GlobalObject*>
JSContext::global() const
{
    /*
     * It's safe to use |unsafeGet()| here because any realm that is
     * on-stack will be marked automatically, so there's no need for a read
     * barrier on it. Once the realm is popped, the handle is no longer
     * safe to use.
     */
    MOZ_ASSERT(realm_, "Caller needs to enter a realm first");
    return js::Handle<js::GlobalObject*>::fromMarkedLocation(realm_->global_.unsafeGet());
}

namespace js {

class MOZ_RAII AssertRealmUnchanged
{
  public:
    explicit AssertRealmUnchanged(JSContext* cx
                                  MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx), oldRealm(cx->realm())
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~AssertRealmUnchanged() {
        MOZ_ASSERT(cx->realm() == oldRealm);
    }

  protected:
    JSContext* const cx;
    JS::Realm* const oldRealm;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoRealm
{
    JSContext* const cx_;
    JS::Realm* const origin_;

  public:
    template <typename T>
    inline AutoRealm(JSContext* cx, const T& target);
    inline ~AutoRealm();

    JSContext* context() const { return cx_; }
    JS::Realm* origin() const { return origin_; }

  protected:
    inline AutoRealm(JSContext* cx, JS::Realm* target);

  private:
    AutoRealm(const AutoRealm&) = delete;
    AutoRealm& operator=(const AutoRealm&) = delete;
};

class MOZ_RAII AutoAtomsZone
{
    JSContext* const cx_;
    JS::Realm* const origin_;
    const AutoLockForExclusiveAccess& lock_;

    AutoAtomsZone(const AutoAtomsZone&) = delete;
    AutoAtomsZone& operator=(const AutoAtomsZone&) = delete;

  public:
    inline AutoAtomsZone(JSContext* cx, AutoLockForExclusiveAccess& lock);
    inline ~AutoAtomsZone();
};

// Enter a realm directly. Only use this where there's no target GC thing
// to pass to AutoRealm or where you need to avoid the assertions in
// JS::Compartment::enterCompartmentOf().
class AutoRealmUnchecked : protected AutoRealm
{
  public:
    inline AutoRealmUnchecked(JSContext* cx, JS::Realm* target);
};

/*
 * Use this to change the behavior of an AutoRealm slightly on error. If
 * the exception happens to be an Error object, copy it to the origin compartment
 * instead of wrapping it.
 */
class ErrorCopier
{
    mozilla::Maybe<AutoRealm>& ar;

  public:
    explicit ErrorCopier(mozilla::Maybe<AutoRealm>& ar)
      : ar(ar) {}
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

class MOZ_RAII AutoSuppressAllocationMetadataBuilder {
    JS::Zone* zone;
    bool saved;

  public:
    explicit AutoSuppressAllocationMetadataBuilder(JSContext* cx)
      : AutoSuppressAllocationMetadataBuilder(cx->realm()->zone())
    { }

    explicit AutoSuppressAllocationMetadataBuilder(JS::Zone* zone)
      : zone(zone),
        saved(zone->suppressAllocationMetadataBuilder)
    {
        zone->suppressAllocationMetadataBuilder = true;
    }

    ~AutoSuppressAllocationMetadataBuilder() {
        zone->suppressAllocationMetadataBuilder = saved;
    }
};

} /* namespace js */

namespace JS {
template <>
struct GCPolicy<js::CrossCompartmentKey> : public StructGCPolicy<js::CrossCompartmentKey> {
    static bool isTenured(const js::CrossCompartmentKey& key) { return key.isTenured(); }
};
} // namespace JS

#endif /* vm_JSCompartment_h */
