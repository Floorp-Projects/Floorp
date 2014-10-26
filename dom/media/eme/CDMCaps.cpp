/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CDMCaps.h"
#include "gmp-decryption.h"
#include "EMELog.h"
#include "nsThreadUtils.h"

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
  const auto& keys = mData.mUsableKeyIds;
  for (size_t i = 0; i < keys.Length(); i++) {
    if (keys[i].mId == aKeyId) {
      return true;
    }
  }
  return false;
}

bool
CDMCaps::AutoLock::SetKeyUsable(const CencKeyId& aKeyId,
                                const nsString& aSessionId)
{
  mData.mMonitor.AssertCurrentThreadOwns();
  UsableKey key(aKeyId, aSessionId);
  if (mData.mUsableKeyIds.Contains(key)) {
    return false;
  }
  mData.mUsableKeyIds.AppendElement(key);
  auto& waiters = mData.mWaitForKeys;
  size_t i = 0;
  while (i < waiters.Length()) {
    auto& w = waiters[i];
    if (w.mKeyId == aKeyId) {
      if (waiters[i].mTarget) {
        EME_LOG("SetKeyUsable() notified waiter.");
        w.mTarget->Dispatch(w.mContinuation, NS_DISPATCH_NORMAL);
      } else {
        w.mContinuation->Run();
      }
      waiters.RemoveElementAt(i);
    } else {
      i++;
    }
  }
  return true;
}

bool
CDMCaps::AutoLock::SetKeyUnusable(const CencKeyId& aKeyId,
                                  const nsString& aSessionId)
{
  mData.mMonitor.AssertCurrentThreadOwns();
  UsableKey key(aKeyId, aSessionId);
  if (!mData.mUsableKeyIds.Contains(key)) {
    return false;
  }
  auto& keys = mData.mUsableKeyIds;
  for (size_t i = 0; i < keys.Length(); i++) {
    if (keys[i].mId == aKeyId &&
        keys[i].mSessionId == aSessionId) {
      keys.RemoveElementAt(i);
      break;
    }
  }
  return true;
}

void
CDMCaps::AutoLock::CallWhenKeyUsable(const CencKeyId& aKey,
                                     nsIRunnable* aContinuation,
                                     nsIThread* aTarget)
{
  mData.mMonitor.AssertCurrentThreadOwns();
  MOZ_ASSERT(!IsKeyUsable(aKey));
  MOZ_ASSERT(aContinuation);
  mData.mWaitForKeys.AppendElement(WaitForKeys(aKey, aContinuation, aTarget));
}

void
CDMCaps::AutoLock::DropKeysForSession(const nsAString& aSessionId)
{
  mData.mMonitor.AssertCurrentThreadOwns();
  auto& keys = mData.mUsableKeyIds;
  size_t i = 0;
  while (i < keys.Length()) {
    if (keys[i].mSessionId == aSessionId) {
      keys.RemoveElementAt(i);
    } else {
      i++;
    }
  }
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
CDMCaps::AutoLock::GetUsableKeysForSession(const nsAString& aSessionId,
                                           nsTArray<CencKeyId>& aOutKeyIds)
{
  for (size_t i = 0; i < mData.mUsableKeyIds.Length(); i++) {
    const auto& key = mData.mUsableKeyIds[i];
    if (key.mSessionId.Equals(aSessionId)) {
      aOutKeyIds.AppendElement(key.mId);
    }
  }
}

} // namespace mozilla