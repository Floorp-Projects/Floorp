/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIO_RING_BUFFER_H_
#define MOZILLA_AUDIO_RING_BUFFER_H_

#include "AudioSampleFormat.h"
#include "mozilla/Span.h"

#include <functional>

namespace mozilla {

/**
 * AudioRingBuffer works with audio sample format float or short. The
 * implementation wrap around the RingBuffer thus it is not thread-safe. Reads
 * and writes must happen in the same thread which may be different than the
 * construction thread. The memory is pre-allocated in the constructor, but may
 * also be re-allocated on the fly should a larger length be needed. The sample
 * format has to be specified in order to be used.
 */
class AudioRingBuffer final {
 public:
  explicit AudioRingBuffer(uint32_t aSizeInBytes);
  ~AudioRingBuffer();

  /**
   * Set the sample format to either short or float. The sample format must be
   * set before the using any other method.
   */
  void SetSampleFormat(AudioSampleFormat aFormat);

  /**
   * Write `aBuffer.Length()` number of samples when the format is float.
   */
  uint32_t Write(const Span<const float>& aBuffer);

  /**
   * Write `aBuffer.Length()` number of samples when the format is short.
   */
  uint32_t Write(const Span<const int16_t>& aBuffer);

  /**
   * Write `aSamples` number of samples from `aBuffer`. Note the `aBuffer` does
   * not change.
   */
  uint32_t Write(const AudioRingBuffer& aBuffer, uint32_t aSamples);

  /**
   * Write `aSamples` number of zeros before the beginning of the existing data.
   */
  uint32_t PrependSilence(uint32_t aSamples);

  /**
   * Write `aSamples` number of zeros.
   */
  uint32_t WriteSilence(uint32_t aSamples);

  /**
   * Read `aBuffer.Length()` number of samples when the format is float.
   */
  uint32_t Read(const Span<float>& aBuffer);

  /**
   * Read `aBuffer.Length()` number of samples when the format is short.
   */
  uint32_t Read(const Span<int16_t>& aBuffer);

  /**
   * Read the internal buffer without extra copies when sample format is float.
   * Check also the RingBuffer::ReadNoCopy() for more details.
   */
  uint32_t ReadNoCopy(
      std::function<uint32_t(const Span<const float>&)>&& aCallable);

  /**
   * Read the internal buffer without extra copies when sample format is short.
   * Check also the RingBuffer::ReadNoCopy() for more details.
   */
  uint32_t ReadNoCopy(
      std::function<uint32_t(const Span<const int16_t>&)>&& aCallable);

  /**
   * Remove `aSamples` number of samples.
   */
  uint32_t Discard(uint32_t aSamples);

  /**
   * Remove all available samples.
   */
  uint32_t Clear();

  /**
   * Set the length of the ring buffer in bytes. Must be divisible by the sample
   * size. Will not deallocate memory if the underlying buffer is large enough.
   * Returns false if setting the length requires allocating memory and the
   * allocation fails.
   */
  bool SetLengthBytes(uint32_t aLengthBytes);

  /**
   * Return the number of samples this buffer can hold.
   */
  uint32_t Capacity() const;

  /**
   * Return true if the buffer is full.
   */
  bool IsFull() const;

  /**
   * Return true if the buffer is empty.
   */
  bool IsEmpty() const;

  /**
   * Return the number of samples available for writing.
   */
  uint32_t AvailableWrite() const;

  /**
   * Return the number of samples available for reading.
   */
  uint32_t AvailableRead() const;

 private:
  class AudioRingBufferPrivate;
  UniquePtr<AudioRingBufferPrivate> mPtr;
};

}  // namespace mozilla

#endif  // MOZILLA_AUDIO_RING_BUFFER_H_
