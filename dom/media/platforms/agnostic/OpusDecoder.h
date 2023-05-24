/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(OpusDecoder_h_)
#  define OpusDecoder_h_

#  include "PlatformDecoderModule.h"

#  include "mozilla/Maybe.h"
#  include "nsTArray.h"

struct OpusMSDecoder;

namespace mozilla {

class OpusParser;

DDLoggedTypeDeclNameAndBase(OpusDataDecoder, MediaDataDecoder);

class OpusDataDecoder final : public MediaDataDecoder,
                              public DecoderDoctorLifeLogger<OpusDataDecoder> {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OpusDataDecoder, final);

  explicit OpusDataDecoder(const CreateDecoderParams& aParams);

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  nsCString GetDescriptionName() const override {
    return "opus audio decoder"_ns;
  }
  nsCString GetCodecName() const override { return "opus"_ns; }

  // Return true if mimetype is Opus
  static bool IsOpus(const nsACString& aMimeType);

 private:
  ~OpusDataDecoder();

  nsresult DecodeHeader(const unsigned char* aData, size_t aLength);

  const AudioInfo mInfo;
  nsCOMPtr<nsISerialEventTarget> mThread;

  // Opus decoder state
  UniquePtr<OpusParser> mOpusParser;
  OpusMSDecoder* mOpusDecoder;

  uint16_t mSkip;  // Samples left to trim before playback.
  bool mDecodedHeader;

  // Opus padding should only be discarded on the final packet.  Once this
  // is set to true, if the reader attempts to decode any further packets it
  // will raise an error so we can indicate that the file is invalid.
  bool mPaddingDiscarded;
  int64_t mFrames;
  int64_t mTotalFrames = 0;
  Maybe<int64_t> mLastFrameTime;
  AutoTArray<uint8_t, 8> mMappingTable;
  AudioConfig::ChannelLayout::ChannelMap mChannelMap;
  bool mDefaultPlaybackDeviceMono;
};

}  // namespace mozilla
#endif
