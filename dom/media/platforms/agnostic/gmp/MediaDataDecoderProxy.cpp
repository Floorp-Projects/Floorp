/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDataDecoderProxy.h"
#include "MediaData.h"

namespace mozilla {

void
MediaDataDecoderCallbackProxy::Error()
{
  mProxyCallback->Error();
}

void
MediaDataDecoderCallbackProxy::FlushComplete()
{
  mProxyDecoder->FlushComplete();
}

nsresult
MediaDataDecoderProxy::Init()
{
  MOZ_ASSERT(!mIsShutdown);
  nsRefPtr<InitTask> task(new InitTask(mProxyDecoder));
  nsresult rv = mProxyThread->Dispatch(task, NS_DISPATCH_SYNC);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_SUCCESS(task->Result(), task->Result());

  return NS_OK;
}

nsresult
MediaDataDecoderProxy::Input(MediaRawData* aSample)
{
  MOZ_ASSERT(!IsOnProxyThread());
  MOZ_ASSERT(!mIsShutdown);

  nsCOMPtr<nsIRunnable> task(new InputTask(mProxyDecoder, aSample));
  nsresult rv = mProxyThread->Dispatch(task, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
MediaDataDecoderProxy::Flush()
{
  MOZ_ASSERT(!IsOnProxyThread());
  MOZ_ASSERT(!mIsShutdown);

  mFlushComplete.Set(false);

  nsCOMPtr<nsIRunnable> task;
  task = NS_NewRunnableMethod(mProxyDecoder, &MediaDataDecoder::Flush);
  nsresult rv = mProxyThread->Dispatch(task, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  mFlushComplete.WaitUntil(true);

  return NS_OK;
}

nsresult
MediaDataDecoderProxy::Drain()
{
  MOZ_ASSERT(!IsOnProxyThread());
  MOZ_ASSERT(!mIsShutdown);

  nsCOMPtr<nsIRunnable> task;
  task = NS_NewRunnableMethod(mProxyDecoder, &MediaDataDecoder::Drain);
  nsresult rv = mProxyThread->Dispatch(task, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
MediaDataDecoderProxy::Shutdown()
{
  // Note that this *may* be called from the proxy thread also.
  MOZ_ASSERT(!mIsShutdown);
#if defined(DEBUG)
  mIsShutdown = true;
#endif
  nsCOMPtr<nsIRunnable> task;
  task = NS_NewRunnableMethod(mProxyDecoder, &MediaDataDecoder::Shutdown);
  nsresult rv = mProxyThread->Dispatch(task, NS_DISPATCH_SYNC);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

void
MediaDataDecoderProxy::FlushComplete()
{
  mFlushComplete.Set(true);
}

} // namespace mozilla
