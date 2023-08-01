/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OpusDecoder.h"

#include <inttypes.h>  // For PRId64

#include "OpusParser.h"
#include "TimeUnits.h"
#include "VideoUtils.h"
#include "VorbisDecoder.h"  // For VorbisLayout
#include "mozilla/EndianUtils.h"
#include "mozilla/PodOperations.h"
#include "mozilla/SyncRunnable.h"
#include <opus/opus.h>
extern "C" {
#include <opus/opus_multistream.h>
}

#define OPUS_DEBUG(arg, ...)                                           \
  DDMOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, "::%s: " arg, __func__, \
            ##__VA_ARGS__)

namespace mozilla {

OpusDataDecoder::OpusDataDecoder(const CreateDecoderParams& aParams)
    : mInfo(aParams.AudioConfig()),
      mOpusDecoder(nullptr),
      mSkip(0),
      mDecodedHeader(false),
      mPaddingDiscarded(false),
      mFrames(0),
      mChannelMap(AudioConfig::ChannelLayout::UNKNOWN_MAP),
      mDefaultPlaybackDeviceMono(aParams.mOptions.contains(
          CreateDecoderParams::Option::DefaultPlaybackDeviceMono)) {}

OpusDataDecoder::~OpusDataDecoder() {
  if (mOpusDecoder) {
    opus_multistream_decoder_destroy(mOpusDecoder);
    mOpusDecoder = nullptr;
  }
}

RefPtr<ShutdownPromise> OpusDataDecoder::Shutdown() {
  // mThread may not be set if Init hasn't been called first.
  MOZ_ASSERT(!mThread || mThread->IsOnCurrentThread());
  return ShutdownPromise::CreateAndResolve(true, __func__);
}

RefPtr<MediaDataDecoder::InitPromise> OpusDataDecoder::Init() {
  mThread = GetCurrentSerialEventTarget();
  if (!mInfo.mCodecSpecificConfig.is<OpusCodecSpecificData>()) {
    MOZ_ASSERT_UNREACHABLE();
    OPUS_DEBUG("Opus decoder got non-opus codec specific data");
    return InitPromise::CreateAndReject(
        MediaResult(
            NS_ERROR_DOM_MEDIA_FATAL_ERR,
            RESULT_DETAIL("Opus decoder got non-opus codec specific data!")),
        __func__);
  }
  const OpusCodecSpecificData opusCodecSpecificData =
      mInfo.mCodecSpecificConfig.as<OpusCodecSpecificData>();
  RefPtr<MediaByteBuffer> opusHeaderBlob =
      opusCodecSpecificData.mHeadersBinaryBlob;
  size_t length = opusHeaderBlob->Length();
  uint8_t* p = opusHeaderBlob->Elements();
  if (NS_FAILED(DecodeHeader(p, length))) {
    OPUS_DEBUG("Error decoding header!");
    return InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Error decoding header!")),
        __func__);
  }

  MOZ_ASSERT(mMappingTable.Length() >= uint32_t(mOpusParser->mChannels));
  int r;
  mOpusDecoder = opus_multistream_decoder_create(
      mOpusParser->mRate, mOpusParser->mChannels, mOpusParser->mStreams,
      mOpusParser->mCoupledStreams, mMappingTable.Elements(), &r);

  if (!mOpusDecoder) {
    OPUS_DEBUG("Error creating decoder!");
    return InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Error creating decoder!")),
        __func__);
  }

  // Opus has a special feature for stereo coding where it represent wide
  // stereo channels by 180-degree out of phase. This improves quality, but
  // needs to be disabled when the output is downmixed to mono. Playback number
  // of channels are set in AudioSink, using the same method
  // `DecideAudioPlaybackChannels()`, and triggers downmix if needed.
  if (mDefaultPlaybackDeviceMono || DecideAudioPlaybackChannels(mInfo) == 1) {
    opus_multistream_decoder_ctl(mOpusDecoder,
                                 OPUS_SET_PHASE_INVERSION_DISABLED(1));
  }

  mSkip = mOpusParser->mPreSkip;
  mPaddingDiscarded = false;

  if (opusCodecSpecificData.mContainerCodecDelayFrames !=
      mOpusParser->mPreSkip) {
    NS_WARNING(
        "Invalid Opus header: container CodecDelay and Opus pre-skip do not "
        "match!");
  }
  OPUS_DEBUG("Opus preskip in extradata: %" PRId64 " frames",
             opusCodecSpecificData.mContainerCodecDelayFrames);

  if (mInfo.mRate != (uint32_t)mOpusParser->mRate) {
    NS_WARNING("Invalid Opus header: container and codec rate do not match!");
  }
  if (mInfo.mChannels != (uint32_t)mOpusParser->mChannels) {
    NS_WARNING(
        "Invalid Opus header: container and codec channels do not match!");
  }

  return r == OPUS_OK
             ? InitPromise::CreateAndResolve(TrackInfo::kAudioTrack, __func__)
             : InitPromise::CreateAndReject(
                   MediaResult(
                       NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL(
                           "could not create opus multistream decoder!")),
                   __func__);
}

