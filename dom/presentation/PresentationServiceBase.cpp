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
PresentationServiceBase::GetExistentSessionIdAtLaunchInternal(
  uint64_t aWindowId,
  nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<nsString>* sessionIdArray;
  if (mRespondingSessionIds.Get(aWindowId, &sessionIdArray) &&
      !sessionIdArray->IsEmpty()) {
    aSessionId.Assign((*sessionIdArray)[0]);
  }
  else {
    aSessionId.Truncate();
  }
  return NS_OK;
}

nsresult
PresentationServiceBase::GetWindowIdBySessionIdInternal(
  const nsAString& aSessionId,
  uint64_t* aWindowId)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mRespondingWindowIds.Get(aSessionId, aWindowId)) {
    return NS_OK;
  }
  return NS_ERROR_NOT_AVAILABLE;
}

void
PresentationServiceBase::AddRespondingSessionId(uint64_t aWindowId,
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
PresentationServiceBase::RemoveRespondingSessionId(const nsAString& aSessionId)
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
PresentationServiceBase::UpdateWindowIdBySessionIdInternal(
  const nsAString& aSessionId,
  const uint64_t aWindowId)
{
  MOZ_ASSERT(NS_IsMainThread());

  RemoveRespondingSessionId(aSessionId);
  AddRespondingSessionId(aWindowId, aSessionId);
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
