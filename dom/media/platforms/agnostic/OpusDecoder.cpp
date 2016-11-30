/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OpusDecoder.h"
#include "OpusParser.h"
#include "TimeUnits.h"
#include "VorbisUtils.h"
#include "VorbisDecoder.h" // For VorbisLayout
#include "mozilla/EndianUtils.h"
#include "mozilla/PodOperations.h"
#include "mozilla/SyncRunnable.h"

#include <inttypes.h>  // For PRId64

#include "opus/opus.h"
extern "C" {
#include "opus/opus_multistream.h"
}

#define OPUS_DEBUG(arg, ...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, \
    ("OpusDataDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

OpusDataDecoder::OpusDataDecoder(const CreateDecoderParams& aParams)
  : mInfo(aParams.AudioConfig())
  , mTaskQueue(aParams.mTaskQueue)
  , mCallback(aParams.mCallback)
  , mOpusDecoder(nullptr)
  , mSkip(0)
  , mDecodedHeader(false)
  , mPaddingDiscarded(false)
  , mFrames(0)
  , mIsFlushing(false)
{
}

OpusDataDecoder::~OpusDataDecoder()
{
  if (mOpusDecoder) {
    opus_multistream_decoder_destroy(mOpusDecoder);
    mOpusDecoder = nullptr;
  }
}

void
OpusDataDecoder::Shutdown()
{
}

void
OpusDataDecoder::AppendCodecDelay(MediaByteBuffer* config, uint64_t codecDelayUS)
{
  uint8_t buffer[sizeof(uint64_t)];
  BigEndian::writeUint64(buffer, codecDelayUS);
  config->AppendElements(buffer, sizeof(uint64_t));
}

RefPtr<MediaDataDecoder::InitPromise>
OpusDataDecoder::Init()
{
  size_t length = mInfo.mCodecSpecificConfig->Length();
  uint8_t *p = mInfo.mCodecSpecificConfig->Elements();
  if (length < sizeof(uint64_t)) {
    OPUS_DEBUG("CodecSpecificConfig too short to read codecDelay!");
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }
  int64_t codecDelay = BigEndian::readUint64(p);
  length -= sizeof(uint64_t);
  p += sizeof(uint64_t);
  if (NS_FAILED(DecodeHeader(p, length))) {
    OPUS_DEBUG("Error decoding header!");
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }

  int r;
  mOpusDecoder = opus_multistream_decoder_create(mOpusParser->mRate,
                                                 mOpusParser->mChannels,
                                                 mOpusParser->mStreams,
                                                 mOpusParser->mCoupledStreams,
                                                 mMappingTable,
                                                 &r);
  mSkip = mOpusParser->mPreSkip;
  mPaddingDiscarded = false;

  if (codecDelay != FramesToUsecs(mOpusParser->mPreSkip,
                                  mOpusParser->mRate).value()) {
    NS_WARNING("Invalid Opus header: CodecDelay and pre-skip do not match!");
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }

  if (mInfo.mRate != (uint32_t)mOpusParser->mRate) {
    NS_WARNING("Invalid Opus header: container and codec rate do not match!");
  }
  if (mInfo.mChannels != (uint32_t)mOpusParser->mChannels) {
    NS_WARNING("Invalid Opus header: container and codec channels do not match!");
  }

  return r == OPUS_OK ? InitPromise::CreateAndResolve(TrackInfo::kAudioTrack, __func__)
                      : InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
}

nsresult
OpusDataDecoder::DecodeHeader(const unsigned char* aData, size_t aLength)
{
  MOZ_ASSERT(!mOpusParser);
  MOZ_ASSERT(!mOpusDecoder);
  MOZ_ASSERT(!mDecodedHeader);
  mDecodedHeader = true;

  mOpusParser = new OpusParser;
  if (!mOpusParser->DecodeHeader(const_cast<unsigned char*>(aData), aLength)) {
    return NS_ERROR_FAILURE;
  }
  int channels = mOpusParser->mChannels;

  AudioConfig::ChannelLayout layout(channels);
  if (!layout.IsValid()) {
    OPUS_DEBUG("Invalid channel mapping. Source is %d channels", channels);
    return NS_ERROR_FAILURE;
  }

  AudioConfig::ChannelLayout vorbisLayout(
    channels, VorbisDataDecoder::VorbisLayout(channels));
  AudioConfig::ChannelLayout smpteLayout(channels);
  static_assert(sizeof(mOpusParser->mMappingTable) / sizeof(mOpusParser->mMappingTable[0]) >= MAX_AUDIO_CHANNELS,
                       "Invalid size set");
  uint8_t map[sizeof(mOpusParser->mMappingTable) / sizeof(mOpusParser->mMappingTable[0])];
  if (vorbisLayout.MappingTable(smpteLayout, map)) {
    for (int i = 0; i < channels; i++) {
      mMappingTable[i] = mOpusParser->mMappingTable[map[i]];
    }
  } else {
    // Should never get here as vorbis layout is always convertible to SMPTE
    // default layout.
    PodCopy(mMappingTable, mOpusParser->mMappingTable, MAX_AUDIO_CHANNELS);
  }

  return NS_OK;
}

void
OpusDataDecoder::Input(MediaRawData* aSample)
{
  mTaskQueue->Dispatch(NewRunnableMethod<RefPtr<MediaRawData>>(
                       this, &OpusDataDecoder::ProcessDecode, aSample));
}

void
OpusDataDecoder::ProcessDecode(MediaRawData* aSample)
{
  if (mIsFlushing) {
    return;
  }

  MediaResult rv = DoDecode(aSample);
  if (NS_FAILED(rv)) {
    mCallback->Error(rv);
    return;
  }
  mCallback->InputExhausted();
}

MediaResult
OpusDataDecoder::DoDecode(MediaRawData* aSample)
{
  uint32_t channels = mOpusParser->mChannels;

  if (mPaddingDiscarded) {
    // Discard padding should be used only on the final packet, so
    // decoding after a padding discard is invalid.
    OPUS_DEBUG("Opus error, discard padding on interstitial packet");
    return MediaResult(
      NS_ERROR_DOM_MEDIA_FATAL_ERR,
      RESULT_DETAIL("Discard padding on interstitial packet"));
  }

  if (!mLastFrameTime || mLastFrameTime.ref() != aSample->mTime) {
    // We are starting a new block.
    mFrames = 0;
    mLastFrameTime = Some(aSample->mTime);
  }

  // Maximum value is 63*2880, so there's no chance of overflow.
  uint32_t frames_number = opus_packet_get_nb_frames(aSample->Data(),
                                                    aSample->Size());
  if (frames_number <= 0) {
    OPUS_DEBUG("Invalid packet header: r=%ld length=%ld",
               frames_number, aSample->Size());
    return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                       RESULT_DETAIL("Invalid packet header: r=%d length=%u",
                                     frames_number, uint32_t(aSample->Size())));
  }

  uint32_t samples = opus_packet_get_samples_per_frame(
    aSample->Data(), opus_int32(mOpusParser->mRate));


  // A valid Opus packet must be between 2.5 and 120 ms long (48kHz).
  uint32_t frames = frames_number*samples;
  if (frames < 120 || frames > 5760) {
    OPUS_DEBUG("Invalid packet frames: %u", frames);
    return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                       RESULT_DETAIL("Invalid packet frames:%u", frames));
  }

  AlignedAudioBuffer buffer(frames * channels);
  if (!buffer) {
    return MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__);
  }

  // Decode to the appropriate sample type.
