/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWrapperCache_h___
#define nsWrapperCache_h___

#include "nsCycleCollectionParticipant.h"
#include "mozilla/Assertions.h"
#include "js/RootingAPI.h"

struct JSTracer;
class JSObject;
struct JSContext;
class XPCWrappedNativeScope;

namespace mozilla {
namespace dom {
namespace workers {

class DOMBindingBase;

} // namespace workers
} // namespace dom
} // namespace mozilla

#define NS_WRAPPERCACHE_IID \
{ 0x6f3179a1, 0x36f7, 0x4a5c, \
  { 0x8c, 0xf1, 0xad, 0xc8, 0x7c, 0xde, 0x3e, 0x87 } }

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
 * The cache can store 2 types of objects:
 *
 *  If WRAPPER_IS_DOM_BINDING is not set (IsDOMBinding() returns false):
 *    - a slim wrapper or the JSObject of an XPCWrappedNative wrapper
 *
 *  If WRAPPER_IS_DOM_BINDING is set (IsDOMBinding() returns true):
 *    - a DOM binding object (regular JS object or proxy)
 *
 * The finalizer for the wrapper clears the cache.
 *
 * A number of the methods are implemented in nsWrapperCacheInlines.h because we
 * have to include some JS headers that don't play nicely with the rest of the
 * codebase. Include nsWrapperCacheInlines.h if you need to call those methods.
 */
class nsWrapperCache
{
  friend class mozilla::dom::workers::DOMBindingBase;

public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_WRAPPERCACHE_IID)

  nsWrapperCache() : mWrapperPtrBits(0)
  {
  }
  ~nsWrapperCache()
  {
    MOZ_ASSERT(!PreservingWrapper(),
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
   * be passed into a JS API function and that it won't be stored without being
   * rooted (or otherwise signaling the stored value to the CC).
   */
  JSObject* GetWrapperPreserveColor() const
  {
    return GetJSObjectFromBits();
  }

  void SetWrapper(JSObject* aWrapper)
  {
    MOZ_ASSERT(!PreservingWrapper(), "Clearing a preserved wrapper!");
    MOZ_ASSERT(aWrapper, "Use ClearWrapper!");

    SetWrapperBits(aWrapper);
  }

  /**
   * Clear the wrapper. This should be called from the finalizer for the
   * wrapper.
   */
  void ClearWrapper()
  {
    MOZ_ASSERT(!PreservingWrapper(), "Clearing a preserved wrapper!");

    SetWrapperBits(NULL);
  }

  bool PreservingWrapper()
  {
    return (mWrapperPtrBits & WRAPPER_BIT_PRESERVED) != 0;
  }

  void SetIsDOMBinding()
  {
    MOZ_ASSERT(!mWrapperPtrBits,
               "This flag should be set before creating any wrappers.");
    mWrapperPtrBits = WRAPPER_IS_DOM_BINDING;
  }

  bool IsDOMBinding() const
  {
    return (mWrapperPtrBits & WRAPPER_IS_DOM_BINDING) != 0;
  }

  void SetHasSystemOnlyWrapper()
  {
    MOZ_ASSERT(GetWrapperPreserveColor(),
               "This flag should be set after wrapper creation.");
    MOZ_ASSERT(IsDOMBinding(),
               "This flag should only be set for DOM bindings.");
    mWrapperPtrBits |= WRAPPER_HAS_SOW;
  }

  bool HasSystemOnlyWrapper() const
  {
    return (mWrapperPtrBits & WRAPPER_HAS_SOW) != 0;
  }

  /**
   * Wrap the object corresponding to this wrapper cache. If non-null is
   * returned, the object has already been stored in the wrapper cache.
   */
  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> scope)
  {
    MOZ_ASSERT(!IsDOMBinding(), "Someone forgot to override WrapObject");
    return nullptr;
  }

  /**
   * Returns true if the object has a non-gray wrapper.
   */
  bool IsBlack();

  /**
   * Returns true if the object has a black wrapper,
   * and all the GC things it is keeping alive are black too.
   */
  bool IsBlackAndDoesNotNeedTracing(nsISupports* aThis);

  // Only meant to be called by code that preserves a wrapper.
  void SetPreservingWrapper(bool aPreserve)
  {
    if(aPreserve) {
      mWrapperPtrBits |= WRAPPER_BIT_PRESERVED;
    }
    else {
      mWrapperPtrBits &= ~WRAPPER_BIT_PRESERVED;
    }
  }

  void TraceWrapper(const TraceCallbacks& aCallbacks, void* aClosure)
  {
    if (PreservingWrapper()) {
      JSObject *wrapper = GetWrapperPreserveColor();
      if (wrapper) {
        uintptr_t flags = mWrapperPtrBits & kWrapperBitMask;
        aCallbacks.Trace(&wrapper, "Preserved wrapper", aClosure);
        mWrapperPtrBits = reinterpret_cast<uintptr_t>(wrapper) | flags;
      }
    }
  }

private:
  JSObject *GetJSObjectFromBits() const
  {
    return reinterpret_cast<JSObject*>(mWrapperPtrBits & ~kWrapperBitMask);
  }
  void SetWrapperBits(void *aWrapper)
  {
    mWrapperPtrBits = reinterpret_cast<uintptr_t>(aWrapper) |
                      (mWrapperPtrBits & WRAPPER_IS_DOM_BINDING);
  }

  void TraceJSObjectFromBits(JSTracer *aTrc, const char *aName);

  /**
   * If this bit is set then we're preserving the wrapper, which in effect ties
   * the lifetime of the JS object stored in the cache to the lifetime of the
   * native object. We rely on the cycle collector to break the cycle that this
   * causes between the native object and the JS object, so it is important that
   * any native object that supports preserving of its wrapper
   * traces/traverses/unlinks the cached JS object (see
   * NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER,
   * NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS and
   * NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER).
   */
  enum { WRAPPER_BIT_PRESERVED = 1 << 0 };

  /**
   * If this bit is set then the wrapper for the native object is a DOM binding
   * (regular JS object or proxy).
   */
  enum { WRAPPER_IS_DOM_BINDING = 1 << 1 };

  /**
   * If this bit is set then the wrapper for the native object is a DOM binding
   * (regular JS object or proxy) that has a system only wrapper for same-origin
   * access.
   */
  enum { WRAPPER_HAS_SOW = 1 << 2 };

  enum { kWrapperBitMask = (WRAPPER_BIT_PRESERVED | WRAPPER_IS_DOM_BINDING |
                            WRAPPER_HAS_SOW) };

  uintptr_t mWrapperPtrBits;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsWrapperCache, NS_WRAPPERCACHE_IID)

