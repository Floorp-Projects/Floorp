/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
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
#include "js/UniquePtr.h"
#include "vm/JSObject.h"
#include "vm/JSScript.h"

namespace js {

// A key in a WrapperMap, a compartment's map from entities in other
// compartments to the local values the compartment's own code must use to refer
// to them.
//
// WrapperMaps have a complex key type because, in addition to mapping JSObjects
// to their cross-compartment wrappers, they must also map non-atomized
// JSStrings to their copies in the local compartment, and debuggee entities
// (objects, scripts, etc.) to their representative objects in the Debugger API.
class CrossCompartmentKey {
 public:
  // [SMDOC]: Cross-compartment wrapper map entries for Debugger API objects
  //
  // The Debugger API creates objects like Debugger.Object, Debugger.Script,
  // Debugger.Environment, etc. to refer to things in the debuggee. Each
  // Debugger gets at most one Debugger.Mumble for each referent:
  // Debugger.Mumbles are unique per referent per Debugger.
  //
  // Since a Debugger and its debuggee must be in different compartments, a
  // Debugger.Mumble's pointer to its referent is a cross-compartment edge, from
  // the debugger's compartment into the debuggee compartment. Like any other
  // sort of cross-compartment edge, the GC needs to be able to find all of
  // these edges readily.
  //
  // Our solution is to treat Debugger.Mumble objects as wrappers stored in
  // JSCompartment::crossCompartmentWrappers, where the GC already looks when it
  // needs to find any other sort of cross-compartment edges. This also meshes
  // nicely with existing sanity checks that trace the heap looking for
  // cross-compartment edges and check that each one has an entry in the right
  // wrapper map.
  //
  // That approach means that a given referent may have multiple entries in the
  // wrapper map: its ordinary cross-compartment wrapper, and then any
  // Debugger.Mumbles referring to it. If there are multiple Debuggers in a
  // compartment, each needs its own Debugger.Mumble for the referent, and each
  // of those needs its own entry in the WrapperMap. And some referents may have
  // more than one type of Debugger.Mumble that can refer to them: for example,
  // a WasmInstanceObject can be the referent of both a Debugger.Script and a
  // Debugger.Source.
  //
  // Hence, to look up a Debugger.Mumble in the WrapperMap, we need a key that
  // includes 1) the referent, 2) the Debugger to which the Mumble belongs, and
  // 3) the specific type of Mumble we're looking for. Since mozilla::Variant
  // distinguishes alternatives by type only, we include a distinct type in
  // WrappedType for each sort of Debugger.Mumble.

  // Common structure for all Debugger.Mumble keys.
  template <typename Referent>
  struct Debuggee {
    Debuggee(NativeObject* debugger, Referent* referent)
        : debugger(debugger), referent(referent) {}

    bool operator==(const Debuggee& other) const {
      return debugger == other.debugger && referent == other.referent;
    }

    bool operator!=(const Debuggee& other) const { return !(*this == other); }

    NativeObject* debugger;
    Referent* referent;
  };

  // Key under which we find debugger's Debugger.Object referring to referent.
  struct DebuggeeObject : Debuggee<JSObject> {
    DebuggeeObject(NativeObject* debugger, JSObject* referent)
        : Debuggee(debugger, referent) {}
  };

  // Keys under which we find Debugger.Scripts.
  using DebuggeeJSScript = Debuggee<JSScript>;
  using DebuggeeWasmScript = Debuggee<NativeObject>;  // WasmInstanceObject
  using DebuggeeLazyScript = Debuggee<LazyScript>;

  // Key under which we find debugger's Debugger.Environment referring to
  // referent.
  struct DebuggeeEnvironment : Debuggee<JSObject> {
    DebuggeeEnvironment(NativeObject* debugger, JSObject* referent)
        : Debuggee(debugger, referent) {}
  };

  // Key under which we find debugger's Debugger.Source referring to referent.
  struct DebuggeeSource : Debuggee<NativeObject> {
    DebuggeeSource(NativeObject* debugger, NativeObject* referent)
        : Debuggee(debugger, referent) {}
  };

  using WrappedType =
      mozilla::Variant<JSObject*, JSString*, DebuggeeObject, DebuggeeJSScript,
                       DebuggeeWasmScript, DebuggeeLazyScript,
                       DebuggeeEnvironment, DebuggeeSource>;

