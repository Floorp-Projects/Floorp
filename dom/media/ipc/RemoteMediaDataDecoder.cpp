/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteMediaDataDecoder.h"

#include "base/thread.h"

#include "IRemoteDecoderChild.h"

namespace mozilla {

using base::Thread;

RemoteMediaDataDecoder::RemoteMediaDataDecoder(
  IRemoteDecoderChild* aChild,
  nsIThread* aManagerThread,
  AbstractThread* aAbstractManagerThread)
  : mChild(aChild)
  , mManagerThread(aManagerThread)
  , mAbstractManagerThread(aAbstractManagerThread)
{
}

RemoteMediaDataDecoder::~RemoteMediaDataDecoder()
{
  // We're about to be destroyed and drop our ref to
  // *DecoderChild. Make sure we put a ref into the
  // task queue for the *DecoderChild thread to keep
  // it alive until we send the delete message.
  RefPtr<IRemoteDecoderChild> child = mChild.forget();

  RefPtr<Runnable> task = NS_NewRunnableFunction(
    "dom::RemoteMediaDataDecoder::~RemoteMediaDataDecoder", [child]() {
      MOZ_ASSERT(child);
      child->DestroyIPDL();
    });

  // Drop our references to the child so that the last ref
  // always gets released on the manager thread.
  child = nullptr;

  mManagerThread->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
}

RefPtr<MediaDataDecoder::InitPromise>
RemoteMediaDataDecoder::Init()
{
  RefPtr<RemoteMediaDataDecoder> self = this;
  return InvokeAsync(mAbstractManagerThread,
                     __func__,
                     [self]() { return self->mChild->Init(); })
    ->Then(mAbstractManagerThread,
           __func__,
           [self, this](TrackType aTrack) {
             mDescription =
               mChild->GetDescriptionName() + NS_LITERAL_CSTRING(" (remote)");
             mIsHardwareAccelerated =
               mChild->IsHardwareAccelerated(mHardwareAcceleratedReason);
             mConversion = mChild->NeedsConversion();
             return InitPromise::CreateAndResolve(aTrack, __func__);
           },
           [self](const MediaResult& aError) {
             return InitPromise::CreateAndReject(aError, __func__);
           });
}

RefPtr<MediaDataDecoder::DecodePromise>
RemoteMediaDataDecoder::Decode(MediaRawData* aSample)
{
  RefPtr<RemoteMediaDataDecoder> self = this;
  RefPtr<MediaRawData> sample = aSample;
  return InvokeAsync(mAbstractManagerThread, __func__, [self, sample]() {
    return self->mChild->Decode(sample);
  });
}

RefPtr<MediaDataDecoder::FlushPromise>
RemoteMediaDataDecoder::Flush()
{
  RefPtr<RemoteMediaDataDecoder> self = this;
  return InvokeAsync(mAbstractManagerThread, __func__, [self]() {
    return self->mChild->Flush();
  });
}

RefPtr<MediaDataDecoder::DecodePromise>
RemoteMediaDataDecoder::Drain()
{
  RefPtr<RemoteMediaDataDecoder> self = this;
  return InvokeAsync(mAbstractManagerThread, __func__, [self]() {
    return self->mChild->Drain();
  });
}

RefPtr<ShutdownPromise>
RemoteMediaDataDecoder::Shutdown()
{
  RefPtr<RemoteMediaDataDecoder> self = this;
  return InvokeAsync(mAbstractManagerThread, __func__, [self]() {
    self->mChild->Shutdown();
    return ShutdownPromise::CreateAndResolve(true, __func__);
  });
}

bool
RemoteMediaDataDecoder::IsHardwareAccelerated(nsACString& aFailureReason) const
{
  aFailureReason = mHardwareAcceleratedReason;
  return mIsHardwareAccelerated;
}

void
RemoteMediaDataDecoder::SetSeekThreshold(const media::TimeUnit& aTime)
{
  RefPtr<RemoteMediaDataDecoder> self = this;
  media::TimeUnit time = aTime;
  mManagerThread->Dispatch(
    NS_NewRunnableFunction("dom::RemoteMediaDataDecoder::SetSeekThreshold",
                           [=]() {
                             MOZ_ASSERT(self->mChild);
                             self->mChild->SetSeekThreshold(time);
                           }),
    NS_DISPATCH_NORMAL);
}

MediaDataDecoder::ConversionRequired
RemoteMediaDataDecoder::NeedsConversion() const
{
  return mConversion;
}

nsCString
RemoteMediaDataDecoder::GetDescriptionName() const
{
  return mDescription;
}

} // namespace mozilla