#ifdef MOZ_SAMPLE_TYPE_FLOAT32
  int ret = opus_multistream_decode_float(mOpusDecoder,
                                          aSample->Data(), aSample->Size(),
                                          buffer.get(), frames, false);
#else
  int ret = opus_multistream_decode(mOpusDecoder,
                                    aSample->Data(), aSample->Size(),
                                    buffer.get(), frames, false);
#endif
  if (ret < 0) {
    return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                       RESULT_DETAIL("Opus decoding error:%d", ret));
  }
  NS_ASSERTION(uint32_t(ret) == frames, "Opus decoded too few audio samples");
  CheckedInt64 startTime = aSample->mTime;

  // Trim the initial frames while the decoder is settling.
  if (mSkip > 0) {
    int32_t skipFrames = std::min<int32_t>(mSkip, frames);
    int32_t keepFrames = frames - skipFrames;
    OPUS_DEBUG("Opus decoder skipping %d of %d frames", skipFrames, frames);
    PodMove(buffer.get(),
            buffer.get() + skipFrames * channels,
            keepFrames * channels);
    startTime = startTime + FramesToUsecs(skipFrames, mOpusParser->mRate);
    frames = keepFrames;
    mSkip -= skipFrames;
  }

  if (aSample->mDiscardPadding > 0) {
    OPUS_DEBUG("Opus decoder discarding %u of %u frames",
               aSample->mDiscardPadding, frames);
    // Padding discard is only supposed to happen on the final packet.
    // Record the discard so we can return an error if another packet is
    // decoded.
    if (aSample->mDiscardPadding > frames) {
    // Discarding more than the entire packet is invalid.
      OPUS_DEBUG("Opus error, discard padding larger than packet");
      return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                         RESULT_DETAIL("Discard padding larger than packet"));
    }

    mPaddingDiscarded = true;
    frames = frames - aSample->mDiscardPadding;
  }

  // Apply the header gain if one was specified.
