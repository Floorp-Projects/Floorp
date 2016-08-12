/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IccCallback.h"

#include "mozilla/dom/ContactsBinding.h"
#include "mozilla/dom/IccCardLockError.h"
#include "mozilla/dom/MozIccBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsIIccContact.h"
#include "nsJSUtils.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {
namespace icc {

namespace {

static nsresult
IccContactToMozContact(JSContext* aCx, GlobalObject& aGlobal,
                       nsIIccContact* aIccContact,
                       mozContact** aMozContact)
{
  *aMozContact = nullptr;

  ContactProperties properties;

  // Names
  char16_t** rawStringArray = nullptr;
  uint32_t count = 0;
  nsresult rv = aIccContact->GetNames(&count, &rawStringArray);
  NS_ENSURE_SUCCESS(rv, rv);
  if (count > 0) {
    Sequence<nsString>& nameSeq = properties.mName.Construct().SetValue();
    for (uint32_t i = 0; i < count; i++) {
      nameSeq.AppendElement(
        rawStringArray[i] ? nsDependentString(rawStringArray[i])
                          : EmptyString(),
        fallible);
    }
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, rawStringArray);
  }

  // Numbers
  rawStringArray = nullptr;
  count = 0;
  rv = aIccContact->GetNumbers(&count, &rawStringArray);
  NS_ENSURE_SUCCESS(rv, rv);
  if (count > 0) {
    Sequence<ContactTelField>& numberSeq = properties.mTel.Construct().SetValue();
    for (uint32_t i = 0; i < count; i++) {
      ContactTelField number;
      number.mValue.Construct() =
        rawStringArray[i] ? nsDependentString(rawStringArray[i])
                          : EmptyString();
      numberSeq.AppendElement(number, fallible);
    }
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, rawStringArray);
  }

  // Emails
  rawStringArray = nullptr;
  count = 0;
  rv = aIccContact->GetEmails(&count, &rawStringArray);
  NS_ENSURE_SUCCESS(rv, rv);
  if (count > 0) {
    Sequence<ContactField>& emailSeq = properties.mEmail.Construct().SetValue();
    for (uint32_t i = 0; i < count; i++) {
      ContactField email;
      email.mValue.Construct() =
        rawStringArray[i] ? nsDependentString(rawStringArray[i])
                          : EmptyString();
      emailSeq.AppendElement(email, fallible);
    }
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, rawStringArray);
  }

  ErrorResult er;
  RefPtr<mozContact> contact
    = mozContact::Constructor(aGlobal, aCx, properties, er);
  rv = er.StealNSResult();
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString contactId;
  rv = aIccContact->GetId(contactId);
  NS_ENSURE_SUCCESS(rv, rv);

  contact->SetId(contactId, er);
  rv = er.StealNSResult();
  NS_ENSURE_SUCCESS(rv, rv);

  contact.forget(aMozContact);

  return NS_OK;
}

static nsresult
IccContactListToMozContactList(JSContext* aCx, GlobalObject& aGlobal,
                               nsIIccContact** aContacts, uint32_t aCount,
                               nsTArray<RefPtr<mozContact>>& aContactList)
{
  aContactList.SetCapacity(aCount);
  for (uint32_t i = 0; i < aCount ; i++) {
    RefPtr<mozContact> contact;
    nsresult rv =
      IccContactToMozContact(aCx, aGlobal, aContacts[i], getter_AddRefs(contact));
    NS_ENSURE_SUCCESS(rv, rv);

    aContactList.AppendElement(contact);
  }

  return NS_OK;
}

} // anonymous namespace

NS_IMPL_ISUPPORTS(IccCallback, nsIIccCallback)

IccCallback::IccCallback(nsPIDOMWindowInner* aWindow, DOMRequest* aRequest,
                         bool aIsCardLockEnabled)
  : mWindow(aWindow)
  , mRequest(aRequest)
  , mIsCardLockEnabled(aIsCardLockEnabled)
{
}

IccCallback::IccCallback(nsPIDOMWindowInner* aWindow, Promise* aPromise)
  : mWindow(aWindow)
  , mPromise(aPromise)
{
}

