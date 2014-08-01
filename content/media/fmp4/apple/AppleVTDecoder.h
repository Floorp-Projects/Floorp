/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AppleVTDecoder_h
#define mozilla_AppleVTDecoder_h

#include "PlatformDecoderModule.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsIThread.h"
#include "ReorderQueue.h"

#include "VideoToolbox/VideoToolbox.h"

namespace mozilla {

class MediaTaskQueue;
class MediaDataDecoderCallback;
namespace layers {
  class ImageContainer;
}
class FrameRef;

class AppleVTDecoder : public MediaDataDecoder {
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
  // Return hook for VideoToolbox callback.
  nsresult OutputFrame(CVPixelBufferRef aImage,
                       nsAutoPtr<FrameRef> frameRef);
private:
  const mp4_demuxer::VideoDecoderConfig& mConfig;
  RefPtr<MediaTaskQueue> mTaskQueue;
  MediaDataDecoderCallback* mCallback;
  layers::ImageContainer* mImageContainer;
  CMVideoFormatDescriptionRef mFormat;
  VTDecompressionSessionRef mSession;
  ReorderQueue mReorderQueue;

  // Method to pass a frame to VideoToolbox for decoding.
  nsresult SubmitFrame(mp4_demuxer::MP4Sample* aSample);
  // Method to set up the decompression session.
  nsresult InitializeSession();
  nsresult WaitForAsynchronousFrames();
  void DrainReorderedFrames();
};

} // namespace mozilla

#endif // mozilla_AppleVTDecoder_h
