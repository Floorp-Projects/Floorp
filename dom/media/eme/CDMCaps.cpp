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
  : mMonitor("CDMCaps")
  , mCaps(0)
{
}

CDMCaps::~CDMCaps()
{
}

void
CDMCaps::Lock()
{
  mMonitor.Lock();
}

void
CDMCaps::Unlock()
{
  mMonitor.Unlock();
}

bool
CDMCaps::HasCap(uint64_t aCap)
{
  mMonitor.AssertCurrentThreadOwns();
  return (mCaps & aCap) == aCap;
}

CDMCaps::AutoLock::AutoLock(CDMCaps& aInstance)
  : mData(aInstance)
{
  mData.Lock();
}

CDMCaps::AutoLock::~AutoLock()
{
  mData.Unlock();
}

void
CDMCaps::AutoLock::SetCaps(uint64_t aCaps)
{
  EME_LOG("SetCaps()");
  mData.mMonitor.AssertCurrentThreadOwns();
  mData.mCaps = aCaps;
  for (size_t i = 0; i < mData.mWaitForCaps.Length(); i++) {
    NS_DispatchToMainThread(mData.mWaitForCaps[i], NS_DISPATCH_NORMAL);
  }
  mData.mWaitForCaps.Clear();
}

void
CDMCaps::AutoLock::CallOnMainThreadWhenCapsAvailable(nsIRunnable* aContinuation)
{
  mData.mMonitor.AssertCurrentThreadOwns();
  if (mData.mCaps) {
    NS_DispatchToMainThread(aContinuation, NS_DISPATCH_NORMAL);
    MOZ_ASSERT(mData.mWaitForCaps.IsEmpty());
  } else {
    mData.mWaitForCaps.AppendElement(aContinuation);
  }
}

bool
CDMCaps::AutoLock::IsKeyUsable(const CencKeyId& aKeyId)
{
  mData.mMonitor.AssertCurrentThreadOwns();
  const auto& keys = mData.mKeyStatuses;
  for (size_t i = 0; i < keys.Length(); i++) {
    if (keys[i].mId == aKeyId && keys[i].mStatus == kGMPUsable) {
      return true;
    }
  }
  return false;
}

bool
CDMCaps::AutoLock::SetKeyStatus(const CencKeyId& aKeyId,
                                const nsString& aSessionId,
                                GMPMediaKeyStatus aStatus)
{
  mData.mMonitor.AssertCurrentThreadOwns();
  KeyStatus key(aKeyId, aSessionId, aStatus);
  auto index = mData.mKeyStatuses.IndexOf(key);

  if (aStatus == kGMPUnknown) {
    // Return true if the element is found to notify key changes.
    return mData.mKeyStatuses.RemoveElement(key);
  }

  if (index != mData.mKeyStatuses.NoIndex) {
    if (mData.mKeyStatuses[index].mStatus == aStatus) {
      return false;
    }
    mData.mKeyStatuses[index].mStatus = aStatus;
  } else {
    mData.mKeyStatuses.AppendElement(key);
  }

  if (aStatus != kGMPUsable) {
    return true;
  }

  auto& waiters = mData.mWaitForKeys;
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
CDMCaps::AutoLock::NotifyWhenKeyIdUsable(const CencKeyId& aKey,
                                         SamplesWaitingForKey* aListener)
{
  mData.mMonitor.AssertCurrentThreadOwns();
  MOZ_ASSERT(!IsKeyUsable(aKey));
  MOZ_ASSERT(aListener);
  mData.mWaitForKeys.AppendElement(WaitForKeys(aKey, aListener));
}

bool
CDMCaps::AutoLock::AreCapsKnown()
{
  mData.mMonitor.AssertCurrentThreadOwns();
  return mData.mCaps != 0;
}

bool
CDMCaps::AutoLock::CanDecryptAndDecodeAudio()
{
  return mData.HasCap(GMP_EME_CAP_DECRYPT_AND_DECODE_AUDIO);
}

bool
CDMCaps::AutoLock::CanDecryptAndDecodeVideo()
{
  return mData.HasCap(GMP_EME_CAP_DECRYPT_AND_DECODE_VIDEO);
}

bool
CDMCaps::AutoLock::CanDecryptAudio()
{
  return mData.HasCap(GMP_EME_CAP_DECRYPT_AUDIO);
}

bool
CDMCaps::AutoLock::CanDecryptVideo()
{
  return mData.HasCap(GMP_EME_CAP_DECRYPT_VIDEO);
}

void
CDMCaps::AutoLock::GetKeyStatusesForSession(const nsAString& aSessionId,
                                            nsTArray<KeyStatus>& aOutKeyStatuses)
{
  for (size_t i = 0; i < mData.mKeyStatuses.Length(); i++) {
    const auto& key = mData.mKeyStatuses[i];
    if (key.mSessionId.Equals(aSessionId)) {
      aOutKeyStatuses.AppendElement(key);
    }
  }
}

} // namespace mozilla
