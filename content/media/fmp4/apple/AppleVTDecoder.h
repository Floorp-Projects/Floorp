/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AppleVTDecoder_h
#define mozilla_AppleVTDecoder_h

#include "AppleVDADecoder.h"

#include "VideoToolbox/VideoToolbox.h"

namespace mozilla {

class AppleVTDecoder : public AppleVDADecoder {
public:
  AppleVTDecoder(const mp4_demuxer::VideoDecoderConfig& aConfig,
                 MediaTaskQueue* aVideoTaskQueue,
                 MediaDataDecoderCallback* aCallback,
                 layers::ImageContainer* aImageContainer);
  ~AppleVTDecoder();
  virtual nsresult Init() MOZ_OVERRIDE;
  virtual nsresult Input(mp4_demuxer::MP4Sample* aSample) MOZ_OVERRIDE;
  virtual nsresult Flush() MOZ_OVERRIDE;
  virtual nsresult Drain() MOZ_OVERRIDE;
  virtual nsresult Shutdown() MOZ_OVERRIDE;

private:
  CMVideoFormatDescriptionRef mFormat;
  VTDecompressionSessionRef mSession;

  // Method to pass a frame to VideoToolbox for decoding.
  nsresult SubmitFrame(mp4_demuxer::MP4Sample* aSample);
  // Method to set up the decompression session.
  nsresult InitializeSession();
  nsresult WaitForAsynchronousFrames();
  CFDictionaryRef CreateDecoderSpecification();
  CFDictionaryRef CreateDecoderExtensions();
};

} // namespace mozilla

#endif // mozilla_AppleVTDecoder_h
