/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CFStateChangeEvent.h"
#include "nsIDOMClassInfo.h"
#include "nsDOMClassInfoID.h"
#include "nsContentUtils.h"

DOMCI_DATA(CFStateChangeEvent, mozilla::dom::network::CFStateChangeEvent)

namespace mozilla {
namespace dom {
namespace network {

already_AddRefed<CFStateChangeEvent>
CFStateChangeEvent::Create(bool aSuccess,
                           uint16_t aAction,
                           uint16_t aReason,
                           nsAString& aNumber,
                           uint16_t aTimeSeconds,
                           uint16_t aServiceClass)
{
  NS_ASSERTION(!aNumber.IsEmpty(), "Empty number!");

  nsRefPtr<CFStateChangeEvent> event = new CFStateChangeEvent();

  event->mSuccess = aSuccess;
  event->mAction = aAction;
  event->mReason = aReason;
  event->mNumber = aNumber;
  event->mTimeSeconds = aTimeSeconds;
  event->mServiceClass = aServiceClass;

  return event.forget();
}

NS_IMPL_ADDREF_INHERITED(CFStateChangeEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(CFStateChangeEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN(CFStateChangeEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCFStateChangeEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CFStateChangeEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMETHODIMP
CFStateChangeEvent::GetSuccess(bool* aSuccess)
{
  *aSuccess = mSuccess;
  return NS_OK;
}

NS_IMETHODIMP
CFStateChangeEvent::GetAction(uint16_t* aAction)
{
  *aAction = mAction;
  return NS_OK;
}

NS_IMETHODIMP
CFStateChangeEvent::GetReason(uint16_t* aReason)
{
  *aReason = mReason;
  return NS_OK;
}

NS_IMETHODIMP
CFStateChangeEvent::GetNumber(nsAString& aNumber)
{
  aNumber.Assign(mNumber);
  return NS_OK;
}

NS_IMETHODIMP
CFStateChangeEvent::GetTimeSeconds(uint16_t* aTimeSeconds)
{
  *aTimeSeconds = mTimeSeconds;
  return NS_OK;
}

NS_IMETHODIMP
CFStateChangeEvent::GetServiceClass(uint16_t* aServiceClass)
{
  *aServiceClass = mServiceClass;
  return NS_OK;
}

}
}
}
