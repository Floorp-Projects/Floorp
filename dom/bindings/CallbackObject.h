/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "js/Wrapper.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsWrapperCache.h"
#include "nsJSEnvironment.h"
#include "xpcpublic.h"
#include "jsapi.h"
#include "js/ContextOptions.h"
#include "js/TracingAPI.h"

namespace mozilla {

class PromiseJobRunnable;

namespace dom {

#define DOM_CALLBACKOBJECT_IID                       \
  {                                                  \
    0xbe74c190, 0x6d76, 0x4991, {                    \
      0x84, 0xb9, 0x65, 0x06, 0x99, 0xe6, 0x93, 0x2b \
    }                                                \
  }

class CallbackObject : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(DOM_CALLBACKOBJECT_IID)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(CallbackObject)

  // The caller may pass a global object which will act as an override for the
  // incumbent script settings object when the callback is invoked (overriding
  // the entry point computed from aCallback). If no override is required, the
  // caller should pass null.  |aCx| is used to capture the current
  // stack, which is later used as an async parent when the callback
  // is invoked.  aCx can be nullptr, in which case no stack is
  // captured.
  explicit CallbackObject(JSContext* aCx, JS::Handle<JSObject*> aCallback,
                          JS::Handle<JSObject*> aCallbackGlobal,
                          nsIGlobalObject* aIncumbentGlobal) {
    if (aCx && JS::ContextOptionsRef(aCx).asyncStack()) {
      JS::RootedObject stack(aCx);
      if (!JS::CaptureCurrentStack(aCx, &stack)) {
        JS_ClearPendingException(aCx);
      }
      Init(aCallback, aCallbackGlobal, stack, aIncumbentGlobal);
    } else {
      Init(aCallback, aCallbackGlobal, nullptr, aIncumbentGlobal);
    }
  }

  // Instead of capturing the current stack to use as an async parent when the
  // callback is invoked, the caller can use this overload to pass in a stack
  // for that purpose.
  explicit CallbackObject(JSObject* aCallback, JSObject* aCallbackGlobal,
                          JSObject* aAsyncStack,
                          nsIGlobalObject* aIncumbentGlobal) {
    Init(aCallback, aCallbackGlobal, aAsyncStack, aIncumbentGlobal);
  }

  // This is guaranteed to be non-null from the time the CallbackObject is
  // created until JavaScript has had a chance to run. It will only return null
  // after a JavaScript caller has called nukeSandbox on a Sandbox object and
  // the cycle collector has had a chance to run, unless Reset() is explicitly
  // called (see below).
  //
  // This means that any native callee which receives a CallbackObject as an
  // argument can safely rely on the callback being non-null so long as it
  // doesn't trigger any scripts before it accesses it.
  JS::Handle<JSObject*> CallbackOrNull() const {
    mCallback.exposeToActiveJS();
    return CallbackPreserveColor();
  }

  JSObject* CallbackGlobalOrNull() const {
    mCallbackGlobal.exposeToActiveJS();
    return mCallbackGlobal;
  }

  // Like CallbackOrNull(), but will return a new dead proxy object in the
  // caller's realm if the callback is null.
  JSObject* Callback(JSContext* aCx);

  JSObject* GetCreationStack() const { return mCreationStack; }

  void MarkForCC() {
    mCallback.exposeToActiveJS();
    mCallbackGlobal.exposeToActiveJS();
    mCreationStack.exposeToActiveJS();
  }

  /*
   * This getter does not change the color of the JSObject meaning that the
   * object returned is not guaranteed to be kept alive past the next CC.
   *
   * This should only be called if you are certain that the return value won't
   * be passed into a JS API function and that it won't be stored without being
   * rooted (or otherwise signaling the stored value to the CC).
   *
   * Note that calling Reset() will also affect the value of any handle
   * previously returned here. Don't call Reset() if a handle is still in use.
   */
  JS::Handle<JSObject*> CallbackPreserveColor() const {
    // Calling fromMarkedLocation() is safe because we trace our mCallback, and
    // because the value of mCallback cannot change after if has been set
    // (except for calling Reset() as described above).
    return JS::Handle<JSObject*>::fromMarkedLocation(mCallback.address());
  }
  JS::Handle<JSObject*> CallbackGlobalPreserveColor() const {
    // The comment in CallbackPreserveColor applies here as well.
    return JS::Handle<JSObject*>::fromMarkedLocation(mCallbackGlobal.address());
  }

  /*
   * If the callback is known to be non-gray, then this method can be
   * used instead of CallbackOrNull() to avoid the overhead of
   * ExposeObjectToActiveJS().
   */
  JS::Handle<JSObject*> CallbackKnownNotGray() const {
    JS::AssertObjectIsNotGray(mCallback);
    return CallbackPreserveColor();
  }

