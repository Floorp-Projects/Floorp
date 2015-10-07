/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaDataDecoderProxy_h_)
#define MediaDataDecoderProxy_h_

#include "PlatformDecoderModule.h"
#include "mozilla/nsRefPtr.h"
#include "nsThreadUtils.h"
#include "nscore.h"

namespace mozilla {

class InputTask : public nsRunnable {
public:
  InputTask(MediaDataDecoder* aDecoder,
            MediaRawData* aSample)
   : mDecoder(aDecoder)
   , mSample(aSample)
  {}

  NS_IMETHOD Run() {
    mDecoder->Input(mSample);
    return NS_OK;
  }

private:
  nsRefPtr<MediaDataDecoder> mDecoder;
  nsRefPtr<MediaRawData> mSample;
};

template<typename T>
class Condition {
public:
  explicit Condition(T aValue)
   : mMonitor("Condition")
   , mCondition(aValue)
  {}

  void Set(T aValue) {
    MonitorAutoLock mon(mMonitor);
    mCondition = aValue;
    mon.NotifyAll();
  }

  void WaitUntil(T aValue) {
    MonitorAutoLock mon(mMonitor);
    while (mCondition != aValue) {
      mon.Wait();
    }
  }

private:
  Monitor mMonitor;
  T mCondition;
};

class MediaDataDecoderProxy;

class MediaDataDecoderCallbackProxy : public MediaDataDecoderCallback {
public:
  MediaDataDecoderCallbackProxy(MediaDataDecoderProxy* aProxyDecoder, MediaDataDecoderCallback* aCallback)
   : mProxyDecoder(aProxyDecoder)
   , mProxyCallback(aCallback)
  {
  }

  void Output(MediaData* aData) override {
    mProxyCallback->Output(aData);
  }

  void Error() override;

  void InputExhausted() override {
    mProxyCallback->InputExhausted();
  }

  void DrainComplete() override {
    mProxyCallback->DrainComplete();
  }

  void ReleaseMediaResources() override {
    mProxyCallback->ReleaseMediaResources();
  }

  void FlushComplete();

  bool OnReaderTaskQueue() override
  {
    return mProxyCallback->OnReaderTaskQueue();
  }

private:
  MediaDataDecoderProxy* mProxyDecoder;
  MediaDataDecoderCallback* mProxyCallback;
};

class MediaDataDecoderProxy : public MediaDataDecoder {
public:
  MediaDataDecoderProxy(nsIThread* aProxyThread, MediaDataDecoderCallback* aCallback)
   : mProxyThread(aProxyThread)
   , mProxyCallback(this, aCallback)
   , mFlushComplete(false)
#if defined(DEBUG)
   , mIsShutdown(false)
#endif
  {
    mProxyThreadWrapper = CreateXPCOMAbstractThreadWrapper(aProxyThread, false);
  }

  // Ideally, this would return a regular MediaDataDecoderCallback pointer
  // to retain the clean abstraction, but until MediaDataDecoderCallback
  // supports the FlushComplete interface, this will have to do.  When MDDC
  // supports FlushComplete, this, the GMP*Decoders, and the
  // *CallbackAdapters can be reverted to accepting a regular
  // MediaDataDecoderCallback pointer.
  MediaDataDecoderCallbackProxy* Callback()
  {
    return &mProxyCallback;
  }

  void SetProxyTarget(MediaDataDecoder* aProxyDecoder)
  {
    MOZ_ASSERT(aProxyDecoder);
    mProxyDecoder = aProxyDecoder;
  }

  // These are called from the decoder thread pool.
  // Init and Shutdown run synchronously on the proxy thread, all others are
  // asynchronously and responded to via the MediaDataDecoderCallback.
  // Note: the nsresults returned by the proxied decoder are lost.
  nsRefPtr<InitPromise> Init() override;
  nsresult Input(MediaRawData* aSample) override;
  nsresult Flush() override;
  nsresult Drain() override;
  nsresult Shutdown() override;

  // Called by MediaDataDecoderCallbackProxy.
  void FlushComplete();

private:
  nsRefPtr<InitPromise> InternalInit();

#ifdef DEBUG
  bool IsOnProxyThread() {
    return NS_GetCurrentThread() == mProxyThread;
  }
#endif

  friend class InputTask;
  friend class InitTask;

  nsRefPtr<MediaDataDecoder> mProxyDecoder;
  nsCOMPtr<nsIThread> mProxyThread;
  nsRefPtr<AbstractThread> mProxyThreadWrapper;

  MediaDataDecoderCallbackProxy mProxyCallback;

  Condition<bool> mFlushComplete;
#if defined(DEBUG)
  bool mIsShutdown;
#endif
};

} // namespace mozilla

#endif // MediaDataDecoderProxy_h_
