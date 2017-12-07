/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(OpusDecoder_h_)
#define OpusDecoder_h_

#include "PlatformDecoderModule.h"

#include "mozilla/Maybe.h"
#include "nsAutoPtr.h"

struct OpusMSDecoder;

namespace mozilla {

class OpusParser;

DDLoggedTypeDeclNameAndBase(OpusDataDecoder, MediaDataDecoder);

class OpusDataDecoder
  : public MediaDataDecoder
  , public DecoderDoctorLifeLogger<OpusDataDecoder>
{
public:
  explicit OpusDataDecoder(const CreateDecoderParams& aParams);
  ~OpusDataDecoder();

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  nsCString GetDescriptionName() const override
  {
    return NS_LITERAL_CSTRING("opus audio decoder");
  }

  // Return true if mimetype is Opus
  static bool IsOpus(const nsACString& aMimeType);

  // Pack pre-skip/CodecDelay, given in microseconds, into a
  // MediaByteBuffer. The decoder expects this value to come
  // from the container (if any) and to precede the OpusHead
  // block in the CodecSpecificConfig buffer to verify the
  // values match.
  static void AppendCodecDelay(MediaByteBuffer* config, uint64_t codecDelayUS);

private:
  nsresult DecodeHeader(const unsigned char* aData, size_t aLength);

  RefPtr<DecodePromise> ProcessDecode(MediaRawData* aSample);

  const AudioInfo& mInfo;
  const RefPtr<TaskQueue> mTaskQueue;

  // Opus decoder state
  nsAutoPtr<OpusParser> mOpusParser;
  OpusMSDecoder* mOpusDecoder;

  uint16_t mSkip;        // Samples left to trim before playback.
  bool mDecodedHeader;

  // Opus padding should only be discarded on the final packet.  Once this
  // is set to true, if the reader attempts to decode any further packets it
  // will raise an error so we can indicate that the file is invalid.
  bool mPaddingDiscarded;
  int64_t mFrames;
  Maybe<int64_t> mLastFrameTime;
  uint8_t mMappingTable[MAX_AUDIO_CHANNELS]; // Channel mapping table.
};

} // namespace mozilla
#endif
