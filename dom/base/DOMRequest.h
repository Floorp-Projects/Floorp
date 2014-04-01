/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_domrequest_h__
#define mozilla_dom_domrequest_h__

#include "nsIDOMDOMRequest.h"
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/DOMRequestBinding.h"

#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {

class DOMRequest : public DOMEventTargetHelper,
                   public nsIDOMDOMRequest
{
protected:
  JS::Heap<JS::Value> mResult;
  nsCOMPtr<nsISupports> mError;
  bool mDone;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMDOMREQUEST
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(DOMRequest,
                                                         DOMEventTargetHelper)

  // WrapperCache
  nsPIDOMWindow* GetParentObject() const
  {
    return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // WebIDL Interface
  DOMRequestReadyState ReadyState() const
  {
    return mDone ? DOMRequestReadyState::Done
                 : DOMRequestReadyState::Pending;
  }

  JS::Value Result(JSContext* = nullptr) const
  {
    NS_ASSERTION(mDone || mResult == JSVAL_VOID,
               "Result should be undefined when pending");
    return mResult;
  }

  nsISupports* GetError() const
  {
    NS_ASSERTION(mDone || !mError,
                 "Error should be null when pending");
    return mError;
  }

  IMPL_EVENT_HANDLER(success)
  IMPL_EVENT_HANDLER(error)


  void FireSuccess(JS::Handle<JS::Value> aResult);
  void FireError(const nsAString& aError);
  void FireError(nsresult aError);
  void FireDetailedError(nsISupports* aError);

  DOMRequest(nsPIDOMWindow* aWindow);

  virtual ~DOMRequest()
  {
    mResult = JSVAL_VOID;
    mozilla::DropJSObjects(this);
  }

protected:
  void FireEvent(const nsAString& aType, bool aBubble, bool aCancelable);

  void RootResultVal();
};

class DOMRequestService MOZ_FINAL : public nsIDOMRequestService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMREQUESTSERVICE

  // Returns an owning reference! No one should call this but the factory.
  static DOMRequestService* FactoryCreate()
  {
    DOMRequestService* res = new DOMRequestService;
    NS_ADDREF(res);
    return res;
  }
};

} // namespace dom
} // namespace mozilla

#define DOMREQUEST_SERVICE_CONTRACTID "@mozilla.org/dom/dom-request-service;1"

#endif // mozilla_dom_domrequest_h__
