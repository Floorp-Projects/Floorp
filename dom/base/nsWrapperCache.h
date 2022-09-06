/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWrapperCache_h___
#define nsWrapperCache_h___

#include "nsCycleCollectionParticipant.h"
#include "mozilla/Assertions.h"
#include "mozilla/ServoUtils.h"
#include "mozilla/RustCell.h"
#include "js/HeapAPI.h"
#include "js/TracingAPI.h"
#include "js/TypeDecls.h"
#include "nsISupports.h"
#include "nsISupportsUtils.h"
#include <type_traits>

namespace mozilla::dom {
class ContentProcessMessageManager;
class InProcessBrowserChildMessageManager;
class BrowserChildMessageManager;
}  // namespace mozilla::dom
class SandboxPrivate;
class nsWindowRoot;

#define NS_WRAPPERCACHE_IID                          \
  {                                                  \
    0x6f3179a1, 0x36f7, 0x4a5c, {                    \
      0x8c, 0xf1, 0xad, 0xc8, 0x7c, 0xde, 0x3e, 0x87 \
    }                                                \
  }

// There are two sets of flags used by DOM nodes. One comes from reusing the
// remaining bits of the inherited nsWrapperCache flags (mFlags), and another is
// exclusive to nsINode (mBoolFlags).
//
// Both sets of flags are 32 bits. On 64-bit platforms, this can cause two
// wasted 32-bit fields due to alignment requirements. Some compilers are
// smart enough to coalesce the fields if we make mBoolFlags the first member
// of nsINode, but others (such as MSVC) are not.
//
// So we just store mBoolFlags directly on nsWrapperCache on 64-bit platforms.
// This may waste space for some other nsWrapperCache-derived objects that have
// a 32-bit field as their first member, but those objects are unlikely to be as
// numerous or performance-critical as DOM nodes.
#ifdef HAVE_64BIT_BUILD
static_assert(sizeof(void*) == 8, "These architectures should be 64-bit");
#  define BOOL_FLAGS_ON_WRAPPER_CACHE
#else
static_assert(sizeof(void*) == 4, "Only support 32-bit and 64-bit");
#endif

/**
 * Class to store the wrapper for an object. This can only be used with objects
 * that only have one non-security wrapper at a time (for an XPCWrappedNative
 * this is usually ensured by setting an explicit parent in the PreCreate hook
 * for the class).
 *
 * An instance of nsWrapperCache can be gotten from an object that implements
 * a wrapper cache by calling QueryInterface on it. Note that this breaks XPCOM
 * rules a bit (this object doesn't derive from nsISupports).
 *
 * The cache can store objects other than wrappers. We allow wrappers to use a
 * separate JSObject to store their state (mostly expandos). If the wrapper is
 * collected and we want to preserve this state we actually store the state
 * object in the cache.
 *
 * The cache can store 3 types of objects: a DOM binding object (regular JS
 * object or proxy), an nsOuterWindowProxy or an XPCWrappedNative wrapper.
 *
 * The finalizer for the wrapper clears the cache.
 *
 * A compacting GC can move the wrapper object. Pointers to moved objects are
 * usually found and updated by tracing the heap, however non-preserved wrappers
 * are weak references and are not traced, so another approach is
 * necessary. Instead a class hook (objectMovedOp) is provided that is called
 * when an object is moved and is responsible for ensuring pointers are
 * updated. It does this by calling UpdateWrapper() on the wrapper
 * cache. SetWrapper() asserts that the hook is implemented for any wrapper set.
 *
 * A number of the methods are implemented in nsWrapperCacheInlines.h because we
 * have to include some JS headers that don't play nicely with the rest of the
 * codebase. Include nsWrapperCacheInlines.h if you need to call those methods.
 */

