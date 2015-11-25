/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Promise_h
#define mozilla_dom_Promise_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/WeakPtr.h"
#include "nsWrapperCache.h"
#include "nsAutoPtr.h"
#include "js/TypeDecls.h"
#include "jspubtd.h"

// Bug 1083361 introduces a new mechanism for tracking uncaught
// rejections. This #define serves to track down the parts of code
// that need to be removed once clients have been put together
// to take advantage of the new mechanism. New code should not
// depend on code #ifdefed to this #define.
#define DOM_PROMISE_DEPRECATED_REPORTING 1

#if defined(DOM_PROMISE_DEPRECATED_REPORTING)
#include "mozilla/dom/workers/bindings/WorkerFeature.h"
#endif // defined(DOM_PROMISE_DEPRECATED_REPORTING)

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class AnyCallback;
class DOMError;
class MediaStreamError;
class PromiseCallback;
class PromiseInit;
class PromiseNativeHandler;
class PromiseDebugging;

class Promise;

#if defined(DOM_PROMISE_DEPRECATED_REPORTING)
class PromiseReportRejectFeature : public workers::WorkerFeature
{
  // PromiseReportRejectFeature is held by an nsAutoPtr on the Promise which
  // means that this object will be destroyed before the Promise is destroyed.
  Promise* MOZ_NON_OWNING_REF mPromise;

public:
  explicit PromiseReportRejectFeature(Promise* aPromise)
    : mPromise(aPromise)
  {
    MOZ_ASSERT(mPromise);
  }

  virtual bool
  Notify(JSContext* aCx, workers::Status aStatus) override;
};
#endif // defined(DOM_PROMISE_DEPRECATED_REPORTING)

#define NS_PROMISE_IID \
  { 0x1b8d6215, 0x3e67, 0x43ba, \
    { 0x8a, 0xf9, 0x31, 0x5e, 0x8f, 0xce, 0x75, 0x65 } }