  explicit CrossCompartmentKey(JSObject* obj) : wrapped(obj) {
    MOZ_RELEASE_ASSERT(obj);
  }
  explicit CrossCompartmentKey(JSString* str) : wrapped(str) {
    MOZ_RELEASE_ASSERT(str);
  }
  explicit CrossCompartmentKey(const JS::Value& v)
      : wrapped(v.isString() ? WrappedType(v.toString())
                             : WrappedType(&v.toObject())) {}

  // For most debuggee keys, we must let the caller choose the key type
  // themselves. But for JSScript and LazyScript, there is only one key type
  // that makes sense, so we provide an overloaded constructor.
  explicit CrossCompartmentKey(DebuggeeObject&& key) : wrapped(key) {}
  explicit CrossCompartmentKey(DebuggeeSource&& key) : wrapped(key) {}
  explicit CrossCompartmentKey(DebuggeeEnvironment&& key) : wrapped(key) {}
  explicit CrossCompartmentKey(DebuggeeWasmScript&& key) : wrapped(key) {}
  explicit CrossCompartmentKey(NativeObject* debugger, JSScript* referent)
      : wrapped(DebuggeeJSScript(debugger, referent)) {}
  explicit CrossCompartmentKey(NativeObject* debugger, LazyScript* referent)
      : wrapped(DebuggeeLazyScript(debugger, referent)) {}

  bool operator==(const CrossCompartmentKey& other) const {
    return wrapped == other.wrapped;
  }
  bool operator!=(const CrossCompartmentKey& other) const {
    return wrapped != other.wrapped;
  }

  template <typename T>
  bool is() const {
    return wrapped.is<T>();
  }
  template <typename T>
  const T& as() const {
    return wrapped.as<T>();
  }

 private:
  template <typename F>
  struct ApplyToWrappedMatcher {
    F f_;
    explicit ApplyToWrappedMatcher(F f) : f_(f) {}
    auto operator()(JSObject*& obj) { return f_(&obj); }
    auto operator()(JSString*& str) { return f_(&str); }
    template <typename Referent>
    auto operator()(Debuggee<Referent>& dbg) {
      return f_(&dbg.referent);
    }
  };

  template <typename F>
  struct ApplyToDebuggerMatcher {
    F f_;
    explicit ApplyToDebuggerMatcher(F f) : f_(f) {}

    using ReturnType = decltype(f_(static_cast<NativeObject**>(nullptr)));
    ReturnType operator()(JSObject*& obj) { return ReturnType(); }
    ReturnType operator()(JSString*& str) { return ReturnType(); }
    template <typename Referent>
    ReturnType operator()(Debuggee<Referent>& dbg) {
      return f_(&dbg.debugger);
    }
  };

 public:
  template <typename F>
  auto applyToWrapped(F f) {
    return wrapped.match(ApplyToWrappedMatcher<F>(f));
  }

  template <typename F>
  auto applyToDebugger(F f) {
    return wrapped.match(ApplyToDebuggerMatcher<F>(f));
  }

  struct IsDebuggerKeyMatcher {
    bool operator()(JSObject* const& obj) { return false; }
    bool operator()(JSString* const& str) { return false; }
    template <typename Referent>
    bool operator()(Debuggee<Referent> const& dbg) {
      return true;
    }
  };

  bool isDebuggerKey() const { return wrapped.match(IsDebuggerKeyMatcher()); }

  JS::Compartment* compartment() {
    return applyToWrapped([](auto tp) { return (*tp)->maybeCompartment(); });
  }

  JS::Zone* zone() {
    return applyToWrapped([](auto tp) { return (*tp)->zone(); });
  }

  struct Hasher : public DefaultHasher<CrossCompartmentKey> {
    struct HashFunctor {
      HashNumber operator()(JSObject* obj) {
        return DefaultHasher<JSObject*>::hash(obj);
      }
      HashNumber operator()(JSString* str) {
        return DefaultHasher<JSString*>::hash(str);
      }
      template <typename Referent>
      HashNumber operator()(const Debuggee<Referent>& dbg) {
        return mozilla::HashGeneric(dbg.debugger, dbg.referent);
      }
    };
    static HashNumber hash(const CrossCompartmentKey& key) {
      return key.wrapped.addTagToHash(key.wrapped.match(HashFunctor()));
    }

    static bool match(const CrossCompartmentKey& l,
                      const CrossCompartmentKey& k) {
      return l.wrapped == k.wrapped;
    }
  };