class JS_HAZ_ROOTED nsWrapperCache {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_WRAPPERCACHE_IID)

  nsWrapperCache() = default;
  ~nsWrapperCache() {
    // Preserved wrappers should never end up getting cleared, but this can
    // happen during shutdown when a leaked wrapper object is finalized, causing
    // its wrapper to be cleared.
    MOZ_ASSERT(!PreservingWrapper() || js::RuntimeIsBeingDestroyed(),
               "Destroying cache with a preserved wrapper!");
  }

  /**
   * Get the cached wrapper.
   *
   * This getter clears the gray bit before handing out the JSObject which means
   * that the object is guaranteed to be kept alive past the next CC.
   */
  JSObject* GetWrapper() const;

  /**
   * Get the cached wrapper.
   *
   * This getter does not change the color of the JSObject meaning that the
   * object returned is not guaranteed to be kept alive past the next CC.
   *
   * This should only be called if you are certain that the return value won't
   * be passed into a JSAPI function and that it won't be stored without being
   * rooted (or otherwise signaling the stored value to the CC).
   */
  JSObject* GetWrapperPreserveColor() const;

  /**
   * Get the cached wrapper.
   *
   * This getter does not check whether the wrapper is dead and in the process
   * of being finalized.
   *
   * This should only be called if you really need to see the raw contents of
   * this cache, for example as part of finalization. Don't store the result
   * anywhere or pass it into JSAPI functions that may cause the value to
   * escape.
   */
  JSObject* GetWrapperMaybeDead() const { return mWrapper; }

#ifdef DEBUG
 private:
  static bool HasJSObjectMovedOp(JSObject* aWrapper);

  static void AssertUpdatedWrapperZone(const JSObject* aNewObject,
                                       const JSObject* aOldObject);

 public:
