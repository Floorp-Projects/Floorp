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
  AppleVTDecoder(const VideoInfo& aConfig,
                 FlushableTaskQueue* aVideoTaskQueue,
                 MediaDataDecoderCallback* aCallback,
                 layers::ImageContainer* aImageContainer);
  virtual ~AppleVTDecoder();
  RefPtr<InitPromise> Init() override;
  nsresult Input(MediaRawData* aSample) override;
  bool IsHardwareAccelerated(nsACString& aFailureReason) const override
  {
    return mIsHardwareAccelerated;
  }

  const char* GetDescriptionName() const override
  {
    return mIsHardwareAccelerated
      ? "apple hardware VT decoder"
      : "apple software VT decoder";
  }

protected:
  void ProcessFlush() override;
  void ProcessDrain() override;
  void ProcessShutdown() override;

private:
  CMVideoFormatDescriptionRef mFormat;
  VTDecompressionSessionRef mSession;

  // Method to pass a frame to VideoToolbox for decoding.
  nsresult ProcessDecode(MediaRawData* aSample);
  // Method to set up the decompression session.
  nsresult InitializeSession();
  nsresult WaitForAsynchronousFrames();
  CFDictionaryRef CreateDecoderSpecification();
  CFDictionaryRef CreateDecoderExtensions();
  Atomic<bool> mIsHardwareAccelerated;
};

} // namespace mozilla

#endif // mozilla_AppleVTDecoder_h
