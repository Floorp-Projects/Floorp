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

DDLoggedTypeDeclNameAndBase(AppleVTDecoder, MediaDataDecoder);

class AppleVTDecoder
  : public MediaDataDecoder
  , public DecoderDoctorLifeLogger<AppleVTDecoder>
{
public:
  AppleVTDecoder(const VideoInfo& aConfig,
                 TaskQueue* aTaskQueue,
                 layers::ImageContainer* aImageContainer);

  class AppleFrameRef {
  public:
    media::TimeUnit decode_timestamp;
    media::TimeUnit composition_timestamp;
    media::TimeUnit duration;
    int64_t byte_offset;
    bool is_sync_point;

    explicit AppleFrameRef(const MediaRawData& aSample)
      : decode_timestamp(aSample.mTimecode)
      , composition_timestamp(aSample.mTime)
      , duration(aSample.mDuration)
      , byte_offset(aSample.mOffset)
      , is_sync_point(aSample.mKeyframe)
    {
    }
  };

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  void SetSeekThreshold(const media::TimeUnit& aTime) override;

  bool IsHardwareAccelerated(nsACString& aFailureReason) const override
  {
    return mIsHardwareAccelerated;
  }

  nsCString GetDescriptionName() const override
  {
    return mIsHardwareAccelerated
           ? NS_LITERAL_CSTRING("apple hardware VT decoder")
           : NS_LITERAL_CSTRING("apple software VT decoder");
  }

  ConversionRequired NeedsConversion() const override
  {
    return ConversionRequired::kNeedAVCC;
  }

  // Access from the taskqueue and the decoder's thread.
  // OutputFrame is thread-safe.
  void OutputFrame(CVPixelBufferRef aImage, AppleFrameRef aFrameRef);

private:
  virtual ~AppleVTDecoder();
  RefPtr<FlushPromise> ProcessFlush();
  RefPtr<DecodePromise> ProcessDrain();
  void ProcessShutdown();
  void ProcessDecode(MediaRawData* aSample);

  void AssertOnTaskQueueThread()
  {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  }

  AppleFrameRef* CreateAppleFrameRef(const MediaRawData* aSample);
  CFDictionaryRef CreateOutputConfiguration();

  const RefPtr<MediaByteBuffer> mExtraData;
  const uint32_t mPictureWidth;
  const uint32_t mPictureHeight;
  const uint32_t mDisplayWidth;
  const uint32_t mDisplayHeight;

  // Method to set up the decompression session.
  MediaResult InitializeSession();
  nsresult WaitForAsynchronousFrames();
  CFDictionaryRef CreateDecoderSpecification();
  CFDictionaryRef CreateDecoderExtensions();

  const RefPtr<TaskQueue> mTaskQueue;
  const uint32_t mMaxRefFrames;
  const RefPtr<layers::ImageContainer> mImageContainer;
  const bool mUseSoftwareImages;

  // Set on reader/decode thread calling Flush() to indicate that output is
  // not required and so input samples on mTaskQueue need not be processed.
  // Cleared on mTaskQueue in ProcessDrain().
  Atomic<bool> mIsFlushing;
  // Protects mReorderQueue and mPromise.
  Monitor mMonitor;
  ReorderQueue mReorderQueue;
  MozPromiseHolder<DecodePromise> mPromise;

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
