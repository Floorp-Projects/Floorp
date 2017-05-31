/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_H264Converter_h
#define mozilla_H264Converter_h

#include "PlatformDecoderModule.h"
#include "mozilla/Maybe.h"

namespace mozilla {

class DecoderDoctorDiagnostics;

// H264Converter is a MediaDataDecoder wrapper used to ensure that
// only AVCC or AnnexB is fed to the underlying MediaDataDecoder.
// The H264Converter allows playback of content where the SPS NAL may not be
// provided in the init segment (e.g. AVC3 or Annex B)
// H264Converter will monitor the input data, and will delay creation of the
// MediaDataDecoder until a SPS and PPS NALs have been extracted.

class H264Converter : public MediaDataDecoder
{
public:

  H264Converter(PlatformDecoderModule* aPDM,
                const CreateDecoderParams& aParams);
  virtual ~H264Converter();

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;
  const char* GetDescriptionName() const override
  {
    if (mDecoder) {
      return mDecoder->GetDescriptionName();
    }
    return "H264Converter decoder (pending)";
  }
  void SetSeekThreshold(const media::TimeUnit& aTime) override;
  bool SupportDecoderRecycling() const override
  {
    if (mDecoder) {
      return mDecoder->SupportDecoderRecycling();
    }
    return false;
  }

  ConversionRequired NeedsConversion() const override
  {
    if (mDecoder) {
      return mDecoder->NeedsConversion();
    }
    // Default so no conversion is performed.
    return ConversionRequired::kNeedAVCC;
  }
  nsresult GetLastError() const { return mLastError; }

private:
  // Will create the required MediaDataDecoder if need AVCC and we have a SPS NAL.
  // Returns NS_ERROR_FAILURE if error is permanent and can't be recovered and
  // will set mError accordingly.
  nsresult CreateDecoder(const VideoInfo& aConfig,
                         DecoderDoctorDiagnostics* aDiagnostics);
  nsresult CreateDecoderAndInit(MediaRawData* aSample);
  nsresult CheckForSPSChange(MediaRawData* aSample);
  void UpdateConfigFromExtraData(MediaByteBuffer* aExtraData);

  void OnDecoderInitDone(const TrackType aTrackType);
  void OnDecoderInitFailed(const MediaResult& aError);

  bool CanRecycleDecoder() const;

  void DecodeFirstSample(MediaRawData* aSample);

  RefPtr<PlatformDecoderModule> mPDM;
  const VideoInfo mOriginalConfig;
  VideoInfo mCurrentConfig;
  RefPtr<layers::KnowsCompositor> mKnowsCompositor;
  RefPtr<layers::ImageContainer> mImageContainer;
  const RefPtr<TaskQueue> mTaskQueue;
  RefPtr<MediaRawData> mPendingSample;
  RefPtr<MediaDataDecoder> mDecoder;
  MozPromiseRequestHolder<InitPromise> mInitPromiseRequest;
  MozPromiseRequestHolder<DecodePromise> mDecodePromiseRequest;
  MozPromiseHolder<DecodePromise> mDecodePromise;
  MozPromiseRequestHolder<FlushPromise> mFlushRequest;
  MozPromiseRequestHolder<ShutdownPromise> mShutdownRequest;
  RefPtr<ShutdownPromise> mShutdownPromise;

  RefPtr<GMPCrashHelper> mGMPCrashHelper;
  Maybe<bool> mNeedAVCC;
  nsresult mLastError;
  bool mNeedKeyframe = true;
  const TrackInfo::TrackType mType;
  MediaEventProducer<TrackInfo::TrackType>* const mOnWaitingForKeyEvent;
  const CreateDecoderParams::OptionSet mDecoderOptions;
  Maybe<bool> mCanRecycleDecoder;
};

} // namespace mozilla

#endif // mozilla_H264Converter_h
