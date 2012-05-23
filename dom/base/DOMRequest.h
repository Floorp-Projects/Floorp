/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_domrequest_h__
#define mozilla_dom_domrequest_h__

#include "nsIDOMDOMRequest.h"
#include "nsIDOMDOMError.h"
#include "nsDOMEventTargetHelper.h"
#include "nsContentUtils.h"

#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {

class DOMRequest : public nsDOMEventTargetHelper,
                   public nsIDOMDOMRequest
{
  NS_DECL_EVENT_HANDLER(success)
  NS_DECL_EVENT_HANDLER(error)

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMDOMREQUEST
  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(DOMRequest,
                                                         nsDOMEventTargetHelper)

  void FireSuccess(jsval aResult);
  void FireError(const nsAString& aError);

  DOMRequest(nsIDOMWindow* aWindow);

  virtual ~DOMRequest()
  {
    UnrootResultVal();
  }

  bool mDone;
  jsval mResult;
  nsCOMPtr<nsIDOMDOMError> mError;
  bool mRooted;

private:
  void FireEvent(const nsAString& aType);

  void RootResultVal()
  {
    if (!mRooted) {
      NS_HOLD_JS_OBJECTS(this, DOMRequest);
      mRooted = true;
    }
  }

  void UnrootResultVal()
  {
    if (mRooted) {
      NS_DROP_JS_OBJECTS(this, DOMRequest);
      mRooted = false;
    }
  }
};

class DOMRequestService : public nsIDOMRequestService
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
