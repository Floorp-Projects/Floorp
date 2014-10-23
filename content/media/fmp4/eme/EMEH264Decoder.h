/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMEH264Decoder_h_
#define EMEH264Decoder_h_

#include "PlatformDecoderModule.h"
#include "mp4_demuxer/DecoderData.h"
#include "ImageContainer.h"
#include "GMPVideoDecoderProxy.h"
#include "mozIGeckoMediaPluginService.h"

namespace mozilla {

class CDMProxy;
class MediaTaskQueue;

class EMEH264Decoder : public MediaDataDecoder
                     , public GMPVideoDecoderCallbackProxy
{
  typedef mp4_demuxer::MP4Sample MP4Sample;
public:
  EMEH264Decoder(CDMProxy* aProxy,
                 const mp4_demuxer::VideoDecoderConfig& aConfig,
                 layers::LayersBackend aLayersBackend,
                 layers::ImageContainer* aImageContainer,
                 MediaTaskQueue* aTaskQueue,
                 MediaDataDecoderCallback* aCallback);
  ~EMEH264Decoder();

  // MediaDataDecoder
  virtual nsresult Init() MOZ_OVERRIDE;
  virtual nsresult Input(MP4Sample* aSample) MOZ_OVERRIDE;
  virtual nsresult Flush() MOZ_OVERRIDE;
  virtual nsresult Drain() MOZ_OVERRIDE;
  virtual nsresult Shutdown() MOZ_OVERRIDE;

  // GMPVideoDecoderProxyCallback
  virtual void Decoded(GMPVideoi420Frame* aDecodedFrame) MOZ_OVERRIDE;
  virtual void ReceivedDecodedReferenceFrame(const uint64_t aPictureId) MOZ_OVERRIDE;
  virtual void ReceivedDecodedFrame(const uint64_t aPictureId) MOZ_OVERRIDE;
  virtual void InputDataExhausted() MOZ_OVERRIDE;
  virtual void DrainComplete() MOZ_OVERRIDE;
  virtual void ResetComplete() MOZ_OVERRIDE;
  virtual void Error(GMPErr aErr) MOZ_OVERRIDE;
  virtual void Terminated() MOZ_OVERRIDE;

private:

  nsresult GmpInit();
  nsresult GmpInput(MP4Sample* aSample);
  void GmpFlush();
  void GmpDrain();
  void GmpShutdown();

#ifdef DEBUG
  bool IsOnGMPThread() {
    return NS_GetCurrentThread() == mGMPThread;
  }
#endif

  class DeliverSample : public nsRunnable {
  public:
    DeliverSample(EMEH264Decoder* aDecoder,
                  mp4_demuxer::MP4Sample* aSample)
      : mDecoder(aDecoder)
      , mSample(aSample)
    {}

    NS_IMETHOD Run() {
      mDecoder->GmpInput(mSample.forget());
      return NS_OK;
    }
  private:
    nsRefPtr<EMEH264Decoder> mDecoder;
    nsAutoPtr<mp4_demuxer::MP4Sample> mSample;
  };

  class InitTask : public nsRunnable {
  public:
    explicit InitTask(EMEH264Decoder* aDecoder)
      : mDecoder(aDecoder)
    {}
    NS_IMETHOD Run() {
      mResult = mDecoder->GmpInit();
      return NS_OK;
    }
    nsresult mResult;
    EMEH264Decoder* mDecoder;
  };

  nsCOMPtr<mozIGeckoMediaPluginService> mMPS;
  nsCOMPtr<nsIThread> mGMPThread;
  nsRefPtr<CDMProxy> mProxy;
  GMPVideoDecoderProxy* mGMP;
  GMPVideoHost* mHost;

  VideoInfo mVideoInfo;
  const mp4_demuxer::VideoDecoderConfig& mConfig;
  nsRefPtr<layers::ImageContainer> mImageContainer;
  nsRefPtr<MediaTaskQueue> mTaskQueue;
  MediaDataDecoderCallback* mCallback;
  int64_t mLastStreamOffset;
  Monitor mMonitor;
  bool mFlushComplete;
};

}

#endif // EMEH264Decoder_h_
