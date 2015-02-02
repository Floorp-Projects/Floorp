/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaDataDecoderProxy_h_)
#define MediaDataDecoderProxy_h_

#include "PlatformDecoderModule.h"
#include "mp4_demuxer/DecoderData.h"
#include "nsAutoPtr.h"
#include "nsRefPtr.h"
#include "nsThreadUtils.h"
#include "nscore.h"

namespace mozilla {

class InputTask : public nsRunnable {
public:
  InputTask(MediaDataDecoder* aDecoder,
            mp4_demuxer::MP4Sample* aSample)
   : mDecoder(aDecoder)
   , mSample(aSample)
  {}

  NS_IMETHOD Run() {
    mDecoder->Input(mSample.forget());
    return NS_OK;
  }

private:
  nsRefPtr<MediaDataDecoder> mDecoder;
  nsAutoPtr<mp4_demuxer::MP4Sample> mSample;
};

class InitTask : public nsRunnable {
public:
  explicit InitTask(MediaDataDecoder* aDecoder)
   : mDecoder(aDecoder)
   , mResultValid(false)
  {}

  NS_IMETHOD Run() {
    mResult = mDecoder->Init();
    mResultValid = true;
    return NS_OK;
  }

  nsresult Result() {
    MOZ_ASSERT(mResultValid);
    return mResult;
  }

private:
  MediaDataDecoder* mDecoder;
  nsresult mResult;
  bool mResultValid;
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
  explicit MediaDataDecoderCallbackProxy(MediaDataDecoderProxy* aProxyDecoder, MediaDataDecoderCallback* aCallback)
   : mProxyDecoder(aProxyDecoder)
   , mProxyCallback(aCallback)
  {
  }

  virtual void Output(MediaData* aData) MOZ_OVERRIDE {
    mProxyCallback->Output(aData);
  }

  virtual void Error() MOZ_OVERRIDE;

  virtual void InputExhausted() MOZ_OVERRIDE {
    mProxyCallback->InputExhausted();
  }

  virtual void DrainComplete() MOZ_OVERRIDE {
    mProxyCallback->DrainComplete();
  }

  virtual void NotifyResourcesStatusChanged() MOZ_OVERRIDE {
    mProxyCallback->NotifyResourcesStatusChanged();
  }

  virtual void ReleaseMediaResources() MOZ_OVERRIDE {
    mProxyCallback->ReleaseMediaResources();
  }

  virtual void FlushComplete();

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
  virtual nsresult Init() MOZ_OVERRIDE;
  virtual nsresult Input(mp4_demuxer::MP4Sample* aSample) MOZ_OVERRIDE;
  virtual nsresult Flush() MOZ_OVERRIDE;
  virtual nsresult Drain() MOZ_OVERRIDE;
  virtual nsresult Shutdown() MOZ_OVERRIDE;

  // Called by MediaDataDecoderCallbackProxy.
  void FlushComplete();

private:
#ifdef DEBUG
  bool IsOnProxyThread() {
    return NS_GetCurrentThread() == mProxyThread;
  }
#endif

  friend class InputTask;
  friend class InitTask;

  nsRefPtr<MediaDataDecoder> mProxyDecoder;
  nsCOMPtr<nsIThread> mProxyThread;

  MediaDataDecoderCallbackProxy mProxyCallback;

  Condition<bool> mFlushComplete;
#if defined(DEBUG)
  bool mIsShutdown;
#endif
};

} // namespace mozilla

#endif // MediaDataDecoderProxy_h_