nsresult OpusDataDecoder::DecodeHeader(const unsigned char* aData,
                                       size_t aLength) {
  MOZ_ASSERT(!mOpusParser);
  MOZ_ASSERT(!mOpusDecoder);
  MOZ_ASSERT(!mDecodedHeader);
  mDecodedHeader = true;

  mOpusParser = MakeUnique<OpusParser>();
  if (!mOpusParser->DecodeHeader(const_cast<unsigned char*>(aData), aLength)) {
    return NS_ERROR_FAILURE;
  }
  int channels = mOpusParser->mChannels;

  mMappingTable.SetLength(channels);
  AudioConfig::ChannelLayout vorbisLayout(
      channels, VorbisDataDecoder::VorbisLayout(channels));
  if (vorbisLayout.IsValid()) {
    mChannelMap = vorbisLayout.Map();

    AudioConfig::ChannelLayout smpteLayout(
        AudioConfig::ChannelLayout::SMPTEDefault(vorbisLayout));

    AutoTArray<uint8_t, 8> map;
    map.SetLength(channels);
    if (mOpusParser->mChannelMapping == 1 &&
        vorbisLayout.MappingTable(smpteLayout, &map)) {
      for (int i = 0; i < channels; i++) {
        mMappingTable[i] = mOpusParser->mMappingTable[map[i]];
      }
    } else {
      // Use Opus set channel mapping and return channels as-is.
      PodCopy(mMappingTable.Elements(), mOpusParser->mMappingTable, channels);
    }
  } else {
    // Create a dummy mapping table so that channel ordering stay the same
    // during decoding.
    for (int i = 0; i < channels; i++) {
      mMappingTable[i] = i;
    }
  }

  return NS_OK;
}

RefPtr<MediaDataDecoder::DecodePromise> OpusDataDecoder::Decode(
    MediaRawData* aSample) {
  MOZ_ASSERT(mThread->IsOnCurrentThread());
  PROCESS_DECODE_LOG(aSample);
  uint32_t channels = mOpusParser->mChannels;

  if (mPaddingDiscarded) {
    // Discard padding should be used only on the final packet, so
    // decoding after a padding discard is invalid.
    OPUS_DEBUG("Opus error, discard padding on interstitial packet");
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Discard padding on interstitial packet")),
        __func__);
  }

  if (!mLastFrameTime ||
      mLastFrameTime.ref() != aSample->mTime.ToMicroseconds()) {
    // We are starting a new block.
    mFrames = 0;
    mLastFrameTime = Some(aSample->mTime.ToMicroseconds());
  }

  // Maximum value is 63*2880, so there's no chance of overflow.
  int frames_number =
      opus_packet_get_nb_frames(aSample->Data(), aSample->Size());
  if (frames_number <= 0) {
    OPUS_DEBUG("Invalid packet header: r=%d length=%zu", frames_number,
               aSample->Size());
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                    RESULT_DETAIL("Invalid packet header: r=%d length=%u",
                                  frames_number, uint32_t(aSample->Size()))),
        __func__);
  }

  int samples = opus_packet_get_samples_per_frame(
      aSample->Data(), opus_int32(mOpusParser->mRate));

  // A valid Opus packet must be between 2.5 and 120 ms long (48kHz).
  CheckedInt32 totalFrames =
      CheckedInt32(frames_number) * CheckedInt32(samples);
  if (!totalFrames.isValid()) {
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                    RESULT_DETAIL("Frames count overflow")),
        __func__);
  }

  int frames = totalFrames.value();
  if (frames < 120 || frames > 5760) {
    OPUS_DEBUG("Invalid packet frames: %d", frames);
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                    RESULT_DETAIL("Invalid packet frames:%d", frames)),
        __func__);
  }

  AlignedAudioBuffer buffer(frames * channels);
  if (!buffer) {
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__), __func__);
  }

  // Decode to the appropriate sample type.
