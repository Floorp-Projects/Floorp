/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/voicemail/VoicemailParent.h"

#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {
namespace voicemail {

NS_IMPL_ISUPPORTS(VoicemailParent,
                  nsIVoicemailListener)

bool
VoicemailParent::Init()
{
  mService = do_GetService(NS_VOICEMAIL_SERVICE_CONTRACTID);
  return mService && NS_SUCCEEDED(mService->RegisterListener(this));
}

bool
VoicemailParent::RecvGetAttributes(const uint32_t& aServiceId,
                                   nsString* aNumber,
                                   nsString* aDisplayName,
                                   bool* aHasMessages,
                                   int32_t* aMessageCount,
                                   nsString* aReturnNumber,
                                   nsString* aReturnMessage)
{
  nsCOMPtr<nsIVoicemailProvider> provider;
  NS_ENSURE_SUCCESS(mService->GetItemByServiceId(aServiceId,
                                                 getter_AddRefs(provider)), false);

  provider->GetNumber(*aNumber);
  provider->GetDisplayName(*aDisplayName);
  provider->GetHasMessages(aHasMessages);
  provider->GetMessageCount(aMessageCount);
  provider->GetReturnNumber(*aReturnNumber);
  provider->GetReturnMessage(*aReturnMessage);

  return true;
}

void
VoicemailParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mService->UnregisterListener(this);
  mService = nullptr;
}

// nsIVoicemailListener

NS_IMETHODIMP
VoicemailParent::NotifyInfoChanged(nsIVoicemailProvider* aProvider)
{
  uint32_t serviceId = 0;
  nsString number, displayName;

  aProvider->GetServiceId(&serviceId);
  aProvider->GetNumber(number);
  aProvider->GetDisplayName(displayName);

  return SendNotifyInfoChanged(serviceId, number, displayName)
    ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
VoicemailParent::NotifyStatusChanged(nsIVoicemailProvider* aProvider)
{
  uint32_t serviceId = 0;
  bool hasMessages = false;
  int32_t messageCount = 0;
  nsString returnNumber, returnMessage;

  aProvider->GetServiceId(&serviceId);
  aProvider->GetHasMessages(&hasMessages);
  aProvider->GetMessageCount(&messageCount);
  aProvider->GetReturnNumber(returnNumber);
  aProvider->GetReturnMessage(returnMessage);

  return SendNotifyStatusChanged(serviceId, hasMessages, messageCount,
                                 returnNumber, returnMessage)
    ? NS_OK : NS_ERROR_FAILURE;
}

} // namespace voicemail
} // namespace dom
} // namespace mozilla
