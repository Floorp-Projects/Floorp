/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicCardPayment.h"
#include "mozilla/dom/PaymentMethodChangeEvent.h"
#include "mozilla/dom/PaymentRequestUpdateEvent.h"
#include "PaymentRequestUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(PaymentMethodChangeEvent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PaymentMethodChangeEvent,
                                                PaymentRequestUpdateEvent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PaymentMethodChangeEvent,
                                                  PaymentRequestUpdateEvent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(PaymentMethodChangeEvent,
                                               PaymentRequestUpdateEvent)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(PaymentMethodChangeEvent, PaymentRequestUpdateEvent)
NS_IMPL_RELEASE_INHERITED(PaymentMethodChangeEvent, PaymentRequestUpdateEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PaymentMethodChangeEvent)
NS_INTERFACE_MAP_END_INHERITING(PaymentRequestUpdateEvent)

already_AddRefed<PaymentMethodChangeEvent>
PaymentMethodChangeEvent::Constructor(
  mozilla::dom::EventTarget* aOwner,
  const nsAString& aType,
  const PaymentMethodChangeEventInit& aEventInitDict)
{
  RefPtr<PaymentMethodChangeEvent> e = new PaymentMethodChangeEvent(aOwner);
  bool trusted = e->Init(aOwner);
  e->InitEvent(aType, aEventInitDict.mBubbles, aEventInitDict.mCancelable);
  e->SetTrusted(trusted);
  e->SetComposed(aEventInitDict.mComposed);
  e->init(aEventInitDict);
  return e.forget();
}

already_AddRefed<PaymentMethodChangeEvent>
PaymentMethodChangeEvent::Constructor(
  const GlobalObject& aGlobal,
  const nsAString& aType,
  const PaymentMethodChangeEventInit& aEventInitDict,
  ErrorResult& aRv)
{
  nsCOMPtr<mozilla::dom::EventTarget> owner =
    do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<PaymentMethodChangeEvent> event = Constructor(owner, aType, aEventInitDict);

  if (!aEventInitDict.mMethodDetails) {
    return event.forget();
  }

  ChangeDetails details;
  RefPtr<BasicCardService> service = BasicCardService::GetService();
  MOZ_ASSERT(service);
  if (service->IsBasicCardPayment(aEventInitDict.mMethodName)) {
    BasicCardChangeDetails methodDetails;
    BasicCardDetails bcDetails;
    JS::RootedValue value(aGlobal.Context(), JS::ObjectValue(*(aEventInitDict.mMethodDetails)));
    if (!methodDetails.Init(aGlobal.Context(), value)) {
      return event.forget();
    } else {
      if(methodDetails.mBillingAddress.WasPassed()) {
        RefPtr<PaymentAddress> address = methodDetails.mBillingAddress.Value();
        address->GetCountry(bcDetails.billingAddress.country);
        address->GetAddressLine(bcDetails.billingAddress.addressLine);
        address->GetCity(bcDetails.billingAddress.city);
        address->GetRegion(bcDetails.billingAddress.region);
        address->GetRegionCode(bcDetails.billingAddress.regionCode);
        address->GetDependentLocality(bcDetails.billingAddress.dependentLocality);
        address->GetPostalCode(bcDetails.billingAddress.postalCode);
        address->GetSortingCode(bcDetails.billingAddress.sortingCode);
        address->GetOrganization(bcDetails.billingAddress.organization);
        address->GetRecipient(bcDetails.billingAddress.recipient);
        address->GetPhone(bcDetails.billingAddress.phone);
      }
      details = bcDetails;
    }
  } else {
    JS::RootedObject object(aGlobal.Context(), aEventInitDict.mMethodDetails);
    nsAutoString serializedDetails;
    nsresult rv = SerializeFromJSObject(aGlobal.Context(), object, serializedDetails);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
    GeneralDetails gDetails;
    gDetails.details = serializedDetails;
    details = gDetails;
  }
  event->SetMethodDetails(details);

  return event.forget();
}