#ifdef MOZ_SAMPLE_TYPE_FLOAT32
  int ret = opus_multistream_decode_float(mOpusDecoder, aSample->Data(),
                                          aSample->Size(), buffer.get(), frames,
                                          false);
#else
  int ret =
      opus_multistream_decode(mOpusDecoder, aSample->Data(), aSample->Size(),
                              buffer.get(), frames, false);
#endif
  if (ret < 0) {
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                    RESULT_DETAIL("Opus decoding error:%d", ret)),
        __func__);
  }
  NS_ASSERTION(ret == frames, "Opus decoded too few audio samples");
  auto startTime = aSample->mTime;

  OPUS_DEBUG("Decoding frames: [%lf, %lf]", aSample->mTime.ToSeconds(),
             aSample->GetEndTime().ToSeconds());

  // Trim the initial frames while the decoder is settling.
  if (mSkip > 0) {
    int32_t skipFrames = std::min<int32_t>(mSkip, frames);
    int32_t keepFrames = frames - skipFrames;
    OPUS_DEBUG("Opus decoder trimming %d of %d frames", skipFrames, frames);
    PodMove(buffer.get(), buffer.get() + skipFrames * channels,
            keepFrames * channels);
    startTime = startTime + media::TimeUnit(skipFrames, mOpusParser->mRate);
    frames = keepFrames;
    mSkip -= skipFrames;
    aSample->mTime += media::TimeUnit(skipFrames, 48000);
    aSample->mDuration -= media::TimeUnit(skipFrames, 48000);
    OPUS_DEBUG("Adjusted frame after trimming pre-roll: [%lf, %lf]",
               aSample->mTime.ToSeconds(), aSample->GetEndTime().ToSeconds());
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
      int32_t val = static_cast<int32_t>((gain_Q16 * buffer[i] + 32768) >> 16);
      buffer[i] = static_cast<AudioDataValue>(MOZ_CLIP_TO_15(val));
    }
  }
#endif

  auto duration = media::TimeUnit(frames, mOpusParser->mRate);
  if (!duration.IsValid()) {
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
                    RESULT_DETAIL("Overflow converting WebM audio duration")),
        __func__);
  }
  auto time = startTime -
              media::TimeUnit(mOpusParser->mPreSkip, mOpusParser->mRate) +
              media::TimeUnit(mFrames, mOpusParser->mRate);
  if (!time.IsValid()) {
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
                    RESULT_DETAIL("Overflow shifting tstamp by codec delay")),
        __func__);
  };

  mFrames += frames;
  mTotalFrames += frames;

  OPUS_DEBUG("Total frames so far: %" PRId64, mTotalFrames);

  if (!frames) {
    return DecodePromise::CreateAndResolve(DecodedData(), __func__);
  }

  // Trim extra allocated frames.
  buffer.SetLength(frames * channels);

  return DecodePromise::CreateAndResolve(
      DecodedData{new AudioData(aSample->mOffset, time, std::move(buffer),
                                mOpusParser->mChannels, mOpusParser->mRate,
                                mChannelMap)},
      __func__);
}

RefPtr<MediaDataDecoder::DecodePromise> OpusDataDecoder::Drain() {
  MOZ_ASSERT(mThread->IsOnCurrentThread());
  return DecodePromise::CreateAndResolve(DecodedData(), __func__);
}

RefPtr<MediaDataDecoder::FlushPromise> OpusDataDecoder::Flush() {
  MOZ_ASSERT(mThread->IsOnCurrentThread());
  if (!mOpusDecoder) {
    return FlushPromise::CreateAndResolve(true, __func__);
  }

  MOZ_ASSERT(mOpusDecoder);
  // Reset the decoder.
  opus_multistream_decoder_ctl(mOpusDecoder, OPUS_RESET_STATE);
  mSkip = mOpusParser->mPreSkip;
  mPaddingDiscarded = false;
  mLastFrameTime.reset();
  return FlushPromise::CreateAndResolve(true, __func__);
}

/* static */
bool OpusDataDecoder::IsOpus(const nsACString& aMimeType) {
  return aMimeType.EqualsLiteral("audio/opus");
}

}  // namespace mozilla
#undef OPUS_DEBUG
