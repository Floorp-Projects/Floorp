/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BOX_H_
#define BOX_H_

#include <stdint.h>
#include "nsTArray.h"
#include "MediaResource.h"
#include "mozilla/EndianUtils.h"
#include "AtomType.h"
#include "BufferReader.h"

namespace mozilla {
class ByteStream;

class BumpAllocator {
 public:
  uint8_t* Allocate(size_t aNumBytes);

 private:
  nsTArray<nsTArray<uint8_t>> mBuffers;
};

class BoxContext {
 public:
  BoxContext(ByteStream* aSource, const MediaByteRangeSet& aByteRanges)
      : mSource(aSource), mByteRanges(aByteRanges) {}

  RefPtr<ByteStream> mSource;
  const MediaByteRangeSet& mByteRanges;
  BumpAllocator mAllocator;
};

struct ByteSlice {
  const uint8_t* mBytes;
  size_t mSize;
};

class Box {
 public:
  Box(BoxContext* aContext, uint64_t aOffset, const Box* aParent = nullptr);
  Box();

  bool IsAvailable() const { return !mRange.IsEmpty(); }
  uint64_t Offset() const { return mRange.mStart; }
  uint64_t Length() const { return mRange.mEnd - mRange.mStart; }
  uint64_t NextOffset() const { return mRange.mEnd; }
  const MediaByteRange& Range() const { return mRange; }
  const Box* Parent() const { return mParent; }
  bool IsType(const char* aType) const { return mType == AtomType(aType); }

  Box Next() const;
  Box FirstChild() const;
  // Reads the box contents, excluding the header.
  nsTArray<uint8_t> Read() const;

  // Reads the complete box; its header and body.
  nsTArray<uint8_t> ReadCompleteBox() const;

  // Reads from the content of the box, excluding header.
  bool Read(nsTArray<uint8_t>* aDest, const MediaByteRange& aRange) const;

  static const uint64_t kMAX_BOX_READ;

  // Returns a slice, pointing to the data of this box. The lifetime of
  // the memory this slice points to matches the box's context's lifetime.
  ByteSlice ReadAsSlice();

 private:
  bool Contains(MediaByteRange aRange) const;
  BoxContext* mContext;
  mozilla::MediaByteRange mRange;
  uint64_t mBodyOffset;
  uint64_t mChildOffset;
  AtomType mType;
  const Box* mParent;
};

// BoxReader serves box data through an AutoByteReader. The box data is
// stored either in the box's context's bump allocator, or in the ByteStream
// itself if the ByteStream implements the Access() method.
// NOTE: The data the BoxReader reads may be stored in the Box's BoxContext.
// Ensure that the BoxReader doesn't outlive the BoxContext!
class MOZ_RAII BoxReader {
 public:
  explicit BoxReader(Box& aBox)
      : mData(aBox.ReadAsSlice()), mReader(mData.mBytes, mData.mSize) {}
  BufferReader* operator->() { return &mReader; }

 private:
  ByteSlice mData;
  BufferReader mReader;
};
}  // namespace mozilla

#endif
