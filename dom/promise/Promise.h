/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Promise_h
#define mozilla_dom_Promise_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/PromiseBinding.h"
#include "nsWrapperCache.h"
#include "nsAutoPtr.h"
#include "nsPIDOMWindow.h"
#include "js/TypeDecls.h"

namespace mozilla {
namespace dom {

class AnyCallback;
class PromiseCallback;
class PromiseInit;
class PromiseNativeHandler;

class Promise MOZ_FINAL : public nsISupports,
                          public nsWrapperCache
{
  friend class NativePromiseCallback;
  friend class PromiseResolverMixin;
  friend class PromiseResolverTask;
  friend class PromiseTask;
  friend class RejectPromiseCallback;
  friend class ResolvePromiseCallback;
  friend class WorkerPromiseResolverTask;
  friend class WorkerPromiseTask;
  friend class WrapperPromiseCallback;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Promise)

  Promise(nsPIDOMWindow* aWindow);
  ~Promise();

  void MaybeResolve(JSContext* aCx,
                    JS::Handle<JS::Value> aValue);
  void MaybeReject(JSContext* aCx,
                   JS::Handle<JS::Value> aValue);

  // WebIDL

  nsPIDOMWindow* GetParentObject() const
  {
    return mWindow;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  static already_AddRefed<Promise>
  Constructor(const GlobalObject& aGlobal, PromiseInit& aInit,
              ErrorResult& aRv);

  static already_AddRefed<Promise>
  Resolve(const GlobalObject& aGlobal, JSContext* aCx,
          const Optional<JS::Handle<JS::Value>>& aValue, ErrorResult& aRv);

  static already_AddRefed<Promise>
  Resolve(nsPIDOMWindow* aWindow, JSContext* aCx,
          JS::Handle<JS::Value> aValue, ErrorResult& aRv);

  static already_AddRefed<Promise>
  Reject(const GlobalObject& aGlobal, JSContext* aCx,
         const Optional<JS::Handle<JS::Value>>& aValue, ErrorResult& aRv);

  static already_AddRefed<Promise>
  Reject(nsPIDOMWindow* aWindow, JSContext* aCx,
         JS::Handle<JS::Value> aValue, ErrorResult& aRv);

  already_AddRefed<Promise>
  Then(AnyCallback* aResolveCallback, AnyCallback* aRejectCallback);

  already_AddRefed<Promise>
  Catch(AnyCallback* aRejectCallback);

  // FIXME(nsm): Bug 956197
  static already_AddRefed<Promise>
  All(const GlobalObject& aGlobal, JSContext* aCx,
      const Sequence<JS::Value>& aIterable, ErrorResult& aRv);

  // FIXME(nsm): Bug 956197
  static already_AddRefed<Promise>
  Race(const GlobalObject& aGlobal, JSContext* aCx,
       const Sequence<JS::Value>& aIterable, ErrorResult& aRv);

  void AppendNativeHandler(PromiseNativeHandler* aRunnable);

private:
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
  void MaybeReportRejected();

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

  nsRefPtr<nsPIDOMWindow> mWindow;

  nsTArray<nsRefPtr<PromiseCallback> > mResolveCallbacks;
  nsTArray<nsRefPtr<PromiseCallback> > mRejectCallbacks;

  JS::Heap<JS::Value> mResult;
  PromiseState mState;
  bool mTaskPending;
  bool mHadRejectCallback;

  bool mResolvePending;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Promise_h
