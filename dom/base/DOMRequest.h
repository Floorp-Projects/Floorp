/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_domrequest_h__
#define mozilla_dom_domrequest_h__

#include "nsIDOMRequestService.h"
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMRequestBinding.h"

#include "nsCOMPtr.h"

namespace mozilla {

class ErrorResult;

namespace dom {

class AnyCallback;
class Promise;

class DOMRequest : public DOMEventTargetHelper
{
protected:
  JS::Heap<JS::Value> mResult;
  RefPtr<DOMException> mError;
  RefPtr<Promise> mPromise;
  bool mDone;

public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(DOMRequest,
                                                         DOMEventTargetHelper)

  // WrapperCache
  nsPIDOMWindowInner* GetParentObject() const
  {
    return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  DOMRequestReadyState ReadyState() const
  {
    return mDone ? DOMRequestReadyState::Done
                 : DOMRequestReadyState::Pending;
  }

  void GetResult(JSContext*, JS::MutableHandle<JS::Value> aRetval) const
  {
    NS_ASSERTION(mDone || mResult.isUndefined(),
                 "Result should be undefined when pending");
    aRetval.set(mResult);
  }

  DOMException* GetError() const
  {
    NS_ASSERTION(mDone || !mError,
                 "Error should be null when pending");
    return mError;
  }

  IMPL_EVENT_HANDLER(success)
  IMPL_EVENT_HANDLER(error)

  void
  Then(JSContext* aCx, AnyCallback* aResolveCallback,
       AnyCallback* aRejectCallback,
       JS::MutableHandle<JS::Value> aRetval,
       mozilla::ErrorResult& aRv);

  void FireSuccess(JS::Handle<JS::Value> aResult);
  void FireError(const nsAString& aError);
  void FireError(nsresult aError);
  void FireDetailedError(DOMException& aError);

  explicit DOMRequest(nsPIDOMWindowInner* aWindow);
  explicit DOMRequest(nsIGlobalObject* aGlobal);

protected:
  virtual ~DOMRequest();

  void FireEvent(const nsAString& aType, bool aBubble, bool aCancelable);

  void RootResultVal();
};

class DOMRequestService final : public nsIDOMRequestService
{
  ~DOMRequestService() {}

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMREQUESTSERVICE

  // No one should call this but the factory.
  static already_AddRefed<DOMRequestService> FactoryCreate()
  {
    return MakeAndAddRef<DOMRequestService>();
  }
};

} // namespace dom
} // namespace mozilla

#define DOMREQUEST_SERVICE_CONTRACTID "@mozilla.org/dom/dom-request-service;1"

#endif // mozilla_dom_domrequest_h__
