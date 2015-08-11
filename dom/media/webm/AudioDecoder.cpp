/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebMReader.h"

#ifdef MOZ_TREMOR
#include "tremor/ivorbiscodec.h"
#else
#include "vorbis/codec.h"
#endif

#include "OpusParser.h"

#include "VorbisUtils.h"
#include "OggReader.h"

#undef LOG

#ifdef PR_LOGGING
#include "prprf.h"
#define LOG(type, msg) MOZ_LOG(gMediaDecoderLog, type, msg)
#else
#define LOG(type, msg)
#endif

namespace mozilla {

extern PRLogModuleInfo* gMediaDecoderLog;

ogg_packet InitOggPacket(const unsigned char* aData, size_t aLength,
                         bool aBOS, bool aEOS,
                         int64_t aGranulepos, int64_t aPacketNo)
{
  ogg_packet packet;
  packet.packet = const_cast<unsigned char*>(aData);
  packet.bytes = aLength;
  packet.b_o_s = aBOS;
  packet.e_o_s = aEOS;
  packet.granulepos = aGranulepos;
  packet.packetno = aPacketNo;
  return packet;
}

class VorbisDecoder : public WebMAudioDecoder
{
public:
  nsRefPtr<InitPromise> Init() override;
  void Shutdown() override;
  nsresult ResetDecode() override;
  nsresult DecodeHeader(const unsigned char* aData, size_t aLength) override;
  nsresult FinishInit(AudioInfo& aInfo) override;
  bool Decode(const unsigned char* aData, size_t aLength,
              int64_t aOffset, uint64_t aTstampUsecs,
              int64_t aDiscardPadding, int32_t* aTotalFrames) override;
  explicit VorbisDecoder(WebMReader* aReader);
  ~VorbisDecoder();
private:
  nsRefPtr<WebMReader> mReader;

