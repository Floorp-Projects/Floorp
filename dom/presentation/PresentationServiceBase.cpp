/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PresentationServiceBase.h"

#include "nsString.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS0(PresentationServiceBase)

nsresult
PresentationServiceBase::SessionIdManager::GetWindowId(
                                                   const nsAString& aSessionId,
                                                   uint64_t* aWindowId)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mRespondingWindowIds.Get(aSessionId, aWindowId)) {
    return NS_OK;
  }
  return NS_ERROR_NOT_AVAILABLE;
}

nsresult
PresentationServiceBase::SessionIdManager::GetSessionIds(
                                               uint64_t aWindowId,
                                               nsTArray<nsString>& aSessionIds)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<nsString>* sessionIdArray;
  if (!mRespondingSessionIds.Get(aWindowId, &sessionIdArray)) {
    return NS_ERROR_INVALID_ARG;
  }

  aSessionIds.Assign(*sessionIdArray);
  return NS_OK;
}

void
PresentationServiceBase::SessionIdManager::AddSessionId(
                                                   uint64_t aWindowId,
                                                   const nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(aWindowId == 0)) {
    return;
  }

  nsTArray<nsString>* sessionIdArray;
  if (!mRespondingSessionIds.Get(aWindowId, &sessionIdArray)) {
    sessionIdArray = new nsTArray<nsString>();
    mRespondingSessionIds.Put(aWindowId, sessionIdArray);
  }

  sessionIdArray->AppendElement(nsString(aSessionId));
  mRespondingWindowIds.Put(aSessionId, aWindowId);
}

void
PresentationServiceBase::SessionIdManager::RemoveSessionId(
                                                   const nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());

  uint64_t windowId = 0;
  if (mRespondingWindowIds.Get(aSessionId, &windowId)) {
    mRespondingWindowIds.Remove(aSessionId);
    nsTArray<nsString>* sessionIdArray;
    if (mRespondingSessionIds.Get(windowId, &sessionIdArray)) {
      sessionIdArray->RemoveElement(nsString(aSessionId));
      if (sessionIdArray->IsEmpty()) {
        mRespondingSessionIds.Remove(windowId);
      }
    }
  }
}

nsresult
PresentationServiceBase::SessionIdManager::UpdateWindowId(
                                                   const nsAString& aSessionId,
                                                   const uint64_t aWindowId)
{
  MOZ_ASSERT(NS_IsMainThread());

  RemoveSessionId(aSessionId);
  AddSessionId(aWindowId, aSessionId);
  return NS_OK;
}

nsresult
PresentationServiceBase::GetWindowIdBySessionIdInternal(
                                                   const nsAString& aSessionId,
                                                   uint8_t aRole,
                                                   uint64_t* aWindowId)
{
  MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
             aRole == nsIPresentationService::ROLE_RECEIVER);

  if (NS_WARN_IF(!aWindowId)) {
    return NS_ERROR_INVALID_POINTER;
  }

  if (aRole == nsIPresentationService::ROLE_CONTROLLER) {
    return mControllerSessionIdManager.GetWindowId(aSessionId, aWindowId);
  }

  return mReceiverSessionIdManager.GetWindowId(aSessionId, aWindowId);
}

void
PresentationServiceBase::AddRespondingSessionId(uint64_t aWindowId,
                                                const nsAString& aSessionId,
                                                uint8_t aRole)
{
  MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
             aRole == nsIPresentationService::ROLE_RECEIVER);

  if (aRole == nsIPresentationService::ROLE_CONTROLLER) {
    mControllerSessionIdManager.AddSessionId(aWindowId, aSessionId);
  } else {
    mReceiverSessionIdManager.AddSessionId(aWindowId, aSessionId);
  }
}

void
PresentationServiceBase::RemoveRespondingSessionId(const nsAString& aSessionId,
                                                   uint8_t aRole)
{
  MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
             aRole == nsIPresentationService::ROLE_RECEIVER);

  if (aRole == nsIPresentationService::ROLE_CONTROLLER) {
    mControllerSessionIdManager.RemoveSessionId(aSessionId);
  } else {
    mReceiverSessionIdManager.RemoveSessionId(aSessionId);
  }
}

nsresult
PresentationServiceBase::UpdateWindowIdBySessionIdInternal(
                                                   const nsAString& aSessionId,
                                                   uint8_t aRole,
                                                   const uint64_t aWindowId)
{
  MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
             aRole == nsIPresentationService::ROLE_RECEIVER);

  if (aRole == nsIPresentationService::ROLE_CONTROLLER) {
    return mControllerSessionIdManager.UpdateWindowId(aSessionId, aWindowId);
  }

  return mReceiverSessionIdManager.UpdateWindowId(aSessionId, aWindowId);
}

} // namespace dom
} // namespace mozilla