  nsIGlobalObject* IncumbentGlobalOrNull() const { return mIncumbentGlobal; }

  enum ExceptionHandling {
    // Report any exception and don't throw it to the caller code.
    eReportExceptions,
    // Throw an exception to the caller code if the thrown exception is a
    // binding object for a DOMException from the caller's scope, otherwise
    // report it.
    eRethrowContentExceptions,
    // Throw exceptions to the caller code, unless the caller realm is
    // provided, the exception is not a DOMException from the caller
    // realm, and the caller realm does not subsume our unwrapped callback.
    eRethrowExceptions
  };

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this);
  }

 protected:
  virtual ~CallbackObject() { mozilla::DropJSObjects(this); }

  explicit CallbackObject(CallbackObject* aCallbackObject) {
    Init(aCallbackObject->mCallback, aCallbackObject->mCallbackGlobal,
         aCallbackObject->mCreationStack, aCallbackObject->mIncumbentGlobal);
  }

  bool operator==(const CallbackObject& aOther) const {
    JSObject* wrappedThis = CallbackPreserveColor();
    JSObject* wrappedOther = aOther.CallbackPreserveColor();
    if (!wrappedThis || !wrappedOther) {
      return this == &aOther;
    }

    JSObject* thisObj = js::UncheckedUnwrap(wrappedThis);
    JSObject* otherObj = js::UncheckedUnwrap(wrappedOther);
    return thisObj == otherObj;
  }

  class JSObjectsDropper final {
   public:
    explicit JSObjectsDropper(CallbackObject* aHolder) : mHolder(aHolder) {}

    ~JSObjectsDropper() { mHolder->ClearJSObjects(); }

   private:
    RefPtr<CallbackObject> mHolder;
  };

 private:
  inline void InitNoHold(JSObject* aCallback, JSObject* aCallbackGlobal,
                         JSObject* aCreationStack,
                         nsIGlobalObject* aIncumbentGlobal) {
    MOZ_ASSERT(aCallback && !mCallback);
    MOZ_ASSERT(aCallbackGlobal);
    MOZ_DIAGNOSTIC_ASSERT(js::GetObjectCompartment(aCallback) ==
                          js::GetObjectCompartment(aCallbackGlobal));
    MOZ_ASSERT(JS_IsGlobalObject(aCallbackGlobal));
    mCallback = aCallback;
    mCallbackGlobal = aCallbackGlobal;
    mCreationStack = aCreationStack;
    if (aIncumbentGlobal) {
      mIncumbentGlobal = aIncumbentGlobal;
      // We don't want to expose to JS here (change the color).  If someone ever
      // reads mIncumbentJSGlobal, that will expose.  If not, no need to expose
      // here.
      mIncumbentJSGlobal = aIncumbentGlobal->GetGlobalJSObjectPreserveColor();
    }
  }

  inline void Init(JSObject* aCallback, JSObject* aCallbackGlobal,
                   JSObject* aCreationStack,
                   nsIGlobalObject* aIncumbentGlobal) {
    // Set script objects before we hold, on the off chance that a GC could
    // somehow happen in there... (which would be pretty odd, granted).
    InitNoHold(aCallback, aCallbackGlobal, aCreationStack, aIncumbentGlobal);
    mozilla::HoldJSObjects(this);
  }

  // Provide a way to clear this object's pointers to GC things after the
  // callback has been run. Note that CallbackOrNull() will return null after
  // this point. This should only be called if the object is known not to be
  // used again, and no handles (e.g. those returned by CallbackPreserveColor)
  // are in use.
  void Reset() { ClearJSReferences(); }
  friend class mozilla::PromiseJobRunnable;

  inline void ClearJSReferences() {
    mCallback = nullptr;
    mCallbackGlobal = nullptr;
    mCreationStack = nullptr;
    mIncumbentJSGlobal = nullptr;
  }

  CallbackObject(const CallbackObject&) = delete;
  CallbackObject& operator=(const CallbackObject&) = delete;

 protected:
  void ClearJSObjects() {
    MOZ_ASSERT_IF(mIncumbentJSGlobal, mCallback);
    if (mCallback) {
      ClearJSReferences();
    }
  }

  // For use from subclasses that want to be usable with Rooted.
  void Trace(JSTracer* aTracer);

  // For use from subclasses that want to be traced for a bit then possibly
  // switch to HoldJSObjects and do other slow JS-related init work we might do.
  // If we have more than one owner, this will HoldJSObjects and do said slow
  // init work; otherwise it will just forget all our JS references.
  void FinishSlowJSInitIfMoreThanOneOwner(JSContext* aCx);

  // Struct used as a way to force a CallbackObject constructor to not call
  // HoldJSObjects. We're putting it here so that CallbackObject subclasses will
  // have access to it, but outside code will not.
  //
  // Places that use this need to ensure that the callback is traced (e.g. via a
  // Rooted) until the HoldJSObjects call happens.
  struct FastCallbackConstructor {};

  // Just like the public version without the FastCallbackConstructor argument,
  // except for not calling HoldJSObjects and not capturing async stacks (on the
  // assumption that we will do that last whenever we decide to actually
  // HoldJSObjects; see FinishSlowJSInitIfMoreThanOneOwner).  If you use this,
  // you MUST ensure that the object is traced until the HoldJSObjects happens!
  CallbackObject(JSObject* aCallback, JSObject* aCallbackGlobal,
                 const FastCallbackConstructor&) {
    InitNoHold(aCallback, aCallbackGlobal, nullptr, nullptr);
  }

  // mCallback is not unwrapped, so it can be a cross-compartment-wrapper.
  // This is done to ensure that, if JS code can't call a callback f(), or get
  // its members, directly itself, this code won't call f(), or get its members,
  // on the code's behalf.
  JS::Heap<JSObject*> mCallback;
  // mCallbackGlobal is the global that we were in when we created the
  // callback. In particular, it is guaranteed to be same-compartment with
  // aCallback. We store it separately, because we have no way to recover the
  // global if mCallback is a cross-compartment wrapper.
  JS::Heap<JSObject*> mCallbackGlobal;
  JS::Heap<JSObject*> mCreationStack;
  // Ideally, we'd just hold a reference to the nsIGlobalObject, since that's
  // what we need to pass to AutoIncumbentScript. Unfortunately, that doesn't
  // hold the actual JS global alive. So we maintain an additional pointer to
  // the JS global itself so that we can trace it.
  //
  // At some point we should consider trying to make native globals hold their
  // scripted global alive, at which point we can get rid of the duplication
  // here.
  nsCOMPtr<nsIGlobalObject> mIncumbentGlobal;
  JS::TenuredHeap<JSObject*> mIncumbentJSGlobal;

  class MOZ_STACK_CLASS CallSetup {
    /**
     * A class that performs whatever setup we need to safely make a
     * call while this class is on the stack, After the constructor
     * returns, the call is safe to make if GetContext() returns
     * non-null.
     */
   public:
    // If aExceptionHandling == eRethrowContentExceptions then aRealm
    // needs to be set to the realm in which exceptions will be rethrown.
    //
    // If aExceptionHandling == eRethrowExceptions then aRealm may be set
    // to the realm in which exceptions will be rethrown.  In that case
    // they will only be rethrown if that realm's principal subsumes the
    // principal of our (unwrapped) callback.
    CallSetup(CallbackObject* aCallback, ErrorResult& aRv,
              const char* aExecutionReason,
              ExceptionHandling aExceptionHandling, JS::Realm* aRealm = nullptr,
              bool aIsJSImplementedWebIDL = false);
    MOZ_CAN_RUN_SCRIPT ~CallSetup();

    JSContext* GetContext() const { return mCx; }

   private:
    // We better not get copy-constructed
    CallSetup(const CallSetup&) = delete;

    bool ShouldRethrowException(JS::Handle<JS::Value> aException);

    // Members which can go away whenever
    JSContext* mCx;

    // Caller's realm. This will only have a sensible value if
    // mExceptionHandling == eRethrowContentExceptions or eRethrowExceptions.
    JS::Realm* mRealm;

    // And now members whose construction/destruction order we need to control.
    Maybe<AutoEntryScript> mAutoEntryScript;
    Maybe<AutoIncumbentScript> mAutoIncumbentScript;

    Maybe<JS::Rooted<JSObject*>> mRootedCallable;
    // The global of mRootedCallable.
    Maybe<JS::Rooted<JSObject*>> mRootedCallableGlobal;

    // Members which are used to set the async stack.
    Maybe<JS::Rooted<JSObject*>> mAsyncStack;
    Maybe<JS::AutoSetAsyncStackForNewCalls> mAsyncStackSetter;

    // Can't construct a JSAutoRealm without a JSContext either.  Also,
    // Put mAr after mAutoEntryScript so that we exit the realm before we
    // pop the script settings stack. Though in practice we'll often manually
    // order those two things.
    Maybe<JSAutoRealm> mAr;

    // An ErrorResult to possibly re-throw exceptions on and whether
    // we should re-throw them.
    ErrorResult& mErrorResult;
    const ExceptionHandling mExceptionHandling;
    const bool mIsMainThread;
  };
};

