/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChannelMediaDecoder.h"
#include "MediaResource.h"
#include "MediaShutdownManager.h"

namespace mozilla {

ChannelMediaDecoder::ResourceCallback::ResourceCallback(
  AbstractThread* aMainThread)
  : mAbstractMainThread(aMainThread)
{
  MOZ_ASSERT(aMainThread);
}

void
ChannelMediaDecoder::ResourceCallback::Connect(ChannelMediaDecoder* aDecoder)
{
  MOZ_ASSERT(NS_IsMainThread());
  mDecoder = aDecoder;
  mTimer = do_CreateInstance("@mozilla.org/timer;1");
  mTimer->SetTarget(mAbstractMainThread->AsEventTarget());
}

void
ChannelMediaDecoder::ResourceCallback::Disconnect()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoder) {
    mDecoder = nullptr;
    mTimer->Cancel();
    mTimer = nullptr;
  }
}

MediaDecoderOwner*
ChannelMediaDecoder::ResourceCallback::GetMediaOwner() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mDecoder ? mDecoder->GetOwner() : nullptr;
}

void
ChannelMediaDecoder::ResourceCallback::SetInfinite(bool aInfinite)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoder) {
    mDecoder->SetInfinite(aInfinite);
  }
}

void
ChannelMediaDecoder::ResourceCallback::NotifyNetworkError()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoder) {
    mDecoder->NetworkError();
  }
}

/* static */ void
ChannelMediaDecoder::ResourceCallback::TimerCallback(nsITimer* aTimer,
                                                     void* aClosure)
{
  MOZ_ASSERT(NS_IsMainThread());
  ResourceCallback* thiz = static_cast<ResourceCallback*>(aClosure);
  MOZ_ASSERT(thiz->mDecoder);
  thiz->mDecoder->NotifyDataArrivedInternal();
  thiz->mTimerArmed = false;
}

void
ChannelMediaDecoder::ResourceCallback::NotifyDataArrived()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mDecoder) {
    return;
  }

  mDecoder->DownloadProgressed();

  if (mTimerArmed) {
    return;
  }
  // In situations where these notifications come from stochastic network
  // activity, we can save significant computation by throttling the
  // calls to MediaDecoder::NotifyDataArrived() which will update the buffer
  // ranges of the reader.
  mTimerArmed = true;
  mTimer->InitWithFuncCallback(
    TimerCallback, this, sDelay, nsITimer::TYPE_ONE_SHOT);
}

void
ChannelMediaDecoder::ResourceCallback::NotifyDataEnded(nsresult aStatus)
{
  RefPtr<ResourceCallback> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=]() {
    if (!self->mDecoder) {
      return;
    }
    self->mDecoder->NotifyDownloadEnded(aStatus);
    if (NS_SUCCEEDED(aStatus)) {
      MediaDecoderOwner* owner = self->GetMediaOwner();
      MOZ_ASSERT(owner);
      owner->DownloadSuspended();

      // NotifySuspendedStatusChanged will tell the element that download
      // has been suspended "by the cache", which is true since we never
      // download anything. The element can then transition to HAVE_ENOUGH_DATA.
      self->mDecoder->NotifySuspendedStatusChanged();
    }
  });
  mAbstractMainThread->Dispatch(r.forget());
}

void
ChannelMediaDecoder::ResourceCallback::NotifyPrincipalChanged()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoder) {
    mDecoder->NotifyPrincipalChanged();
  }
}

void
ChannelMediaDecoder::ResourceCallback::NotifySuspendedStatusChanged()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoder) {
    mDecoder->NotifySuspendedStatusChanged();
  }
}

void
ChannelMediaDecoder::ResourceCallback::NotifyBytesConsumed(int64_t aBytes,
                                                           int64_t aOffset)
{
  RefPtr<ResourceCallback> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=]() {
    if (self->mDecoder) {
      self->mDecoder->NotifyBytesConsumed(aBytes, aOffset);
    }
  });
  mAbstractMainThread->Dispatch(r.forget());
}

ChannelMediaDecoder::ChannelMediaDecoder(MediaDecoderInit& aInit)
  : MediaDecoder(aInit)
  , mResourceCallback(new ResourceCallback(aInit.mOwner->AbstractMainThread()))
{
  mResourceCallback->Connect(this);
}

void
ChannelMediaDecoder::Shutdown()
{
  mResourceCallback->Disconnect();
  MediaDecoder::Shutdown();
}

nsresult
ChannelMediaDecoder::CreateResource(nsIChannel* aChannel,
                                    bool aIsPrivateBrowsing)
{
  MOZ_ASSERT(!mResource);
  mResource =
    MediaResource::Create(mResourceCallback, aChannel, aIsPrivateBrowsing);
  return mResource ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
ChannelMediaDecoder::CreateResource(MediaResource* aOriginal)
{
  MOZ_ASSERT(!mResource);
  mResource = aOriginal->CloneData(mResourceCallback);
  return mResource ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
ChannelMediaDecoder::OpenResource(nsIStreamListener** aStreamListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (aStreamListener) {
    *aStreamListener = nullptr;
  }
  return mResource->Open(aStreamListener);
}

nsresult
ChannelMediaDecoder::Load(nsIStreamListener** aStreamListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mResource, "Can't load without a MediaResource");

  nsresult rv = MediaShutdownManager::Instance().Register(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = OpenResource(aStreamListener);
  NS_ENSURE_SUCCESS(rv, rv);

  SetStateMachine(CreateStateMachine());
  NS_ENSURE_TRUE(GetStateMachine(), NS_ERROR_FAILURE);

  return InitializeStateMachine();
}

} // namespace mozilla
