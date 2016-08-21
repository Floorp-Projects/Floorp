/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaDataDecoderProxy_h_)
#define MediaDataDecoderProxy_h_

#include "PlatformDecoderModule.h"
#include "mozilla/RefPtr.h"
#include "nsThreadUtils.h"
#include "nscore.h"
#include "GMPService.h"

namespace mozilla {

class InputTask : public Runnable {
public:
  InputTask(MediaDataDecoder* aDecoder,
            MediaRawData* aSample)
   : mDecoder(aDecoder)
   , mSample(aSample)
  {}

  NS_IMETHOD Run() override {
    mDecoder->Input(mSample);
    return NS_OK;
  }

private:
  RefPtr<MediaDataDecoder> mDecoder;
  RefPtr<MediaRawData> mSample;
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
  MediaDataDecoderCallbackProxy(MediaDataDecoderProxy* aProxyDecoder,
                                MediaDataDecoderCallback* aCallback)
   : mProxyDecoder(aProxyDecoder)
   , mProxyCallback(aCallback)
  {
  }

  void Output(MediaData* aData) override {
    mProxyCallback->Output(aData);
  }

  void Error(MediaDataDecoderError aError) override;

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

  void WaitingForKey() override
  {
    mProxyCallback->WaitingForKey();
  }

private:
  MediaDataDecoderProxy* mProxyDecoder;
  MediaDataDecoderCallback* mProxyCallback;
};

class MediaDataDecoderProxy : public MediaDataDecoder {
public:
  MediaDataDecoderProxy(already_AddRefed<AbstractThread> aProxyThread,
                        MediaDataDecoderCallback* aCallback)
   : mProxyThread(aProxyThread)
   , mProxyCallback(this, aCallback)
   , mFlushComplete(false)
#if defined(DEBUG)
   , mIsShutdown(false)
#endif
  {
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
  RefPtr<InitPromise> Init() override;
  nsresult Input(MediaRawData* aSample) override;
  nsresult Flush() override;
  nsresult Drain() override;
  nsresult Shutdown() override;

  const char* GetDescriptionName() const override
  {
    return "GMP proxy data decoder";
  }

  // Called by MediaDataDecoderCallbackProxy.
  void FlushComplete();

private:
  RefPtr<InitPromise> InternalInit();

#ifdef DEBUG
  bool IsOnProxyThread() {
    return mProxyThread && mProxyThread->IsCurrentThreadIn();
  }
#endif

  friend class InputTask;
  friend class InitTask;

  RefPtr<MediaDataDecoder> mProxyDecoder;
  RefPtr<AbstractThread> mProxyThread;

  MediaDataDecoderCallbackProxy mProxyCallback;

  Condition<bool> mFlushComplete;
#if defined(DEBUG)
  bool mIsShutdown;
#endif
};

} // namespace mozilla

#endif // MediaDataDecoderProxy_h_
