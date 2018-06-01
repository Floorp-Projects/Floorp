/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SamplesWaitingForKey.h"
#include "MediaData.h"
#include "MediaEventSource.h"
#include "mozilla/CDMCaps.h"
#include "mozilla/CDMProxy.h"
#include "mozilla/TaskQueue.h"

namespace mozilla {

SamplesWaitingForKey::SamplesWaitingForKey(
  CDMProxy* aProxy, TrackInfo::TrackType aType,
  MediaEventProducer<TrackInfo::TrackType>* aOnWaitingForKey)
  : mMutex("SamplesWaitingForKey")
  , mProxy(aProxy)
  , mType(aType)
  , mOnWaitingForKeyEvent(aOnWaitingForKey)
{
}

SamplesWaitingForKey::~SamplesWaitingForKey()
{
  Flush();
}

RefPtr<SamplesWaitingForKey::WaitForKeyPromise>
SamplesWaitingForKey::WaitIfKeyNotUsable(MediaRawData* aSample)
{
  if (!aSample || !aSample->mCrypto.mValid || !mProxy) {
    return WaitForKeyPromise::CreateAndResolve(aSample, __func__);
  }
  auto caps = mProxy->Capabilites().Lock();
  const auto& keyid = aSample->mCrypto.mKeyId;
  if (caps->IsKeyUsable(keyid)) {
    return WaitForKeyPromise::CreateAndResolve(aSample, __func__);
  }
  SampleEntry entry;
  entry.mSample = aSample;
  RefPtr<WaitForKeyPromise> p = entry.mPromise.Ensure(__func__);
  {
    MutexAutoLock lock(mMutex);
    mSamples.AppendElement(std::move(entry));
  }
  if (mOnWaitingForKeyEvent) {
    mOnWaitingForKeyEvent->Notify(mType);
  }
  caps->NotifyWhenKeyIdUsable(aSample->mCrypto.mKeyId, this);
  return p;
}

void
SamplesWaitingForKey::NotifyUsable(const CencKeyId& aKeyId)
{
  MutexAutoLock lock(mMutex);
  size_t i = 0;
  while (i < mSamples.Length()) {
    auto& entry = mSamples[i];
    if (aKeyId == entry.mSample->mCrypto.mKeyId) {
      entry.mPromise.Resolve(entry.mSample, __func__);
      mSamples.RemoveElementAt(i);
    } else {
      i++;
    }
  }
}

void
SamplesWaitingForKey::Flush()
{
  MutexAutoLock lock(mMutex);
  for (auto& sample : mSamples) {
    sample.mPromise.Reject(true, __func__);
  }
  mSamples.Clear();
}

} // namespace mozilla
