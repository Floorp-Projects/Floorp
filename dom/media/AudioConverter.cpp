/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioConverter.h"
#include <string.h>
#include <speex/speex_resampler.h>
#include <cmath>

/*
 *  Parts derived from MythTV AudioConvert Class
 *  Created by Jean-Yves Avenard.
 *
 *  Copyright (C) Bubblestuff Pty Ltd 2013
 *  Copyright (C) foobum@gmail.com 2010
 */

namespace mozilla {

AudioConverter::AudioConverter(const AudioConfig& aIn, const AudioConfig& aOut)
  : mIn(aIn)
  , mOut(aOut)
  , mResampler(nullptr)
{
  MOZ_DIAGNOSTIC_ASSERT(aIn.Format() == aOut.Format() &&
                        aIn.Interleaved() == aOut.Interleaved(),
                        "No format or rate conversion is supported at this stage");
  MOZ_DIAGNOSTIC_ASSERT(aOut.Channels() <= 2 ||
                        aIn.Channels() == aOut.Channels(),
                        "Only down/upmixing to mono or stereo is supported at this stage");
  MOZ_DIAGNOSTIC_ASSERT(aOut.Interleaved(), "planar audio format not supported");
  mIn.Layout().MappingTable(mOut.Layout(), mChannelOrderMap);
  if (aIn.Rate() != aOut.Rate()) {
    RecreateResampler();
  }
}

AudioConverter::~AudioConverter()
{
  if (mResampler) {
    speex_resampler_destroy(mResampler);
    mResampler = nullptr;
  }
}

bool
AudioConverter::CanWorkInPlace() const
{
  bool needDownmix = mIn.Channels() > mOut.Channels();
  bool needUpmix = mIn.Channels() < mOut.Channels();
  bool canDownmixInPlace =
    mIn.Channels() * AudioConfig::SampleSize(mIn.Format()) >=
    mOut.Channels() * AudioConfig::SampleSize(mOut.Format());
  bool needResample = mIn.Rate() != mOut.Rate();
  bool canResampleInPlace = mIn.Rate() >= mOut.Rate();
  // We should be able to work in place if 1s of audio input takes less space
  // than 1s of audio output. However, as we downmix before resampling we can't
  // perform any upsampling in place (e.g. if incoming rate >= outgoing rate)
  return !needUpmix && (!needDownmix || canDownmixInPlace) &&
         (!needResample || canResampleInPlace);
}

size_t
AudioConverter::ProcessInternal(void* aOut, const void* aIn, size_t aFrames)
{
  if (mIn.Channels() > mOut.Channels()) {
    return DownmixAudio(aOut, aIn, aFrames);
  } else if (mIn.Channels() < mOut.Channels()) {
    return UpmixAudio(aOut, aIn, aFrames);
  } else if (mIn.Layout() != mOut.Layout() && CanReorderAudio()) {
    ReOrderInterleavedChannels(aOut, aIn, aFrames);
  } else if (aIn != aOut) {
    memmove(aOut, aIn, FramesOutToBytes(aFrames));
  }
  return aFrames;
}

// Reorder interleaved channels.
// Can work in place (e.g aOut == aIn).
template <class AudioDataType>
void
_ReOrderInterleavedChannels(AudioDataType* aOut, const AudioDataType* aIn,
                            uint32_t aFrames, uint32_t aChannels,
                            const uint8_t* aChannelOrderMap)
{
  MOZ_DIAGNOSTIC_ASSERT(aChannels <= MAX_AUDIO_CHANNELS);
  AudioDataType val[MAX_AUDIO_CHANNELS];
  for (uint32_t i = 0; i < aFrames; i++) {
    for (uint32_t j = 0; j < aChannels; j++) {
      val[j] = aIn[aChannelOrderMap[j]];
    }
    for (uint32_t j = 0; j < aChannels; j++) {
      aOut[j] = val[j];
    }
    aOut += aChannels;
    aIn += aChannels;
  }
}

void
AudioConverter::ReOrderInterleavedChannels(void* aOut, const void* aIn,
                                           size_t aFrames) const
{
  MOZ_DIAGNOSTIC_ASSERT(mIn.Channels() == mOut.Channels());

  if (mOut.Channels() == 1 || mOut.Layout() == mIn.Layout()) {
    // If channel count is 1, planar and non-planar formats are the same and
    // there's nothing to reorder.
    if (aOut != aIn) {
      memmove(aOut, aIn, FramesOutToBytes(aFrames));
    }
    return;
  }

  uint32_t bits = AudioConfig::FormatToBits(mOut.Format());
  switch (bits) {
    case 8:
      _ReOrderInterleavedChannels((uint8_t*)aOut, (const uint8_t*)aIn,
                                  aFrames, mIn.Channels(), mChannelOrderMap);
      break;
    case 16:
      _ReOrderInterleavedChannels((int16_t*)aOut,(const int16_t*)aIn,
                                  aFrames, mIn.Channels(), mChannelOrderMap);
      break;
    default:
      MOZ_DIAGNOSTIC_ASSERT(AudioConfig::SampleSize(mOut.Format()) == 4);
      _ReOrderInterleavedChannels((int32_t*)aOut,(const int32_t*)aIn,
                                  aFrames, mIn.Channels(), mChannelOrderMap);
      break;
  }
}

static inline int16_t clipTo15(int32_t aX)
{
  return aX < -32768 ? -32768 : aX <= 32767 ? aX : 32767;
}

size_t
AudioConverter::DownmixAudio(void* aOut, const void* aIn, size_t aFrames) const
{
  MOZ_ASSERT(mIn.Format() == AudioConfig::FORMAT_S16 ||
             mIn.Format() == AudioConfig::FORMAT_FLT);
  MOZ_ASSERT(mIn.Channels() >= mOut.Channels());
  MOZ_ASSERT(mIn.Layout() == AudioConfig::ChannelLayout(mIn.Channels()),
             "Can only downmix input data in SMPTE layout");
  MOZ_ASSERT(mOut.Layout() == AudioConfig::ChannelLayout(2) ||
             mOut.Layout() == AudioConfig::ChannelLayout(1));

  uint32_t channels = mIn.Channels();

  if (channels == 1 && mOut.Channels() == 1) {
    if (aOut != aIn) {
      memmove(aOut, aIn, FramesOutToBytes(aFrames));
    }
    return aFrames;
  }

  if (channels > 2) {
    if (mIn.Format() == AudioConfig::FORMAT_FLT) {
      // Downmix matrix. Per-row normalization 1 for rows 3,4 and 2 for rows 5-8.
      static const float dmatrix[6][8][2]= {
          /*3*/{{0.5858f,0},{0,0.5858f},{0.4142f,0.4142f}},
          /*4*/{{0.4226f,0},{0,0.4226f},{0.366f, 0.2114f},{0.2114f,0.366f}},
          /*5*/{{0.6510f,0},{0,0.6510f},{0.4600f,0.4600f},{0.5636f,0.3254f},{0.3254f,0.5636f}},
          /*6*/{{0.5290f,0},{0,0.5290f},{0.3741f,0.3741f},{0.3741f,0.3741f},{0.4582f,0.2645f},{0.2645f,0.4582f}},
          /*7*/{{0.4553f,0},{0,0.4553f},{0.3220f,0.3220f},{0.3220f,0.3220f},{0.2788f,0.2788f},{0.3943f,0.2277f},{0.2277f,0.3943f}},
          /*8*/{{0.3886f,0},{0,0.3886f},{0.2748f,0.2748f},{0.2748f,0.2748f},{0.3366f,0.1943f},{0.1943f,0.3366f},{0.3366f,0.1943f},{0.1943f,0.3366f}},
      };
      // Re-write the buffer with downmixed data
      const float* in = static_cast<const float*>(aIn);
      float* out = static_cast<float*>(aOut);
      for (uint32_t i = 0; i < aFrames; i++) {
        float sampL = 0.0;
        float sampR = 0.0;
        for (uint32_t j = 0; j < channels; j++) {
          sampL += in[i*mIn.Channels()+j]*dmatrix[mIn.Channels()-3][j][0];
          sampR += in[i*mIn.Channels()+j]*dmatrix[mIn.Channels()-3][j][1];
        }
        *out++ = sampL;
        *out++ = sampR;
      }
    } else if (mIn.Format() == AudioConfig::FORMAT_S16) {
      // Downmix matrix. Per-row normalization 1 for rows 3,4 and 2 for rows 5-8.
      // Coefficients in Q14.
      static const int16_t dmatrix[6][8][2]= {
          /*3*/{{9598, 0},{0,   9598},{6786,6786}},
          /*4*/{{6925, 0},{0,   6925},{5997,3462},{3462,5997}},
          /*5*/{{10663,0},{0,  10663},{7540,7540},{9234,5331},{5331,9234}},
          /*6*/{{8668, 0},{0,   8668},{6129,6129},{6129,6129},{7507,4335},{4335,7507}},
          /*7*/{{7459, 0},{0,   7459},{5275,5275},{5275,5275},{4568,4568},{6460,3731},{3731,6460}},
          /*8*/{{6368, 0},{0,   6368},{4502,4502},{4502,4502},{5514,3184},{3184,5514},{5514,3184},{3184,5514}}
      };
      // Re-write the buffer with downmixed data
      const int16_t* in = static_cast<const int16_t*>(aIn);
      int16_t* out = static_cast<int16_t*>(aOut);
      for (uint32_t i = 0; i < aFrames; i++) {
        int32_t sampL = 0;
        int32_t sampR = 0;
        for (uint32_t j = 0; j < channels; j++) {
          sampL+=in[i*channels+j]*dmatrix[channels-3][j][0];
          sampR+=in[i*channels+j]*dmatrix[channels-3][j][1];
        }
        *out++ = clipTo15((sampL + 8192)>>14);
        *out++ = clipTo15((sampR + 8192)>>14);
      }
    } else {
      MOZ_DIAGNOSTIC_ASSERT(false, "Unsupported data type");
    }

    // If we are to continue downmixing to mono, start working on the output
    // buffer.
    aIn = aOut;
    channels = 2;
  }

  if (mOut.Channels() == 1) {
    if (mIn.Format() == AudioConfig::FORMAT_FLT) {
      const float* in = static_cast<const float*>(aIn);
      float* out = static_cast<float*>(aOut);
      for (size_t fIdx = 0; fIdx < aFrames; ++fIdx) {
        float sample = 0.0;
        // The sample of the buffer would be interleaved.
        sample = (in[fIdx*channels] + in[fIdx*channels + 1]) * 0.5;
        *out++ = sample;
      }
    } else if (mIn.Format() == AudioConfig::FORMAT_S16) {
      const int16_t* in = static_cast<const int16_t*>(aIn);
      int16_t* out = static_cast<int16_t*>(aOut);
      for (size_t fIdx = 0; fIdx < aFrames; ++fIdx) {
        int32_t sample = 0.0;
        // The sample of the buffer would be interleaved.
        sample = (in[fIdx*channels] + in[fIdx*channels + 1]) * 0.5;
        *out++ = sample;
      }
    } else {
      MOZ_DIAGNOSTIC_ASSERT(false, "Unsupported data type");
    }
  }
  return aFrames;
}

size_t
AudioConverter::ResampleAudio(void* aOut, const void* aIn, size_t aFrames)
{
  if (!mResampler) {
    return 0;
  }
  uint32_t outframes = ResampleRecipientFrames(aFrames);
  uint32_t inframes = aFrames;

  int error;
  if (mOut.Format() == AudioConfig::FORMAT_FLT) {
    const float* in = reinterpret_cast<const float*>(aIn);
    float* out = reinterpret_cast<float*>(aOut);
    error =
      speex_resampler_process_interleaved_float(mResampler, in, &inframes,
                                                out, &outframes);
  } else if (mOut.Format() == AudioConfig::FORMAT_S16) {
    const int16_t* in = reinterpret_cast<const int16_t*>(aIn);
    int16_t* out = reinterpret_cast<int16_t*>(aOut);
    error =
      speex_resampler_process_interleaved_int(mResampler, in, &inframes,
                                              out, &outframes);
  } else {
    MOZ_DIAGNOSTIC_ASSERT(false, "Unsupported data type");
    error = RESAMPLER_ERR_ALLOC_FAILED;
  }
  MOZ_ASSERT(error == RESAMPLER_ERR_SUCCESS);
  if (error != RESAMPLER_ERR_SUCCESS) {
    speex_resampler_destroy(mResampler);
    mResampler = nullptr;
    return 0;
  }
  MOZ_ASSERT(inframes == aFrames, "Some frames will be dropped");
  return outframes;
}

void
AudioConverter::RecreateResampler()
{
  if (mResampler) {
    speex_resampler_destroy(mResampler);
  }
  int error;
  mResampler = speex_resampler_init(mOut.Channels(),
                                    mIn.Rate(),
                                    mOut.Rate(),
                                    SPEEX_RESAMPLER_QUALITY_DEFAULT,
                                    &error);

  if (error == RESAMPLER_ERR_SUCCESS) {
    speex_resampler_skip_zeros(mResampler);
  } else {
    NS_WARNING("Failed to initialize resampler.");
    mResampler = nullptr;
  }
}

size_t
AudioConverter::DrainResampler(void* aOut)
{
  if (!mResampler) {
    return 0;
  }
  int frames = speex_resampler_get_input_latency(mResampler);
  AlignedByteBuffer buffer(FramesOutToBytes(frames));
  if (!buffer) {
    // OOM
    return 0;
  }
  frames = ResampleAudio(aOut, buffer.Data(), frames);
  // Tore down the resampler as it's easier than handling follow-up.
  RecreateResampler();
  return frames;
}

size_t
AudioConverter::UpmixAudio(void* aOut, const void* aIn, size_t aFrames) const
{
  MOZ_ASSERT(mIn.Format() == AudioConfig::FORMAT_S16 ||
             mIn.Format() == AudioConfig::FORMAT_FLT);
  MOZ_ASSERT(mIn.Channels() < mOut.Channels());
  MOZ_ASSERT(mIn.Channels() == 1, "Can only upmix mono for now");
  MOZ_ASSERT(mOut.Channels() == 2, "Can only upmix to stereo for now");

  if (mOut.Channels() != 2) {
    return 0;
  }

  // Upmix mono to stereo.
  // This is a very dumb mono to stereo upmixing, power levels are preserved
  // following the calculation: left = right = -3dB*mono.
  if (mIn.Format() == AudioConfig::FORMAT_FLT) {
    const float m3db = std::sqrt(0.5); // -3dB = sqrt(1/2)
    const float* in = static_cast<const float*>(aIn);
    float* out = static_cast<float*>(aOut);
    for (size_t fIdx = 0; fIdx < aFrames; ++fIdx) {
      float sample = in[fIdx] * m3db;
      // The samples of the buffer would be interleaved.
      *out++ = sample;
      *out++ = sample;
    }
  } else if (mIn.Format() == AudioConfig::FORMAT_S16) {
    const int16_t* in = static_cast<const int16_t*>(aIn);
    int16_t* out = static_cast<int16_t*>(aOut);
    for (size_t fIdx = 0; fIdx < aFrames; ++fIdx) {
      int16_t sample = ((int32_t)in[fIdx] * 11585) >> 14; // close enough to i*sqrt(0.5)
      // The samples of the buffer would be interleaved.
      *out++ = sample;
      *out++ = sample;
    }
  } else {
    MOZ_DIAGNOSTIC_ASSERT(false, "Unsupported data type");
  }

  return aFrames;
}

size_t
AudioConverter::ResampleRecipientFrames(size_t aFrames) const
{
  if (!aFrames && mIn.Rate() != mOut.Rate()) {
    // The resampler will be drained, account for frames currently buffered
    // in the resampler.
    if (!mResampler) {
      return 0;
    }
    return speex_resampler_get_output_latency(mResampler);
  } else {
    return (uint64_t)aFrames * mOut.Rate() / mIn.Rate() + 1;
  }
}

size_t
AudioConverter::FramesOutToSamples(size_t aFrames) const
{
  return aFrames * mOut.Channels();
}

size_t
AudioConverter::SamplesInToFrames(size_t aSamples) const
{
  return aSamples / mIn.Channels();
}

size_t
AudioConverter::FramesOutToBytes(size_t aFrames) const
{
  return FramesOutToSamples(aFrames) * AudioConfig::SampleSize(mOut.Format());
}
} // namespace mozilla
