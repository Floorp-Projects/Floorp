/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AppleVTDecoder_h
#define mozilla_AppleVTDecoder_h

#include "PlatformDecoderModule.h"
#include "mozilla/Atomics.h"
#include "nsIThread.h"
#include "ReorderQueue.h"
#include "TimeUnits.h"

#include "VideoToolbox/VideoToolbox.h"

namespace mozilla {

class AppleVTDecoder : public MediaDataDecoder {
public:
  AppleVTDecoder(const VideoInfo& aConfig,
                 TaskQueue* aTaskQueue,
                 MediaDataDecoderCallback* aCallback,
                 layers::ImageContainer* aImageContainer);

  class AppleFrameRef {
  public:
    media::TimeUnit decode_timestamp;
    media::TimeUnit composition_timestamp;
    media::TimeUnit duration;
    int64_t byte_offset;
    bool is_sync_point;

    explicit AppleFrameRef(const MediaRawData& aSample)
      : decode_timestamp(media::TimeUnit::FromMicroseconds(aSample.mTimecode))
      , composition_timestamp(media::TimeUnit::FromMicroseconds(aSample.mTime))
      , duration(media::TimeUnit::FromMicroseconds(aSample.mDuration))
      , byte_offset(aSample.mOffset)
      , is_sync_point(aSample.mKeyframe)
    {
    }
  };

  RefPtr<InitPromise> Init() override;
  nsresult Input(MediaRawData* aSample) override;
  nsresult Flush() override;
  nsresult Drain() override;
  nsresult Shutdown() override;
  void SetSeekThreshold(const media::TimeUnit& aTime) override;

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

  // Access from the taskqueue and the decoder's thread.
  // OutputFrame is thread-safe.
  nsresult OutputFrame(CVPixelBufferRef aImage,
                       AppleFrameRef aFrameRef);

private:
  virtual ~AppleVTDecoder();
  void ProcessFlush();
  void ProcessDrain();
  void ProcessShutdown();
  nsresult ProcessDecode(MediaRawData* aSample);

  void AssertOnTaskQueueThread()
  {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  }

  AppleFrameRef* CreateAppleFrameRef(const MediaRawData* aSample);
  void DrainReorderedFrames();
  void ClearReorderedFrames();
  CFDictionaryRef CreateOutputConfiguration();

  const RefPtr<MediaByteBuffer> mExtraData;
  MediaDataDecoderCallback* mCallback;
  const uint32_t mPictureWidth;
  const uint32_t mPictureHeight;
  const uint32_t mDisplayWidth;
  const uint32_t mDisplayHeight;

  // Number of times a sample was queued via Input(). Will be decreased upon
  // the decoder's callback being invoked.
  // This is used to calculate how many frames has been buffered by the decoder.
  Atomic<uint32_t> mQueuedSamples;

  // Method to set up the decompression session.
  nsresult InitializeSession();
  nsresult WaitForAsynchronousFrames();
  CFDictionaryRef CreateDecoderSpecification();
  CFDictionaryRef CreateDecoderExtensions();
  // Method to pass a frame to VideoToolbox for decoding.
  nsresult DoDecode(MediaRawData* aSample);

  const RefPtr<TaskQueue> mTaskQueue;
  const uint32_t mMaxRefFrames;
  const RefPtr<layers::ImageContainer> mImageContainer;
  // Increased when Input is called, and decreased when ProcessFrame runs.
  // Reaching 0 indicates that there's no pending Input.
  Atomic<uint32_t> mInputIncoming;
  Atomic<bool> mIsShutDown;
  const bool mUseSoftwareImages;

  // Set on reader/decode thread calling Flush() to indicate that output is
  // not required and so input samples on mTaskQueue need not be processed.
  // Cleared on mTaskQueue in ProcessDrain().
  Atomic<bool> mIsFlushing;
  // Protects mReorderQueue.
  Monitor mMonitor;
  ReorderQueue mReorderQueue;
  // Decoded frame will be dropped if its pts is smaller than this
  // value. It shold be initialized before Input() or after Flush(). So it is
  // safe to access it in OutputFrame without protecting.
  Maybe<media::TimeUnit> mSeekTargetThreshold;

  CMVideoFormatDescriptionRef mFormat;
  VTDecompressionSessionRef mSession;
  Atomic<bool> mIsHardwareAccelerated;
};

} // namespace mozilla

#endif // mozilla_AppleVTDecoder_h