PaymentMethodChangeEvent::PaymentMethodChangeEvent(EventTarget* aOwner)
  : PaymentRequestUpdateEvent(aOwner)
{
  MOZ_ASSERT(aOwner);
}

void
PaymentMethodChangeEvent::init(const PaymentMethodChangeEventInit& aEventInitDict)
{
  mMethodName.Assign(aEventInitDict.mMethodName);
}

void
PaymentMethodChangeEvent::GetMethodName(nsAString& aMethodName)
{
  aMethodName.Assign(mMethodName);
}

void
PaymentMethodChangeEvent::SetMethodName(const nsAString& aMethodName)
{
  mMethodName = aMethodName;
}

void
PaymentMethodChangeEvent::GetMethodDetails(JSContext* aCx,
                                           JS::MutableHandle<JSObject*> aRetVal)
{
  MOZ_ASSERT(aCx);
  RefPtr<BasicCardService> service = BasicCardService::GetService();
  MOZ_ASSERT(service);

  aRetVal.set(nullptr);
  switch(mMethodDetails.type()) {
    case ChangeDetails::GeneralMethodDetails: {
      const GeneralDetails& rawDetails = mMethodDetails.generalDetails();
      DeserializeToJSObject(rawDetails.details, aCx, aRetVal);
      break;
    }
    case ChangeDetails::BasicCardMethodDetails: {
      const BasicCardDetails& rawDetails = mMethodDetails.basicCardDetails();
      BasicCardChangeDetails basicCardDetails;
      PaymentOptions options;
      mRequest->GetOptions(options);
      if (options.mRequestBillingAddress) {
        if (!rawDetails.billingAddress.country.IsEmpty() ||
            !rawDetails.billingAddress.addressLine.IsEmpty() ||
            !rawDetails.billingAddress.region.IsEmpty() ||
            !rawDetails.billingAddress.regionCode.IsEmpty() ||
            !rawDetails.billingAddress.city.IsEmpty() ||
            !rawDetails.billingAddress.dependentLocality.IsEmpty() ||
            !rawDetails.billingAddress.postalCode.IsEmpty() ||
            !rawDetails.billingAddress.sortingCode.IsEmpty() ||
            !rawDetails.billingAddress.organization.IsEmpty() ||
            !rawDetails.billingAddress.recipient.IsEmpty() ||
            !rawDetails.billingAddress.phone.IsEmpty()) {
          nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(GetParentObject());
          basicCardDetails.mBillingAddress.Construct();
          basicCardDetails.mBillingAddress.Value() =
            new PaymentAddress(window,
                               rawDetails.billingAddress.country,
                               rawDetails.billingAddress.addressLine,
                               rawDetails.billingAddress.region,
                               rawDetails.billingAddress.regionCode,
                               rawDetails.billingAddress.city,
                               rawDetails.billingAddress.dependentLocality,
                               rawDetails.billingAddress.postalCode,
                               rawDetails.billingAddress.sortingCode,
                               rawDetails.billingAddress.organization,
                               rawDetails.billingAddress.recipient,
                               rawDetails.billingAddress.phone);
        }
      }
      MOZ_ASSERT(aCx);
      JS::RootedValue value(aCx);
      if (NS_WARN_IF(!basicCardDetails.ToObjectInternal(aCx, &value))) {
        return;
      }
      aRetVal.set(&value.toObject());
      break;
    }
    default: {
      break;
    }
  }
}

void
PaymentMethodChangeEvent::SetMethodDetails(const ChangeDetails& aMethodDetails)
{
  mMethodDetails = aMethodDetails;
}

PaymentMethodChangeEvent::~PaymentMethodChangeEvent()
{
}

JSObject*
PaymentMethodChangeEvent::WrapObjectInternal(JSContext* aCx,
                                             JS::Handle<JSObject*> aGivenProto)
{
  return PaymentMethodChangeEvent_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
