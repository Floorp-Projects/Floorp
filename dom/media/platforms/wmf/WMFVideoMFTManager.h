/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(WMFVideoMFTManager_h_)
#define WMFVideoMFTManager_h_

#include "WMF.h"
#include "MFTDecoder.h"
#include "nsAutoPtr.h"
#include "nsRect.h"
#include "WMFMediaDataDecoder.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

class DXVA2Manager;

class WMFVideoMFTManager : public MFTManager {
public:
  WMFVideoMFTManager(const VideoInfo& aConfig,
                     mozilla::layers::LayersBackend aLayersBackend,
                     mozilla::layers::ImageContainer* aImageContainer,
                     bool aDXVAEnabled);
  ~WMFVideoMFTManager();

  bool Init();

  HRESULT Input(MediaRawData* aSample) override;

  HRESULT Output(int64_t aStreamOffset, RefPtr<MediaData>& aOutput) override;

  void Shutdown() override;

  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;

  TrackInfo::TrackType GetType() override {
    return TrackInfo::kVideoTrack;
  }

  const char* GetDescriptionName() const override
  {
    nsCString failureReason;
    return IsHardwareAccelerated(failureReason)
      ? "wmf hardware video decoder" : "wmf software video decoder";
  }

  void Flush() override
  {
    MFTManager::Flush();
    mDraining = false;
    mSamplesCount = 0;
  }

  void Drain() override
  {
    MFTManager::Drain();
    mDraining = true;
  }

private:

  bool ValidateVideoInfo();

  bool InitializeDXVA(bool aForceD3D9);

  bool InitInternal(bool aForceD3D9);

  HRESULT ConfigureVideoFrameGeometry();

  HRESULT CreateBasicVideoFrame(IMFSample* aSample,
                                int64_t aStreamOffset,
                                VideoData** aOutVideoData);

  HRESULT CreateD3DVideoFrame(IMFSample* aSample,
                              int64_t aStreamOffset,
                              VideoData** aOutVideoData);

  HRESULT SetDecoderMediaTypes();

  bool CanUseDXVA(IMFMediaType* aType);

  // Video frame geometry.
  VideoInfo mVideoInfo;
  uint32_t mVideoStride;
  nsIntSize mImageSize;

  RefPtr<layers::ImageContainer> mImageContainer;
  nsAutoPtr<DXVA2Manager> mDXVA2Manager;

  RefPtr<IMFSample> mLastInput;
  float mLastDuration;
  int64_t mLastTime = 0;
  bool mDraining = false;
  int64_t mSamplesCount = 0;

  bool mDXVAEnabled;
  const layers::LayersBackend mLayersBackend;
  bool mUseHwAccel;

  nsCString mDXVAFailureReason;

  enum StreamType {
    Unknown,
    H264,
    VP8,
    VP9
  };

  StreamType mStreamType;

  const GUID& GetMFTGUID();
  const GUID& GetMediaSubtypeGUID();

  uint32_t mNullOutputCount;
  bool mGotValidOutputAfterNullOutput;
  bool mGotExcessiveNullOutput;
  bool mIsValid;
};

} // namespace mozilla

#endif // WMFVideoMFTManager_h_
