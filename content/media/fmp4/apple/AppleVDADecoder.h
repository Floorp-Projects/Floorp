/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AppleVDADecoder_h
#define mozilla_AppleVDADecoder_h

#include "PlatformDecoderModule.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ReentrantMonitor.h"
#include "MP4Reader.h"
#include "MP4Decoder.h"
#include "nsIThread.h"
#include "ReorderQueue.h"

#include "VideoDecodeAcceleration/VDADecoder.h"

namespace mozilla {

class MediaTaskQueue;
class MediaDataDecoderCallback;
namespace layers {
  class ImageContainer;
}

class AppleFrameRef;

class AppleVDADecoder : public MediaDataDecoder {
public:
  // Return a new created AppleVDADecoder or nullptr if media or hardware is
  // not supported by current configuration.
  static already_AddRefed<AppleVDADecoder> CreateVDADecoder(
    const mp4_demuxer::VideoDecoderConfig& aConfig,
    MediaTaskQueue* aVideoTaskQueue,
    MediaDataDecoderCallback* aCallback,
    layers::ImageContainer* aImageContainer);

  AppleVDADecoder(const mp4_demuxer::VideoDecoderConfig& aConfig,
                  MediaTaskQueue* aVideoTaskQueue,
                  MediaDataDecoderCallback* aCallback,
                  layers::ImageContainer* aImageContainer);
  ~AppleVDADecoder();
  virtual nsresult Init() MOZ_OVERRIDE;
  virtual nsresult Input(mp4_demuxer::MP4Sample* aSample) MOZ_OVERRIDE;
  virtual nsresult Flush() MOZ_OVERRIDE;
  virtual nsresult Drain() MOZ_OVERRIDE;
  virtual nsresult Shutdown() MOZ_OVERRIDE;

  nsresult OutputFrame(CVPixelBufferRef aImage,
                       nsAutoPtr<AppleFrameRef> aFrameRef);

 protected:
  void DrainReorderedFrames();
  void ClearReorderedFrames();
  CFDictionaryRef CreateOutputConfiguration();

  const mp4_demuxer::VideoDecoderConfig& mConfig;
  RefPtr<MediaTaskQueue> mTaskQueue;
  MediaDataDecoderCallback* mCallback;
  layers::ImageContainer* mImageContainer;
  ReorderQueue mReorderQueue;

private:
  VDADecoder mDecoder;

  // Method to pass a frame to VideoToolbox for decoding.
  nsresult SubmitFrame(mp4_demuxer::MP4Sample* aSample);
  // Method to set up the decompression session.
  nsresult InitializeSession();
  CFDictionaryRef CreateDecoderSpecification();
};

} // namespace mozilla

#endif // mozilla_AppleVDADecoder_h
