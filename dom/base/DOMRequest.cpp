/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMRequest.h"

#include "mozilla/Util.h"
#include "DOMError.h"
#include "nsEventDispatcher.h"
#include "nsDOMEvent.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "DOMCursor.h"

using mozilla::dom::DOMRequest;
using mozilla::dom::DOMRequestService;
using mozilla::dom::DOMCursor;
using mozilla::AutoPushJSContext;

DOMRequest::DOMRequest(nsIDOMWindow* aWindow)
  : mResult(JSVAL_VOID)
  , mDone(false)
  , mRooted(false)
{
  SetIsDOMBinding();
  Init(aWindow);
}

// We need this constructor for dom::Activity that inherits from DOMRequest
// but has no window available from the constructor.
DOMRequest::DOMRequest()
  : mResult(JSVAL_VOID)
  , mDone(false)
  , mRooted(false)
{
  SetIsDOMBinding();
}

void
DOMRequest::Init(nsIDOMWindow* aWindow)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  BindToOwner(window->IsInnerWindow() ? window.get() :
                                        window->GetCurrentInnerWindow());
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DOMRequest,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mError)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DOMRequest,
                                                nsDOMEventTargetHelper)
  if (tmp->mRooted) {
    tmp->UnrootResultVal();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mError)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(DOMRequest,
                                               nsDOMEventTargetHelper)
  // Don't need NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER because
  // nsDOMEventTargetHelper does it for us.
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mResult)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DOMRequest)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMRequest)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(DOMRequest, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(DOMRequest, nsDOMEventTargetHelper)

/* virtual */ JSObject*
DOMRequest::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return DOMRequestBinding::Wrap(aCx, aScope, this);
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
      MOZ_NOT_REACHED("Unrecognized readyState.");
  }

  return NS_OK;
}

NS_IMETHODIMP
DOMRequest::GetResult(JS::Value* aResult)
{
  *aResult = Result();
  return NS_OK;
}

NS_IMETHODIMP
DOMRequest::GetError(nsIDOMDOMError** aError)
{
  NS_IF_ADDREF(*aError = GetError());
  return NS_OK;
}

void
DOMRequest::FireSuccess(JS::Value aResult)
{
  NS_ASSERTION(!mDone, "mDone shouldn't have been set to true already!");
  NS_ASSERTION(!mError, "mError shouldn't have been set!");
  NS_ASSERTION(mResult == JSVAL_VOID, "mResult shouldn't have been set!");

  mDone = true;
  if (JSVAL_IS_GCTHING(aResult)) {
    RootResultVal();
  }
  mResult = aResult;

  FireEvent(NS_LITERAL_STRING("success"), false, false);
}

void
DOMRequest::FireError(const nsAString& aError)
{
  NS_ASSERTION(!mDone, "mDone shouldn't have been set to true already!");
  NS_ASSERTION(!mError, "mError shouldn't have been set!");
  NS_ASSERTION(mResult == JSVAL_VOID, "mResult shouldn't have been set!");

  mDone = true;
  mError = DOMError::CreateWithName(aError);

  FireEvent(NS_LITERAL_STRING("error"), true, true);
}

void
DOMRequest::FireError(nsresult aError)
{
  NS_ASSERTION(!mDone, "mDone shouldn't have been set to true already!");
  NS_ASSERTION(!mError, "mError shouldn't have been set!");
  NS_ASSERTION(mResult == JSVAL_VOID, "mResult shouldn't have been set!");

  mDone = true;
  mError = DOMError::CreateForNSResult(aError);

  FireEvent(NS_LITERAL_STRING("error"), true, true);
}

void
DOMRequest::FireEvent(const nsAString& aType, bool aBubble, bool aCancelable)
{
  if (NS_FAILED(CheckInnerWindowCorrectness())) {
    return;
  }

  nsCOMPtr<nsIDOMEvent> event;
  NS_NewDOMEvent(getter_AddRefs(event), this, nullptr, nullptr);
  nsresult rv = event->InitEvent(aType, aBubble, aCancelable);
  if (NS_FAILED(rv)) {
    return;
  }

  event->SetTrusted(true);

  bool dummy;
  DispatchEvent(event, &dummy);
}