#define NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY                                   \
  if ( aIID.Equals(NS_GET_IID(nsWrapperCache)) ) {                            \
    *aInstancePtr = static_cast<nsWrapperCache*>(this);                       \
    return NS_OK;                                                             \
  }


// Cycle collector macros for wrapper caches.

#define NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER \
  tmp->TraceWrapper(aCallbacks, aClosure);

#define NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER \
  nsContentUtils::ReleaseWrapper(p, tmp);

#define NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(_class) \
  NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(_class)              \
    NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER        \
  NS_IMPL_CYCLE_COLLECTION_TRACE_END

#define NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(_class) \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)         \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER   \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                   \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)       \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS    \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                 \
  NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(_class)

#define NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(_class, _field) \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                 \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field)                     \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER           \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                           \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)               \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field)                   \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS            \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                         \
  NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(_class)

#define NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_2(_class, _field1,\
                                                _field2)        \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                 \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field1)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field2)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER           \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                           \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)               \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field1)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field2)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS            \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                         \
  NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(_class)

#define NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_3(_class, _field1,\
                                                _field2,        \
                                                _field3)        \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                 \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field1)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field2)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field3)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER           \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                           \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)               \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field1)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field2)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field3)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS            \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                         \
  NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(_class)

#define NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_4(_class, _field1,\
                                                _field2,        \
                                                _field3,        \
                                                _field4)        \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                 \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field1)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field2)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field3)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field4)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER           \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                           \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)               \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field1)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field2)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field3)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field4)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS            \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                         \
  NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(_class)

#define NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_5(_class, _field1,\
                                                _field2,        \
                                                _field3,        \
                                                _field4,        \
                                                _field5)        \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                 \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field1)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field2)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field3)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field4)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field5)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER           \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                           \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)               \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field1)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field2)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field3)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field4)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field5)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS            \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                         \
  NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(_class)

#define NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_6(_class, _field1,\
                                                _field2,        \
                                                _field3,        \
                                                _field4,        \
                                                _field5,        \
                                                _field6)        \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                 \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field1)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field2)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field3)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field4)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field5)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field6)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER           \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                           \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)               \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field1)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field2)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field3)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field4)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field5)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field6)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS            \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                         \
  NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(_class)

#define NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_7(_class, _field1,\
                                                _field2,        \
                                                _field3,        \
                                                _field4,        \
                                                _field5,        \
                                                _field6,        \
                                                _field7)        \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                 \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field1)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field2)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field3)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field4)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field5)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field6)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field7)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER           \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                           \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)               \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field1)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field2)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field3)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field4)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field5)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field6)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field7)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS            \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                         \
  NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(_class)

#define NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_8(_class, _field1,\
                                                _field2,        \
                                                _field3,        \
                                                _field4,        \
                                                _field5,        \
                                                _field6,        \
                                                _field7,        \
                                                _field8)        \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                 \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field1)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field2)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field3)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field4)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field5)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field6)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field7)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field8)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER           \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                           \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)               \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field1)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field2)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field3)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field4)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field5)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field6)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field7)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field8)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS            \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                         \
  NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(_class)

#define NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_9(_class, _field1,\
                                                _field2,        \
                                                _field3,        \
                                                _field4,        \
                                                _field5,        \
                                                _field6,        \
                                                _field7,        \
                                                _field8,        \
                                                _field9)        \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                 \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field1)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field2)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field3)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field4)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field5)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field6)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field7)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field8)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field9)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER           \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                           \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)               \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field1)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field2)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field3)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field4)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field5)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field6)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field7)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field8)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field9)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS            \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                         \
  NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(_class)

#define NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_10(_class, _field1,\
                                                 _field2,       \
                                                 _field3,       \
                                                 _field4,       \
                                                 _field5,       \
                                                 _field6,       \
                                                 _field7,       \
                                                 _field8,       \
                                                 _field9,       \
                                                 _field10)      \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(_class)                 \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field1)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field2)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field3)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field4)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field5)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field6)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field7)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field8)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field9)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(_field10)                   \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER           \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                           \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(_class)               \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field1)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field2)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field3)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field4)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field5)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field6)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field7)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field8)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field9)                  \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(_field10)                 \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS            \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END                         \
  NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(_class)

#endif /* nsWrapperCache_h___ */
