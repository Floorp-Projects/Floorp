/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "OpusTrackEncoder.h"
#include "nsString.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/ProfilerLabels.h"
#include "VideoUtils.h"

#include <opus/opus.h>

#define LOG(args, ...)

namespace mozilla {

// The Opus format supports up to 8 channels, and supports multitrack audio up
// to 255 channels, but the current implementation supports only mono and
// stereo, and downmixes any more than that.
constexpr int MAX_SUPPORTED_AUDIO_CHANNELS = 8;

// http://www.opus-codec.org/docs/html_api-1.0.2/group__opus__encoder.html
// In section "opus_encoder_init", channels must be 1 or 2 of input signal.
constexpr int MAX_CHANNELS = 2;

// A maximum data bytes for Opus to encode.
constexpr int MAX_DATA_BYTES = 4096;

// http://tools.ietf.org/html/draft-ietf-codec-oggopus-00#section-4
// Second paragraph, " The granule position of an audio data page is in units
// of PCM audio samples at a fixed rate of 48 kHz."
constexpr int kOpusSamplingRate = 48000;

// The duration of an Opus frame, and it must be 2.5, 5, 10, 20, 40 or 60 ms.
constexpr int kFrameDurationMs = 20;

// The supported sampling rate of input signal (Hz),
// must be one of the following. Will resampled to 48kHz otherwise.
constexpr int kOpusSupportedInputSamplingRates[] = {8000, 12000, 16000, 24000,
                                                    48000};

namespace {

// An endian-neutral serialization of integers. Serializing T in little endian
// format to aOutput, where T is a 16 bits or 32 bits integer.
template <typename T>
static void SerializeToBuffer(T aValue, nsTArray<uint8_t>* aOutput) {
  for (uint32_t i = 0; i < sizeof(T); i++) {
    aOutput->AppendElement((uint8_t)(0x000000ff & (aValue >> (i * 8))));
  }
}

static inline void SerializeToBuffer(const nsCString& aComment,
                                     nsTArray<uint8_t>* aOutput) {
  // Format of serializing a string to buffer is, the length of string (32 bits,
  // little endian), and the string.
  SerializeToBuffer((uint32_t)(aComment.Length()), aOutput);
  aOutput->AppendElements(aComment.get(), aComment.Length());
}

static void SerializeOpusIdHeader(uint8_t aChannelCount, uint16_t aPreskip,
                                  uint32_t aInputSampleRate,
                                  nsTArray<uint8_t>* aOutput) {
  // The magic signature, null terminator has to be stripped off from strings.
  constexpr uint8_t magic[] = "OpusHead";
  aOutput->AppendElements(magic, sizeof(magic) - 1);

  // The version must always be 1 (8 bits, unsigned).
  aOutput->AppendElement(1);

  // Number of output channels (8 bits, unsigned).
  aOutput->AppendElement(aChannelCount);

  // Number of samples (at 48 kHz) to discard from the decoder output when
  // starting playback (16 bits, unsigned, little endian).
  SerializeToBuffer(aPreskip, aOutput);

  // The sampling rate of input source (32 bits, unsigned, little endian).
  SerializeToBuffer(aInputSampleRate, aOutput);

  // Output gain, an encoder should set this field to zero (16 bits, signed,
  // little endian).
  SerializeToBuffer((int16_t)0, aOutput);

  // Channel mapping family. Family 0 allows only 1 or 2 channels (8 bits,
  // unsigned).
  aOutput->AppendElement(0);
}

static void SerializeOpusCommentHeader(const nsCString& aVendor,
                                       const nsTArray<nsCString>& aComments,
                                       nsTArray<uint8_t>* aOutput) {
  // The magic signature, null terminator has to be stripped off.
  constexpr uint8_t magic[] = "OpusTags";
  aOutput->AppendElements(magic, sizeof(magic) - 1);

  // The vendor; Should append in the following order:
  // vendor string length (32 bits, unsigned, little endian)
  // vendor string.
  SerializeToBuffer(aVendor, aOutput);

  // Add comments; Should append in the following order:
  // comment list length (32 bits, unsigned, little endian)
  // comment #0 string length (32 bits, unsigned, little endian)
  // comment #0 string
  // comment #1 string length (32 bits, unsigned, little endian)
  // comment #1 string ...
  SerializeToBuffer((uint32_t)aComments.Length(), aOutput);
  for (uint32_t i = 0; i < aComments.Length(); ++i) {
    SerializeToBuffer(aComments[i], aOutput);
  }
}

bool IsSampleRateSupported(TrackRate aSampleRate) {
  // According to www.opus-codec.org, creating an opus encoder requires the
  // sampling rate of source signal be one of 8000, 12000, 16000, 24000, or
  // 48000. If this constraint is not satisfied, we resample the input to 48kHz.
  AutoTArray<int, 5> supportedSamplingRates;
  supportedSamplingRates.AppendElements(
      kOpusSupportedInputSamplingRates,
      ArrayLength(kOpusSupportedInputSamplingRates));
  return supportedSamplingRates.Contains(aSampleRate);
}

}  // Anonymous namespace.

OpusTrackEncoder::OpusTrackEncoder(TrackRate aTrackRate,
                                   MediaQueue<EncodedFrame>& aEncodedDataQueue)
    : AudioTrackEncoder(aTrackRate, aEncodedDataQueue),
      mOutputSampleRate(IsSampleRateSupported(aTrackRate) ? aTrackRate
                                                          : kOpusSamplingRate),
      mEncoder(nullptr),
      mLookahead(0),
      mLookaheadWritten(0),
      mResampler(nullptr),
      mNumOutputFrames(0) {}

OpusTrackEncoder::~OpusTrackEncoder() {
  if (mEncoder) {
    opus_encoder_destroy(mEncoder);
  }
  if (mResampler) {
    speex_resampler_destroy(mResampler);
    mResampler = nullptr;
  }
}

nsresult OpusTrackEncoder::Init(int aChannels) {
  NS_ENSURE_TRUE((aChannels <= MAX_SUPPORTED_AUDIO_CHANNELS) && (aChannels > 0),
                 NS_ERROR_FAILURE);

  // This version of encoder API only support 1 or 2 channels,
  // So set the mChannels less or equal 2 and
  // let InterleaveTrackData downmix pcm data.
  mChannels = aChannels > MAX_CHANNELS ? MAX_CHANNELS : aChannels;

  // Reject non-audio sample rates.
  NS_ENSURE_TRUE(mTrackRate >= 8000, NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(mTrackRate <= 192000, NS_ERROR_INVALID_ARG);

  if (NeedsResampler()) {
    int error;
    mResampler = speex_resampler_init(mChannels, mTrackRate, kOpusSamplingRate,
                                      SPEEX_RESAMPLER_QUALITY_DEFAULT, &error);

    if (error != RESAMPLER_ERR_SUCCESS) {
      return NS_ERROR_FAILURE;
    }
  }

  int error = 0;
  mEncoder = opus_encoder_create(mOutputSampleRate, mChannels,
                                 OPUS_APPLICATION_AUDIO, &error);

  if (error != OPUS_OK) {
    return NS_ERROR_FAILURE;
  }

  if (mAudioBitrate) {
    int bps = static_cast<int>(
        std::min<uint32_t>(mAudioBitrate, std::numeric_limits<int>::max()));
    error = opus_encoder_ctl(mEncoder, OPUS_SET_BITRATE(bps));
    if (error != OPUS_OK) {
      return NS_ERROR_FAILURE;
    }
  }

  // In the case of Opus we need to calculate the codec delay based on the
  // pre-skip. For more information see:
  // https://tools.ietf.org/html/rfc7845#section-4.2
  error = opus_encoder_ctl(mEncoder, OPUS_GET_LOOKAHEAD(&mLookahead));
  if (error != OPUS_OK) {
    mLookahead = 0;
    return NS_ERROR_FAILURE;
  }

  SetInitialized();

  return NS_OK;
}

int OpusTrackEncoder::GetLookahead() const {
  return mLookahead * kOpusSamplingRate / mOutputSampleRate;
}

int OpusTrackEncoder::NumInputFramesPerPacket() const {
  return mTrackRate * kFrameDurationMs / 1000;
}

int OpusTrackEncoder::NumOutputFramesPerPacket() const {
  return mOutputSampleRate * kFrameDurationMs / 1000;
}

bool OpusTrackEncoder::NeedsResampler() const {
  // A resampler is needed when mTrackRate is not supported by the opus encoder.
  // This is equivalent to !IsSampleRateSupported(mTrackRate) but less cycles.
  return mTrackRate != mOutputSampleRate &&
         mOutputSampleRate == kOpusSamplingRate;
}

already_AddRefed<TrackMetadataBase> OpusTrackEncoder::GetMetadata() {
  AUTO_PROFILER_LABEL("OpusTrackEncoder::GetMetadata", OTHER);

  MOZ_ASSERT(mInitialized);

  if (!mInitialized) {
    return nullptr;
  }

  RefPtr<OpusMetadata> meta = new OpusMetadata();
  meta->mChannels = mChannels;
  meta->mSamplingFrequency = mTrackRate;

  // Ogg and Webm timestamps are always sampled at 48k for Opus.
  SerializeOpusIdHeader(mChannels,
                        mLookahead * (kOpusSamplingRate / mOutputSampleRate),
                        mTrackRate, &meta->mIdHeader);

  nsCString vendor;
  vendor.AppendASCII(opus_get_version_string());

  nsTArray<nsCString> comments;
  comments.AppendElement(
      nsLiteralCString("ENCODER=Mozilla" MOZ_APP_UA_VERSION));

  SerializeOpusCommentHeader(vendor, comments, &meta->mCommentHeader);

  return meta.forget();
}

nsresult OpusTrackEncoder::Encode(AudioSegment* aSegment) {
  AUTO_PROFILER_LABEL("OpusTrackEncoder::Encode", OTHER);

  MOZ_ASSERT(aSegment);
  MOZ_ASSERT(mInitialized || mCanceled);

  if (mCanceled || IsEncodingComplete()) {
    return NS_ERROR_FAILURE;
  }

  if (!mInitialized) {
    // calculation below depends on the truth that mInitialized is true.
    return NS_ERROR_FAILURE;
  }

  int result = 0;
  // Loop until we run out of packets of input data
  while (result >= 0 && !IsEncodingComplete()) {
    // re-sampled frames left last time which didn't fit into an Opus packet
    // duration.
    const int framesLeft = mResampledLeftover.Length() / mChannels;
    MOZ_ASSERT(NumOutputFramesPerPacket() >= framesLeft);
    // Fetch input frames such that there will be n frames where (n +
    // framesLeft) >= NumOutputFramesPerPacket() after re-sampling.
    const int framesToFetch = NumInputFramesPerPacket() -
                              (framesLeft * mTrackRate / kOpusSamplingRate) +
                              (NeedsResampler() ? 1 : 0);

    if (!mEndOfStream && aSegment->GetDuration() < framesToFetch) {
      // Not enough raw data
      return NS_OK;
    }

    // Start encoding data.
    AutoTArray<AudioDataValue, 9600> pcm;
    pcm.SetLength(NumOutputFramesPerPacket() * mChannels);

    int frameCopied = 0;

    for (AudioSegment::ChunkIterator iter(*aSegment);
         !iter.IsEnded() && frameCopied < framesToFetch; iter.Next()) {
      AudioChunk chunk = *iter;

      // Chunk to the required frame size.
      TrackTime frameToCopy =
          std::min(chunk.GetDuration(),
                   static_cast<TrackTime>(framesToFetch - frameCopied));

      // Possible greatest value of framesToFetch = 3844: see
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1349421#c8. frameToCopy
      // should not be able to exceed this value.
      MOZ_ASSERT(frameToCopy <= 3844, "frameToCopy exceeded expected range");

      if (!chunk.IsNull()) {
        // Append the interleaved data to the end of pcm buffer.
        AudioTrackEncoder::InterleaveTrackData(
            chunk, frameToCopy, mChannels,
            pcm.Elements() + frameCopied * mChannels);
      } else {
        CheckedInt<int> memsetLength =
            CheckedInt<int>(frameToCopy) * mChannels * sizeof(AudioDataValue);
        if (!memsetLength.isValid()) {
          // This should never happen, but we use a defensive check because
          // we really don't want a bad memset
          MOZ_ASSERT_UNREACHABLE("memsetLength invalid!");
          return NS_ERROR_FAILURE;
        }
        memset(pcm.Elements() + frameCopied * mChannels, 0,
               memsetLength.value());
      }

      frameCopied += frameToCopy;
    }

    // Possible greatest value of framesToFetch = 3844: see
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1349421#c8. frameCopied
    // should not be able to exceed this value.
    MOZ_ASSERT(frameCopied <= 3844, "frameCopied exceeded expected range");

    int framesInPCM = frameCopied;
    if (mResampler) {
      AutoTArray<AudioDataValue, 9600> resamplingDest;
      uint32_t inframes = frameCopied;
      uint32_t outframes = inframes * kOpusSamplingRate / mTrackRate + 1;

      // We want to consume all the input data, so we slightly oversize the
      // resampled data buffer so we can fit the output data in. We cannot
      // really predict the output frame count at each call.
      resamplingDest.SetLength(outframes * mChannels);

      float* in = reinterpret_cast<float*>(pcm.Elements());
      float* out = reinterpret_cast<float*>(resamplingDest.Elements());
      speex_resampler_process_interleaved_float(mResampler, in, &inframes, out,
                                                &outframes);

      MOZ_ASSERT(pcm.Length() >= mResampledLeftover.Length());
      PodCopy(pcm.Elements(), mResampledLeftover.Elements(),
              mResampledLeftover.Length());

      uint32_t outframesToCopy = std::min(
          outframes,
          static_cast<uint32_t>(NumOutputFramesPerPacket() - framesLeft));

      MOZ_ASSERT(pcm.Length() - mResampledLeftover.Length() >=
                 outframesToCopy * mChannels);
      PodCopy(pcm.Elements() + mResampledLeftover.Length(),
              resamplingDest.Elements(), outframesToCopy * mChannels);
      int frameLeftover = outframes - outframesToCopy;
      mResampledLeftover.SetLength(frameLeftover * mChannels);
      PodCopy(mResampledLeftover.Elements(),
              resamplingDest.Elements() + outframesToCopy * mChannels,
              mResampledLeftover.Length());
      // This is always at 48000Hz.
      framesInPCM = framesLeft + outframesToCopy;
    }

    // Remove the raw data which has been pulled to pcm buffer.
    // The value of frameCopied should be equal to (or smaller than, if eos)
    // NumOutputFramesPerPacket().
    aSegment->RemoveLeading(frameCopied);

    // Has reached the end of input stream and all queued data has pulled for
    // encoding.
    bool isFinalPacket = false;
    if (aSegment->GetDuration() == 0 && mEndOfStream &&
        framesInPCM < NumOutputFramesPerPacket()) {
      // Pad |mLookahead| samples to the end of the track to prevent loss of
      // original data.
      const int toWrite = std::min(mLookahead - mLookaheadWritten,
                                   NumOutputFramesPerPacket() - framesInPCM);
      PodZero(pcm.Elements() + framesInPCM * mChannels, toWrite * mChannels);
      mLookaheadWritten += toWrite;
      framesInPCM += toWrite;
      if (mLookaheadWritten == mLookahead) {
        isFinalPacket = true;
      }
    }

    MOZ_ASSERT_IF(!isFinalPacket, framesInPCM == NumOutputFramesPerPacket());

    // Append null data to pcm buffer if the leftover data is not enough for
    // opus encoder.
    if (framesInPCM < NumOutputFramesPerPacket() && isFinalPacket) {
      PodZero(pcm.Elements() + framesInPCM * mChannels,
              (NumOutputFramesPerPacket() - framesInPCM) * mChannels);
    }
    auto frameData = MakeRefPtr<EncodedFrame::FrameData>();
    // Encode the data with Opus Encoder.
    frameData->SetLength(MAX_DATA_BYTES);
    // result is returned as opus error code if it is negative.
    result = 0;
    const float* pcmBuf = static_cast<float*>(pcm.Elements());
    result = opus_encode_float(mEncoder, pcmBuf, NumOutputFramesPerPacket(),
                               frameData->Elements(), MAX_DATA_BYTES);
    frameData->SetLength(result >= 0 ? result : 0);

    if (result < 0) {
      LOG("[Opus] Fail to encode data! Result: %s.", opus_strerror(result));
    }
    if (isFinalPacket) {
      if (mResampler) {
        speex_resampler_destroy(mResampler);
        mResampler = nullptr;
      }
      mResampledLeftover.SetLength(0);
    }

    // timestamp should be the time of the first sample
    mEncodedDataQueue.Push(MakeAndAddRef<EncodedFrame>(
        media::TimeUnit(mNumOutputFrames + mLookahead, mOutputSampleRate),
        static_cast<uint64_t>(framesInPCM) * kOpusSamplingRate /
            mOutputSampleRate,
        kOpusSamplingRate, EncodedFrame::OPUS_AUDIO_FRAME,
        std::move(frameData)));

    mNumOutputFrames += NumOutputFramesPerPacket();
    LOG("[Opus] mOutputTimeStamp %.3f.",
        media::TimeUnit(mNumOutputFrames, mOutputSampleRate).ToSeconds());

    if (isFinalPacket) {
      LOG("[Opus] Done encoding.");
      mEncodedDataQueue.Finish();
    }
  }

  return result >= 0 ? NS_OK : NS_ERROR_FAILURE;
}

}  // namespace mozilla

#undef LOG
