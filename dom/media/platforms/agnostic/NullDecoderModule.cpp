/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DummyMediaDataDecoder.h"

namespace mozilla {

class NullVideoDataCreator : public DummyDataCreator
{
public:
  NullVideoDataCreator() { }

  already_AddRefed<MediaData> Create(MediaRawData* aSample) override
  {
    // Create a dummy VideoData with no image. This gives us something to
    // send to media streams if necessary.
    RefPtr<VideoData> v(new VideoData(aSample->mOffset,
                                      aSample->mTime,
                                      aSample->mDuration,
                                      aSample->mKeyframe,
                                      aSample->mTimecode,
                                      gfx::IntSize(),
                                      0));
    return v.forget();
  }
};

class NullDecoderModule : public PlatformDecoderModule
{
public:

  // Decode thread.
  already_AddRefed<MediaDataDecoder> CreateVideoDecoder(
    const CreateDecoderParams& aParams) override
  {
    UniquePtr<DummyDataCreator> creator = MakeUnique<NullVideoDataCreator>();
    RefPtr<MediaDataDecoder> decoder = new DummyMediaDataDecoder(
      std::move(creator), NS_LITERAL_CSTRING("null media data decoder"), aParams);
    return decoder.forget();
  }

  // Decode thread.
  already_AddRefed<MediaDataDecoder> CreateAudioDecoder(
    const CreateDecoderParams& aParams) override
  {
    MOZ_ASSERT(false, "Audio decoders are unsupported.");
    return nullptr;
  }

  bool SupportsMimeType(const nsACString& aMimeType,
                        DecoderDoctorDiagnostics* aDiagnostics) const override
  {
    return true;
  }
};

already_AddRefed<PlatformDecoderModule>
CreateNullDecoderModule()
{
  RefPtr<PlatformDecoderModule> pdm = new NullDecoderModule();
  return pdm.forget();
}

} // namespace mozilla
