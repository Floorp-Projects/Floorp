/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Gecko DOM code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Peter Van der Beken <peterv@propagandism.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsWrapperCache_h___
#define nsWrapperCache_h___

#include "nsCycleCollectionParticipant.h"

struct JSObject;
struct JSContext;
class XPCWrappedNativeScope;

typedef PRUptrdiff PtrBits;

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
 *  If WRAPPER_IS_PROXY is not set (IsProxy() returns false):
 *    - a slim wrapper or the JSObject of an XPCWrappedNative wrapper
 *
 *  If WRAPPER_IS_PROXY is set (IsProxy() returns true):
 *    - a DOM binding object (proxy)
 *
 * The finalizer for the wrapper clears the cache.
 *
 * A number of the methods are implemented in nsWrapperCacheInlines.h because we
 * have to include some JS headers that don't play nicely with the rest of the
 * codebase. Include nsWrapperCacheInlines.h if you need to call those methods.
 */
class nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_WRAPPERCACHE_IID)

  nsWrapperCache() : mWrapperPtrBits(0)
  {
  }
  ~nsWrapperCache()
  {
    NS_ASSERTION(!PreservingWrapper(),
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
    NS_ASSERTION(!PreservingWrapper(), "Clearing a preserved wrapper!");
    NS_ASSERTION(aWrapper, "Use ClearWrapper!");

    SetWrapperBits(aWrapper);
  }

  /**
   * Clear the wrapper. This should be called from the finalizer for the
   * wrapper.
   */
  void ClearWrapper()
  {
    NS_ASSERTION(!PreservingWrapper(), "Clearing a preserved wrapper!");

    SetWrapperBits(NULL);
  }

  bool PreservingWrapper()
  {
    return (mWrapperPtrBits & WRAPPER_BIT_PRESERVED) != 0;
  }

  void SetIsProxy()
  {
    NS_ASSERTION(!mWrapperPtrBits,
                 "This flag should be set before creating any wrappers.");
    mWrapperPtrBits = WRAPPER_IS_PROXY;
  }
  void ClearIsProxy()
  {
    NS_ASSERTION(!mWrapperPtrBits || mWrapperPtrBits == WRAPPER_IS_PROXY,
                 "This flag should be cleared before creating any wrappers.");
    mWrapperPtrBits = 0;
  }

  bool IsProxy() const
  {
    return (mWrapperPtrBits & WRAPPER_IS_PROXY) != 0;
  }


  /**
   * Wrap the object corresponding to this wrapper cache. If non-null is
   * returned, the object has already been stored in the wrapper cache and the
   * value set in triedToWrap is meaningless. If null is returned then
   * triedToWrap indicates whether an error occurred, if it's false then the
   * object doesn't actually support creating a wrapper through its WrapObject
   * hook.
   */
  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap)
  {
    *triedToWrap = false;
    return nsnull;
  }

  /**
   * Returns true if the object has a non-gray wrapper.
   */
  bool IsBlack();

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

private:
  JSObject *GetJSObjectFromBits() const
  {
    return reinterpret_cast<JSObject*>(mWrapperPtrBits & ~kWrapperBitMask);
  }
  void SetWrapperBits(void *aWrapper)
  {
    mWrapperPtrBits = reinterpret_cast<PtrBits>(aWrapper) |
                      (mWrapperPtrBits & WRAPPER_IS_PROXY);
  }

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
   * If this bit is set then the wrapper for the native object is a proxy. Note
   * that that doesn't necessarily mean that the JS object stored in the cache
   * is a JS proxy, as we sometimes store objects other than the wrapper in the
   * cache.
   */
  enum { WRAPPER_IS_PROXY = 1 << 1 };

  enum { kWrapperBitMask = (WRAPPER_BIT_PRESERVED | WRAPPER_IS_PROXY) };

  PtrBits mWrapperPtrBits;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsWrapperCache, NS_WRAPPERCACHE_IID)

#define NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY                                   \
  if ( aIID.Equals(NS_GET_IID(nsWrapperCache)) ) {                            \
    *aInstancePtr = static_cast<nsWrapperCache*>(this);                       \
    return NS_OK;                                                             \
  }

#endif /* nsWrapperCache_h___ */