template <class WebIDLCallbackT, class XPCOMCallbackT>
class CallbackObjectHolder;

template <class T, class U>
void ImplCycleCollectionUnlink(CallbackObjectHolder<T, U>& aField);

class CallbackObjectHolderBase {
 protected:
  // Returns null on all failures
  already_AddRefed<nsISupports> ToXPCOMCallback(CallbackObject* aCallback,
                                                const nsIID& aIID) const;
};

template <class WebIDLCallbackT, class XPCOMCallbackT>
class CallbackObjectHolder : CallbackObjectHolderBase {
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
      : mPtrBits(reinterpret_cast<uintptr_t>(aCallback)) {
    NS_IF_ADDREF(aCallback);
  }

  explicit CallbackObjectHolder(XPCOMCallbackT* aCallback)
      : mPtrBits(reinterpret_cast<uintptr_t>(aCallback) | XPCOMCallbackFlag) {
    NS_IF_ADDREF(aCallback);
  }

  CallbackObjectHolder(CallbackObjectHolder&& aOther)
      : mPtrBits(aOther.mPtrBits) {
    aOther.mPtrBits = 0;
    static_assert(sizeof(CallbackObjectHolder) == sizeof(void*),
                  "This object is expected to be as small as a pointer, and it "
                  "is currently passed by value in various places. If it is "
                  "bloating, we may want to pass it by reference then.");
  }