  bool isTenured() const {
    auto self = const_cast<CrossCompartmentKey*>(this);
    return self->applyToWrapped([](auto tp) { return (*tp)->isTenured(); });
  }

  void trace(JSTracer* trc);
  bool needsSweep();

 private:
  CrossCompartmentKey() = delete;
  explicit CrossCompartmentKey(WrappedType&& wrapped)
      : wrapped(std::move(wrapped)) {}
  WrappedType wrapped;
};

// The data structure for storing CCWs, which has a map per target compartment
// so we can access them easily. Note string CCWs are stored separately from the
// others because they have target compartment nullptr.
class WrapperMap {
  static const size_t InitialInnerMapSize = 4;

  using InnerMap =
      NurseryAwareHashMap<CrossCompartmentKey, JS::Value,
                          CrossCompartmentKey::Hasher, SystemAllocPolicy>;
  using OuterMap =
      GCHashMap<JS::Compartment*, InnerMap, DefaultHasher<JS::Compartment*>,
                SystemAllocPolicy>;

  OuterMap map;

 public:
  class Enum {
   public:
    enum SkipStrings : bool { WithStrings = false, WithoutStrings = true };

   private:
    Enum(const Enum&) = delete;
    void operator=(const Enum&) = delete;

    void goToNext() {
      if (outer.isNothing()) {
        return;
      }
      for (; !outer->empty(); outer->popFront()) {
        JS::Compartment* c = outer->front().key();
        // Need to skip string at first, because the filter may not be
        // happy with a nullptr.
        if (!c && skipStrings) {
          continue;
        }
        if (filter && !filter->match(c)) {
          continue;
        }
        InnerMap& m = outer->front().value();
        if (!m.empty()) {
          if (inner.isSome()) {
            inner.reset();
          }
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
    explicit Enum(WrapperMap& m, SkipStrings s = WithStrings)
        : filter(nullptr), skipStrings(s) {
      outer.emplace(m.map);
      goToNext();
    }

    Enum(WrapperMap& m, const CompartmentFilter& f, SkipStrings s = WithStrings)
        : filter(&f), skipStrings(s) {
      outer.emplace(m.map);
      goToNext();
    }

    Enum(WrapperMap& m, JS::Compartment* target) {
      // Leave the outer map as nothing and only iterate the inner map we
      // find here.
      auto p = m.map.lookup(target);
      if (p) {
        inner.emplace(p->value());
      }
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
        if (!inner->empty()) {
          return;
        }
      }
      goToNext();
    }

    void removeFront() {
      MOZ_ASSERT(inner.isSome());
      inner->removeFront();
    }
  };

  class Ptr : public InnerMap::Ptr {
    friend class WrapperMap;

    InnerMap* map;

    Ptr() : InnerMap::Ptr(), map(nullptr) {}
    Ptr(const InnerMap::Ptr& p, InnerMap& m) : InnerMap::Ptr(p), map(&m) {}
  };

  WrapperMap() {}
  explicit WrapperMap(size_t aLen) : map(aLen) {}

  bool empty() {
    if (map.empty()) {
      return true;
    }
    for (OuterMap::Enum e(map); !e.empty(); e.popFront()) {
      if (!e.front().value().empty()) {
        return false;
      }
    }
    return true;
  }

  Ptr lookup(const CrossCompartmentKey& k) const {
    auto op = map.lookup(const_cast<CrossCompartmentKey&>(k).compartment());
    if (op) {
      auto ip = op->value().lookup(k);
      if (ip) {
        return Ptr(ip, op->value());
      }
    }
    return Ptr();
  }

  void remove(Ptr p) {
    if (p) {
      p.map->remove(p);
    }
  }

  MOZ_MUST_USE bool put(const CrossCompartmentKey& k, const JS::Value& v) {
    JS::Compartment* c = const_cast<CrossCompartmentKey&>(k).compartment();
    MOZ_ASSERT(k.is<JSString*>() == !c);
    auto p = map.lookupForAdd(c);
    if (!p) {
      InnerMap m(InitialInnerMapSize);
      if (!map.add(p, c, std::move(m))) {
        return false;
      }
    }
    return p->value().put(k, v);
  }

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) {
    size_t size = map.shallowSizeOfExcludingThis(mallocSizeOf);
    for (OuterMap::Enum e(map); !e.empty(); e.popFront()) {
      size += e.front().value().sizeOfExcludingThis(mallocSizeOf);
    }
    return size;
  }
  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) {
    size_t size = map.shallowSizeOfIncludingThis(mallocSizeOf);
    for (OuterMap::Enum e(map); !e.empty(); e.popFront()) {
      size += e.front().value().sizeOfIncludingThis(mallocSizeOf);
    }
    return size;
  }

