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
#include "nsIPrivateDOMEvent.h"
#include "nsDOMEvent.h"

using mozilla::dom::DOMRequest;
using mozilla::dom::DOMRequestService;

DOMRequest::DOMRequest(nsIDOMWindow* aWindow)
  : mDone(false)
  , mResult(JSVAL_VOID)
  , mRooted(false)
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
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DOMRequest,
                                                nsDOMEventTargetHelper)
  tmp->mResult = JSVAL_VOID;
  tmp->UnrootResultVal();
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(success)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(error)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(DOMRequest,
                                               nsDOMEventTargetHelper)
  // Don't need NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER because
  // nsDOMEventTargetHelper does it for us.
  if (JSVAL_IS_GCTHING(tmp->mResult)) {
    void *gcThing = JSVAL_TO_GCTHING(tmp->mResult);
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_CALLBACK(gcThing, "mResult")
  }
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
  NS_ABORT_IF_FALSE(!mDone, "Already fired success/error");

  mDone = true;
  RootResultVal();
  mResult = aResult;

  FireEvent(NS_LITERAL_STRING("success"));
}

void
DOMRequest::FireError(const nsAString& aError)
{
  NS_ABORT_IF_FALSE(!mDone, "Already fired success/error");

  mDone = true;
  mError = DOMError::CreateWithName(aError);

  FireEvent(NS_LITERAL_STRING("error"));
}

void
DOMRequest::FireEvent(const nsAString& aType)
{
  if (NS_FAILED(CheckInnerWindowCorrectness())) {
    return;
  }

  nsRefPtr<nsDOMEvent> event = new nsDOMEvent(nsnull, nsnull);
  nsresult rv = event->InitEvent(aType, false, false);
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

NS_IMPL_ISUPPORTS1(DOMRequestService, nsIDOMRequestService)

NS_IMETHODIMP
DOMRequestService::CreateRequest(nsIDOMWindow* aWindow,
                                 nsIDOMDOMRequest** aRequest)
{
  NS_ADDREF(*aRequest = new DOMRequest(aWindow));
  
  return NS_OK;
}

NS_IMETHODIMP
DOMRequestService::FireSuccess(nsIDOMDOMRequest* aRequest,
                               const jsval& aResult)
{
  static_cast<DOMRequest*>(aRequest)->FireSuccess(aResult);

  return NS_OK;
}

NS_IMETHODIMP
DOMRequestService::FireError(nsIDOMDOMRequest* aRequest,
                             const nsAString& aError)
{
  static_cast<DOMRequest*>(aRequest)->FireError(aError);

  return NS_OK;
}
