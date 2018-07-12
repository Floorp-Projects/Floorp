/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMRequest.h"

#include "DOMException.h"
#include "nsThreadUtils.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "jsfriendapi.h"
#include "nsContentUtils.h"

using mozilla::dom::AnyCallback;
using mozilla::dom::DOMException;
using mozilla::dom::DOMRequest;
using mozilla::dom::DOMRequestService;
using mozilla::dom::Promise;
using mozilla::dom::AutoJSAPI;
using mozilla::dom::RootingCx;

DOMRequest::DOMRequest(nsPIDOMWindowInner* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mResult(JS::UndefinedValue())
  , mDone(false)
{
}

DOMRequest::DOMRequest(nsIGlobalObject* aGlobal)
  : DOMEventTargetHelper(aGlobal)
  , mResult(JS::UndefinedValue())
  , mDone(false)
{
}

DOMRequest::~DOMRequest()
{
  mResult.setUndefined();
  mozilla::DropJSObjects(this);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DOMRequest,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mError)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromise)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DOMRequest,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mError)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPromise)
  tmp->mResult.setUndefined();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(DOMRequest,
                                               DOMEventTargetHelper)
  // Don't need NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER because
  // DOMEventTargetHelper does it for us.
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mResult)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMRequest)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(DOMRequest, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(DOMRequest, DOMEventTargetHelper)

/* virtual */ JSObject*
DOMRequest::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DOMRequest_Binding::Wrap(aCx, this, aGivenProto);
}

void
DOMRequest::FireSuccess(JS::Handle<JS::Value> aResult)
{
  NS_ASSERTION(!mDone, "mDone shouldn't have been set to true already!");
  NS_ASSERTION(!mError, "mError shouldn't have been set!");
  NS_ASSERTION(mResult.isUndefined(), "mResult shouldn't have been set!");

  mDone = true;
  if (aResult.isGCThing()) {
    RootResultVal();
  }
  mResult = aResult;

  FireEvent(NS_LITERAL_STRING("success"), false, false);

  if (mPromise) {
    mPromise->MaybeResolve(mResult);
  }
}

void
DOMRequest::FireError(const nsAString& aError)
{
  NS_ASSERTION(!mDone, "mDone shouldn't have been set to true already!");
  NS_ASSERTION(!mError, "mError shouldn't have been set!");
  NS_ASSERTION(mResult.isUndefined(), "mResult shouldn't have been set!");

  mDone = true;
  // XXX Error code chosen arbitrarily
  mError = DOMException::Create(NS_ERROR_DOM_UNKNOWN_ERR,
                                NS_ConvertUTF16toUTF8(aError));

  FireEvent(NS_LITERAL_STRING("error"), true, true);

  if (mPromise) {
    mPromise->MaybeRejectBrokenly(mError);
  }
}

void
DOMRequest::FireError(nsresult aError)
{
  NS_ASSERTION(!mDone, "mDone shouldn't have been set to true already!");
  NS_ASSERTION(!mError, "mError shouldn't have been set!");
  NS_ASSERTION(mResult.isUndefined(), "mResult shouldn't have been set!");

  mDone = true;
  mError = DOMException::Create(aError);

  FireEvent(NS_LITERAL_STRING("error"), true, true);

  if (mPromise) {
    mPromise->MaybeRejectBrokenly(mError);
  }
}

void
DOMRequest::FireDetailedError(DOMException& aError)
{
  NS_ASSERTION(!mDone, "mDone shouldn't have been set to true already!");
  NS_ASSERTION(!mError, "mError shouldn't have been set!");
  NS_ASSERTION(mResult.isUndefined(), "mResult shouldn't have been set!");

  mDone = true;
  mError = &aError;

  FireEvent(NS_LITERAL_STRING("error"), true, true);

  if (mPromise) {
    mPromise->MaybeRejectBrokenly(mError);
  }
}

void
DOMRequest::FireEvent(const nsAString& aType, bool aBubble, bool aCancelable)
{
  if (NS_FAILED(CheckInnerWindowCorrectness())) {
    return;
  }

  RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
  event->InitEvent(aType, aBubble, aCancelable);
  event->SetTrusted(true);

  DispatchEvent(*event);
}

void
DOMRequest::RootResultVal()
{
  mozilla::HoldJSObjects(this);
}

