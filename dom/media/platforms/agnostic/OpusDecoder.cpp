/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OpusDecoder.h"
#include "TimeUnits.h"
#include "VorbisUtils.h"
#include "VorbisDecoder.h" // For VorbisLayout
#include "mozilla/Endian.h"
#include "mozilla/PodOperations.h"
#include "mozilla/SyncRunnable.h"

#include <stdint.h>
#include <inttypes.h>  // For PRId64

#define OPUS_DEBUG(arg, ...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, \
    ("OpusDataDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

OpusDataDecoder::OpusDataDecoder(const AudioInfo& aConfig,
                                 TaskQueue* aTaskQueue,
                                 MediaDataDecoderCallback* aCallback)
  : mInfo(aConfig)
  , mTaskQueue(aTaskQueue)
  , mCallback(aCallback)
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

nsresult
OpusDataDecoder::Shutdown()
{
  return NS_OK;
}

RefPtr<MediaDataDecoder::InitPromise>
OpusDataDecoder::Init()
{
  size_t length = mInfo.mCodecSpecificConfig->Length();
  uint8_t *p = mInfo.mCodecSpecificConfig->Elements();
  if (length < sizeof(uint64_t)) {
    return InitPromise::CreateAndReject(DecoderFailureReason::INIT_ERROR, __func__);
  }
  int64_t codecDelay = BigEndian::readUint64(p);
  length -= sizeof(uint64_t);
  p += sizeof(uint64_t);
  if (NS_FAILED(DecodeHeader(p, length))) {
    return InitPromise::CreateAndReject(DecoderFailureReason::INIT_ERROR, __func__);
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
    return InitPromise::CreateAndReject(DecoderFailureReason::INIT_ERROR, __func__);
  }

  if (mInfo.mRate != (uint32_t)mOpusParser->mRate) {
    NS_WARNING("Invalid Opus header: container and codec rate do not match!");
  }
  if (mInfo.mChannels != (uint32_t)mOpusParser->mChannels) {
    NS_WARNING("Invalid Opus header: container and codec channels do not match!");
  }

  return r == OPUS_OK ? InitPromise::CreateAndResolve(TrackInfo::kAudioTrack, __func__)
                      : InitPromise::CreateAndReject(DecoderFailureReason::INIT_ERROR, __func__);
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

nsresult
OpusDataDecoder::Input(MediaRawData* aSample)
{
  mTaskQueue->Dispatch(NewRunnableMethod<RefPtr<MediaRawData>>(
                       this, &OpusDataDecoder::ProcessDecode, aSample));

  return NS_OK;
}

void
OpusDataDecoder::ProcessDecode(MediaRawData* aSample)
{
  if (mIsFlushing) {
    return;
  }
  if (DoDecode(aSample) == -1) {
    mCallback->Error();
  } else if(mTaskQueue->IsEmpty()) {
    mCallback->InputExhausted();
  }
}

int
OpusDataDecoder::DoDecode(MediaRawData* aSample)
{
  int64_t aDiscardPadding = 0;
  if (aSample->mExtraData) {
    aDiscardPadding = BigEndian::readInt64(aSample->mExtraData->Elements());
  }
  uint32_t channels = mOpusParser->mChannels;

  if (mPaddingDiscarded) {
    // Discard padding should be used only on the final packet, so
    // decoding after a padding discard is invalid.
    OPUS_DEBUG("Opus error, discard padding on interstitial packet");
    return -1;
  }

  if (!mLastFrameTime || mLastFrameTime.ref() != aSample->mTime) {
    // We are starting a new block.
    mFrames = 0;
    mLastFrameTime = Some(aSample->mTime);
  }

  // Maximum value is 63*2880, so there's no chance of overflow.
  int32_t frames_number = opus_packet_get_nb_frames(aSample->Data(),
                                                    aSample->Size());
  if (frames_number <= 0) {
    OPUS_DEBUG("Invalid packet header: r=%ld length=%ld",
               frames_number, aSample->Size());
    return -1;
  }

  int32_t samples = opus_packet_get_samples_per_frame(aSample->Data(),
                                           opus_int32(mOpusParser->mRate));


  // A valid Opus packet must be between 2.5 and 120 ms long (48kHz).
  int32_t frames = frames_number*samples;
  if (frames < 120 || frames > 5760) {
    OPUS_DEBUG("Invalid packet frames: %ld", frames);
    return -1;
  }

  AlignedAudioBuffer buffer(frames * channels);
  if (!buffer) {
    return -1;
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
    return -1;
  }
  NS_ASSERTION(ret == frames, "Opus decoded too few audio samples");
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

  if (aDiscardPadding < 0) {
    // Negative discard padding is invalid.
    OPUS_DEBUG("Opus error, negative discard padding");
    return -1;
  }
  if (aDiscardPadding > 0) {
    OPUS_DEBUG("OpusDecoder discardpadding %" PRId64 "", aDiscardPadding);
    CheckedInt64 discardFrames =
      TimeUnitToFrames(media::TimeUnit::FromNanoseconds(aDiscardPadding),
                       mOpusParser->mRate);
    if (!discardFrames.isValid()) {
      NS_WARNING("Int overflow in DiscardPadding");
      return -1;
    }
    if (discardFrames.value() > frames) {
      // Discarding more than the entire packet is invalid.
      OPUS_DEBUG("Opus error, discard padding larger than packet");
      return -1;
    }
    OPUS_DEBUG("Opus decoder discarding %d of %d frames",
        int32_t(discardFrames.value()), frames);
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
    NS_WARNING("OpusDataDecoder: Int overflow converting WebM audio duration");
    return -1;
  }
  CheckedInt64 time =
    startTime - FramesToUsecs(mOpusParser->mPreSkip, mOpusParser->mRate) +
    FramesToUsecs(mFrames, mOpusParser->mRate);
  if (!time.isValid()) {
    NS_WARNING("OpusDataDecoder: Int overflow shifting tstamp by codec delay");
    return -1;
  };

  mCallback->Output(new AudioData(aSample->mOffset,
                                  time.value(),
                                  duration.value(),
                                  frames,
                                  Move(buffer),
                                  mOpusParser->mChannels,
                                  mOpusParser->mRate));
  mFrames += frames;
  return frames;
}

void
OpusDataDecoder::ProcessDrain()
{
  mCallback->DrainComplete();
}

nsresult
OpusDataDecoder::Drain()
{
  mTaskQueue->Dispatch(NewRunnableMethod(this, &OpusDataDecoder::ProcessDrain));
  return NS_OK;
}

nsresult
OpusDataDecoder::Flush()
{
  if (!mOpusDecoder) {
    return NS_OK;
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
  return NS_OK;
}

/* static */
bool
OpusDataDecoder::IsOpus(const nsACString& aMimeType)
{
  return aMimeType.EqualsLiteral("audio/webm; codecs=opus") ||
         aMimeType.EqualsLiteral("audio/ogg; codecs=opus");
}

} // namespace mozilla
#undef OPUS_DEBUG