  bool hasNurseryAllocatedWrapperEntries(const CompartmentFilter& f) {
    for (OuterMap::Enum e(map); !e.empty(); e.popFront()) {
      JS::Compartment* c = e.front().key();
      if (c && !f.match(c)) {
        continue;
      }
      InnerMap& m = e.front().value();
      if (m.hasNurseryEntries()) {
        return true;
      }
    }
    return false;
  }

  void sweepAfterMinorGC(JSTracer* trc) {
    for (OuterMap::Enum e(map); !e.empty(); e.popFront()) {
      InnerMap& m = e.front().value();
      m.sweepAfterMinorGC(trc);
      if (m.empty()) {
        e.removeFront();
      }
    }
  }

  void sweep() {
    for (OuterMap::Enum e(map); !e.empty(); e.popFront()) {
      InnerMap& m = e.front().value();
      m.sweep();
      if (m.empty()) {
        e.removeFront();
      }
    }
  }
};

}  // namespace js

class JS::Compartment {
  JS::Zone* zone_;
  JSRuntime* runtime_;
  bool invisibleToDebugger_;

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

  // True if all outgoing wrappers have been nuked. This happens when all realms
  // have been nuked and NukeCrossCompartmentWrappers is called with the
  // NukeAllReferences option. This prevents us from creating new wrappers for
  // the compartment.
  bool nukedOutgoingWrappers = false;

  JS::Zone* zone() { return zone_; }
  const JS::Zone* zone() const { return zone_; }

  JSRuntime* runtimeFromMainThread() const {
    MOZ_ASSERT(js::CurrentThreadCanAccessRuntime(runtime_));
    return runtime_;
  }

  // Note: Unrestricted access to the zone's runtime from an arbitrary
  // thread can easily lead to races. Use this method very carefully.
  JSRuntime* runtimeFromAnyThread() const { return runtime_; }

  // Certain compartments are implementation details of the embedding, and
  // references to them should never leak out to script. For realms belonging to
  // this compartment, onNewGlobalObject does not fire, and addDebuggee is a
  // no-op.
  bool invisibleToDebugger() const { return invisibleToDebugger_; }

  RealmVector& realms() { return realms_; }

  // Cross-compartment wrappers are shared by all realms in the compartment, but
  // they still have a per-realm ObjectGroup etc. To prevent us from having
  // multiple realms, each with some cross-compartment wrappers potentially
  // keeping the realm alive longer than necessary, we always allocate CCWs in
  // the first realm.
  js::GlobalObject& firstGlobal() const;
  js::GlobalObject& globalForNewCCW() const { return firstGlobal(); }

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
  bool getNonWrapperObjectForCurrentCompartment(JSContext* cx,
                                                js::MutableHandleObject obj);
  bool getOrCreateWrapper(JSContext* cx, js::HandleObject existing,
                          js::MutableHandleObject obj);

 public:
  explicit Compartment(JS::Zone* zone, bool invisibleToDebugger);

  void destroy(js::FreeOp* fop);

  MOZ_MUST_USE inline bool wrap(JSContext* cx, JS::MutableHandleValue vp);

  MOZ_MUST_USE bool wrap(JSContext* cx, js::MutableHandleString strp);
  MOZ_MUST_USE bool wrap(JSContext* cx, js::MutableHandle<JS::BigInt*> bi);
  MOZ_MUST_USE bool wrap(JSContext* cx, JS::MutableHandleObject obj);
  MOZ_MUST_USE bool wrap(JSContext* cx,
                         JS::MutableHandle<JS::PropertyDescriptor> desc);
  MOZ_MUST_USE bool wrap(JSContext* cx,
                         JS::MutableHandle<JS::GCVector<JS::Value>> vec);
  MOZ_MUST_USE bool rewrap(JSContext* cx, JS::MutableHandleObject obj,
                           JS::HandleObject existing);

  MOZ_MUST_USE bool putWrapper(JSContext* cx,
                               const js::CrossCompartmentKey& wrapped,
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
        : js::WrapperMap::Enum(c->crossCompartmentWrappers) {}
  };