#endif

  void SetWrapper(JSObject* aWrapper) {
    MOZ_ASSERT(!PreservingWrapper(), "Clearing a preserved wrapper!");
    MOZ_ASSERT(aWrapper, "Use ClearWrapper!");
    MOZ_ASSERT(HasJSObjectMovedOp(aWrapper),
               "Object has not provided the hook to update the wrapper if it "
               "is moved");

    SetWrapperJSObject(aWrapper);
  }

  /**
   * Clear the cache.
   */
  void ClearWrapper() {
    // Preserved wrappers should never end up getting cleared, but this can
    // happen during shutdown when a leaked wrapper object is finalized, causing
    // its wrapper to be cleared.
    MOZ_ASSERT(!PreservingWrapper() || js::RuntimeIsBeingDestroyed(),
               "Clearing a preserved wrapper!");
    SetWrapperJSObject(nullptr);
  }

  /**
   * Clear the cache if it still contains a specific wrapper object. This should
   * be called from the finalizer for the wrapper.
   */
  void ClearWrapper(JSObject* obj) {
    if (obj == mWrapper) {
      ClearWrapper();
    }
  }

  /**
   * Update the wrapper when the object moves between globals.
   */
  template <typename T>
  void UpdateWrapperForNewGlobal(T* aScriptObjectHolder, JSObject* aNewWrapper);

  /**
   * Update the wrapper if the object it contains is moved.
   *
   * This method must be called from the objectMovedOp class extension hook for
   * any wrapper cached object.
   */
  void UpdateWrapper(JSObject* aNewObject, const JSObject* aOldObject) {
#ifdef DEBUG
    AssertUpdatedWrapperZone(aNewObject, aOldObject);
#endif
    if (mWrapper) {
      MOZ_ASSERT(mWrapper == aOldObject);
      mWrapper = aNewObject;
    }
  }

  bool PreservingWrapper() const {
    return HasWrapperFlag(WRAPPER_BIT_PRESERVED);
  }

  /**
   * Wrap the object corresponding to this wrapper cache. If non-null is
   * returned, the object has already been stored in the wrapper cache.
   */
  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> aGivenProto) = 0;

  /**
   * Returns true if the object has a wrapper that is known live from the point
   * of view of cycle collection.
   */
  bool HasKnownLiveWrapper() const;

  /**
   * Returns true if the object has a known-live wrapper (from the CC point of
   * view) and all the GC things it is keeping alive are already known-live from
   * CC's point of view.
   */
  bool HasKnownLiveWrapperAndDoesNotNeedTracing(nsISupports* aThis);

  bool HasNothingToTrace(nsISupports* aThis);

  /**
   * Mark our wrapper, if any, as live as far as the CC is concerned.
   */
  void MarkWrapperLive();

  // Only meant to be called by code that preserves a wrapper.
  void SetPreservingWrapper(bool aPreserve) {
    if (aPreserve) {
      SetWrapperFlags(WRAPPER_BIT_PRESERVED);
    } else {
      UnsetWrapperFlags(WRAPPER_BIT_PRESERVED);
    }
  }

  void TraceWrapper(const TraceCallbacks& aCallbacks, void* aClosure) {
    if (PreservingWrapper() && mWrapper) {
      aCallbacks.Trace(this, "Preserved wrapper", aClosure);
    }
  }

  /*
   * The following methods for getting and manipulating flags allow the unused
   * bits of mFlags to be used by derived classes.
   */

  using FlagsType = uint32_t;

  FlagsType GetFlags() const {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!mozilla::IsInServoTraversal());
    return mFlags.Get() & ~kWrapperFlagsMask;
  }

  // This can be called from stylo threads too, so it needs to be atomic, as
  // this value may be mutated from multiple threads during servo traversal from
  // rust.
  bool HasFlag(FlagsType aFlag) const {
    MOZ_ASSERT((aFlag & kWrapperFlagsMask) == 0, "Bad flag mask");
    return __atomic_load_n(mFlags.AsPtr(), __ATOMIC_RELAXED) & aFlag;
  }

  // Identical to HasFlag, but more explicit about its handling of multiple
  // flags. This can be called from stylo threads too.
  bool HasAnyOfFlags(FlagsType aFlags) const { return HasFlag(aFlags); }

  // This can also be called from stylo, in the sequential part of the
  // traversal, though it's probably not worth differentiating them for the
  // purposes of assertions.
  bool HasAllFlags(FlagsType aFlags) const {
    MOZ_ASSERT((aFlags & kWrapperFlagsMask) == 0, "Bad flag mask");
    return (__atomic_load_n(mFlags.AsPtr(), __ATOMIC_RELAXED) & aFlags) ==
           aFlags;
  }

  void SetFlags(FlagsType aFlagsToSet) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT((aFlagsToSet & kWrapperFlagsMask) == 0, "Bad flag mask");
    mFlags.Set(mFlags.Get() | aFlagsToSet);
  }

  void UnsetFlags(FlagsType aFlagsToUnset) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT((aFlagsToUnset & kWrapperFlagsMask) == 0, "Bad flag mask");
    mFlags.Set(mFlags.Get() & ~aFlagsToUnset);
  }

  void PreserveWrapper(nsISupports* aScriptObjectHolder) {
    if (PreservingWrapper()) {
      return;
    }

    nsISupports* ccISupports;
    aScriptObjectHolder->QueryInterface(NS_GET_IID(nsCycleCollectionISupports),
                                        reinterpret_cast<void**>(&ccISupports));
    MOZ_ASSERT(ccISupports);

    nsXPCOMCycleCollectionParticipant* participant;
    CallQueryInterface(ccISupports, &participant);
    PreserveWrapper(ccISupports, participant);
  }

  void PreserveWrapper(void* aScriptObjectHolder,
                       nsScriptObjectTracer* aTracer) {
    if (PreservingWrapper()) {
      return;
    }

    JSObject* wrapper = GetWrapper();  // Read barrier for incremental GC.
    HoldJSObjects(aScriptObjectHolder, aTracer, JS::GetObjectZone(wrapper));
    SetPreservingWrapper(true);
#ifdef DEBUG
    // Make sure the cycle collector will be able to traverse to the wrapper.
    CheckCCWrapperTraversal(aScriptObjectHolder, aTracer);
#endif
  }

  void ReleaseWrapper(void* aScriptObjectHolder);

  void TraceWrapper(JSTracer* aTrc, const char* name) {
    if (mWrapper) {
      js::UnsafeTraceManuallyBarrieredEdge(aTrc, &mWrapper, name);
    }
  }

 protected:
  void PoisonWrapper() {
    if (mWrapper) {
      // Set the pointer to a value that will cause a crash if it is
      // dereferenced.
      mWrapper = reinterpret_cast<JSObject*>(1);
    }
  }

 private:
  void SetWrapperJSObject(JSObject* aWrapper);

  // We'd like to assert that these aren't used from servo threads, but we don't
  // have a great way to do that because:
  //  * We can't just assert that they get used on the main thread, because
  //    these are used from workers.
  //  * We can't just assert that they aren't used when IsInServoTraversal(),
  //    because the traversal has a sequential, main-thread-only phase, where we
  //    run animations that can fiddle with JS promises.
  FlagsType GetWrapperFlags() const { return mFlags.Get() & kWrapperFlagsMask; }

  bool HasWrapperFlag(FlagsType aFlag) const {
    MOZ_ASSERT((aFlag & ~kWrapperFlagsMask) == 0, "Bad wrapper flag bits");
    return !!(mFlags.Get() & aFlag);
  }

  void SetWrapperFlags(FlagsType aFlagsToSet) {
    MOZ_ASSERT((aFlagsToSet & ~kWrapperFlagsMask) == 0,
               "Bad wrapper flag bits");
    mFlags.Set(mFlags.Get() | aFlagsToSet);
  }

  void UnsetWrapperFlags(FlagsType aFlagsToUnset) {
    MOZ_ASSERT((aFlagsToUnset & ~kWrapperFlagsMask) == 0,
               "Bad wrapper flag bits");
    mFlags.Set(mFlags.Get() & ~aFlagsToUnset);
  }

  void HoldJSObjects(void* aScriptObjectHolder, nsScriptObjectTracer* aTracer,
                     JS::Zone* aZone);

