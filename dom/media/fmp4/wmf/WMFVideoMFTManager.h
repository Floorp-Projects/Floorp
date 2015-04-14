/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(WMFVideoMFTManager_h_)
#define WMFVideoMFTManager_h_

#include "WMF.h"
#include "MP4Reader.h"
#include "MFTDecoder.h"
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

  virtual TemporaryRef<MFTDecoder> Init() override;

  virtual HRESULT Input(MediaRawData* aSample) override;

  virtual HRESULT Output(int64_t aStreamOffset,
                         nsRefPtr<MediaData>& aOutput) override;

  virtual void Shutdown() override;

  virtual bool IsHardwareAccelerated() const override;

private:

  bool InitializeDXVA();

  HRESULT ConfigureVideoFrameGeometry();

  HRESULT CreateBasicVideoFrame(IMFSample* aSample,
                                int64_t aStreamOffset,
                                VideoData** aOutVideoData);

  HRESULT CreateD3DVideoFrame(IMFSample* aSample,
                              int64_t aStreamOffset,
                              VideoData** aOutVideoData);

  // Video frame geometry.
  VideoInfo mVideoInfo;
  uint32_t mVideoStride;
  uint32_t mVideoWidth;
  uint32_t mVideoHeight;
  nsIntRect mPictureRegion;

  RefPtr<MFTDecoder> mDecoder;
  RefPtr<layers::ImageContainer> mImageContainer;
  nsAutoPtr<DXVA2Manager> mDXVA2Manager;

  const bool mDXVAEnabled;
  const layers::LayersBackend mLayersBackend;
  bool mUseHwAccel;

  enum StreamType {
    Unknown,
    H264,
    VP8,
    VP9
  };

  StreamType mStreamType;

  const GUID& GetMFTGUID();
  const GUID& GetMediaSubtypeGUID();
};

} // namespace mozilla

#endif // WMFVideoMFTManager_h_
