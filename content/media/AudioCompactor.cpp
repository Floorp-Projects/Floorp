/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AudioCompactor.h"
#if defined(MOZ_MEMORY)
# include "mozmemory.h"
#endif

namespace mozilla {

static size_t
MallocGoodSize(size_t aSize)
{
# if defined(MOZ_MEMORY)
  return malloc_good_size(aSize);
# else
  return aSize;
# endif
}

static size_t
TooMuchSlop(size_t aSize, size_t aAllocSize, size_t aMaxSlop)
{
  // If the allocated size is less then our target size, then we
  // are chunking.  This means it will be completely filled with
  // zero slop.
  size_t slop = (aAllocSize > aSize) ? (aAllocSize - aSize) : 0;
  return slop > aMaxSlop;
}

uint32_t
AudioCompactor::GetChunkSamples(uint32_t aFrames, uint32_t aChannels,
                                size_t aMaxSlop)
{
  size_t size = AudioDataSize(aFrames, aChannels);
  size_t chunkSize = MallocGoodSize(size);

  // Reduce the chunk size until we meet our slop goal or the chunk
  // approaches an unreasonably small size.
  while (chunkSize > 64 && TooMuchSlop(size, chunkSize, aMaxSlop)) {
    chunkSize = MallocGoodSize(chunkSize / 2);
  }

  // Calculate the number of samples based on expected malloc size
  // in order to allow as many frames as possible to be packed.
  return chunkSize / sizeof(AudioDataValue);
}

uint32_t
AudioCompactor::NativeCopy::operator()(AudioDataValue *aBuffer,
                                       uint32_t aSamples)
{
  NS_ASSERTION(aBuffer, "cannot copy to null buffer pointer");
  NS_ASSERTION(aSamples, "cannot copy zero values");

  size_t bufferBytes = aSamples * sizeof(AudioDataValue);
  size_t maxBytes = std::min(bufferBytes, mSourceBytes - mNextByte);
  uint32_t frames = maxBytes / BytesPerFrame(mChannels);
  size_t bytes = frames * BytesPerFrame(mChannels);

  NS_ASSERTION((mNextByte + bytes) <= mSourceBytes,
               "tried to copy beyond source buffer");
  NS_ASSERTION(bytes <= bufferBytes, "tried to copy beyond destination buffer");

  memcpy(aBuffer, mSource + mNextByte, bytes);

  mNextByte += bytes;
  return frames;
}

} // namespace mozilla
