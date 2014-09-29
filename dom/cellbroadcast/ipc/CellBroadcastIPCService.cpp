/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CellBroadcastIPCService.h"
#include "mozilla/dom/ContentChild.h"

namespace mozilla {
namespace dom {
namespace cellbroadcast {

NS_IMPL_ISUPPORTS(CellBroadcastIPCService, nsICellBroadcastService)

CellBroadcastIPCService::CellBroadcastIPCService()
  : mActorDestroyed(false)
{
  ContentChild::GetSingleton()->SendPCellBroadcastConstructor(this);
}

CellBroadcastIPCService::~CellBroadcastIPCService()
{
  if (!mActorDestroyed) {
    Send__delete__(this);
  }

  mListeners.Clear();
}

/*
 * Implementation of nsICellBroadcastService.
 */

NS_IMETHODIMP
CellBroadcastIPCService::RegisterListener(nsICellBroadcastListener* aListener)
{
  MOZ_ASSERT(!mListeners.Contains(aListener));

  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_UNEXPECTED);

  // nsTArray doesn't fail.
  mListeners.AppendElement(aListener);

  return NS_OK;
}

NS_IMETHODIMP
CellBroadcastIPCService::UnregisterListener(nsICellBroadcastListener* aListener)
{
  MOZ_ASSERT(mListeners.Contains(aListener));

  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_UNEXPECTED);

  // We always have the element here, so it can't fail.
  mListeners.RemoveElement(aListener);

  return NS_OK;
}

/*
 * Implementation of PCellBroadcastChild.
 */

bool
CellBroadcastIPCService::RecvNotifyReceivedMessage(const uint32_t& aServiceId,
                                              const uint32_t& aGsmGeographicalScope,
                                              const uint16_t& aMessageCode,
                                              const uint16_t& aMessageId,
                                              const nsString& aLanguage,
                                              const nsString& aBody,
                                              const uint32_t& aMessageClass,
                                              const uint64_t& aTimestamp,
                                              const uint32_t& aCdmaServiceCategory,
                                              const bool& aHasEtwsInfo,
                                              const uint32_t& aEtwsWarningType,
                                              const bool& aEtwsEmergencyUserAlert,
                                              const bool& aEtwsPopup)
{
  // UnregisterListener() could be triggered in
  // nsICellBroadcastListener::NotifyMessageReceived().
  // Make a immutable copy for notifying the event.
  nsTArray<nsCOMPtr<nsICellBroadcastListener>> immutableListeners(mListeners);
  for (uint32_t i = 0; i < immutableListeners.Length(); i++) {
    immutableListeners[i]->NotifyMessageReceived(aServiceId,
                                                 aGsmGeographicalScope,
                                                 aMessageCode,
                                                 aMessageId,
                                                 aLanguage,
                                                 aBody,
                                                 aMessageClass,
                                                 aTimestamp,
                                                 aCdmaServiceCategory,
                                                 aHasEtwsInfo,
                                                 aEtwsWarningType,
                                                 aEtwsEmergencyUserAlert,
                                                 aEtwsPopup);
  }

  return true;
}

void
CellBroadcastIPCService::ActorDestroy(ActorDestroyReason aWhy)
{
  mActorDestroyed = true;
}

} // namespace cellbroadcast
} // namespace dom
} // namespace mozilla