#ifdef DEBUG
 public:
  void CheckCCWrapperTraversal(void* aScriptObjectHolder,
                               nsScriptObjectTracer* aTracer);

 private:
#endif  // DEBUG

  /**
   * If this bit is set then we're preserving the wrapper, which in effect ties
   * the lifetime of the JS object stored in the cache to the lifetime of the
   * native object. We rely on the cycle collector to break the cycle that this
   * causes between the native object and the JS object, so it is important that
   * any native object that supports preserving of its wrapper
   * traces/traverses/unlinks the cached JS object (see
   * NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER and
   * NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER).
   */
  enum { WRAPPER_BIT_PRESERVED = 1 << 0 };

  enum { kWrapperFlagsMask = WRAPPER_BIT_PRESERVED };

  JSObject* mWrapper = nullptr;

  // Rust code needs to read and write some flags atomically, but we don't want
  // to make the wrapper flags atomic whole-sale because main-thread code would
  // become more expensive (loads wouldn't change, but flag setting /
  // unsetting could become slower enough to be noticeable). Making this an
  // Atomic whole-sale needs more measuring.
  //
  // In order to not mess with aliasing rules the type should not be frozen, so
  // we use a RustCell, which contains an UnsafeCell internally. See also the
  // handling of ServoData (though that's a bit different).
  mozilla::RustCell<FlagsType> mFlags{0};

 protected:
#ifdef BOOL_FLAGS_ON_WRAPPER_CACHE
  uint32_t mBoolFlags = 0;
#endif
};

enum { WRAPPER_CACHE_FLAGS_BITS_USED = 1 };

NS_DEFINE_STATIC_IID_ACCESSOR(nsWrapperCache, NS_WRAPPERCACHE_IID)

#define NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY           \
  if (aIID.Equals(NS_GET_IID(nsWrapperCache))) {        \
    *aInstancePtr = static_cast<nsWrapperCache*>(this); \
    return NS_OK;                                       \
  }

#define NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY \
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY     \
  else

// Cycle collector macros for wrapper caches.
//
// The NS_DECL_*WRAPPERCACHE_* macros make it easier to mark classes as holding
// just a single pointer to a JS value. That information is then used for
// certain GC optimizations.

#define NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS_AMBIGUOUS(_class, _base)   \
  class NS_CYCLE_COLLECTION_INNERCLASS                                         \
      : public nsXPCOMCycleCollectionParticipant {                             \
   public:                                                                     \
    constexpr explicit NS_CYCLE_COLLECTION_INNERCLASS(Flags aFlags = 0)        \
        : nsXPCOMCycleCollectionParticipant(aFlags |                           \
                                            FlagMaybeSingleZoneJSHolder) {}    \
                                                                               \
   private:                                                                    \
    NS_DECL_CYCLE_COLLECTION_CLASS_BODY(_class, _base)                         \
    NS_IMETHOD_(void)                                                          \
    Trace(void* p, const TraceCallbacks& cb, void* closure) override;          \
    NS_IMETHOD_(void)                                                          \
    TraceWrapper(void* aPtr, const TraceCallbacks& aCb, void* aClosure) final; \
    NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                     \
  };                                                                           \
  NS_CHECK_FOR_RIGHT_PARTICIPANT_IMPL(_class)                                  \
  static NS_CYCLE_COLLECTION_INNERCLASS NS_CYCLE_COLLECTION_INNERNAME;         \
  NOT_INHERITED_CANT_OVERRIDE

#define NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(_class) \
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS_AMBIGUOUS(_class, _class)