  CallbackObjectHolder(const CallbackObjectHolder& aOther) = delete;

  CallbackObjectHolder() : mPtrBits(0) {}

  ~CallbackObjectHolder() { UnlinkSelf(); }

  void operator=(WebIDLCallbackT* aCallback) {
    UnlinkSelf();
    mPtrBits = reinterpret_cast<uintptr_t>(aCallback);
    NS_IF_ADDREF(aCallback);
  }

  void operator=(XPCOMCallbackT* aCallback) {
    UnlinkSelf();
    mPtrBits = reinterpret_cast<uintptr_t>(aCallback) | XPCOMCallbackFlag;
    NS_IF_ADDREF(aCallback);
  }

  void operator=(CallbackObjectHolder&& aOther) {
    UnlinkSelf();
    mPtrBits = aOther.mPtrBits;
    aOther.mPtrBits = 0;
  }

  void operator=(const CallbackObjectHolder& aOther) = delete;

  void Reset() { UnlinkSelf(); }

  nsISupports* GetISupports() const {
    return reinterpret_cast<nsISupports*>(mPtrBits & ~XPCOMCallbackFlag);
  }

  already_AddRefed<nsISupports> Forget() {
    // This can be called from random threads.  Make sure to not refcount things
    // in here!
    nsISupports* supp = GetISupports();
    mPtrBits = 0;
    return dont_AddRef(supp);
  }

  // Boolean conversion operator so people can use this in boolean tests
  explicit operator bool() const { return GetISupports(); }

  CallbackObjectHolder Clone() const {
    CallbackObjectHolder result;
    result.mPtrBits = mPtrBits;
    NS_IF_ADDREF(GetISupports());
    return result;
  }

  // Even if HasWebIDLCallback returns true, GetWebIDLCallback() might still
  // return null.
  bool HasWebIDLCallback() const { return !(mPtrBits & XPCOMCallbackFlag); }

  WebIDLCallbackT* GetWebIDLCallback() const {
    MOZ_ASSERT(HasWebIDLCallback());
    return reinterpret_cast<WebIDLCallbackT*>(mPtrBits);
  }

  XPCOMCallbackT* GetXPCOMCallback() const {
    MOZ_ASSERT(!HasWebIDLCallback());
    return reinterpret_cast<XPCOMCallbackT*>(mPtrBits & ~XPCOMCallbackFlag);
  }

  bool operator==(WebIDLCallbackT* aOtherCallback) const {
    if (!aOtherCallback) {
      // If other is null, then we must be null to be equal.
      return !GetISupports();
    }

    if (!HasWebIDLCallback() || !GetWebIDLCallback()) {
      // If other is non-null, then we can't be equal if we have a
      // non-WebIDL callback or a null callback.
      return false;
    }

    return *GetWebIDLCallback() == *aOtherCallback;
  }

  bool operator==(XPCOMCallbackT* aOtherCallback) const {
    return (!aOtherCallback && !GetISupports()) ||
           (!HasWebIDLCallback() && GetXPCOMCallback() == aOtherCallback);
  }

