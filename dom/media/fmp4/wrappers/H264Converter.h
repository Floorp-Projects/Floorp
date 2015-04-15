/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_H264Converter_h
#define mozilla_H264Converter_h

#include "PlatformDecoderModule.h"

namespace mozilla {

// H264Converter is a MediaDataDecoder wrapper used to ensure that
// only AVCC or AnnexB is fed to the underlying MediaDataDecoder.
// The H264Converter allows playback of content where the SPS NAL may not be
// provided in the init segment (e.g. AVC3 or Annex B)
// H264Converter will monitor the input data, and will delay creation of the
// MediaDataDecoder until a SPS and PPS NALs have been extracted.

class H264Converter : public MediaDataDecoder {
public:

  H264Converter(PlatformDecoderModule* aPDM,
                const VideoInfo& aConfig,
                layers::LayersBackend aLayersBackend,
                layers::ImageContainer* aImageContainer,
                FlushableMediaTaskQueue* aVideoTaskQueue,
                MediaDataDecoderCallback* aCallback);
  virtual ~H264Converter();

  virtual nsresult Init() override;
  virtual nsresult Input(MediaRawData* aSample) override;
  virtual nsresult Flush() override;
  virtual nsresult Drain() override;
  virtual nsresult Shutdown() override;
  virtual bool IsWaitingMediaResources() override;
  virtual bool IsHardwareAccelerated() const override;

  // Return true if mimetype is H.264.
  static bool IsH264(const TrackInfo& aConfig);

private:
  // Will create the required MediaDataDecoder if need AVCC and we have a SPS NAL.
  // Returns NS_ERROR_FAILURE if error is permanent and can't be recovered and
  // will set mError accordingly.
  nsresult CreateDecoder();
  nsresult CreateDecoderAndInit(MediaRawData* aSample);
  nsresult CheckForSPSChange(MediaRawData* aSample);
  void UpdateConfigFromExtraData(MediaByteBuffer* aExtraData);

  nsRefPtr<PlatformDecoderModule> mPDM;
  VideoInfo mCurrentConfig;
  layers::LayersBackend mLayersBackend;
  nsRefPtr<layers::ImageContainer> mImageContainer;
  nsRefPtr<FlushableMediaTaskQueue> mVideoTaskQueue;
  MediaDataDecoderCallback* mCallback;
  nsRefPtr<MediaDataDecoder> mDecoder;
  bool mNeedAVCC;
  nsresult mLastError;
};

} // namespace mozilla

#endif // mozilla_H264Converter_h
