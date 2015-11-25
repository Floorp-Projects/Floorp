/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMRequest.h"

#include "DOMError.h"
#include "nsThreadUtils.h"
#include "DOMCursor.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "jsfriendapi.h"

using mozilla::dom::AnyCallback;
using mozilla::dom::DOMError;
using mozilla::dom::DOMRequest;
using mozilla::dom::DOMRequestService;
using mozilla::dom::DOMCursor;
using mozilla::dom::Promise;
using mozilla::dom::AutoJSAPI;

DOMRequest::DOMRequest(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow->IsInnerWindow() ?
                           aWindow : aWindow->GetCurrentInnerWindow())
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
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mResult)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DOMRequest)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMRequest)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(DOMRequest, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(DOMRequest, DOMEventTargetHelper)

/* virtual */ JSObject*
DOMRequest::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DOMRequestBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_EVENT_HANDLER(DOMRequest, success)
NS_IMPL_EVENT_HANDLER(DOMRequest, error)

NS_IMETHODIMP
DOMRequest::GetReadyState(nsAString& aReadyState)
{
  DOMRequestReadyState readyState = ReadyState();
  switch (readyState) {
    case DOMRequestReadyState::Pending:
      aReadyState.AssignLiteral("pending");
      break;
    case DOMRequestReadyState::Done:
      aReadyState.AssignLiteral("done");
      break;
    default:
      MOZ_CRASH("Unrecognized readyState.");
  }

  return NS_OK;
}

NS_IMETHODIMP
DOMRequest::GetResult(JS::MutableHandle<JS::Value> aResult)
{
  GetResult(nullptr, aResult);
  return NS_OK;
}

NS_IMETHODIMP
DOMRequest::GetError(nsISupports** aError)
{
  NS_IF_ADDREF(*aError = GetError());
  return NS_OK;
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
  mError = new DOMError(GetOwner(), aError);

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
  mError = new DOMError(GetOwner(), aError);

  FireEvent(NS_LITERAL_STRING("error"), true, true);

  if (mPromise) {
    mPromise->MaybeRejectBrokenly(mError);
  }
}

void
DOMRequest::FireDetailedError(DOMError* aError)
{
  NS_ASSERTION(!mDone, "mDone shouldn't have been set to true already!");
  NS_ASSERTION(!mError, "mError shouldn't have been set!");
  NS_ASSERTION(mResult.isUndefined(), "mResult shouldn't have been set!");
  NS_ASSERTION(aError, "No detailed error provided");

  mDone = true;
  mError = aError;

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

  bool dummy;
  DispatchEvent(event, &dummy);
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
  JS::Rooted<JSObject*> global(aCx, mPromise->GetWrapper());
  global = js::GetGlobalForObjectCrossCompartment(global);
  mPromise->Then(aCx, global, aResolveCallback, aRejectCallback, aRetval, aRv);
}

NS_IMPL_ISUPPORTS(DOMRequestService, nsIDOMRequestService)

NS_IMETHODIMP
DOMRequestService::CreateRequest(nsIDOMWindow* aWindow,
                                 nsIDOMDOMRequest** aRequest)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(aWindow));
  NS_ENSURE_STATE(win);
  NS_ADDREF(*aRequest = new DOMRequest(win));

  return NS_OK;
}

NS_IMETHODIMP
DOMRequestService::CreateCursor(nsIDOMWindow* aWindow,
                                nsICursorContinueCallback* aCallback,
                                nsIDOMDOMCursor** aCursor)
{
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(aWindow));
  NS_ENSURE_STATE(win);
  NS_ADDREF(*aCursor = new DOMCursor(win, aCallback));

  return NS_OK;
}

NS_IMETHODIMP
DOMRequestService::FireSuccess(nsIDOMDOMRequest* aRequest,
                               JS::Handle<JS::Value> aResult)
{
  NS_ENSURE_STATE(aRequest);
  static_cast<DOMRequest*>(aRequest)->FireSuccess(aResult);

  return NS_OK;
}

NS_IMETHODIMP
DOMRequestService::FireError(nsIDOMDOMRequest* aRequest,
                             const nsAString& aError)
{
  NS_ENSURE_STATE(aRequest);
  static_cast<DOMRequest*>(aRequest)->FireError(aError);

  return NS_OK;
}

NS_IMETHODIMP
DOMRequestService::FireDetailedError(nsIDOMDOMRequest* aRequest,
                                     nsISupports* aError)
{
  NS_ENSURE_STATE(aRequest);
  nsCOMPtr<DOMError> err = do_QueryInterface(aError);
  NS_ENSURE_STATE(err);
  static_cast<DOMRequest*>(aRequest)->FireDetailedError(err);

  return NS_OK;
}

class FireSuccessAsyncTask : public nsRunnable
{

  FireSuccessAsyncTask(JSContext* aCx,
                       DOMRequest* aRequest,
                       const JS::Value& aResult) :
    mReq(aRequest),
    mResult(aCx, aResult)
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
    mozilla::ThreadsafeAutoSafeJSContext cx;
    RefPtr<FireSuccessAsyncTask> asyncTask = new FireSuccessAsyncTask(cx, aRequest, aResult);
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToCurrentThread(asyncTask)));
    return NS_OK;
  }

  NS_IMETHODIMP
  Run()
  {
    mReq->FireSuccess(JS::Handle<JS::Value>::fromMarkedLocation(mResult.address()));
    return NS_OK;
  }

private:
  RefPtr<DOMRequest> mReq;
  JS::PersistentRooted<JS::Value> mResult;
};

class FireErrorAsyncTask : public nsRunnable
{
public:
  FireErrorAsyncTask(DOMRequest* aRequest,
                     const nsAString& aError) :
    mReq(aRequest),
    mError(aError)
  {
  }

  NS_IMETHODIMP
  Run()
  {
    mReq->FireError(mError);
    return NS_OK;
  }
private:
  RefPtr<DOMRequest> mReq;
  nsString mError;
};

NS_IMETHODIMP
DOMRequestService::FireSuccessAsync(nsIDOMDOMRequest* aRequest,
                                    JS::Handle<JS::Value> aResult)
{
  NS_ENSURE_STATE(aRequest);
  return FireSuccessAsyncTask::Dispatch(static_cast<DOMRequest*>(aRequest), aResult);
}

NS_IMETHODIMP
DOMRequestService::FireErrorAsync(nsIDOMDOMRequest* aRequest,
                                  const nsAString& aError)
{
  NS_ENSURE_STATE(aRequest);
  nsCOMPtr<nsIRunnable> asyncTask =
    new FireErrorAsyncTask(static_cast<DOMRequest*>(aRequest), aError);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToCurrentThread(asyncTask)));
  return NS_OK;
}

NS_IMETHODIMP
DOMRequestService::FireDone(nsIDOMDOMCursor* aCursor) {
  NS_ENSURE_STATE(aCursor);
  static_cast<DOMCursor*>(aCursor)->FireDone();

  return NS_OK;
}
