/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(WMFVideoMFTManager_h_)
#  define WMFVideoMFTManager_h_

#  include "MFTDecoder.h"
#  include "MediaResult.h"
#  include "WMF.h"
#  include "WMFMediaDataDecoder.h"
#  include "mozilla/Atomics.h"
#  include "mozilla/RefPtr.h"
#  include "nsAutoPtr.h"
#  include "mozilla/gfx/Rect.h"

namespace mozilla {

class DXVA2Manager;

class WMFVideoMFTManager : public MFTManager {
 public:
  WMFVideoMFTManager(const VideoInfo& aConfig,
                     layers::KnowsCompositor* aKnowsCompositor,
                     layers::ImageContainer* aImageContainer, float aFramerate,
                     const CreateDecoderParams::OptionSet& aOptions,
                     bool aDXVAEnabled);
  ~WMFVideoMFTManager();

  MediaResult Init();

  HRESULT Input(MediaRawData* aSample) override;

  HRESULT Output(int64_t aStreamOffset, RefPtr<MediaData>& aOutput) override;

  void Shutdown() override;

  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;

  TrackInfo::TrackType GetType() override { return TrackInfo::kVideoTrack; }

  nsCString GetDescriptionName() const override;

  MediaDataDecoder::ConversionRequired NeedsConversion() const override {
    return mStreamType == H264
               ? MediaDataDecoder::ConversionRequired::kNeedAnnexB
               : MediaDataDecoder::ConversionRequired::kNeedNone;
  }

 private:
  MediaResult ValidateVideoInfo();

  bool InitializeDXVA();

  MediaResult InitInternal();

  HRESULT CreateBasicVideoFrame(IMFSample* aSample, int64_t aStreamOffset,
                                VideoData** aOutVideoData);

  HRESULT CreateD3DVideoFrame(IMFSample* aSample, int64_t aStreamOffset,
                              VideoData** aOutVideoData);

  HRESULT SetDecoderMediaTypes();

  bool CanUseDXVA(IMFMediaType* aType, float aFramerate);

  // Video frame geometry.
  const VideoInfo mVideoInfo;
  const gfx::IntSize mImageSize;
  gfx::IntSize mDecodedImageSize;
  uint32_t mVideoStride;
  Maybe<gfx::YUVColorSpace> mColorSpace;

  RefPtr<layers::ImageContainer> mImageContainer;
  RefPtr<layers::KnowsCompositor> mKnowsCompositor;
  nsAutoPtr<DXVA2Manager> mDXVA2Manager;

  media::TimeUnit mLastDuration;

  bool mDXVAEnabled;
  bool mUseHwAccel;

  nsCString mDXVAFailureReason;

  enum StreamType { Unknown, H264, VP8, VP9 };

  StreamType mStreamType;

  const GUID& GetMFTGUID();
  const GUID& GetMediaSubtypeGUID();

  uint32_t mNullOutputCount = 0;
  bool mGotValidOutputAfterNullOutput = false;
  bool mGotExcessiveNullOutput = false;
  bool mIsValid = true;
  bool mIMFUsable = false;
  const float mFramerate;
  const bool mLowLatency;
};

}  // namespace mozilla

#endif  // WMFVideoMFTManager_h_
