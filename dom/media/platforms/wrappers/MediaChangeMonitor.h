/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_H264Converter_h
#define mozilla_H264Converter_h

#include "PlatformDecoderModule.h"
#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

class DecoderDoctorDiagnostics;

DDLoggedTypeDeclNameAndBase(MediaChangeMonitor, MediaDataDecoder);

// MediaChangeMonitor is a MediaDataDecoder wrapper used to ensure that
// only one type of content is fed to the underlying MediaDataDecoder.
// The MediaChangeMonitor allows playback of content where some out of band
// extra data (such as SPS NAL for H264 content) may not be provided in the
// init segment (e.g. AVC3 or Annex B) MediaChangeMonitor will monitor the
// input data, and will delay creation of the MediaDataDecoder until such out
// of band have been extracted should the underlying decoder required it.

class MediaChangeMonitor : public MediaDataDecoder,
                           public DecoderDoctorLifeLogger<MediaChangeMonitor> {
 public:
  MediaChangeMonitor(PlatformDecoderModule* aPDM,
                     const CreateDecoderParams& aParams);
  virtual ~MediaChangeMonitor();

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;
  nsCString GetDescriptionName() const override {
    if (mDecoder) {
      return mDecoder->GetDescriptionName();
    }
    return NS_LITERAL_CSTRING("MediaChangeMonitor decoder (pending)");
  }
  void SetSeekThreshold(const media::TimeUnit& aTime) override;
  bool SupportDecoderRecycling() const override {
    if (mDecoder) {
      return mDecoder->SupportDecoderRecycling();
    }
    return false;
  }

  ConversionRequired NeedsConversion() const override {
    if (mDecoder) {
      return mDecoder->NeedsConversion();
    }
    // Default so no conversion is performed.
    return ConversionRequired::kNeedNone;
  }
  MediaResult GetLastError() const { return mLastError; }

  class CodecChangeMonitor {
   public:
    virtual bool CanBeInstantiated() const = 0;
    virtual MediaResult CheckForChange(MediaRawData* aSample) = 0;
    virtual const TrackInfo& Config() const = 0;
    virtual MediaResult PrepareSample(
        MediaDataDecoder::ConversionRequired aConversion, MediaRawData* aSample,
        bool aNeedKeyFrame) = 0;
    virtual ~CodecChangeMonitor() = default;
  };

 private:
  UniquePtr<CodecChangeMonitor> mChangeMonitor;

  void AssertOnTaskQueue() const {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  }

  bool CanRecycleDecoder() const;

  // Will create the required MediaDataDecoder if need AVCC and we have a SPS
  // NAL. Returns NS_ERROR_FAILURE if error is permanent and can't be recovered
  // and will set mError accordingly.
  MediaResult CreateDecoder(DecoderDoctorDiagnostics* aDiagnostics);
  MediaResult CreateDecoderAndInit(MediaRawData* aSample);
  MediaResult CheckForChange(MediaRawData* aSample);

  void DecodeFirstSample(MediaRawData* aSample);
  void DrainThenFlushDecoder(MediaRawData* aPendingSample);
  void FlushThenShutdownDecoder(MediaRawData* aPendingSample);
  RefPtr<ShutdownPromise> ShutdownDecoder();

  RefPtr<PlatformDecoderModule> mPDM;
  VideoInfo mCurrentConfig;
  RefPtr<layers::KnowsCompositor> mKnowsCompositor;
  RefPtr<layers::ImageContainer> mImageContainer;
  const RefPtr<TaskQueue> mTaskQueue;
  RefPtr<MediaDataDecoder> mDecoder;
  MozPromiseRequestHolder<InitPromise> mInitPromiseRequest;
  MozPromiseHolder<InitPromise> mInitPromise;
  MozPromiseRequestHolder<DecodePromise> mDecodePromiseRequest;
  MozPromiseHolder<DecodePromise> mDecodePromise;
  MozPromiseRequestHolder<FlushPromise> mFlushRequest;
  MediaDataDecoder::DecodedData mPendingFrames;
  MozPromiseRequestHolder<DecodePromise> mDrainRequest;
  MozPromiseRequestHolder<ShutdownPromise> mShutdownRequest;
  RefPtr<ShutdownPromise> mShutdownPromise;
  MozPromiseHolder<FlushPromise> mFlushPromise;

  RefPtr<GMPCrashHelper> mGMPCrashHelper;
  MediaResult mLastError;
  bool mNeedKeyframe = true;
  const bool mErrorIfNoInitializationData;
  const TrackInfo::TrackType mType;
  MediaEventProducer<TrackInfo::TrackType>* const mOnWaitingForKeyEvent;
  const CreateDecoderParams::OptionSet mDecoderOptions;
  const CreateDecoderParams::VideoFrameRate mRate;
  Maybe<bool> mCanRecycleDecoder;
  Maybe<MediaDataDecoder::ConversionRequired> mConversionRequired;
  // Used for debugging purposes only
  Atomic<bool> mInConstructor;
};

}  // namespace mozilla

#endif  // mozilla_H264Converter_h
