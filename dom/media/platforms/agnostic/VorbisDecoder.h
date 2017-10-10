/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(VorbisDecoder_h_)
#define VorbisDecoder_h_

#include "AudioConverter.h"
#include "PlatformDecoderModule.h"
#include "mozilla/Maybe.h"

#ifdef MOZ_TREMOR
#include "tremor/ivorbiscodec.h"
#else
#include "vorbis/codec.h"
#endif

namespace mozilla {

DDLoggedTypeDeclNameAndBase(VorbisDataDecoder, MediaDataDecoder);

class VorbisDataDecoder
  : public MediaDataDecoder
  , public DecoderDoctorLifeLogger<VorbisDataDecoder>
{
public:
  explicit VorbisDataDecoder(const CreateDecoderParams& aParams);
  ~VorbisDataDecoder();

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  nsCString GetDescriptionName() const override
  {
    return NS_LITERAL_CSTRING("vorbis audio decoder");
  }

  // Return true if mimetype is Vorbis
  static bool IsVorbis(const nsACString& aMimeType);
  static const AudioConfig::Channel* VorbisLayout(uint32_t aChannels);

private:
  nsresult DecodeHeader(const unsigned char* aData, size_t aLength);
  RefPtr<DecodePromise> ProcessDecode(MediaRawData* aSample);

  const AudioInfo& mInfo;
  const RefPtr<TaskQueue> mTaskQueue;

  // Vorbis decoder state
  vorbis_info mVorbisInfo;
  vorbis_comment mVorbisComment;
  vorbis_dsp_state mVorbisDsp;
  vorbis_block mVorbisBlock;

  int64_t mPacketCount;
  int64_t mFrames;
  Maybe<int64_t> mLastFrameTime;
  UniquePtr<AudioConverter> mAudioConverter;
};

} // namespace mozilla
#endif
