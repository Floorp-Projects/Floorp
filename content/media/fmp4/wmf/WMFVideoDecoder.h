/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(WMFVideoDecoder_h_)
#define WMFVideoDecoder_h_

#include "wmf.h"
#include "MP4Reader.h"
#include "MFTDecoder.h"
#include "nsRect.h"

#include "mozilla/RefPtr.h"

namespace mozilla {

class DXVA2Manager;

class WMFVideoDecoder : public MediaDataDecoder {
public:
  WMFVideoDecoder(bool aDXVAEnabled);
  ~WMFVideoDecoder();

  // Decode thread.
  nsresult Init(mozilla::layers::LayersBackend aLayersBackend,
                mozilla::layers::ImageContainer* aImageContainer);

  virtual nsresult Shutdown() MOZ_OVERRIDE;

  // Inserts data into the decoder's pipeline.
  virtual DecoderStatus Input(const uint8_t* aData,
                              uint32_t aLength,
                              Microseconds aDTS,
                              Microseconds aPTS,
                              int64_t aOffsetInStream) MOZ_OVERRIDE;

  // Blocks until a decoded sample is produced by the decoder.
  virtual DecoderStatus Output(nsAutoPtr<MediaData>& aOutData) MOZ_OVERRIDE;

  virtual DecoderStatus Flush() MOZ_OVERRIDE;

private:

  bool InitializeDXVA(mozilla::layers::LayersBackend aLayersBackend);

  HRESULT ConfigureVideoFrameGeometry();

  HRESULT CreateBasicVideoFrame(IMFSample* aSample,
                                VideoData** aOutVideoData);

  HRESULT CreateD3DVideoFrame(IMFSample* aSample,
                              VideoData** aOutVideoData);
  // Video frame geometry.
  VideoInfo mVideoInfo;
  uint32_t mVideoStride;
  uint32_t mVideoWidth;
  uint32_t mVideoHeight;
  nsIntRect mPictureRegion;

  // The last offset into the media resource that was passed into Input().
  // This is used to approximate the decoder's position in the media resource.
  int64_t mLastStreamOffset;

  nsAutoPtr<MFTDecoder> mDecoder;
  RefPtr<layers::ImageContainer> mImageContainer;
  nsAutoPtr<DXVA2Manager> mDXVA2Manager;

  const bool mDXVAEnabled;
  bool mUseHwAccel;
};



} // namespace mozilla

#endif