/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Promise_h
#define mozilla_dom_Promise_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/WeakPtr.h"
#include "nsWrapperCache.h"
#include "nsAutoPtr.h"
#include "js/TypeDecls.h"

#include "mozilla/dom/workers/bindings/WorkerFeature.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class AnyCallback;
class DOMError;
class PromiseCallback;
class PromiseInit;
class PromiseNativeHandler;
class PromiseDebugging;

class Promise;
class PromiseReportRejectFeature : public workers::WorkerFeature
{
  // The Promise that owns this feature.
  Promise* mPromise;

public:
  explicit PromiseReportRejectFeature(Promise* aPromise)
    : mPromise(aPromise)
  {
    MOZ_ASSERT(mPromise);
  }

  virtual bool
  Notify(JSContext* aCx, workers::Status aStatus) MOZ_OVERRIDE;
};

class Promise : public nsISupports,
                public nsWrapperCache,
                public SupportsWeakPtr<Promise>
{
  friend class NativePromiseCallback;
  friend class PromiseResolverTask;
  friend class PromiseTask;
  friend class PromiseReportRejectFeature;
  friend class PromiseWorkerProxy;
  friend class PromiseWorkerProxyRunnable;
  friend class RejectPromiseCallback;
  friend class ResolvePromiseCallback;
  friend class ThenableResolverTask;
  friend class WrapperPromiseCallback;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Promise)
  MOZ_DECLARE_REFCOUNTED_TYPENAME(Promise)

  // Promise creation tries to create a JS reflector for the Promise, so is
  // fallible.  Furthermore, we don't want to do JS-wrapping on a 0-refcount
  // object, so we addref before doing that and return the addrefed pointer
  // here.
  static already_AddRefed<Promise>
  Create(nsIGlobalObject* aGlobal, ErrorResult& aRv);

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

  // WebIDL

  nsIGlobalObject* GetParentObject() const
  {
    return mGlobal;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  static already_AddRefed<Promise>
  Constructor(const GlobalObject& aGlobal, PromiseInit& aInit,
              ErrorResult& aRv);

  static already_AddRefed<Promise>
  Resolve(const GlobalObject& aGlobal,
          JS::Handle<JS::Value> aValue, ErrorResult& aRv);

  static already_AddRefed<Promise>
  Resolve(nsIGlobalObject* aGlobal, JSContext* aCx,
          JS::Handle<JS::Value> aValue, ErrorResult& aRv);

  static already_AddRefed<Promise>
  Reject(const GlobalObject& aGlobal,
         JS::Handle<JS::Value> aValue, ErrorResult& aRv);

  static already_AddRefed<Promise>
  Reject(nsIGlobalObject* aGlobal, JSContext* aCx,
         JS::Handle<JS::Value> aValue, ErrorResult& aRv);

  already_AddRefed<Promise>
  Then(JSContext* aCx, AnyCallback* aResolveCallback,
       AnyCallback* aRejectCallback, ErrorResult& aRv);

  already_AddRefed<Promise>
  Catch(JSContext* aCx, AnyCallback* aRejectCallback, ErrorResult& aRv);

  static already_AddRefed<Promise>
  All(const GlobalObject& aGlobal,
      const Sequence<JS::Value>& aIterable, ErrorResult& aRv);

  static already_AddRefed<Promise>
  Race(const GlobalObject& aGlobal,
       const Sequence<JS::Value>& aIterable, ErrorResult& aRv);

  void AppendNativeHandler(PromiseNativeHandler* aRunnable);

protected:
  // Do NOT call this unless you're Promise::Create.  I wish we could enforce
  // that from inside this class too, somehow.
  explicit Promise(nsIGlobalObject* aGlobal);

  virtual ~Promise();

  // Queue an async task to current main or worker thread.
  static void
  DispatchToMainOrWorkerThread(nsIRunnable* aRunnable);

  // Do JS-wrapping after Promise creation.
  void CreateWrapper(ErrorResult& aRv);

  // Create the JS resolving functions of resolve() and reject(). And provide
  // references to the two functions by calling PromiseInit passed from Promise
  // constructor.
  void CallInitFunction(const GlobalObject& aGlobal, PromiseInit& aInit,
                        ErrorResult& aRv);

  bool IsPending()
  {
    return mResolvePending;
  }

  void GetDependentPromises(nsTArray<nsRefPtr<Promise>>& aPromises);

private:
  friend class PromiseDebugging;

  enum PromiseState {
    Pending,
    Resolved,
    Rejected
  };

  enum PromiseTaskSync {
    SyncTask,
    AsyncTask
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

  // This method processes promise's resolve/reject callbacks with promise's
  // result. It's executed when the resolver.resolve() or resolver.reject() is
  // called or when the promise already has a result and new callbacks are
  // appended by then(), catch() or done().
  void RunTask();

  void RunResolveTask(JS::Handle<JS::Value> aValue,
                      Promise::PromiseState aState,
                      PromiseTaskSync aAsynchronous);

  void AppendCallbacks(PromiseCallback* aResolveCallback,
                       PromiseCallback* aRejectCallback);

  // If we have been rejected and our mResult is a JS exception,
  // report it to the error console.
  // Use MaybeReportRejectedOnce() for actual calls.
  void MaybeReportRejected();

  void MaybeReportRejectedOnce() {
    MaybeReportRejected();
    RemoveFeature();
    mResult = JS::UndefinedValue();
  }

  void MaybeResolveInternal(JSContext* aCx,
                            JS::Handle<JS::Value> aValue,
                            PromiseTaskSync aSync = AsyncTask);
  void MaybeRejectInternal(JSContext* aCx,
                           JS::Handle<JS::Value> aValue,
                           PromiseTaskSync aSync = AsyncTask);

  void ResolveInternal(JSContext* aCx,
                       JS::Handle<JS::Value> aValue,
                       PromiseTaskSync aSync = AsyncTask);

  void RejectInternal(JSContext* aCx,
                      JS::Handle<JS::Value> aValue,
                      PromiseTaskSync aSync = AsyncTask);

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
  CreateFunction(JSContext* aCx, JSObject* aParent, Promise* aPromise,
                int32_t aTask);

  static JSObject*
  CreateThenableFunction(JSContext* aCx, Promise* aPromise, uint32_t aTask);

  void HandleException(JSContext* aCx);

  void RemoveFeature();

  // Capture the current stack and store it in aTarget.  If false is
  // returned, an exception is presumably pending on aCx.
  bool CaptureStack(JSContext* aCx, JS::Heap<JSObject*>& aTarget);

  nsRefPtr<nsIGlobalObject> mGlobal;

  nsTArray<nsRefPtr<PromiseCallback> > mResolveCallbacks;
  nsTArray<nsRefPtr<PromiseCallback> > mRejectCallbacks;

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
  bool mTaskPending;
  bool mHadRejectCallback;

  bool mResolvePending;

  // If a rejected promise on a worker has no reject callbacks attached, it
  // needs to know when the worker is shutting down, to report the error on the
  // console before the worker's context is deleted. This feature is used for
  // that purpose.
  nsAutoPtr<PromiseReportRejectFeature> mFeature;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Promise_h
