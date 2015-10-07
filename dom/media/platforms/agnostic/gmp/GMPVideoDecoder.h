/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GMPVideoDecoder_h_)
#define GMPVideoDecoder_h_

#include "GMPVideoDecoderProxy.h"
#include "ImageContainer.h"
#include "MediaDataDecoderProxy.h"
#include "PlatformDecoderModule.h"
#include "mozIGeckoMediaPluginService.h"
#include "MediaInfo.h"

namespace mozilla {

class VideoCallbackAdapter : public GMPVideoDecoderCallbackProxy {
public:
  VideoCallbackAdapter(MediaDataDecoderCallbackProxy* aCallback,
                       VideoInfo aVideoInfo,
                       layers::ImageContainer* aImageContainer)
   : mCallback(aCallback)
   , mLastStreamOffset(0)
   , mVideoInfo(aVideoInfo)
   , mImageContainer(aImageContainer)
  {}

  // GMPVideoDecoderCallbackProxy
  void Decoded(GMPVideoi420Frame* aDecodedFrame) override;
  void ReceivedDecodedReferenceFrame(const uint64_t aPictureId) override;
  void ReceivedDecodedFrame(const uint64_t aPictureId) override;
  void InputDataExhausted() override;
  void DrainComplete() override;
  void ResetComplete() override;
  void Error(GMPErr aErr) override;
  void Terminated() override;

  void SetLastStreamOffset(int64_t aStreamOffset) {
    mLastStreamOffset = aStreamOffset;
  }

private:
  MediaDataDecoderCallbackProxy* mCallback;
  int64_t mLastStreamOffset;

  VideoInfo mVideoInfo;
  nsRefPtr<layers::ImageContainer> mImageContainer;
};

class GMPVideoDecoder : public MediaDataDecoder {
protected:
  GMPVideoDecoder(const VideoInfo& aConfig,
                  layers::LayersBackend aLayersBackend,
                  layers::ImageContainer* aImageContainer,
                  TaskQueue* aTaskQueue,
                  MediaDataDecoderCallbackProxy* aCallback,
                  VideoCallbackAdapter* aAdapter)
   : mConfig(aConfig)
   , mCallback(aCallback)
   , mGMP(nullptr)
   , mHost(nullptr)
   , mAdapter(aAdapter)
   , mConvertNALUnitLengths(false)
  {
  }

public:
  GMPVideoDecoder(const VideoInfo& aConfig,
                  layers::LayersBackend aLayersBackend,
                  layers::ImageContainer* aImageContainer,
                  TaskQueue* aTaskQueue,
                  MediaDataDecoderCallbackProxy* aCallback)
   : mConfig(aConfig)
   , mCallback(aCallback)
   , mGMP(nullptr)
   , mHost(nullptr)
   , mAdapter(new VideoCallbackAdapter(aCallback,
                                       VideoInfo(aConfig.mDisplay.width,
                                                 aConfig.mDisplay.height),
                                       aImageContainer))
   , mConvertNALUnitLengths(false)
  {
  }

  nsRefPtr<InitPromise> Init() override;
  nsresult Input(MediaRawData* aSample) override;
  nsresult Flush() override;
  nsresult Drain() override;
  nsresult Shutdown() override;

protected:
  virtual void InitTags(nsTArray<nsCString>& aTags);
  virtual nsCString GetNodeId();
  virtual GMPUniquePtr<GMPVideoEncodedFrame> CreateFrame(MediaRawData* aSample);

private:
  class GMPInitDoneRunnable : public nsRunnable
  {
  public:
    GMPInitDoneRunnable()
      : mInitDone(false),
        mThread(do_GetCurrentThread())
    {
    }

    NS_IMETHOD Run()
    {
      mInitDone = true;
      return NS_OK;
    }

    void Dispatch()
    {
      mThread->Dispatch(this, NS_DISPATCH_NORMAL);
    }

    bool IsDone()
    {
      MOZ_ASSERT(nsCOMPtr<nsIThread>(do_GetCurrentThread()) == mThread);
      return mInitDone;
    }

  private:
    bool mInitDone;
    nsCOMPtr<nsIThread> mThread;
  };

  void GetGMPAPI(GMPInitDoneRunnable* aInitDone);

  class GMPInitDoneCallback : public GetGMPVideoDecoderCallback
  {
  public:
    GMPInitDoneCallback(GMPVideoDecoder* aDecoder,
                        GMPInitDoneRunnable* aGMPInitDone)
      : mDecoder(aDecoder)
      , mGMPInitDone(aGMPInitDone)
    {
    }

    void Done(GMPVideoDecoderProxy* aGMP, GMPVideoHost* aHost) override
    {
      if (aGMP) {
        mDecoder->GMPInitDone(aGMP, aHost);
      }
      mGMPInitDone->Dispatch();
    }

  private:
    nsRefPtr<GMPVideoDecoder> mDecoder;
    nsRefPtr<GMPInitDoneRunnable> mGMPInitDone;
  };
  void GMPInitDone(GMPVideoDecoderProxy* aGMP, GMPVideoHost* aHost);

  const VideoInfo& mConfig;
  MediaDataDecoderCallbackProxy* mCallback;
  nsCOMPtr<mozIGeckoMediaPluginService> mMPS;
  GMPVideoDecoderProxy* mGMP;
  GMPVideoHost* mHost;
  nsAutoPtr<VideoCallbackAdapter> mAdapter;
  bool mConvertNALUnitLengths;
};

} // namespace mozilla

#endif // GMPVideoDecoder_h_
