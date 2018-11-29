/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LSDatabase.h"

namespace mozilla {
namespace dom {

LSDatabase::LSDatabase()
  : mActor(nullptr)
  , mAllowedToClose(false)
{
  AssertIsOnOwningThread();
}

LSDatabase::~LSDatabase()
{
  AssertIsOnOwningThread();

  if (!mAllowedToClose) {
    AllowToClose();
  }

  if (mActor) {
    mActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mActor, "SendDeleteMeInternal should have cleared!");
  }
}

void
LSDatabase::SetActor(LSDatabaseChild* aActor)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!mActor);

  mActor = aActor;
}

void
LSDatabase::AllowToClose()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mAllowedToClose);

  mAllowedToClose = true;

  if (mActor) {
    mActor->SendAllowToClose();
  }
}

nsresult
LSDatabase::GetLength(uint32_t* aResult)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mAllowedToClose);

  uint32_t result;
  if (NS_WARN_IF(!mActor->SendGetLength(&result))) {
    return NS_ERROR_FAILURE;
  }

  *aResult = result;
  return NS_OK;
}

nsresult
LSDatabase::GetKey(uint32_t aIndex,
                   nsAString& aResult)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mAllowedToClose);

  nsString result;
  if (NS_WARN_IF(!mActor->SendGetKey(aIndex, &result))) {
    return NS_ERROR_FAILURE;
  }

  aResult = result;
  return NS_OK;
}

nsresult
LSDatabase::GetItem(const nsAString& aKey,
                    nsAString& aResult)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mAllowedToClose);

  nsString result;
  if (NS_WARN_IF(!mActor->SendGetItem(nsString(aKey), &result))) {
    return NS_ERROR_FAILURE;
  }

  aResult = result;
  return NS_OK;
}

nsresult
LSDatabase::GetKeys(nsTArray<nsString>& aKeys)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mAllowedToClose);

  nsTArray<nsString> result;
  if (NS_WARN_IF(!mActor->SendGetKeys(&result))) {
    return NS_ERROR_FAILURE;
  }

  aKeys.SwapElements(result);
  return NS_OK;
}

nsresult
LSDatabase::SetItem(const nsAString& aKey,
                    const nsAString& aValue)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mAllowedToClose);

  if (NS_WARN_IF(!mActor->SendSetItem(nsString(aKey), nsString(aValue)))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
LSDatabase::RemoveItem(const nsAString& aKey)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mAllowedToClose);

  if (NS_WARN_IF(!mActor->SendRemoveItem(nsString(aKey)))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
LSDatabase::Clear()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mAllowedToClose);

  if (NS_WARN_IF(!mActor->SendClear())) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
