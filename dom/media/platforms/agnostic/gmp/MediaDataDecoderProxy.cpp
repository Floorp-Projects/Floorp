/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDataDecoderProxy.h"
#include "MediaData.h"

namespace mozilla {

void
MediaDataDecoderCallbackProxy::Error(const MediaResult& aError)
{
  mProxyCallback->Error(aError);
}

void
MediaDataDecoderCallbackProxy::FlushComplete()
{
  mProxyDecoder->FlushComplete();
}

RefPtr<MediaDataDecoder::InitPromise>
MediaDataDecoderProxy::InternalInit()
{
  return mProxyDecoder->Init();
}

RefPtr<MediaDataDecoder::InitPromise>
MediaDataDecoderProxy::Init()
{
  MOZ_ASSERT(!mIsShutdown);

  return InvokeAsync(mProxyThread, this, __func__,
                     &MediaDataDecoderProxy::InternalInit);
}

void
MediaDataDecoderProxy::Input(MediaRawData* aSample)
{
  MOZ_ASSERT(!IsOnProxyThread());
  MOZ_ASSERT(!mIsShutdown);

  nsCOMPtr<nsIRunnable> task(new InputTask(mProxyDecoder, aSample));
  mProxyThread->Dispatch(task.forget());
}

void
MediaDataDecoderProxy::Flush()
{
  MOZ_ASSERT(!IsOnProxyThread());
  MOZ_ASSERT(!mIsShutdown);

  mFlushComplete.Set(false);

  mProxyThread->Dispatch(NewRunnableMethod(mProxyDecoder, &MediaDataDecoder::Flush));

  mFlushComplete.WaitUntil(true);
}

void
MediaDataDecoderProxy::Drain()
{
  MOZ_ASSERT(!IsOnProxyThread());
  MOZ_ASSERT(!mIsShutdown);

  mProxyThread->Dispatch(NewRunnableMethod(mProxyDecoder, &MediaDataDecoder::Drain));
}

void
MediaDataDecoderProxy::Shutdown()
{
  // Note that this *may* be called from the proxy thread also.
  MOZ_ASSERT(!mIsShutdown);
#if defined(DEBUG)
  mIsShutdown = true;
#endif
  mProxyThread->AsXPCOMThread()->Dispatch(NewRunnableMethod(mProxyDecoder,
                                                            &MediaDataDecoder::Shutdown),
                                          NS_DISPATCH_SYNC);
}

void
MediaDataDecoderProxy::FlushComplete()
{
  mFlushComplete.Set(true);
}

} // namespace mozilla
