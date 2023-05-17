/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/KnowsCompositor.h"
#if !defined(GMPVideoDecoder_h_)
#  define GMPVideoDecoder_h_

#  include "GMPVideoDecoderProxy.h"
#  include "ImageContainer.h"
#  include "MediaDataDecoderProxy.h"
#  include "MediaInfo.h"
#  include "PerformanceRecorder.h"
#  include "PlatformDecoderModule.h"
#  include "ReorderQueue.h"
#  include "mozIGeckoMediaPluginService.h"
#  include "nsClassHashtable.h"

namespace mozilla {

struct MOZ_STACK_CLASS GMPVideoDecoderParams {
  explicit GMPVideoDecoderParams(const CreateDecoderParams& aParams);

  const VideoInfo& mConfig;
  layers::ImageContainer* mImageContainer;
  GMPCrashHelper* mCrashHelper;
  layers::KnowsCompositor* mKnowsCompositor;
  const Maybe<TrackingId> mTrackingId;
};

DDLoggedTypeDeclNameAndBase(GMPVideoDecoder, MediaDataDecoder);

class GMPVideoDecoder final : public MediaDataDecoder,
                              public GMPVideoDecoderCallbackProxy,
                              public DecoderDoctorLifeLogger<GMPVideoDecoder> {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPVideoDecoder, final);

  explicit GMPVideoDecoder(const GMPVideoDecoderParams& aParams);

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  nsCString GetDescriptionName() const override {
    return "gmp video decoder"_ns;
  }
  nsCString GetCodecName() const override;
  ConversionRequired NeedsConversion() const override {
    return mConvertToAnnexB ? ConversionRequired::kNeedAnnexB
                            : ConversionRequired::kNeedAVCC;
  }
  bool CanDecodeBatch() const override { return mCanDecodeBatch; }

  // GMPVideoDecoderCallbackProxy
  // All those methods are called on the GMP thread.
  void Decoded(GMPVideoi420Frame* aDecodedFrame) override;
  void ReceivedDecodedReferenceFrame(const uint64_t aPictureId) override;
  void ReceivedDecodedFrame(const uint64_t aPictureId) override;
  void InputDataExhausted() override;
  void DrainComplete() override;
  void ResetComplete() override;
  void Error(GMPErr aErr) override;
  void Terminated() override;

 protected:
  virtual void InitTags(nsTArray<nsCString>& aTags);
  virtual nsCString GetNodeId();
  virtual GMPUniquePtr<GMPVideoEncodedFrame> CreateFrame(MediaRawData* aSample);
  virtual const VideoInfo& GetConfig() const;
  void ProcessReorderQueue(MozPromiseHolder<DecodePromise>& aPromise,
                           const char* aMethodName);

 private:
  ~GMPVideoDecoder() = default;

  class GMPInitDoneCallback : public GetGMPVideoDecoderCallback {
   public:
    explicit GMPInitDoneCallback(GMPVideoDecoder* aDecoder)
        : mDecoder(aDecoder) {}

    void Done(GMPVideoDecoderProxy* aGMP, GMPVideoHost* aHost) override {
      mDecoder->GMPInitDone(aGMP, aHost);
    }

   private:
    RefPtr<GMPVideoDecoder> mDecoder;
  };
  void GMPInitDone(GMPVideoDecoderProxy* aGMP, GMPVideoHost* aHost);

  const VideoInfo mConfig;
  nsCOMPtr<mozIGeckoMediaPluginService> mMPS;
  GMPVideoDecoderProxy* mGMP;
  GMPVideoHost* mHost;
  bool mConvertNALUnitLengths;
  MozPromiseHolder<InitPromise> mInitPromise;
  RefPtr<GMPCrashHelper> mCrashHelper;

  struct SampleMetadata {
    explicit SampleMetadata(MediaRawData* aSample)
        : mOffset(aSample->mOffset), mKeyframe(aSample->mKeyframe) {}
    int64_t mOffset;
    bool mKeyframe;
  };

  nsClassHashtable<nsUint64HashKey, SampleMetadata> mSamples;
  RefPtr<layers::ImageContainer> mImageContainer;
  RefPtr<layers::KnowsCompositor> mKnowsCompositor;
  PerformanceRecorderMulti<DecodeStage> mPerformanceRecorder;
  const Maybe<TrackingId> mTrackingId;

  uint32_t mMaxRefFrames = 0;
  ReorderQueue mReorderQueue;
  DecodedData mUnorderedData;

  MozPromiseHolder<DecodePromise> mDecodePromise;
  MozPromiseHolder<DecodePromise> mDrainPromise;
  MozPromiseHolder<FlushPromise> mFlushPromise;
  bool mConvertToAnnexB = false;
  bool mCanDecodeBatch = false;
  bool mReorderFrames = true;
};

}  // namespace mozilla

#endif  // GMPVideoDecoder_h_