  bool operator==(const CallbackObjectHolder& aOtherCallback) const {
    if (aOtherCallback.HasWebIDLCallback()) {
      return *this == aOtherCallback.GetWebIDLCallback();
    }

    return *this == aOtherCallback.GetXPCOMCallback();
  }

  // Try to return an XPCOMCallbackT version of this object.
  already_AddRefed<XPCOMCallbackT> ToXPCOMCallback() const {
    if (!HasWebIDLCallback()) {
      RefPtr<XPCOMCallbackT> callback = GetXPCOMCallback();
      return callback.forget();
    }

    nsCOMPtr<nsISupports> supp = CallbackObjectHolderBase::ToXPCOMCallback(
        GetWebIDLCallback(), NS_GET_TEMPLATE_IID(XPCOMCallbackT));
    if (supp) {
      // ToXPCOMCallback already did the right QI for us.
      return supp.forget().downcast<XPCOMCallbackT>();
    }
    return nullptr;
  }

  // Try to return a WebIDLCallbackT version of this object.
  already_AddRefed<WebIDLCallbackT> ToWebIDLCallback() const {
    if (HasWebIDLCallback()) {
      RefPtr<WebIDLCallbackT> callback = GetWebIDLCallback();
      return callback.forget();
    }
    return nullptr;
  }

 private:
  static const uintptr_t XPCOMCallbackFlag = 1u;

  friend void ImplCycleCollectionUnlink<WebIDLCallbackT, XPCOMCallbackT>(
      CallbackObjectHolder& aField);

  void UnlinkSelf() {
    // NS_IF_RELEASE because we might have been unlinked before
    nsISupports* ptr = GetISupports();
    // Clear mPtrBits before the release to prevent reentrance.
    mPtrBits = 0;
    NS_IF_RELEASE(ptr);
  }

  uintptr_t mPtrBits;
};

NS_DEFINE_STATIC_IID_ACCESSOR(CallbackObject, DOM_CALLBACKOBJECT_IID)

template <class T, class U>
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    CallbackObjectHolder<T, U>& aField, const char* aName,
    uint32_t aFlags = 0) {
  if (aField) {
    CycleCollectionNoteChild(aCallback, aField.GetISupports(), aName, aFlags);
  }
}

template <class T, class U>
void ImplCycleCollectionUnlink(CallbackObjectHolder<T, U>& aField) {
  aField.UnlinkSelf();
}

// T is expected to be a RefPtr or OwningNonNull around a CallbackObject
// subclass.  This class is used in bindings to safely handle Fast* callbacks;
// it ensures that the callback is traced, and that if something is holding onto
// the callback when we're done with it HoldJSObjects is called.
//
// Since we effectively hold a ref to a refcounted thing (like RefPtr or
// OwningNonNull), we are also MOZ_IS_SMARTPTR_TO_REFCOUNTED for static analysis
// purposes.
template <typename T>
class MOZ_RAII MOZ_IS_SMARTPTR_TO_REFCOUNTED RootedCallback
    : public JS::Rooted<T> {
 public:
  explicit RootedCallback(JSContext* cx) : JS::Rooted<T>(cx), mCx(cx) {}

  // We need a way to make assignment from pointers (how we're normally used)
  // work.
  template <typename S>
  void operator=(S* arg) {
    this->get().operator=(arg);
  }

  // But nullptr can't use the above template, because it doesn't know which S
  // to select.  So we need a special overload for nullptr.
  void operator=(decltype(nullptr) arg) { this->get().operator=(arg); }

  // Codegen relies on being able to do CallbackOrNull() and Callback() on us.
  JS::Handle<JSObject*> CallbackOrNull() const {
    return this->get()->CallbackOrNull();
  }

  JSObject* Callback(JSContext* aCx) const {
    return this->get()->Callback(aCx);
  }

  ~RootedCallback() {
    // Ensure that our callback starts holding on to its own JS objects as
    // needed.  We really do need to check that things are initialized even when
    // T is OwningNonNull, because we might be running before the OwningNonNull
    // ever got assigned to!
    if (IsInitialized(this->get())) {
      this->get()->FinishSlowJSInitIfMoreThanOneOwner(mCx);
    }
  }

 private:
  template <typename U>
  static bool IsInitialized(U& aArg);  // Not implemented

  template <typename U>
  static bool IsInitialized(RefPtr<U>& aRefPtr) {
    return aRefPtr;
  }

  template <typename U>
  static bool IsInitialized(OwningNonNull<U>& aOwningNonNull) {
    return aOwningNonNull.isInitialized();
  }

  JSContext* mCx;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_CallbackObject_h
