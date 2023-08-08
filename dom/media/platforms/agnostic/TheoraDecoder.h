/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(TheoraDecoder_h_)
#  define TheoraDecoder_h_

#  include <stdint.h>

#  include "PlatformDecoderModule.h"
#  include <theora/theoradec.h>

namespace mozilla {

DDLoggedTypeDeclNameAndBase(TheoraDecoder, MediaDataDecoder);

class TheoraDecoder final : public MediaDataDecoder,
                            public DecoderDoctorLifeLogger<TheoraDecoder> {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TheoraDecoder, final);

  explicit TheoraDecoder(const CreateDecoderParams& aParams);

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;

  // Return true if mimetype is a Theora codec
  static bool IsTheora(const nsACString& aMimeType);

  nsCString GetDescriptionName() const override {
    return "theora video decoder"_ns;
  }

  nsCString GetCodecName() const override { return "theora"_ns; }

 private:
  ~TheoraDecoder();
  nsresult DoDecodeHeader(const unsigned char* aData, size_t aLength);

  RefPtr<DecodePromise> ProcessDecode(MediaRawData* aSample);

  const RefPtr<layers::KnowsCompositor> mImageAllocator;
  const RefPtr<layers::ImageContainer> mImageContainer;
  const RefPtr<TaskQueue> mTaskQueue;

  // Theora header & decoder state
  th_info mTheoraInfo;
  th_comment mTheoraComment;
  th_setup_info* mTheoraSetupInfo;
  th_dec_ctx* mTheoraDecoderContext;
  int mPacketCount;

  const VideoInfo mInfo;
  const Maybe<TrackingId> mTrackingId;
};

}  // namespace mozilla

#endif
