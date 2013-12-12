/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A common base class for representing WebIDL callback function and
 * callback interface types in C++.
 *
 * This class implements common functionality like lifetime
 * management, initialization with the JS object, and setup of the
 * call environment.  Subclasses are responsible for providing methods
 * that do the call into JS as needed.
 */

#ifndef mozilla_dom_CallbackObject_h
#define mozilla_dom_CallbackObject_h

#include "nsISupports.h"
#include "nsISupportsImpl.h"
#include "nsCycleCollectionParticipant.h"
#include "jswrapper.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsContentUtils.h"
#include "nsWrapperCache.h"
#include "nsJSEnvironment.h"
#include "xpcpublic.h"

namespace mozilla {
namespace dom {

#define DOM_CALLBACKOBJECT_IID \
{ 0xbe74c190, 0x6d76, 0x4991, \
 { 0x84, 0xb9, 0x65, 0x06, 0x99, 0xe6, 0x93, 0x2b } }

class CallbackObject : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(DOM_CALLBACKOBJECT_IID)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CallbackObject)

  // The caller may pass a global object which will act as an override for the
  // incumbent script settings object when the callback is invoked (overriding
  // the entry point computed from aCallback). If no override is required, the
  // caller should pass null.
  explicit CallbackObject(JS::Handle<JSObject*> aCallback, nsIGlobalObject *aIncumbentGlobal)
  {
    Init(aCallback, aIncumbentGlobal);
  }

  virtual ~CallbackObject()
  {
    DropCallback();
  }

  JS::Handle<JSObject*> Callback() const
  {
    JS::ExposeObjectToActiveJS(mCallback);
    return CallbackPreserveColor();
  }

  /*
   * This getter does not change the color of the JSObject meaning that the
   * object returned is not guaranteed to be kept alive past the next CC.
   *
   * This should only be called if you are certain that the return value won't
   * be passed into a JS API function and that it won't be stored without being
   * rooted (or otherwise signaling the stored value to the CC).
   */
  JS::Handle<JSObject*> CallbackPreserveColor() const
  {
    // Calling fromMarkedLocation() is safe because we trace our mCallback, and
    // because the value of mCallback cannot change after if has been set.
    return JS::Handle<JSObject*>::fromMarkedLocation(mCallback.address());
  }

  nsIGlobalObject* IncumbentGlobalOrNull() const
  {
    return mIncumbentGlobal;
  }

  enum ExceptionHandling {
    // Report any exception and don't throw it to the caller code.
    eReportExceptions,
    // Throw an exception to the caller code if the thrown exception is a
    // binding object for a DOMError from the caller's scope, otherwise report
    // it.
    eRethrowContentExceptions,
    // Throw any exception to the caller code.
    eRethrowExceptions
  };

protected:
  explicit CallbackObject(CallbackObject* aCallbackObject)
  {
    Init(aCallbackObject->mCallback, aCallbackObject->mIncumbentGlobal);
  }

private:
  inline void Init(JSObject* aCallback, nsIGlobalObject* aIncumbentGlobal)
  {
    MOZ_ASSERT(aCallback && !mCallback);
    // Set mCallback before we hold, on the off chance that a GC could somehow
    // happen in there... (which would be pretty odd, granted).
    mCallback = aCallback;
    mozilla::HoldJSObjects(this);

    mIncumbentGlobal = aIncumbentGlobal;
  }

  CallbackObject(const CallbackObject&) MOZ_DELETE;
  CallbackObject& operator =(const CallbackObject&) MOZ_DELETE;

protected:
  void DropCallback()
  {
    if (mCallback) {
      mCallback = nullptr;
      mozilla::DropJSObjects(this);
    }
  }

  JS::Heap<JSObject*> mCallback;
  nsCOMPtr<nsIGlobalObject> mIncumbentGlobal;

  class MOZ_STACK_CLASS CallSetup
  {
    /**
     * A class that performs whatever setup we need to safely make a
     * call while this class is on the stack, After the constructor
     * returns, the call is safe to make if GetContext() returns
     * non-null.
     */
  public:
    // If aExceptionHandling == eRethrowContentExceptions then aCompartment
    // needs to be set to the caller's compartment.
    CallSetup(CallbackObject* aCallback, ErrorResult& aRv,
              ExceptionHandling aExceptionHandling,
              JSCompartment* aCompartment = nullptr);
    ~CallSetup();