nsresult
IccCallback::NotifySuccess(JS::Handle<JS::Value> aResult)
{
  nsCOMPtr<nsIDOMRequestService> rs =
    do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(rs, NS_ERROR_FAILURE);

  return rs->FireSuccessAsync(mRequest, aResult);
}

nsresult
IccCallback::NotifyGetCardLockEnabled(bool aResult)
{
  IccCardLockStatus result;
  result.mEnabled.Construct(aResult);

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> jsResult(cx);
  if (!ToJSValue(cx, result, &jsResult)) {
    jsapi.ClearException();
    return NS_ERROR_TYPE_ERR;
  }

  return NotifySuccess(jsResult);
}

NS_IMETHODIMP
IccCallback::NotifySuccess()
{
  return NotifySuccess(JS::UndefinedHandleValue);
}

NS_IMETHODIMP
IccCallback::NotifySuccessWithBoolean(bool aResult)
{
  if (mPromise) {
    mPromise->MaybeResolve(aResult ? JS::TrueHandleValue : JS::FalseHandleValue);
    return NS_OK;
  }

  return mIsCardLockEnabled
    ? NotifyGetCardLockEnabled(aResult)
    : NotifySuccess(aResult ? JS::TrueHandleValue : JS::FalseHandleValue);
}

NS_IMETHODIMP
IccCallback::NotifyGetCardLockRetryCount(int32_t aCount)
{
  // TODO: Bug 1125018 - Simplify The Result of GetCardLock and
  // getCardLockRetryCount in MozIcc.webidl without a wrapper object.
  IccCardLockRetryCount result;
  result.mRetryCount.Construct(aCount);

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> jsResult(cx);
  if (!ToJSValue(cx, result, &jsResult)) {
    jsapi.ClearException();
    return NS_ERROR_TYPE_ERR;
  }

  return NotifySuccess(jsResult);
}

NS_IMETHODIMP
IccCallback::NotifyError(const nsAString & aErrorMsg)
{
  if (mPromise) {
    mPromise->MaybeRejectBrokenly(aErrorMsg);
    return NS_OK;
  }

  nsCOMPtr<nsIDOMRequestService> rs =
    do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(rs, NS_ERROR_FAILURE);

  return rs->FireErrorAsync(mRequest, aErrorMsg);
}

NS_IMETHODIMP
IccCallback::NotifyCardLockError(const nsAString & aErrorMsg,
                                 int32_t aRetryCount)
{
  RefPtr<IccCardLockError> error =
    new IccCardLockError(mWindow, aErrorMsg, aRetryCount);
  mRequest->FireDetailedError(error);

  return NS_OK;
}

NS_IMETHODIMP
IccCallback::NotifyRetrievedIccContacts(nsIIccContact** aContacts,
                                        uint32_t aCount)
{
  MOZ_ASSERT(aContacts);

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(mWindow);
  GlobalObject global(cx, go->GetGlobalJSObject());

  nsTArray<RefPtr<mozContact>> contactList;

  nsresult rv =
    IccContactListToMozContactList(cx, global, aContacts, aCount, contactList);
  NS_ENSURE_SUCCESS(rv, rv);

  JS::Rooted<JS::Value> jsResult(cx);

  if (!ToJSValue(cx, contactList, &jsResult)) {
    return NS_ERROR_FAILURE;
  }

  return NotifySuccess(jsResult);
}

NS_IMETHODIMP
IccCallback::NotifyUpdatedIccContact(nsIIccContact* aContact)
{
  MOZ_ASSERT(aContact);

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(mWindow);
  GlobalObject global(cx, go->GetGlobalJSObject());

  RefPtr<mozContact> mozContact;
  nsresult rv =
    IccContactToMozContact(cx, global, aContact, getter_AddRefs(mozContact));
  NS_ENSURE_SUCCESS(rv, rv);

  JS::Rooted<JS::Value> jsResult(cx);

  if (!ToJSValue(cx, mozContact, &jsResult)) {
    return NS_ERROR_FAILURE;
  }

  return NotifySuccess(jsResult);
}

} // namespace icc
} // namespace dom
} // namespace mozilla
