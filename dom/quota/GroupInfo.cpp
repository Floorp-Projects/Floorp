/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GroupInfo.h"

#include "mozilla/dom/quota/AssertionsImpl.h"
#include "OriginInfo.h"

namespace mozilla::dom::quota {

already_AddRefed<OriginInfo> GroupInfo::LockedGetOriginInfo(
    const nsACString& aOrigin) {
  AssertCurrentThreadOwnsQuotaMutex();

  for (const auto& originInfo : mOriginInfos) {
    if (originInfo->mOrigin == aOrigin) {
      RefPtr<OriginInfo> result = originInfo;
      return result.forget();
    }
  }

  return nullptr;
}

void GroupInfo::LockedAddOriginInfo(NotNull<RefPtr<OriginInfo>>&& aOriginInfo) {
  AssertCurrentThreadOwnsQuotaMutex();

  NS_ASSERTION(!mOriginInfos.Contains(aOriginInfo),
               "Replacing an existing entry!");
  mOriginInfos.AppendElement(std::move(aOriginInfo));

  uint64_t usage = aOriginInfo->LockedUsage();

  if (!aOriginInfo->LockedPersisted()) {
    AssertNoOverflow(mUsage, usage);
    mUsage += usage;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  AssertNoOverflow(quotaManager->mTemporaryStorageUsage, usage);
  quotaManager->mTemporaryStorageUsage += usage;
}

void GroupInfo::LockedAdjustUsageForRemovedOriginInfo(
    const OriginInfo& aOriginInfo) {
  const uint64_t usage = aOriginInfo.LockedUsage();

  if (!aOriginInfo.LockedPersisted()) {
    AssertNoUnderflow(mUsage, usage);
    mUsage -= usage;
  }

  QuotaManager* const quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  AssertNoUnderflow(quotaManager->mTemporaryStorageUsage, usage);
  quotaManager->mTemporaryStorageUsage -= usage;
}

void GroupInfo::LockedRemoveOriginInfo(const nsACString& aOrigin) {
  AssertCurrentThreadOwnsQuotaMutex();

  const auto foundIt = std::find_if(mOriginInfos.cbegin(), mOriginInfos.cend(),
                                    [&aOrigin](const auto& originInfo) {
                                      return originInfo->mOrigin == aOrigin;
                                    });

  // XXX Or can we MOZ_ASSERT(foundIt != mOriginInfos.cend()) ?
  if (foundIt != mOriginInfos.cend()) {
    LockedAdjustUsageForRemovedOriginInfo(**foundIt);

    mOriginInfos.RemoveElementAt(foundIt);
  }
}

void GroupInfo::LockedRemoveOriginInfos() {
  AssertCurrentThreadOwnsQuotaMutex();

  for (const auto& originInfo : std::exchange(mOriginInfos, {})) {
    LockedAdjustUsageForRemovedOriginInfo(*originInfo);
  }
}

}  // namespace mozilla::dom::quota