    JSContext* GetContext() const
    {
      return mCx;
    }

  private:
    // We better not get copy-constructed
    CallSetup(const CallSetup&) MOZ_DELETE;

    bool ShouldRethrowException(JS::Handle<JS::Value> aException);

    // Members which can go away whenever
    JSContext* mCx;

    // Caller's compartment. This will only have a sensible value if
    // mExceptionHandling == eRethrowContentExceptions.
    JSCompartment* mCompartment;

    // And now members whose construction/destruction order we need to control.
    Maybe<AutoEntryScript> mAutoEntryScript;
    Maybe<AutoIncumbentScript> mAutoIncumbentScript;

    // Constructed the rooter within the scope of mCxPusher above, so that it's
    // always within a request during its lifetime.
    Maybe<JS::Rooted<JSObject*> > mRootedCallable;

    // Can't construct a JSAutoCompartment without a JSContext either.  Also,
    // Put mAc after mAutoEntryScript so that we exit the compartment before
    // we pop the JSContext. Though in practice we'll often manually order
    // those two things.
    Maybe<JSAutoCompartment> mAc;

    // An ErrorResult to possibly re-throw exceptions on and whether
    // we should re-throw them.
    ErrorResult& mErrorResult;
    const ExceptionHandling mExceptionHandling;
    JS::ContextOptions mSavedJSContextOptions;
    const bool mIsMainThread;
  };
};

template<class WebIDLCallbackT, class XPCOMCallbackT>
class CallbackObjectHolder;

template<class T, class U>
void ImplCycleCollectionUnlink(CallbackObjectHolder<T, U>& aField);

class CallbackObjectHolderBase
{
protected:
  // Returns null on all failures
  already_AddRefed<nsISupports> ToXPCOMCallback(CallbackObject* aCallback,
                                                const nsIID& aIID) const;
};

template<class WebIDLCallbackT, class XPCOMCallbackT>
class CallbackObjectHolder : CallbackObjectHolderBase
{
  /**
   * A class which stores either a WebIDLCallbackT* or an XPCOMCallbackT*.  Both
   * types must inherit from nsISupports.  The pointer that's stored can be
   * null.
   *
   * When storing a WebIDLCallbackT*, mPtrBits is set to the pointer value.
   * When storing an XPCOMCallbackT*, mPtrBits is the pointer value with low bit
   * set.
   */
public:
  explicit CallbackObjectHolder(WebIDLCallbackT* aCallback)
    : mPtrBits(reinterpret_cast<uintptr_t>(aCallback))
  {
    NS_IF_ADDREF(aCallback);
  }

  explicit CallbackObjectHolder(XPCOMCallbackT* aCallback)
    : mPtrBits(reinterpret_cast<uintptr_t>(aCallback) | XPCOMCallbackFlag)
  {
    NS_IF_ADDREF(aCallback);
  }

  explicit CallbackObjectHolder(const CallbackObjectHolder& aOther)
    : mPtrBits(aOther.mPtrBits)
  {
    NS_IF_ADDREF(GetISupports());
  }

  CallbackObjectHolder()
    : mPtrBits(0)
  {}

  ~CallbackObjectHolder()
  {
    UnlinkSelf();
  }

  void operator=(WebIDLCallbackT* aCallback)
  {
    UnlinkSelf();
    mPtrBits = reinterpret_cast<uintptr_t>(aCallback);
    NS_IF_ADDREF(aCallback);
  }

  void operator=(XPCOMCallbackT* aCallback)
  {
    UnlinkSelf();
    mPtrBits = reinterpret_cast<uintptr_t>(aCallback) | XPCOMCallbackFlag;
    NS_IF_ADDREF(aCallback);
  }

  void operator=(const CallbackObjectHolder& aOther)
  {
    UnlinkSelf();
    mPtrBits = aOther.mPtrBits;
    NS_IF_ADDREF(GetISupports());
  }

