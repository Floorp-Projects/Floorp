/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(BlankDecoderModule_h_)
#  define BlankDecoderModule_h_

#  include "DummyMediaDataDecoder.h"
#  include "PlatformDecoderModule.h"

namespace mozilla {

namespace layers {
class ImageContainer;
}

class MediaData;
class MediaRawData;

class BlankVideoDataCreator : public DummyDataCreator {
 public:
  BlankVideoDataCreator(uint32_t aFrameWidth, uint32_t aFrameHeight,
                        layers::ImageContainer* aImageContainer);

  already_AddRefed<MediaData> Create(MediaRawData* aSample) override;

 private:
  VideoInfo mInfo;
  gfx::IntRect mPicture;
  uint32_t mFrameWidth;
  uint32_t mFrameHeight;
  RefPtr<layers::ImageContainer> mImageContainer;
};

class BlankAudioDataCreator : public DummyDataCreator {
 public:
  BlankAudioDataCreator(uint32_t aChannelCount, uint32_t aSampleRate);

  already_AddRefed<MediaData> Create(MediaRawData* aSample) override;

 private:
  int64_t mFrameSum;
  uint32_t mChannelCount;
  uint32_t mSampleRate;
};

class BlankDecoderModule : public PlatformDecoderModule {
  template <typename T, typename... Args>
  friend already_AddRefed<T> MakeAndAddRef(Args&&...);

 public:
  static already_AddRefed<PlatformDecoderModule> Create();

  already_AddRefed<MediaDataDecoder> CreateVideoDecoder(
      const CreateDecoderParams& aParams) override;

  already_AddRefed<MediaDataDecoder> CreateAudioDecoder(
      const CreateDecoderParams& aParams) override;

  media::DecodeSupportSet SupportsMimeType(
      const nsACString& aMimeType,
      DecoderDoctorDiagnostics* aDiagnostics) const override;
};

}  // namespace mozilla

#endif /* BlankDecoderModule_h_ */