  // Vorbis decoder state
  vorbis_info mVorbisInfo;
  vorbis_comment mVorbisComment;
  vorbis_dsp_state mVorbisDsp;
  vorbis_block mVorbisBlock;
  int64_t mPacketCount;
};

VorbisDecoder::VorbisDecoder(WebMReader* aReader)
  : mReader(aReader)
  , mPacketCount(0)
{
  // Zero these member vars to avoid crashes in Vorbis clear functions when
  // destructor is called before |Init|.
  PodZero(&mVorbisBlock);
  PodZero(&mVorbisDsp);
  PodZero(&mVorbisInfo);
  PodZero(&mVorbisComment);
}

VorbisDecoder::~VorbisDecoder()
{
  vorbis_block_clear(&mVorbisBlock);
  vorbis_dsp_clear(&mVorbisDsp);
  vorbis_info_clear(&mVorbisInfo);
  vorbis_comment_clear(&mVorbisComment);
}

void
VorbisDecoder::Shutdown()
{
  mReader = nullptr;
}

nsRefPtr<InitPromise>
VorbisDecoder::Init()
{
  vorbis_info_init(&mVorbisInfo);
  vorbis_comment_init(&mVorbisComment);
  PodZero(&mVorbisDsp);
  PodZero(&mVorbisBlock);
  return InitPromise::CreateAndResolve(TrackType::kAudioTrack, __func__);
}

nsresult
VorbisDecoder::ResetDecode()
{
  // Ignore failed results from vorbis_synthesis_restart. They
  // aren't fatal and it fails when ResetDecode is called at a
  // time when no vorbis data has been read.
  vorbis_synthesis_restart(&mVorbisDsp);
  return NS_OK;
}

nsresult
VorbisDecoder::DecodeHeader(const unsigned char* aData, size_t aLength)
{
  bool bos = mPacketCount == 0;
  ogg_packet pkt = InitOggPacket(aData, aLength, bos, false, 0, mPacketCount++);
  MOZ_ASSERT(mPacketCount <= 3);

  int r = vorbis_synthesis_headerin(&mVorbisInfo,
                                    &mVorbisComment,
                                    &pkt);
  return r == 0 ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
VorbisDecoder::FinishInit(AudioInfo& aInfo)
{
  MOZ_ASSERT(mPacketCount == 3);

  int r = vorbis_synthesis_init(&mVorbisDsp, &mVorbisInfo);
  if (r) {
    return NS_ERROR_FAILURE;
  }

  r = vorbis_block_init(&mVorbisDsp, &mVorbisBlock);
  if (r) {
    return NS_ERROR_FAILURE;
  }

  aInfo.mRate = mVorbisDsp.vi->rate;
  aInfo.mChannels = mVorbisDsp.vi->channels;

  return NS_OK;
}

bool
VorbisDecoder::Decode(const unsigned char* aData, size_t aLength,
                      int64_t aOffset, uint64_t aTstampUsecs,
                      int64_t aDiscardPadding, int32_t* aTotalFrames)
{
  MOZ_ASSERT(mPacketCount >= 3);
  ogg_packet pkt = InitOggPacket(aData, aLength, false, false, -1, mPacketCount++);
  bool first_packet = mPacketCount == 4;

  if (vorbis_synthesis(&mVorbisBlock, &pkt)) {
    return false;
  }

  if (vorbis_synthesis_blockin(&mVorbisDsp,
                               &mVorbisBlock)) {
    return false;
  }

  VorbisPCMValue** pcm = 0;
  int32_t frames = vorbis_synthesis_pcmout(&mVorbisDsp, &pcm);
  // If the first packet of audio in the media produces no data, we
  // still need to produce an AudioData for it so that the correct media
  // start time is calculated.  Otherwise we'd end up with a media start
  // time derived from the timecode of the first packet that produced
  // data.
  if (frames == 0 && first_packet) {
    mReader->AudioQueue().Push(new AudioData(aOffset, aTstampUsecs, 0, 0, nullptr,
                                             mVorbisDsp.vi->channels,
                                             mVorbisDsp.vi->rate));
  }
  while (frames > 0) {
    uint32_t channels = mVorbisDsp.vi->channels;
    nsAutoArrayPtr<AudioDataValue> buffer(new AudioDataValue[frames*channels]);
    for (uint32_t j = 0; j < channels; ++j) {
      VorbisPCMValue* channel = pcm[j];
      for (uint32_t i = 0; i < uint32_t(frames); ++i) {
        buffer[i*channels + j] = MOZ_CONVERT_VORBIS_SAMPLE(channel[i]);
      }
    }

    CheckedInt64 duration = FramesToUsecs(frames, mVorbisDsp.vi->rate);
    if (!duration.isValid()) {
      NS_WARNING("Int overflow converting WebM audio duration");
      return false;
    }
    CheckedInt64 total_duration = FramesToUsecs(*aTotalFrames,
                                                mVorbisDsp.vi->rate);
    if (!total_duration.isValid()) {
      NS_WARNING("Int overflow converting WebM audio total_duration");
      return false;
    }

    CheckedInt64 time = total_duration + aTstampUsecs;
    if (!time.isValid()) {
      NS_WARNING("Int overflow adding total_duration and aTstampUsecs");
      return false;
    };

    *aTotalFrames += frames;
    mReader->AudioQueue().Push(new AudioData(aOffset,
                                             time.value(),
                                             duration.value(),
                                             frames,
                                             buffer.forget(),
                                             mVorbisDsp.vi->channels,
                                             mVorbisDsp.vi->rate));
    if (vorbis_synthesis_read(&mVorbisDsp, frames)) {
      return false;
    }

    frames = vorbis_synthesis_pcmout(&mVorbisDsp, &pcm);
  }

  return true;
}

// ------------------------------------------------------------------------

class OpusDecoder : public WebMAudioDecoder
{
public:
  nsRefPtr<InitPromise> Init() override;
  void Shutdown() override;
  nsresult ResetDecode() override;
  nsresult DecodeHeader(const unsigned char* aData, size_t aLength) override;
  nsresult FinishInit(AudioInfo& aInfo) override;
  bool Decode(const unsigned char* aData, size_t aLength,
              int64_t aOffset, uint64_t aTstampUsecs,
              int64_t aDiscardPadding, int32_t* aTotalFrames) override;
  explicit OpusDecoder(WebMReader* aReader);
  ~OpusDecoder();
private:
  nsRefPtr<WebMReader> mReader;

  // Opus decoder state
  nsAutoPtr<OpusParser> mOpusParser;
  OpusMSDecoder* mOpusDecoder;
  uint16_t mSkip;        // Samples left to trim before playback.
  bool mDecodedHeader;

  // Opus padding should only be discarded on the final packet.  Once this
  // is set to true, if the reader attempts to decode any further packets it
  // will raise an error so we can indicate that the file is invalid.
  bool mPaddingDiscarded;
};

OpusDecoder::OpusDecoder(WebMReader* aReader)
  : mReader(aReader)
  , mOpusDecoder(nullptr)
  , mSkip(0)
  , mDecodedHeader(false)
  , mPaddingDiscarded(false)
{
}

OpusDecoder::~OpusDecoder()
{
  if (mOpusDecoder) {
    opus_multistream_decoder_destroy(mOpusDecoder);
    mOpusDecoder = nullptr;
  }
}

void
OpusDecoder::Shutdown()
{
  mReader = nullptr;
}

nsRefPtr<InitPromise>
OpusDecoder::Init()
{
  return InitPromise::CreateAndResolve(TrackType::kAudioTrack, __func__);
}

nsresult
OpusDecoder::ResetDecode()
{
  if (mOpusDecoder) {
    // Reset the decoder.
    opus_multistream_decoder_ctl(mOpusDecoder, OPUS_RESET_STATE);
    mSkip = mOpusParser->mPreSkip;
    mPaddingDiscarded = false;
  }
  return NS_OK;
}

nsresult
OpusDecoder::DecodeHeader(const unsigned char* aData, size_t aLength)
{
  MOZ_ASSERT(!mOpusParser);
  MOZ_ASSERT(!mOpusDecoder);
  MOZ_ASSERT(!mDecodedHeader);
  mDecodedHeader = true;

  mOpusParser = new OpusParser;
  if (!mOpusParser->DecodeHeader(const_cast<unsigned char*>(aData), aLength)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
OpusDecoder::FinishInit(AudioInfo& aInfo)
{
  MOZ_ASSERT(mDecodedHeader);

  int r;
  mOpusDecoder = opus_multistream_decoder_create(mOpusParser->mRate,
                                                 mOpusParser->mChannels,
                                                 mOpusParser->mStreams,
                                                 mOpusParser->mCoupledStreams,
                                                 mOpusParser->mMappingTable,
                                                 &r);
  mSkip = mOpusParser->mPreSkip;
  mPaddingDiscarded = false;

  if (int64_t(mReader->GetCodecDelay()) != FramesToUsecs(mOpusParser->mPreSkip,
                                                         mOpusParser->mRate).value()) {
    LOG(LogLevel::Warning,
        ("Invalid Opus header: CodecDelay and pre-skip do not match!"));
    return NS_ERROR_FAILURE;
  }

  aInfo.mRate = mOpusParser->mRate;
  aInfo.mChannels = mOpusParser->mChannels;

  return r == OPUS_OK ? NS_OK : NS_ERROR_FAILURE;
}

bool
OpusDecoder::Decode(const unsigned char* aData, size_t aLength,
                    int64_t aOffset, uint64_t aTstampUsecs,
                    int64_t aDiscardPadding, int32_t* aTotalFrames)
{
  uint32_t channels = mOpusParser->mChannels;
  // No channel mapping for more than 8 channels.
  if (channels > 8) {
    return false;
  }

  if (mPaddingDiscarded) {
    // Discard padding should be used only on the final packet, so
    // decoding after a padding discard is invalid.
    LOG(LogLevel::Debug, ("Opus error, discard padding on interstitial packet"));
    return false;
  }

  // Maximum value is 63*2880, so there's no chance of overflow.
  int32_t frames_number = opus_packet_get_nb_frames(aData, aLength);
  if (frames_number <= 0) {
    return false; // Invalid packet header.
  }

  int32_t samples =
    opus_packet_get_samples_per_frame(aData, opus_int32(mOpusParser->mRate));

  // A valid Opus packet must be between 2.5 and 120 ms long (48kHz).
  int32_t frames = frames_number*samples;
  if (frames < 120 || frames > 5760)
    return false;

  nsAutoArrayPtr<AudioDataValue> buffer(new AudioDataValue[frames * channels]);

  // Decode to the appropriate sample type.
#ifdef MOZ_SAMPLE_TYPE_FLOAT32
  int ret = opus_multistream_decode_float(mOpusDecoder,
                                          aData, aLength,
                                          buffer, frames, false);
#else
  int ret = opus_multistream_decode(mOpusDecoder,
                                    aData, aLength,
                                    buffer, frames, false);
#endif
  if (ret < 0)
    return false;
  NS_ASSERTION(ret == frames, "Opus decoded too few audio samples");
  CheckedInt64 startTime = aTstampUsecs;

  // Trim the initial frames while the decoder is settling.
  if (mSkip > 0) {
    int32_t skipFrames = std::min<int32_t>(mSkip, frames);
    int32_t keepFrames = frames - skipFrames;
    LOG(LogLevel::Debug, ("Opus decoder skipping %d of %d frames",
                       skipFrames, frames));
    PodMove(buffer.get(),
            buffer.get() + skipFrames * channels,
            keepFrames * channels);
    startTime = startTime + FramesToUsecs(skipFrames, mOpusParser->mRate);
    frames = keepFrames;
    mSkip -= skipFrames;
  }

  if (aDiscardPadding < 0) {
    // Negative discard padding is invalid.
    LOG(LogLevel::Debug, ("Opus error, negative discard padding"));
    return false;
  }
  if (aDiscardPadding > 0) {
    CheckedInt64 discardFrames = UsecsToFrames(aDiscardPadding / NS_PER_USEC,
                                               mOpusParser->mRate);
    if (!discardFrames.isValid()) {
      NS_WARNING("Int overflow in DiscardPadding");
      return false;
    }
    if (discardFrames.value() > frames) {
      // Discarding more than the entire packet is invalid.
      LOG(LogLevel::Debug, ("Opus error, discard padding larger than packet"));
      return false;
    }
    LOG(LogLevel::Debug, ("Opus decoder discarding %d of %d frames",
                       int32_t(discardFrames.value()), frames));
    // Padding discard is only supposed to happen on the final packet.
    // Record the discard so we can return an error if another packet is
    // decoded.
    mPaddingDiscarded = true;
    int32_t keepFrames = frames - discardFrames.value();
    frames = keepFrames;
  }

  // Apply the header gain if one was specified.
#ifdef MOZ_SAMPLE_TYPE_FLOAT32
  if (mOpusParser->mGain != 1.0f) {
    float gain = mOpusParser->mGain;
    int samples = frames * channels;
    for (int i = 0; i < samples; i++) {
      buffer[i] *= gain;
    }
  }
#else
  if (mOpusParser->mGain_Q16 != 65536) {
    int64_t gain_Q16 = mOpusParser->mGain_Q16;
    int samples = frames * channels;
    for (int i = 0; i < samples; i++) {
      int32_t val = static_cast<int32_t>((gain_Q16*buffer[i] + 32768)>>16);
      buffer[i] = static_cast<AudioDataValue>(MOZ_CLIP_TO_15(val));
    }
  }
#endif

  CheckedInt64 duration = FramesToUsecs(frames, mOpusParser->mRate);
  if (!duration.isValid()) {
    NS_WARNING("Int overflow converting WebM audio duration");
    return false;
  }
  CheckedInt64 time = startTime - mReader->GetCodecDelay();
  if (!time.isValid()) {
    NS_WARNING("Int overflow shifting tstamp by codec delay");
    return false;
  };
  mReader->AudioQueue().Push(new AudioData(aOffset,
                                           time.value(),
                                           duration.value(),
                                           frames,
                                           buffer.forget(),
                                           mOpusParser->mChannels,
                                           mOpusParser->mRate));
  return true;
}

} // namespace mozilla