class Promise : public nsISupports,
                public nsWrapperCache,
                public SupportsWeakPtr<Promise>
{
  friend class NativePromiseCallback;
  friend class PromiseReactionJob;
  friend class PromiseResolverTask;
  friend class PromiseTask;
#if defined(DOM_PROMISE_DEPRECATED_REPORTING)
  friend class PromiseReportRejectFeature;
#endif // defined(DOM_PROMISE_DEPRECATED_REPORTING)
  friend class PromiseWorkerProxy;
  friend class PromiseWorkerProxyRunnable;
  friend class RejectPromiseCallback;
  friend class ResolvePromiseCallback;
  friend class PromiseResolveThenableJob;
  friend class FastPromiseResolveThenableJob;
  friend class WrapperPromiseCallback;

public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_PROMISE_IID)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(Promise)
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(Promise)

  // Promise creation tries to create a JS reflector for the Promise, so is
  // fallible.  Furthermore, we don't want to do JS-wrapping on a 0-refcount
  // object, so we addref before doing that and return the addrefed pointer
  // here.
  static already_AddRefed<Promise>
  Create(nsIGlobalObject* aGlobal, ErrorResult& aRv,
         // Passing null for aDesiredProto will use Promise.prototype.
         JS::Handle<JSObject*> aDesiredProto = nullptr);

  typedef void (Promise::*MaybeFunc)(JSContext* aCx,
                                     JS::Handle<JS::Value> aValue);

  void MaybeResolve(JSContext* aCx,
                    JS::Handle<JS::Value> aValue);
  void MaybeReject(JSContext* aCx,
                   JS::Handle<JS::Value> aValue);

  // Helpers for using Promise from C++.
  // Most DOM objects are handled already.  To add a new type T, add a
  // ToJSValue overload in ToJSValue.h.
  // aArg is a const reference so we can pass rvalues like integer constants
  template <typename T>
  void MaybeResolve(const T& aArg) {
    MaybeSomething(aArg, &Promise::MaybeResolve);
  }

  inline void MaybeReject(nsresult aArg) {
    MOZ_ASSERT(NS_FAILED(aArg));
    MaybeSomething(aArg, &Promise::MaybeReject);
  }

  inline void MaybeReject(ErrorResult& aArg) {
    MOZ_ASSERT(aArg.Failed());
    MaybeSomething(aArg, &Promise::MaybeReject);
  }

  void MaybeReject(const RefPtr<MediaStreamError>& aArg);

  // DO NOT USE MaybeRejectBrokenly with in new code.  Promises should be
  // rejected with Error instances.
  // Note: MaybeRejectBrokenly is a template so we can use it with DOMError
  // without instantiating the DOMError specialization of MaybeSomething in
  // every translation unit that includes this header, because that would
  // require use to include DOMError.h either here or in all those translation
  // units.
  template<typename T>
  void MaybeRejectBrokenly(const T& aArg); // Not implemented by default; see
                                           // specializations in the .cpp for
                                           // the T values we support.

  // Called by DOM to let us execute our callbacks.  May be called recursively.
  // Returns true if at least one microtask was processed.
  static bool PerformMicroTaskCheckpoint();

  // WebIDL

  nsIGlobalObject* GetParentObject() const
  {
    return mGlobal;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<Promise>
  Constructor(const GlobalObject& aGlobal, PromiseInit& aInit,
              ErrorResult& aRv, JS::Handle<JSObject*> aDesiredProto);

  static void
  Resolve(const GlobalObject& aGlobal, JS::Handle<JS::Value> aThisv,
          JS::Handle<JS::Value> aValue,
          JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv);

  static already_AddRefed<Promise>
  Resolve(nsIGlobalObject* aGlobal, JSContext* aCx,
          JS::Handle<JS::Value> aValue, ErrorResult& aRv);

  static already_AddRefed<Promise>
  Reject(const GlobalObject& aGlobal, JS::Handle<JS::Value> aThisv,
         JS::Handle<JS::Value> aValue, ErrorResult& aRv);

  static already_AddRefed<Promise>
  Reject(nsIGlobalObject* aGlobal, JSContext* aCx,
         JS::Handle<JS::Value> aValue, ErrorResult& aRv);

  already_AddRefed<Promise>
  Then(JSContext* aCx, AnyCallback* aResolveCallback,
       AnyCallback* aRejectCallback, ErrorResult& aRv);

  already_AddRefed<Promise>
  Catch(JSContext* aCx, AnyCallback* aRejectCallback, ErrorResult& aRv);

  static void
  All(const GlobalObject& aGlobal, JS::Handle<JS::Value> aThisv,
      JS::Handle<JS::Value> aIterable, JS::MutableHandle<JS::Value> aRetval,
      ErrorResult& aRv);

  static already_AddRefed<Promise>
  All(const GlobalObject& aGlobal,
      const nsTArray<RefPtr<Promise>>& aPromiseList, ErrorResult& aRv);

  static void
  Race(const GlobalObject& aGlobal, JS::Handle<JS::Value> aThisv,
       JS::Handle<JS::Value> aIterable, JS::MutableHandle<JS::Value> aRetval,
       ErrorResult& aRv);

  static bool
  PromiseSpecies(JSContext* aCx, unsigned aArgc, JS::Value* aVp);

  void AppendNativeHandler(PromiseNativeHandler* aRunnable);

  JSObject* GlobalJSObject() const;

  JSCompartment* Compartment() const;

  // Return a unique-to-the-process identifier for this Promise.
  uint64_t GetID();

  // Queue an async microtask to current main or worker thread.
  static void
  DispatchToMicroTask(nsIRunnable* aRunnable);

  enum JSCallbackSlots {
    SLOT_PROMISE = 0,
    SLOT_DATA
  };

protected:
  struct PromiseCapability;

  // Do NOT call this unless you're Promise::Create.  I wish we could enforce
  // that from inside this class too, somehow.
  explicit Promise(nsIGlobalObject* aGlobal);

  virtual ~Promise();

  // Do JS-wrapping after Promise creation.  Passing null for aDesiredProto will
  // use the default prototype for the sort of Promise we have.
  void CreateWrapper(JS::Handle<JSObject*> aDesiredProto, ErrorResult& aRv);

  // Create the JS resolving functions of resolve() and reject(). And provide
  // references to the two functions by calling PromiseInit passed from Promise
  // constructor.
  void CallInitFunction(const GlobalObject& aGlobal, PromiseInit& aInit,
                        ErrorResult& aRv);

  // The NewPromiseCapability function from
  // <http://www.ecma-international.org/ecma-262/6.0/#sec-newpromisecapability>.
  // Errors are communicated via aRv.  If aForceCallbackCreation is
  // true, then this function will ensure that aCapability has a
  // useful mResolve/mReject even if mNativePromise is non-null.
  static void NewPromiseCapability(JSContext* aCx, nsIGlobalObject* aGlobal,
                                   JS::Handle<JS::Value> aConstructor,
                                   bool aForceCallbackCreation,
                                   PromiseCapability& aCapability,
                                   ErrorResult& aRv);

  bool IsPending()
  {
    return mResolvePending;
  }

  void GetDependentPromises(nsTArray<RefPtr<Promise>>& aPromises);

  bool IsLastInChain() const
  {
    return mIsLastInChain;
  }

  void SetNotifiedAsUncaught()
  {
    mWasNotifiedAsUncaught = true;
  }

  bool WasNotifiedAsUncaught() const
  {
    return mWasNotifiedAsUncaught;
  }

private:
  friend class PromiseDebugging;

  enum PromiseState {
    Pending,
    Resolved,
    Rejected
  };

  void SetState(PromiseState aState)
  {
    MOZ_ASSERT(mState == Pending);
    MOZ_ASSERT(aState != Pending);
    mState = aState;
  }

  void SetResult(JS::Handle<JS::Value> aValue)
  {
    mResult = aValue;
  }

  // This method enqueues promise's resolve/reject callbacks with promise's
  // result. It's executed when the resolver.resolve() or resolver.reject() is
  // called or when the promise already has a result and new callbacks are
  // appended by then() or catch().
  void TriggerPromiseReactions();

  void Settle(JS::Handle<JS::Value> aValue, Promise::PromiseState aState);
  void MaybeSettle(JS::Handle<JS::Value> aValue, Promise::PromiseState aState);

  void AppendCallbacks(PromiseCallback* aResolveCallback,
                       PromiseCallback* aRejectCallback);

#if defined(DOM_PROMISE_DEPRECATED_REPORTING)
  // If we have been rejected and our mResult is a JS exception,
  // report it to the error console.
  // Use MaybeReportRejectedOnce() for actual calls.
  void MaybeReportRejected();

  void MaybeReportRejectedOnce() {
    MaybeReportRejected();
    RemoveFeature();
    mResult.setUndefined();
  }
#endif // defined(DOM_PROMISE_DEPRECATED_REPORTING)

  void MaybeResolveInternal(JSContext* aCx,
                            JS::Handle<JS::Value> aValue);
  void MaybeRejectInternal(JSContext* aCx,
                           JS::Handle<JS::Value> aValue);

  void ResolveInternal(JSContext* aCx,
                       JS::Handle<JS::Value> aValue);
  void RejectInternal(JSContext* aCx,
                      JS::Handle<JS::Value> aValue);

  template <typename T>
  void MaybeSomething(T& aArgument, MaybeFunc aFunc) {
    ThreadsafeAutoJSContext cx;
    JSObject* wrapper = GetWrapper();
    MOZ_ASSERT(wrapper); // We preserved it!

    JSAutoCompartment ac(cx, wrapper);
    JS::Rooted<JS::Value> val(cx);
    if (!ToJSValue(cx, aArgument, &val)) {
      HandleException(cx);
      return;
    }

    (this->*aFunc)(cx, val);
  }

  // Static methods for the PromiseInit functions.
  static bool
  JSCallback(JSContext *aCx, unsigned aArgc, JS::Value *aVp);

  static bool
  ThenableResolverCommon(JSContext* aCx, uint32_t /* PromiseCallback::Task */ aTask,
                         unsigned aArgc, JS::Value* aVp);
  static bool
  JSCallbackThenableResolver(JSContext *aCx, unsigned aArgc, JS::Value *aVp);
  static bool
  JSCallbackThenableRejecter(JSContext *aCx, unsigned aArgc, JS::Value *aVp);

  static JSObject*
  CreateFunction(JSContext* aCx, Promise* aPromise, int32_t aTask);

  static JSObject*
  CreateThenableFunction(JSContext* aCx, Promise* aPromise, uint32_t aTask);

  void HandleException(JSContext* aCx);

#if defined(DOM_PROMISE_DEPRECATED_REPORTING)
  void RemoveFeature();
#endif // defined(DOM_PROMISE_DEPRECATED_REPORTING)

  // Capture the current stack and store it in aTarget.  If false is
  // returned, an exception is presumably pending on aCx.
  bool CaptureStack(JSContext* aCx, JS::Heap<JSObject*>& aTarget);

  RefPtr<nsIGlobalObject> mGlobal;

  nsTArray<RefPtr<PromiseCallback> > mResolveCallbacks;
  nsTArray<RefPtr<PromiseCallback> > mRejectCallbacks;

  JS::Heap<JS::Value> mResult;
  // A stack that shows where this promise was allocated, if there was
  // JS running at the time.  Otherwise null.
  JS::Heap<JSObject*> mAllocationStack;
  // mRejectionStack is only set when the promise is rejected directly from
  // script, by calling Promise.reject() or the rejection callback we pass to
  // the PromiseInit function.  Promises that are rejected internally do not
  // have a rejection stack.
  JS::Heap<JSObject*> mRejectionStack;
  // mFullfillmentStack is only set when the promise is fulfilled directly from
  // script, by calling Promise.resolve() or the fulfillment callback we pass to
  // the PromiseInit function.  Promises that are fulfilled internally do not
  // have a fulfillment stack.
  JS::Heap<JSObject*> mFullfillmentStack;
  PromiseState mState;

#if defined(DOM_PROMISE_DEPRECATED_REPORTING)
  bool mHadRejectCallback;

  // If a rejected promise on a worker has no reject callbacks attached, it
  // needs to know when the worker is shutting down, to report the error on the
  // console before the worker's context is deleted. This feature is used for
  // that purpose.
  nsAutoPtr<PromiseReportRejectFeature> mFeature;
#endif // defined(DOM_PROMISE_DEPRECATED_REPORTING)

  bool mTaskPending;
  bool mResolvePending;

  // `true` if this Promise is the last in the chain, or `false` if
  // another Promise has been created from this one by a call to
  // `then`, `all`, `race`, etc.
  bool mIsLastInChain;

  // `true` if PromiseDebugging has already notified at least one observer that
  // this promise was left uncaught, `false` otherwise.
  bool mWasNotifiedAsUncaught;

  // The time when this promise was created.
  TimeStamp mCreationTimestamp;

  // The time when this promise transitioned out of the pending state.
  TimeStamp mSettlementTimestamp;

  // Once `GetID()` has been called, a unique-to-the-process identifier for this
  // promise. Until then, `0`.
  uint64_t mID;
};

NS_DEFINE_STATIC_IID_ACCESSOR(Promise, NS_PROMISE_IID)

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Promise_h
