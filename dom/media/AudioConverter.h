/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(AudioConverter_h)
#define AudioConverter_h

#include "MediaInfo.h"

// Forward declaration
typedef struct SpeexResamplerState_ SpeexResamplerState;

namespace mozilla {

template <AudioConfig::SampleFormat T> struct AudioDataBufferTypeChooser;
template <> struct AudioDataBufferTypeChooser<AudioConfig::FORMAT_U8>
{ typedef uint8_t Type; };
template <> struct AudioDataBufferTypeChooser<AudioConfig::FORMAT_S16>
{ typedef int16_t Type; };
template <> struct AudioDataBufferTypeChooser<AudioConfig::FORMAT_S24LSB>
{ typedef int32_t Type; };
template <> struct AudioDataBufferTypeChooser<AudioConfig::FORMAT_S24>
{ typedef int32_t Type; };
template <> struct AudioDataBufferTypeChooser<AudioConfig::FORMAT_S32>
{ typedef int32_t Type; };
template <> struct AudioDataBufferTypeChooser<AudioConfig::FORMAT_FLT>
{ typedef float Type; };

// 'Value' is the type used externally to deal with stored value.
// AudioDataBuffer can perform conversion between different SampleFormat content.
template <AudioConfig::SampleFormat Format, typename Value = typename AudioDataBufferTypeChooser<Format>::Type>
class AudioDataBuffer
{
public:
  AudioDataBuffer() {}
  AudioDataBuffer(Value* aBuffer, size_t aLength)
    : mBuffer(aBuffer, aLength)
  {}
  explicit AudioDataBuffer(const AudioDataBuffer& aOther)
    : mBuffer(aOther.mBuffer)
  {}
  AudioDataBuffer(AudioDataBuffer&& aOther)
    : mBuffer(Move(aOther.mBuffer))
  {}
  template <AudioConfig::SampleFormat OtherFormat, typename OtherValue>
  explicit AudioDataBuffer(const AudioDataBuffer<OtherFormat, OtherValue>& other)
  {
    // TODO: Convert from different type, may use asm routines.
    MOZ_CRASH("Conversion not implemented yet");
  }

  // A u8, s16 and float aligned buffer can only be treated as
  // FORMAT_U8, FORMAT_S16 and FORMAT_FLT respectively.
  // So allow them as copy and move constructors.
  explicit AudioDataBuffer(const AlignedByteBuffer& aBuffer)
    : mBuffer(aBuffer)
  {
    static_assert(Format == AudioConfig::FORMAT_U8,
                  "Conversion not implemented yet");
  }
  explicit AudioDataBuffer(const AlignedShortBuffer& aBuffer)
    : mBuffer(aBuffer)
  {
    static_assert(Format == AudioConfig::FORMAT_S16,
                  "Conversion not implemented yet");
  }
  explicit AudioDataBuffer(const AlignedFloatBuffer& aBuffer)
    : mBuffer(aBuffer)
  {
    static_assert(Format == AudioConfig::FORMAT_FLT,
                  "Conversion not implemented yet");
  }
  explicit AudioDataBuffer(AlignedByteBuffer&& aBuffer)
    : mBuffer(Move(aBuffer))
  {
    static_assert(Format == AudioConfig::FORMAT_U8,
                  "Conversion not implemented yet");
  }
  explicit AudioDataBuffer(AlignedShortBuffer&& aBuffer)
    : mBuffer(Move(aBuffer))
  {
    static_assert(Format == AudioConfig::FORMAT_S16,
                  "Conversion not implemented yet");
  }
  explicit AudioDataBuffer(const AlignedFloatBuffer&& aBuffer)
    : mBuffer(Move(aBuffer))
  {
    static_assert(Format == AudioConfig::FORMAT_FLT,
                  "Conversion not implemented yet");
  }
  AudioDataBuffer& operator=(AudioDataBuffer&& aOther)
  {
    mBuffer = Move(aOther.mBuffer);
    return *this;
  }
  AudioDataBuffer& operator=(const AudioDataBuffer& aOther)
  {
    mBuffer = aOther.mBuffer;
    return *this;
  }

  Value* Data() const { return mBuffer.Data(); }
  size_t Length() const { return mBuffer.Length(); }
  size_t Size() const { return mBuffer.Size(); }
  AlignedBuffer<Value> Forget()
  {
    // Correct type -> Just give values as-is.
    return Move(mBuffer);
  }
private:
  AlignedBuffer<Value> mBuffer;
};

typedef AudioDataBuffer<AudioConfig::FORMAT_DEFAULT> AudioSampleBuffer;

class AudioConverter {
public:
  AudioConverter(const AudioConfig& aIn, const AudioConfig& aOut);
  ~AudioConverter();

