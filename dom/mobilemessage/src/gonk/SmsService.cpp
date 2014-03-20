/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsMessage.h"
#include "SmsService.h"
#include "SmsSegmentInfo.h"
#include "mozilla/Preferences.h"
#include "nsServiceManagerUtils.h"

namespace {

const char* kPrefRilNumRadioInterfaces = "ril.numRadioInterfaces";
#define kPrefDefaultServiceId "dom.sms.defaultServiceId"
const char* kObservedPrefs[] = {
  kPrefDefaultServiceId,
  nullptr
};

uint32_t
getDefaultServiceId()
{
  int32_t id = mozilla::Preferences::GetInt(kPrefDefaultServiceId, 0);
  int32_t numRil = mozilla::Preferences::GetInt(kPrefRilNumRadioInterfaces, 1);

  if (id >= numRil || id < 0) {
    id = 0;
  }

  return id;
}

} // Anonymous namespace

namespace mozilla {
namespace dom {
namespace mobilemessage {

NS_IMPL_ISUPPORTS2(SmsService,
                   nsISmsService,
                   nsIObserver)

SmsService::SmsService()
{
  mRil = do_GetService("@mozilla.org/ril;1");
  NS_WARN_IF_FALSE(mRil, "This shouldn't fail!");

  // Initialize observer.
  Preferences::AddStrongObservers(this, kObservedPrefs);
  mDefaultServiceId = getDefaultServiceId();
}

/*
 * Implementation of nsIObserver.
 */

NS_IMETHODIMP
SmsService::Observe(nsISupports* aSubject,
                    const char* aTopic,
                    const char16_t* aData)
{
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsDependentString data(aData);
    if (data.EqualsLiteral(kPrefDefaultServiceId)) {
      mDefaultServiceId = getDefaultServiceId();
    }
    return NS_OK;
  }

  MOZ_ASSERT(false, "SmsService got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

/*
 * Implementation of nsISmsService.
 */

NS_IMETHODIMP
SmsService::GetSmsDefaultServiceId(uint32_t* aServiceId)
{
  *aServiceId = mDefaultServiceId;
  return NS_OK;
}

NS_IMETHODIMP
SmsService::GetSegmentInfoForText(const nsAString& aText,
                                  nsIMobileMessageCallback* aRequest)
{
  nsCOMPtr<nsIRadioInterface> radioInterface;
  if (mRil) {
    mRil->GetRadioInterface(0, getter_AddRefs(radioInterface));
  }
  NS_ENSURE_TRUE(radioInterface, NS_ERROR_FAILURE);

  return radioInterface->GetSegmentInfoForText(aText, aRequest);
}

NS_IMETHODIMP
SmsService::Send(uint32_t aServiceId,
                 const nsAString& aNumber,
                 const nsAString& aMessage,
                 bool aSilent,
                 nsIMobileMessageCallback* aRequest)
{
  nsCOMPtr<nsIRadioInterface> radioInterface;
  if (mRil) {
    mRil->GetRadioInterface(aServiceId, getter_AddRefs(radioInterface));
  }
  NS_ENSURE_TRUE(radioInterface, NS_ERROR_FAILURE);

  return radioInterface->SendSMS(aNumber, aMessage, aSilent, aRequest);
}

NS_IMETHODIMP
SmsService::IsSilentNumber(const nsAString& aNumber,
                           bool*            aIsSilent)
{
  *aIsSilent = mSilentNumbers.Contains(aNumber);
  return NS_OK;
}

NS_IMETHODIMP
SmsService::AddSilentNumber(const nsAString& aNumber)
{
  if (mSilentNumbers.Contains(aNumber)) {
    return NS_ERROR_UNEXPECTED;
  }

  NS_ENSURE_TRUE(mSilentNumbers.AppendElement(aNumber), NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP
SmsService::RemoveSilentNumber(const nsAString& aNumber)
{
  if (!mSilentNumbers.Contains(aNumber)) {
    return NS_ERROR_INVALID_ARG;
  }

  NS_ENSURE_TRUE(mSilentNumbers.RemoveElement(aNumber), NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP
SmsService::GetSmscAddress(uint32_t aServiceId,
                           nsIMobileMessageCallback *aRequest)
{
  nsCOMPtr<nsIRadioInterface> radioInterface;
  if (mRil) {
    mRil->GetRadioInterface(aServiceId, getter_AddRefs(radioInterface));
  }
  NS_ENSURE_TRUE(radioInterface, NS_ERROR_FAILURE);

  return radioInterface->GetSmscAddress(aRequest);
}

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla
