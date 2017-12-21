/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/CDMCaps.h"
#include "mozilla/EMEUtils.h"
#include "nsThreadUtils.h"
#include "SamplesWaitingForKey.h"

namespace mozilla {

CDMCaps::CDMCaps()
{
}

CDMCaps::~CDMCaps()
{
}

// Keys with MediaKeyStatus::Usable, MediaKeyStatus::Output_downscaled,
// or MediaKeyStatus::Output_restricted status can be used by the CDM
// to decrypt or decrypt-and-decode samples.
static bool
IsUsableStatus(dom::MediaKeyStatus aStatus)
{
  return aStatus == dom::MediaKeyStatus::Usable ||
         aStatus == dom::MediaKeyStatus::Output_restricted ||
         aStatus == dom::MediaKeyStatus::Output_downscaled;
}

bool
CDMCaps::IsKeyUsable(const CencKeyId& aKeyId)
{
  for (const KeyStatus& keyStatus : mKeyStatuses) {
    if (keyStatus.mId == aKeyId) {
      return IsUsableStatus(keyStatus.mStatus);
    }
  }
  return false;
}

bool
CDMCaps::SetKeyStatus(const CencKeyId& aKeyId,
                      const nsString& aSessionId,
                      const dom::Optional<dom::MediaKeyStatus>& aStatus)
{
  if (!aStatus.WasPassed()) {
    // Called from ForgetKeyStatus.
    // Return true if the element is found to notify key changes.
    return mKeyStatuses.RemoveElement(
      KeyStatus(aKeyId, aSessionId, dom::MediaKeyStatus::Internal_error));
  }

  KeyStatus key(aKeyId, aSessionId, aStatus.Value());
  auto index = mKeyStatuses.IndexOf(key);
  if (index != mKeyStatuses.NoIndex) {
    if (mKeyStatuses[index].mStatus == aStatus.Value()) {
      // No change.
      return false;
    }
    auto oldStatus = mKeyStatuses[index].mStatus;
    mKeyStatuses[index].mStatus = aStatus.Value();
    // The old key status was one for which we can decrypt media. We don't
    // need to do the "notify usable" step below, as it should be impossible
    // for us to have anything waiting on this key to become usable, since it
    // was already usable.
    if (IsUsableStatus(oldStatus)) {
      return true;
    }
  } else {
    mKeyStatuses.AppendElement(key);
  }

  // Only call NotifyUsable() for a key when we are going from non-usable
  // to usable state.
  if (!IsUsableStatus(aStatus.Value())) {
    return true;
  }

  auto& waiters = mWaitForKeys;
  size_t i = 0;
  while (i < waiters.Length()) {
    auto& w = waiters[i];
    if (w.mKeyId == aKeyId) {
      w.mListener->NotifyUsable(aKeyId);
      waiters.RemoveElementAt(i);
    } else {
      i++;
    }
  }
  return true;
}

void
CDMCaps::NotifyWhenKeyIdUsable(const CencKeyId& aKey,
                               SamplesWaitingForKey* aListener)
{
  MOZ_ASSERT(!IsKeyUsable(aKey));
  MOZ_ASSERT(aListener);
  mWaitForKeys.AppendElement(WaitForKeys(aKey, aListener));
}

void
CDMCaps::GetKeyStatusesForSession(const nsAString& aSessionId,
                                  nsTArray<KeyStatus>& aOutKeyStatuses)
{
  for (const KeyStatus& keyStatus : mKeyStatuses) {
    if (keyStatus.mSessionId.Equals(aSessionId)) {
      aOutKeyStatuses.AppendElement(keyStatus);
    }
  }
}

void
CDMCaps::GetSessionIdsForKeyId(const CencKeyId& aKeyId,
                               nsTArray<nsCString>& aOutSessionIds)
{
  for (const KeyStatus& keyStatus : mKeyStatuses) {
    if (keyStatus.mId == aKeyId) {
      aOutSessionIds.AppendElement(NS_ConvertUTF16toUTF8(keyStatus.mSessionId));
    }
  }
}

bool
CDMCaps::RemoveKeysForSession(const nsString& aSessionId)
{
  bool changed = false;
  nsTArray<KeyStatus> statuses;
  GetKeyStatusesForSession(aSessionId, statuses);
  for (const KeyStatus& status : statuses) {
    changed |= SetKeyStatus(status.mId,
                            aSessionId,
                            dom::Optional<dom::MediaKeyStatus>());
  }
  return changed;
}

} // namespace mozilla
