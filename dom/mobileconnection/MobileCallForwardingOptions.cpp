/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/mobileconnection/MobileCallForwardingOptions.h"

namespace mozilla {
namespace dom {
namespace mobileconnection {

NS_IMPL_ISUPPORTS(MobileCallForwardingOptions, nsIMobileCallForwardingOptions)

MobileCallForwardingOptions::MobileCallForwardingOptions(bool aActive,
                                                         int16_t aAction,
                                                         int16_t aReason,
                                                         const nsAString& aNumber,
                                                         int16_t aTimeSeconds,
                                                         int16_t aServiceClass)
  : mActive(aActive)
  , mAction(aAction)
  , mReason(aReason)
  , mNumber(aNumber)
  , mTimeSeconds(aTimeSeconds)
  , mServiceClass(aServiceClass)
{
}

NS_IMETHODIMP
MobileCallForwardingOptions::GetActive(bool* aActive)
{
  *aActive = mActive;
  return NS_OK;
}

NS_IMETHODIMP
MobileCallForwardingOptions::GetAction(int16_t* aAction)
{
  *aAction = mAction;
  return NS_OK;
}

NS_IMETHODIMP
MobileCallForwardingOptions::GetReason(int16_t* aReason)
{
  *aReason = mReason;
  return NS_OK;
}

NS_IMETHODIMP
MobileCallForwardingOptions::GetNumber(nsAString& aNumber)
{
  aNumber = mNumber;
  return NS_OK;
}

NS_IMETHODIMP
MobileCallForwardingOptions::GetTimeSeconds(int16_t* aTimeSeconds)
{
  *aTimeSeconds = mTimeSeconds;
  return NS_OK;
}

NS_IMETHODIMP
MobileCallForwardingOptions::GetServiceClass(int16_t *aServiceClass)
{
  *aServiceClass = mServiceClass;
  return NS_OK;
}

} // namespace mobileconnection
} // namespace dom
} // namespace mozilla