void
DOMRequest::Then(JSContext* aCx, AnyCallback* aResolveCallback,
                 AnyCallback* aRejectCallback,
                 JS::MutableHandle<JS::Value> aRetval,
                 mozilla::ErrorResult& aRv)
{
  if (!mPromise) {
    mPromise = Promise::Create(DOMEventTargetHelper::GetParentObject(), aRv);
    if (aRv.Failed()) {
      return;
    }
    if (mDone) {
      // Since we create mPromise lazily, it's possible that the DOMRequest object
      // has already fired its success/error event.  In that case we should
      // manually resolve/reject mPromise here.  mPromise will take care of
      // calling the callbacks on |promise| as needed.
      if (mError) {
        mPromise->MaybeRejectBrokenly(mError);
      } else {
        mPromise->MaybeResolve(mResult);
      }
    }
  }

  // Just use the global of the Promise itself as the callee global.
  JS::Rooted<JSObject*> global(aCx, mPromise->PromiseObj());
  global = JS::GetNonCCWObjectGlobal(global);
  mPromise->Then(aCx, global, aResolveCallback, aRejectCallback, aRetval, aRv);
}

NS_IMPL_ISUPPORTS(DOMRequestService, nsIDOMRequestService)

NS_IMETHODIMP
DOMRequestService::CreateRequest(mozIDOMWindow* aWindow,
                                 DOMRequest** aRequest)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_STATE(aWindow);
  auto* win = nsPIDOMWindowInner::From(aWindow);
  RefPtr<DOMRequest> req = new DOMRequest(win);
  req.forget(aRequest);

  return NS_OK;
}

NS_IMETHODIMP
DOMRequestService::FireSuccess(DOMRequest* aRequest,
                               JS::Handle<JS::Value> aResult)
{
  NS_ENSURE_STATE(aRequest);
  aRequest->FireSuccess(aResult);

  return NS_OK;
}

NS_IMETHODIMP
DOMRequestService::FireError(DOMRequest* aRequest,
                             const nsAString& aError)
{
  NS_ENSURE_STATE(aRequest);
  aRequest->FireError(aError);

  return NS_OK;
}

class FireSuccessAsyncTask : public mozilla::Runnable
{

  FireSuccessAsyncTask(DOMRequest* aRequest, const JS::Value& aResult)
    : mozilla::Runnable("FireSuccessAsyncTask")
    , mReq(aRequest)
    , mResult(RootingCx(), aResult)
  {
  }

public:

  // Due to the fact that initialization can fail during shutdown (since we
  // can't fetch a js context), set up an initiatization function to make sure
  // we can return the failure appropriately
  static nsresult
  Dispatch(DOMRequest* aRequest,
           const JS::Value& aResult)
  {
    RefPtr<FireSuccessAsyncTask> asyncTask =
      new FireSuccessAsyncTask(aRequest, aResult);
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(asyncTask));
    return NS_OK;
  }

  NS_IMETHOD
  Run() override
  {
    mReq->FireSuccess(JS::Handle<JS::Value>::fromMarkedLocation(mResult.address()));
    return NS_OK;
  }

private:
  RefPtr<DOMRequest> mReq;
  JS::PersistentRooted<JS::Value> mResult;
};

class FireErrorAsyncTask : public mozilla::Runnable
{
public:
  FireErrorAsyncTask(DOMRequest* aRequest, const nsAString& aError)
    : mozilla::Runnable("FireErrorAsyncTask")
    , mReq(aRequest)
    , mError(aError)
  {
  }

  NS_IMETHOD
  Run() override
  {
    mReq->FireError(mError);
    return NS_OK;
  }
private:
  RefPtr<DOMRequest> mReq;
  nsString mError;
};

NS_IMETHODIMP
DOMRequestService::FireSuccessAsync(DOMRequest* aRequest,
                                    JS::Handle<JS::Value> aResult)
{
  NS_ENSURE_STATE(aRequest);
  return FireSuccessAsyncTask::Dispatch(aRequest, aResult);
}

NS_IMETHODIMP
DOMRequestService::FireErrorAsync(DOMRequest* aRequest,
                                  const nsAString& aError)
{
  NS_ENSURE_STATE(aRequest);
  nsCOMPtr<nsIRunnable> asyncTask = new FireErrorAsyncTask(aRequest, aError);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(asyncTask));
  return NS_OK;
}
