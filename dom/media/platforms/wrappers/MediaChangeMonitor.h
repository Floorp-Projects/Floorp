/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_H264Converter_h
#define mozilla_H264Converter_h

#include "PDMFactory.h"
#include "PlatformDecoderModule.h"
#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

DDLoggedTypeDeclNameAndBase(MediaChangeMonitor, MediaDataDecoder);

// MediaChangeMonitor is a MediaDataDecoder wrapper used to ensure that
// only one type of content is fed to the underlying MediaDataDecoder.
// The MediaChangeMonitor allows playback of content where some out of band
// extra data (such as SPS NAL for H264 content) may not be provided in the
// init segment (e.g. AVC3 or Annex B) MediaChangeMonitor will monitor the
// input data, and will delay creation of the MediaDataDecoder until such out
// of band have been extracted should the underlying decoder required it.

class MediaChangeMonitor final
    : public MediaDataDecoder,
      public DecoderDoctorLifeLogger<MediaChangeMonitor> {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaChangeMonitor, final);

  static RefPtr<PlatformDecoderModule::CreateDecoderPromise> Create(
      PDMFactory* aPDMFactory, const CreateDecoderParams& aParams);

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
    return "MediaChangeMonitor decoder (pending)"_ns;
  }
  nsCString GetProcessName() const override {
    if (mDecoder) {
      return mDecoder->GetProcessName();
    }
    return "MediaChangeMonitor"_ns;
  }
  nsCString GetCodecName() const override {
    if (mDecoder) {
      return mDecoder->GetCodecName();
    }
    return "MediaChangeMonitor"_ns;
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

  class CodecChangeMonitor {
   public:
    virtual bool CanBeInstantiated() const = 0;
    virtual MediaResult CheckForChange(MediaRawData* aSample) = 0;
    virtual const TrackInfo& Config() const = 0;
    virtual MediaResult PrepareSample(
        MediaDataDecoder::ConversionRequired aConversion, MediaRawData* aSample,
        bool aNeedKeyFrame) = 0;
    virtual bool IsHardwareAccelerated(nsACString& aFailureReason) const {
      return false;
    }
    virtual ~CodecChangeMonitor() = default;
  };

 private:
  MediaChangeMonitor(PDMFactory* aPDMFactory,
                     UniquePtr<CodecChangeMonitor>&& aCodecChangeMonitor,
                     MediaDataDecoder* aDecoder,
                     const CreateDecoderParams& aParams);
  virtual ~MediaChangeMonitor();

  void AssertOnThread() const {
    // mThread may not be set if Init hasn't been called first.
    MOZ_ASSERT(!mThread || mThread->IsOnCurrentThread());
  }

  bool CanRecycleDecoder() const;

  typedef MozPromise<bool, MediaResult, true /* exclusive */>
      CreateDecoderPromise;
  // Will create the required MediaDataDecoder if need AVCC and we have a SPS
  // NAL. Returns NS_ERROR_FAILURE if error is permanent and can't be recovered
  // and will set mError accordingly.
  RefPtr<CreateDecoderPromise> CreateDecoder();
  MediaResult CreateDecoderAndInit(MediaRawData* aSample);
  MediaResult CheckForChange(MediaRawData* aSample);

  void DecodeFirstSample(MediaRawData* aSample);
  void DrainThenFlushDecoder(MediaRawData* aPendingSample);
  void FlushThenShutdownDecoder(MediaRawData* aPendingSample);
  RefPtr<ShutdownPromise> ShutdownDecoder();

  UniquePtr<CodecChangeMonitor> mChangeMonitor;
  RefPtr<PDMFactory> mPDMFactory;
  VideoInfo mCurrentConfig;
  nsCOMPtr<nsISerialEventTarget> mThread;
  RefPtr<MediaDataDecoder> mDecoder;
  MozPromiseRequestHolder<CreateDecoderPromise> mDecoderRequest;
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

  bool mNeedKeyframe = true;
  Maybe<bool> mCanRecycleDecoder;
  Maybe<MediaDataDecoder::ConversionRequired> mConversionRequired;
  bool mDecoderInitialized = false;
  const CreateDecoderParamsForAsync mParams;
};

}  // namespace mozilla

#endif  // mozilla_H264Converter_h