#define NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS_INHERITED(_class,          \
                                                              _base_class)     \
  class NS_CYCLE_COLLECTION_INNERCLASS                                         \
      : public NS_CYCLE_COLLECTION_CLASSNAME(_base_class) {                    \
   public:                                                                     \
    constexpr explicit NS_CYCLE_COLLECTION_INNERCLASS(Flags aFlags)            \
        : NS_CYCLE_COLLECTION_CLASSNAME(_base_class)(                          \
              aFlags | FlagMaybeSingleZoneJSHolder) {}                         \
                                                                               \
   private:                                                                    \
    NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_BODY(_class, _base_class)         \
    NS_IMETHOD_(void)                                                          \
    Trace(void* p, const TraceCallbacks& cb, void* closure) override;          \
    NS_IMETHOD_(void)                                                          \
    TraceWrapper(void* aPtr, const TraceCallbacks& aCb, void* aClosure) final; \
    NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                     \
  };                                                                           \
  NS_CHECK_FOR_RIGHT_PARTICIPANT_IMPL_INHERITED(_class)                        \
  static NS_CYCLE_COLLECTION_INNERCLASS NS_CYCLE_COLLECTION_INNERNAME;

#define NS_DECL_CYCLE_COLLECTION_SKIPPABLE_WRAPPERCACHE_CLASS_AMBIGUOUS(       \
    _class, _base)                                                             \
  class NS_CYCLE_COLLECTION_INNERCLASS                                         \
      : public nsXPCOMCycleCollectionParticipant {                             \
   public:                                                                     \
    constexpr explicit NS_CYCLE_COLLECTION_INNERCLASS(Flags aFlags)            \
        : nsXPCOMCycleCollectionParticipant(aFlags | FlagMightSkip |           \
                                            FlagMaybeSingleZoneJSHolder) {}    \
                                                                               \
   private:                                                                    \
    NS_DECL_CYCLE_COLLECTION_CLASS_BODY(_class, _base)                         \
    NS_IMETHOD_(void)                                                          \
    Trace(void* p, const TraceCallbacks& cb, void* closure) override;          \
    NS_IMETHOD_(void)                                                          \
    TraceWrapper(void* aPtr, const TraceCallbacks& aCb, void* aClosure) final; \
    NS_IMETHOD_(bool) CanSkipReal(void* p, bool aRemovingAllowed) override;    \
    NS_IMETHOD_(bool) CanSkipInCCReal(void* p) override;                       \
    NS_IMETHOD_(bool) CanSkipThisReal(void* p) override;                       \
    NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                     \
  };                                                                           \
  NS_CHECK_FOR_RIGHT_PARTICIPANT_IMPL(_class)                                  \
  static NS_CYCLE_COLLECTION_INNERCLASS NS_CYCLE_COLLECTION_INNERNAME;         \
  NOT_INHERITED_CANT_OVERRIDE

#define NS_DECL_CYCLE_COLLECTION_SKIPPABLE_WRAPPERCACHE_CLASS(_class)     \
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_WRAPPERCACHE_CLASS_AMBIGUOUS(_class, \
                                                                  _class)

#define NS_DECL_CYCLE_COLLECTION_SKIPPABLE_WRAPPERCACHE_CLASS_INHERITED(       \
    _class, _base_class)                                                       \
  class NS_CYCLE_COLLECTION_INNERCLASS                                         \
      : public NS_CYCLE_COLLECTION_CLASSNAME(_base_class) {                    \
   public:                                                                     \
    constexpr explicit NS_CYCLE_COLLECTION_INNERCLASS(Flags aFlags = 0)        \
        : NS_CYCLE_COLLECTION_CLASSNAME(_base_class)(                          \
              aFlags | FlagMightSkip | FlagMaybeSingleZoneJSHolder) {}         \
                                                                               \
   private:                                                                    \
    NS_DECL_CYCLE_COLLECTION_CLASS_BODY(_class, _base_class)                   \
    NS_IMETHOD_(void)                                                          \
    Trace(void* p, const TraceCallbacks& cb, void* closure) override;          \
    NS_IMETHOD_(void)                                                          \
    TraceWrapper(void* aPtr, const TraceCallbacks& aCb, void* aClosure) final; \
    NS_IMETHOD_(bool) CanSkipReal(void* p, bool aRemovingAllowed) override;    \
    NS_IMETHOD_(bool) CanSkipInCCReal(void* p) override;                       \
    NS_IMETHOD_(bool) CanSkipThisReal(void* p) override;                       \
    NS_IMPL_GET_XPCOM_CYCLE_COLLECTION_PARTICIPANT(_class)                     \
  };                                                                           \
  NS_CHECK_FOR_RIGHT_PARTICIPANT_IMPL_INHERITED(_class)                        \
  static NS_CYCLE_COLLECTION_INNERCLASS NS_CYCLE_COLLECTION_INNERNAME;