  nsISupports* GetISupports() const
  {
    return reinterpret_cast<nsISupports*>(mPtrBits & ~XPCOMCallbackFlag);
  }

  // Boolean conversion operator so people can use this in boolean tests
  operator bool() const
  {
    return GetISupports();
  }

  // Even if HasWebIDLCallback returns true, GetWebIDLCallback() might still
  // return null.
  bool HasWebIDLCallback() const
  {
    return !(mPtrBits & XPCOMCallbackFlag);
  }

  WebIDLCallbackT* GetWebIDLCallback() const
  {
    MOZ_ASSERT(HasWebIDLCallback());
    return reinterpret_cast<WebIDLCallbackT*>(mPtrBits);
  }

  XPCOMCallbackT* GetXPCOMCallback() const
  {
    MOZ_ASSERT(!HasWebIDLCallback());
    return reinterpret_cast<XPCOMCallbackT*>(mPtrBits & ~XPCOMCallbackFlag);
  }

  bool operator==(WebIDLCallbackT* aOtherCallback) const
  {
    if (!aOtherCallback) {
      // If other is null, then we must be null to be equal.
      return !GetISupports();
    }

    if (!HasWebIDLCallback() || !GetWebIDLCallback()) {
      // If other is non-null, then we can't be equal if we have a
      // non-WebIDL callback or a null callback.
      return false;
    }

    JSObject* thisObj =
      js::UncheckedUnwrap(GetWebIDLCallback()->CallbackPreserveColor());
    JSObject* otherObj =
      js::UncheckedUnwrap(aOtherCallback->CallbackPreserveColor());
    return thisObj == otherObj;
  }

  bool operator==(XPCOMCallbackT* aOtherCallback) const
  {
    return (!aOtherCallback && !GetISupports()) ||
      (!HasWebIDLCallback() && GetXPCOMCallback() == aOtherCallback);
  }

  bool operator==(const CallbackObjectHolder& aOtherCallback) const
  {
    if (aOtherCallback.HasWebIDLCallback()) {
      return *this == aOtherCallback.GetWebIDLCallback();
    }

    return *this == aOtherCallback.GetXPCOMCallback();
  }

  // Try to return an XPCOMCallbackT version of this object.
  already_AddRefed<XPCOMCallbackT> ToXPCOMCallback() const
  {
    if (!HasWebIDLCallback()) {
      nsRefPtr<XPCOMCallbackT> callback = GetXPCOMCallback();
      return callback.forget();
    }

    nsCOMPtr<nsISupports> supp =
      CallbackObjectHolderBase::ToXPCOMCallback(GetWebIDLCallback(),
                                                NS_GET_TEMPLATE_IID(XPCOMCallbackT));
    // ToXPCOMCallback already did the right QI for us.
    return supp.forget().downcast<XPCOMCallbackT>();
  }

  // Try to return a WebIDLCallbackT version of this object.
  already_AddRefed<WebIDLCallbackT> ToWebIDLCallback() const
  {
    if (HasWebIDLCallback()) {
      nsRefPtr<WebIDLCallbackT> callback = GetWebIDLCallback();
      return callback.forget();
    }
    return nullptr;
  }

private:
  static const uintptr_t XPCOMCallbackFlag = 1u;

  friend void
  ImplCycleCollectionUnlink<WebIDLCallbackT,
                            XPCOMCallbackT>(CallbackObjectHolder& aField);

  void UnlinkSelf()
  {
    // NS_IF_RELEASE because we might have been unlinked before
    nsISupports* ptr = GetISupports();
    NS_IF_RELEASE(ptr);
    mPtrBits = 0;
  }

  uintptr_t mPtrBits;
};

NS_DEFINE_STATIC_IID_ACCESSOR(CallbackObject, DOM_CALLBACKOBJECT_IID)

template<class T, class U>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            CallbackObjectHolder<T, U>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  CycleCollectionNoteChild(aCallback, aField.GetISupports(), aName, aFlags);
}

template<class T, class U>
void
ImplCycleCollectionUnlink(CallbackObjectHolder<T, U>& aField)
{
  aField.UnlinkSelf();
}

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CallbackObject_h
