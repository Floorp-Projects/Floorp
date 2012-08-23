/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMRequest.h"

#include "mozilla/Util.h"
#include "nsDOMClassInfo.h"
#include "DOMError.h"
#include "nsEventDispatcher.h"
#include "nsDOMEvent.h"
#include "nsContentUtils.h"

using mozilla::dom::DOMRequest;
using mozilla::dom::DOMRequestService;

DOMRequest::DOMRequest(nsIDOMWindow* aWindow)
  : mResult(JSVAL_VOID)
  , mDone(false)
  , mRooted(false)
{
  Init(aWindow);
}

// We need this constructor for dom::Activity that inherits from DOMRequest
// but has no window available from the constructor.
DOMRequest::DOMRequest()
  : mResult(JSVAL_VOID)
  , mDone(false)
  , mRooted(false)
{
}

void
DOMRequest::Init(nsIDOMWindow* aWindow)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  BindToOwner(window->IsInnerWindow() ? window.get() :
                                        window->GetCurrentInnerWindow());
}

DOMCI_DATA(DOMRequest, DOMRequest)

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DOMRequest,
                                                  nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(success)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(error)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mError)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DOMRequest,
                                                nsDOMEventTargetHelper)
  if (tmp->mRooted) {
    tmp->mResult = JSVAL_VOID;
    tmp->UnrootResultVal();
  }
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(success)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(error)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mError)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(DOMRequest,
                                               nsDOMEventTargetHelper)
  // Don't need NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER because
  // nsDOMEventTargetHelper does it for us.
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mResult)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DOMRequest)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMRequest)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DOMRequest)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(DOMRequest, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(DOMRequest, nsDOMEventTargetHelper)

NS_IMPL_EVENT_HANDLER(DOMRequest, success);
NS_IMPL_EVENT_HANDLER(DOMRequest, error);

NS_IMETHODIMP
DOMRequest::GetReadyState(nsAString& aReadyState)
{
  mDone ? aReadyState.AssignLiteral("done") :
          aReadyState.AssignLiteral("pending");

  return NS_OK;
}

NS_IMETHODIMP
DOMRequest::GetResult(jsval* aResult)
{
  NS_ASSERTION(mDone || mResult == JSVAL_VOID,
               "Result should be undefined when pending");
  *aResult = mResult;

  return NS_OK;
}

NS_IMETHODIMP
DOMRequest::GetError(nsIDOMDOMError** aError)
{
  NS_ASSERTION(mDone || !mError,
               "Error should be null when pending");

  NS_IF_ADDREF(*aError = mError);

  return NS_OK;
}

void
DOMRequest::FireSuccess(jsval aResult)
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

  nsRefPtr<nsDOMEvent> event = new nsDOMEvent(nullptr, nullptr);
  nsresult rv = event->InitEvent(aType, aBubble, aCancelable);
  if (NS_FAILED(rv)) {
    return;
  }

  rv = event->SetTrusted(true);
  if (NS_FAILED(rv)) {
    return;
  }

  bool dummy;
  DispatchEvent(event, &dummy);
}

void
DOMRequest::RootResultVal()
{
  NS_ASSERTION(!mRooted, "Don't call me if already rooted!");
  NS_HOLD_JS_OBJECTS(this, DOMRequest);
  mRooted = true;
}

void
DOMRequest::UnrootResultVal()
{
  NS_ASSERTION(mRooted, "Don't call me if not rooted!");
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
DOMRequestService::FireSuccess(nsIDOMDOMRequest* aRequest,
                               const jsval& aResult)
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