void
DOMRequest::RootResultVal()
{
  NS_ASSERTION(!mRooted, "Don't call me if already rooted!");
  nsXPCOMCycleCollectionParticipant *participant;
  CallQueryInterface(this, &participant);
  nsContentUtils::HoldJSObjects(NS_CYCLE_COLLECTION_UPCAST(this, DOMRequest),
                                participant);
  mRooted = true;
}

void
DOMRequest::UnrootResultVal()
{
  NS_ASSERTION(mRooted, "Don't call me if not rooted!");
  mResult = JSVAL_VOID;
  NS_DROP_JS_OBJECTS(this, DOMRequest);
  mRooted = false;
}

NS_IMPL_ISUPPORTS1(DOMRequestService, nsIDOMRequestService)

NS_IMETHODIMP
DOMRequestService::CreateRequest(nsIDOMWindow* aWindow,
                                 nsIDOMDOMRequest** aRequest)
{
  NS_ENSURE_STATE(aWindow);
  NS_ADDREF(*aRequest = new DOMRequest(aWindow));

  return NS_OK;
}

NS_IMETHODIMP
DOMRequestService::CreateCursor(nsIDOMWindow* aWindow,
                                nsICursorContinueCallback* aCallback,
                                nsIDOMDOMCursor** aCursor) {
  NS_ADDREF(*aCursor = new DOMCursor(aWindow, aCallback));

  return NS_OK;
}

NS_IMETHODIMP
DOMRequestService::FireSuccess(nsIDOMDOMRequest* aRequest,
                               const JS::Value& aResult)
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

class FireSuccessAsyncTask : public nsRunnable
{

  FireSuccessAsyncTask(DOMRequest* aRequest,
                       const JS::Value& aResult) :
    mReq(aRequest),
    mResult(aResult),
    mIsSetup(false)
  {
  }

public:

  nsresult
  Setup()
  {
    nsresult rv;
    nsIScriptContext* sc = mReq->GetContextForEventHandlers(&rv);
    if (!NS_SUCCEEDED(rv)) {
      return rv;
    }
    AutoPushJSContext cx(sc->GetNativeContext());
    if (!cx) {
      return NS_ERROR_FAILURE;
    }
    JSAutoRequest ar(cx);
    JS_AddValueRoot(cx, &mResult);
    mIsSetup = true;
    return NS_OK;
  }

  // Due to the fact that initialization can fail during shutdown (since we
  // can't fetch a js context), set up an initiatization function to make sure
  // we can return the failure appropriately
  static nsresult
  Dispatch(DOMRequest* aRequest,
           const JS::Value& aResult)
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    nsRefPtr<FireSuccessAsyncTask> asyncTask = new FireSuccessAsyncTask(aRequest, aResult);
    nsresult rv = asyncTask->Setup();
    NS_ENSURE_SUCCESS(rv, rv);
    if (NS_FAILED(NS_DispatchToMainThread(asyncTask))) {
      NS_WARNING("Failed to dispatch to main thread!");
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }

  NS_IMETHODIMP
  Run()
  {
    mReq->FireSuccess(mResult);
    return NS_OK;
  }

  ~FireSuccessAsyncTask()
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    if(!mIsSetup) {
      // If we never set up, no reason to unroot
      return;
    }
    nsresult rv;
    nsIScriptContext* sc = mReq->GetContextForEventHandlers(&rv);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    AutoPushJSContext cx(sc->GetNativeContext());
    MOZ_ASSERT(cx);

    // We need to build a new request, otherwise we assert since there won't be
    // a request available yet.
    JSAutoRequest ar(cx);
    JS_RemoveValueRoot(cx, &mResult);
  }
private:
  nsRefPtr<DOMRequest> mReq;
  JS::Value mResult;
  bool mIsSetup;
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
  nsRefPtr<DOMRequest> mReq;
  nsString mError;
};

NS_IMETHODIMP
DOMRequestService::FireSuccessAsync(nsIDOMDOMRequest* aRequest,
                                    const JS::Value& aResult)
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
  if (NS_FAILED(NS_DispatchToMainThread(asyncTask))) {
    NS_WARNING("Failed to dispatch to main thread!");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
DOMRequestService::FireDone(nsIDOMDOMCursor* aCursor) {
  NS_ENSURE_STATE(aCursor);
  static_cast<DOMCursor*>(aCursor)->FireDone();

  return NS_OK;
}