  // Convert the AudioDataBuffer.
  // Conversion will be done in place if possible. Otherwise a new buffer will
  // be returned.
  // Providing an empty buffer and resampling is expected, the resampler
  // will be drained.
  template <AudioConfig::SampleFormat Format, typename Value>
  AudioDataBuffer<Format, Value> Process(AudioDataBuffer<Format, Value>&& aBuffer)
  {
    MOZ_DIAGNOSTIC_ASSERT(mIn.Format() == mOut.Format() && mIn.Format() == Format);
    AudioDataBuffer<Format, Value> buffer = Move(aBuffer);
    if (CanWorkInPlace()) {
      size_t frames = SamplesInToFrames(buffer.Length());
      frames = ProcessInternal(buffer.Data(), buffer.Data(), frames);
      if (frames && mIn.Rate() != mOut.Rate()) {
        frames = ResampleAudio(buffer.Data(), buffer.Data(), frames);
      }
      AlignedBuffer<Value> temp = buffer.Forget();
      temp.SetLength(FramesOutToSamples(frames));
      return AudioDataBuffer<Format, Value>(Move(temp));;
    }
    return Process(buffer);
  }

  template <AudioConfig::SampleFormat Format, typename Value>
  AudioDataBuffer<Format, Value> Process(const AudioDataBuffer<Format, Value>& aBuffer)
  {
    MOZ_DIAGNOSTIC_ASSERT(mIn.Format() == mOut.Format() && mIn.Format() == Format);
    // Perform the downmixing / reordering in temporary buffer.
    size_t frames = SamplesInToFrames(aBuffer.Length());
    AlignedBuffer<Value> temp1;
    if (!temp1.SetLength(FramesOutToSamples(frames))) {
      return AudioDataBuffer<Format, Value>(Move(temp1));
    }
    frames = ProcessInternal(temp1.Data(), aBuffer.Data(), frames);
    if (mIn.Rate() == mOut.Rate()) {
      MOZ_ALWAYS_TRUE(temp1.SetLength(FramesOutToSamples(frames)));
      return AudioDataBuffer<Format, Value>(Move(temp1));
    }

    // At this point, temp1 contains the buffer reordered and downmixed.
    // If we are downsampling we can re-use it.
    AlignedBuffer<Value>* outputBuffer = &temp1;
    AlignedBuffer<Value> temp2;
    if (!frames || mOut.Rate() > mIn.Rate()) {
      // We are upsampling or about to drain, we can't work in place.
      // Allocate another temporary buffer where the upsampling will occur.
      if (!temp2.SetLength(FramesOutToSamples(ResampleRecipientFrames(frames)))) {
        return AudioDataBuffer<Format, Value>(Move(temp2));
      }
      outputBuffer = &temp2;
    }
    if (!frames) {
      frames = DrainResampler(outputBuffer->Data());
    } else {
      frames = ResampleAudio(outputBuffer->Data(), temp1.Data(), frames);
    }
    MOZ_ALWAYS_TRUE(outputBuffer->SetLength(FramesOutToSamples(frames)));
    return AudioDataBuffer<Format, Value>(Move(*outputBuffer));
  }

  // Attempt to convert the AudioDataBuffer in place.
  // Will return 0 if the conversion wasn't possible.
  template <typename Value>
  size_t Process(Value* aBuffer, size_t aFrames)
  {
    MOZ_DIAGNOSTIC_ASSERT(mIn.Format() == mOut.Format());
    if (!CanWorkInPlace()) {
      return 0;
    }
    size_t frames = ProcessInternal(aBuffer, aBuffer, aFrames);
    if (frames && mIn.Rate() != mOut.Rate()) {
      frames = ResampleAudio(aBuffer, aBuffer, aFrames);
    }
    return frames;
  }

  bool CanWorkInPlace() const;
  bool CanReorderAudio() const
  {
    return mIn.Layout().MappingTable(mOut.Layout());
  }

  const AudioConfig& InputConfig() const { return mIn; }
  const AudioConfig& OutputConfig() const { return mOut; }

private:
  const AudioConfig mIn;
  const AudioConfig mOut;
  uint8_t mChannelOrderMap[MAX_AUDIO_CHANNELS];
  /**
   * ProcessInternal
   * Parameters:
   * aOut  : destination buffer where converted samples will be copied
   * aIn   : source buffer
   * aSamples: number of frames in source buffer
   *
   * Return Value: number of frames converted or 0 if error
   */
  size_t ProcessInternal(void* aOut, const void* aIn, size_t aFrames);
  void ReOrderInterleavedChannels(void* aOut, const void* aIn, size_t aFrames) const;
  size_t DownmixAudio(void* aOut, const void* aIn, size_t aFrames) const;
  size_t UpmixAudio(void* aOut, const void* aIn, size_t aFrames) const;

  size_t FramesOutToSamples(size_t aFrames) const;
  size_t SamplesInToFrames(size_t aSamples) const;
  size_t FramesOutToBytes(size_t aFrames) const;

  // Resampler context.
  SpeexResamplerState* mResampler;
  size_t ResampleAudio(void* aOut, const void* aIn, size_t aFrames);
  size_t ResampleRecipientFrames(size_t aFrames) const;
  void RecreateResampler();
  size_t DrainResampler(void* aOut);
};

} // namespace mozilla

#endif /* AudioConverter_h */