#define NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(_class)             \
  void DeleteCycleCollectable(void) { delete this; }                           \
  class NS_CYCLE_COLLECTION_INNERCLASS : public nsScriptObjectTracer {         \
   public:                                                                     \
    constexpr explicit NS_CYCLE_COLLECTION_INNERCLASS(Flags aFlags = 0)        \
        : nsScriptObjectTracer(aFlags | FlagMaybeSingleZoneJSHolder) {}        \
                                                                               \
   private:                                                                    \
    NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS_BODY(_class)                         \
    NS_IMETHOD_(void)                                                          \
    Trace(void* p, const TraceCallbacks& cb, void* closure) override;          \
    NS_IMETHOD_(void)                                                          \
    TraceWrapper(void* aPtr, const TraceCallbacks& aCb, void* aClosure) final; \
    static constexpr nsScriptObjectTracer* GetParticipant() {                  \
      return &_class::NS_CYCLE_COLLECTION_INNERNAME;                           \
    }                                                                          \
  };                                                                           \
  static NS_CYCLE_COLLECTION_INNERCLASS NS_CYCLE_COLLECTION_INNERNAME;

#define NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER \
  tmp->TraceWrapper(aCallbacks, aClosure);

#define NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER \
  tmp->ReleaseWrapper(p);

#define NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(_class)        \
  static_assert(std::is_base_of<nsWrapperCache, _class>::value,    \
                "Class should inherit nsWrapperCache");            \
  NS_IMPL_CYCLE_COLLECTION_SINGLE_ZONE_SCRIPT_HOLDER_CLASS(_class) \
  NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(_class)                     \
    TraceWrapper(p, aCallbacks, aClosure);                         \
  NS_IMPL_CYCLE_COLLECTION_TRACE_END                               \
  void NS_CYCLE_COLLECTION_CLASSNAME(_class)::TraceWrapper(        \
      void* p, const TraceCallbacks& aCallbacks, void* aClosure) { \
    _class* tmp = DowncastCCParticipant<_class>(p);                \
    NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER               \
  }

#define NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(_class) \
  NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(_class)   \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)         \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER   \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                   \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)       \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(_class, ...) \
  NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(_class)      \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)            \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(__VA_ARGS__)           \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER      \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                      \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)          \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(__VA_ARGS__)         \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WEAK(_class, ...) \
  NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(_class)           \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                 \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(__VA_ARGS__)                \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER           \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE              \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                           \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)               \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(__VA_ARGS__)              \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WEAK_PTR(_class, ...) \
  NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(_class)               \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                     \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(__VA_ARGS__)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER               \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR                        \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                               \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)                   \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(__VA_ARGS__)                  \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// This is used for wrapper cached classes that inherit from cycle
// collected non-wrapper cached classes.
#define NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_INHERITED(_class, _base, ...)   \
  static_assert(!std::is_base_of<nsWrapperCache, _base>::value,               \
                "Base class should not inherit nsWrapperCache");              \
  NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(_class)                         \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(_class, _base)              \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(__VA_ARGS__)                              \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER                         \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                         \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(_class, _base)            \
    /* Assert somewhere, in this case in the traverse method, that the */     \
    /* parent isn't a single zone holder*/                                    \
    MOZ_ASSERT(!_base::NS_CYCLE_COLLECTION_INNERNAME.IsSingleZoneJSHolder()); \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(__VA_ARGS__)                            \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// if NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WITH_JS_MEMBERS is used to implement
// a wrappercache class, one needs to use
// NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS and its variants in the class
// declaration.
#define NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WITH_JS_MEMBERS(              \
    class_, native_members_, js_members_)                                   \
  static_assert(std::is_base_of<nsWrapperCache, class_>::value,             \
                "Class should inherit nsWrapperCache");                     \
  NS_IMPL_CYCLE_COLLECTION_CLASS(class_)                                    \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(class_)                             \
    using ::ImplCycleCollectionUnlink;                                      \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(                                        \
        MOZ_FOR_EACH_EXPAND_HELPER native_members_)                         \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(MOZ_FOR_EACH_EXPAND_HELPER js_members_) \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER                       \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                       \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(class_)                           \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(                                      \
        MOZ_FOR_EACH_EXPAND_HELPER native_members_)                         \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                                     \
  NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(class_)                              \
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBERS(                              \
        MOZ_FOR_EACH_EXPAND_HELPER js_members_)                             \
    NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER                        \
  NS_IMPL_CYCLE_COLLECTION_TRACE_END

#endif /* nsWrapperCache_h___ */