  struct NonStringWrapperEnum : public js::WrapperMap::Enum {
    explicit NonStringWrapperEnum(JS::Compartment* c)
        : js::WrapperMap::Enum(c->crossCompartmentWrappers, WithoutStrings) {}
    explicit NonStringWrapperEnum(JS::Compartment* c,
                                  const js::CompartmentFilter& f)
        : js::WrapperMap::Enum(c->crossCompartmentWrappers, f, WithoutStrings) {
    }
    explicit NonStringWrapperEnum(JS::Compartment* c, JS::Compartment* target)
        : js::WrapperMap::Enum(c->crossCompartmentWrappers, target) {
      MOZ_ASSERT(target);
    }
  };

  struct StringWrapperEnum : public js::WrapperMap::Enum {
    explicit StringWrapperEnum(JS::Compartment* c)
        : js::WrapperMap::Enum(c->crossCompartmentWrappers, nullptr) {}
  };

  /*
   * These methods mark pointers that cross compartment boundaries. They are
   * called in per-zone GCs to prevent the wrappers' outgoing edges from
   * dangling (full GCs naturally follow pointers across compartments) and
   * when compacting to update cross-compartment pointers.
   */
  void traceOutgoingCrossCompartmentWrappers(JSTracer* trc);
  static void traceIncomingCrossCompartmentEdgesForZoneGC(JSTracer* trc);

  void sweepRealms(js::FreeOp* fop, bool keepAtleastOne,
                   bool destroyingRuntime);
  void sweepAfterMinorGC(JSTracer* trc);
  void sweepCrossCompartmentWrappers();

  static void fixupCrossCompartmentWrappersAfterMovingGC(JSTracer* trc);
  void fixupAfterMovingGC();

  MOZ_MUST_USE bool findSweepGroupEdges();
};

namespace js {

// We only set the maybeAlive flag for objects and scripts. It's assumed that,
// if a compartment is alive, then it will have at least some live object or
// script it in. Even if we get this wrong, the worst that will happen is that
// scheduledForDestruction will be set on the compartment, which will cause
// some extra GC activity to try to free the compartment.
template <typename T>
inline void SetMaybeAliveFlag(T* thing) {}

template <>
inline void SetMaybeAliveFlag(JSObject* thing) {
  thing->compartment()->gcState.maybeAlive = true;
}

template <>
inline void SetMaybeAliveFlag(JSScript* thing) {
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
struct WrapperValue {
  /*
   * We use unsafeGet() in the constructors to avoid invoking a read barrier
   * on the wrapper, which may be dead (see the comment about bug 803376 in
   * gc/GC.cpp regarding this). If there is an incremental GC while the
   * wrapper is in use, the AutoWrapper rooter will ensure the wrapper gets
   * marked.
   */
  explicit WrapperValue(const WrapperMap::Ptr& ptr)
      : value(*ptr->value().unsafeGet()) {}

  explicit WrapperValue(const WrapperMap::Enum& e)
      : value(*e.front().value().unsafeGet()) {}

  Value& get() { return value; }
  Value get() const { return value; }
  operator const Value&() const { return value; }
  JSObject& toObject() const { return value.toObject(); }

 private:
  Value value;
};

class MOZ_RAII AutoWrapperVector : public JS::GCVector<WrapperValue, 8>,
                                   private JS::AutoGCRooter {
 public:
  explicit AutoWrapperVector(JSContext* cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : JS::GCVector<WrapperValue, 8>(cx),
        JS::AutoGCRooter(cx, JS::AutoGCRooter::Tag::WrapperVector) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }

  friend void AutoGCRooter::trace(JSTracer* trc);

  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class MOZ_RAII AutoWrapperRooter : private JS::AutoGCRooter {
 public:
  AutoWrapperRooter(JSContext* cx,
                    const WrapperValue& v MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : JS::AutoGCRooter(cx, JS::AutoGCRooter::Tag::Wrapper), value(v) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }

  operator JSObject*() const { return value.get().toObjectOrNull(); }

  friend void JS::AutoGCRooter::trace(JSTracer* trc);

 private:
  WrapperValue value;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} /* namespace js */

namespace JS {
template <>
struct GCPolicy<js::CrossCompartmentKey>
    : public StructGCPolicy<js::CrossCompartmentKey> {
  static bool isTenured(const js::CrossCompartmentKey& key) {
    return key.isTenured();
  }
};
}  // namespace JS

#endif /* vm_Compartment_h */