#ifdef MOZ_SAMPLE_TYPE_FLOAT32
  if (mOpusParser->mGain != 1.0f) {
    float gain = mOpusParser->mGain;
    uint32_t samples = frames * channels;
    for (uint32_t i = 0; i < samples; i++) {
      buffer[i] *= gain;
    }
  }
#else
  if (mOpusParser->mGain_Q16 != 65536) {
    int64_t gain_Q16 = mOpusParser->mGain_Q16;
    uint32_t samples = frames * channels;
    for (uint32_t i = 0; i < samples; i++) {
      int32_t val = static_cast<int32_t>((gain_Q16*buffer[i] + 32768)>>16);
      buffer[i] = static_cast<AudioDataValue>(MOZ_CLIP_TO_15(val));
    }
  }
#endif

  CheckedInt64 duration = FramesToUsecs(frames, mOpusParser->mRate);
  if (!duration.isValid()) {
    return MediaResult(
      NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
      RESULT_DETAIL("Overflow converting WebM audio duration"));
  }
  CheckedInt64 time =
    startTime - FramesToUsecs(mOpusParser->mPreSkip, mOpusParser->mRate) +
    FramesToUsecs(mFrames, mOpusParser->mRate);
  if (!time.isValid()) {
    return MediaResult(
      NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
      RESULT_DETAIL("Overflow shifting tstamp by codec delay"));
  };

  mCallback->Output(new AudioData(aSample->mOffset,
                                  time.value(),
                                  duration.value(),
                                  frames,
                                  Move(buffer),
                                  mOpusParser->mChannels,
                                  mOpusParser->mRate));
  mFrames += frames;
  return NS_OK;
}

void
OpusDataDecoder::ProcessDrain()
{
  mCallback->DrainComplete();
}

void
OpusDataDecoder::Drain()
{
  mTaskQueue->Dispatch(NewRunnableMethod(this, &OpusDataDecoder::ProcessDrain));
}

void
OpusDataDecoder::Flush()
{
  if (!mOpusDecoder) {
    return;
  }
  mIsFlushing = true;
  nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([this] () {
    MOZ_ASSERT(mOpusDecoder);
    // Reset the decoder.
    opus_multistream_decoder_ctl(mOpusDecoder, OPUS_RESET_STATE);
    mSkip = mOpusParser->mPreSkip;
    mPaddingDiscarded = false;
    mLastFrameTime.reset();
  });
  SyncRunnable::DispatchToThread(mTaskQueue, runnable);
  mIsFlushing = false;
}

/* static */
bool
OpusDataDecoder::IsOpus(const nsACString& aMimeType)
{
  return aMimeType.EqualsLiteral("audio/webm; codecs=opus") ||
         aMimeType.EqualsLiteral("audio/ogg; codecs=opus") ||
         aMimeType.EqualsLiteral("audio/mp4; codecs=opus") ||
         aMimeType.EqualsLiteral("audio/opus");
}

} // namespace mozilla
#undef OPUS_DEBUG
