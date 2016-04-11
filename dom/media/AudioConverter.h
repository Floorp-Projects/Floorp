/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(AudioConverter_h)
#define AudioConverter_h

#include "MediaInfo.h"

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

  // Attempt to convert the AudioDataBuffer in place.
  // Will return 0 if the conversion wasn't possible.
  // Process may allocate memory internally should intermediary steps be
  // required.
  template <AudioConfig::SampleFormat Type, typename Value>
  size_t Process(AudioDataBuffer<Type, Value>& aBuffer)
  {
    MOZ_DIAGNOSTIC_ASSERT(mIn.Format() == mOut.Format() && mIn.Format() == Type);
    return Process(aBuffer.Data(), aBuffer.Data(), aBuffer.Size());
  }
  template <typename Value>
  size_t Process(Value* aBuffer, size_t aSamples)
  {
    MOZ_DIAGNOSTIC_ASSERT(mIn.Format() == mOut.Format());
    return Process(aBuffer, aBuffer, aSamples * AudioConfig::SampleSize(mIn.Format()));
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
   * Process
   * Parameters:
   * aOut  : destination buffer where converted samples will be copied
   * aIn   : source buffer
   * aBytes: size in bytes of source buffer
   *
   * Return Value: size in bytes of samples converted or 0 if error
   */
  size_t Process(void* aOut, const void* aIn, size_t aBytes);
  void ReOrderInterleavedChannels(void* aOut, const void* aIn, size_t aDataSize) const;
  size_t DownmixAudio(void* aOut, const void* aIn, size_t aDataSize) const;
};

} // namespace mozilla

#endif /* AudioConverter_h */
