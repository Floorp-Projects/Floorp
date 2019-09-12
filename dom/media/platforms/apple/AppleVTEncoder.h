/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AppleVTEncoder_h_
#define mozilla_AppleVTEncoder_h_

#include <CoreMedia/CoreMedia.h>
#include <VideoToolbox/VideoToolbox.h>

#include "PlatformEncoderModule.h"
#include "TimeUnits.h"

namespace mozilla {

namespace layers {
class Image;
}

class AppleVTEncoder final : public MediaDataEncoder {
 public:
  using Config = VideoConfig<H264Specific>;

  struct FrameParams {
    using TimeUnit = media::TimeUnit;

    const gfx::IntSize mSize;
    const TimeUnit mDecodeTime;
    const TimeUnit mTimestamp;
    const bool mIsKey;

    FrameParams(gfx::IntSize aSize, TimeUnit aDecodeTime, TimeUnit aTimestamp,
                bool aIsKey)
        : mSize(aSize),
          mDecodeTime(aDecodeTime),
          mTimestamp(aTimestamp),
          mIsKey(aIsKey) {}
  };

  AppleVTEncoder(const Config& aConfig, RefPtr<TaskQueue> aTaskQueue)
      : mConfig(aConfig),
        mTaskQueue(aTaskQueue),
        mFramesCompleted(false),
        mError(NS_OK),
        mSession(nullptr) {
    MOZ_ASSERT(mTaskQueue);
  }

  RefPtr<InitPromise> Init() override;
  RefPtr<EncodePromise> Encode(const MediaData* aSample) override;
  RefPtr<EncodePromise> Drain() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  RefPtr<GenericPromise> SetBitrate(Rate aBitsPerSec) override;

  nsCString GetDescriptionName() const override {
    MOZ_ASSERT(mSession);
    return mIsHardwareAccelerated
               ? NS_LITERAL_CSTRING("apple hardware VT encoder")
               : NS_LITERAL_CSTRING("apple software VT encoder");
  }

  void OutputFrame(CMSampleBufferRef aBuffer);

 private:
  virtual ~AppleVTEncoder() { MOZ_ASSERT(!mSession); }
  RefPtr<EncodePromise> ProcessEncode(RefPtr<const VideoData> aSample);
  void ProcessOutput(RefPtr<MediaRawData>&& aOutput);
  void ResolvePromise();
  RefPtr<EncodePromise> ProcessDrain();
  RefPtr<ShutdownPromise> ProcessShutdown();

  CFDictionaryRef BuildSourceImageBufferAttributes();
  CVPixelBufferRef CreateCVPixelBuffer(const layers::Image* aSource);
  bool WriteExtraData(MediaRawData* aDst, CMSampleBufferRef aSrc,
                      const bool aAsAnnexB);
  void AssertOnTaskQueue() { MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn()); }

  const Config mConfig;
  const RefPtr<TaskQueue> mTaskQueue;
  // Access only in mTaskQueue.
  EncodedData mEncodedData;
  bool mFramesCompleted;
  RefPtr<MediaByteBuffer> mAvcc;  // Stores latest avcC data.
  MediaResult mError;

  // Written by Init() but used only in task queue.
  VTCompressionSessionRef mSession;
  // Can be accessed on any thread, but only written on during init.
  Atomic<bool> mIsHardwareAccelerated;
  // Written during init and shutdown.
  Atomic<bool> mInited;
};

}  // namespace mozilla

#endif  // mozilla_AppleVTEncoder_h_
