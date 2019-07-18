/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MediaSpan_h)
#  define MediaSpan_h

#  include "MediaData.h"
#  include "mozilla/RefPtr.h"
#  include "mozilla/Span.h"

namespace mozilla {

// A MediaSpan wraps a MediaByteBuffer and exposes a slice/span, or subregion,
// of the buffer. This allows you to slice off a logical subregion without
// needing to reallocate a new buffer to hold it. You can also append to a
// MediaSpan without affecting other MediaSpans referencing the same buffer
// (the MediaSpan receiving the append will allocate a new buffer to store its
// result if necessary, to ensure other MediaSpans referencing its original
// buffer are unaffected). Note there are no protections here that something
// other than MediaSpans doesn't modify the underlying MediaByteBuffer while
// a MediaSpan is alive.
class MediaSpan {
 public:
  ~MediaSpan() = default;

  explicit MediaSpan(const MediaSpan& aOther) = default;

  MediaSpan(MediaSpan&& aOther) = default;

  explicit MediaSpan(const RefPtr<MediaByteBuffer>& aBuffer)
      : mBuffer(aBuffer), mStart(0), mLength(aBuffer ? aBuffer->Length() : 0) {
    MOZ_DIAGNOSTIC_ASSERT(mBuffer);
  }

  explicit MediaSpan(MediaByteBuffer* aBuffer)
      : mBuffer(aBuffer), mStart(0), mLength(aBuffer ? aBuffer->Length() : 0) {
    MOZ_DIAGNOSTIC_ASSERT(mBuffer);
  }

  MediaSpan& operator=(const MediaSpan& aOther) = default;

  static MediaSpan WithCopyOf(const RefPtr<MediaByteBuffer>& aBuffer) {
    RefPtr<MediaByteBuffer> buffer = new MediaByteBuffer(aBuffer->Length());
    buffer->AppendElements(*aBuffer);
    return MediaSpan(buffer);
  }

  bool IsEmpty() const { return Length() == 0; }

  // Note: It's unsafe to store the pointer returned by this function, as an
  // append operation could cause the wrapped MediaByteBuffer to be
  // reallocated, invalidating pointers previously returned by this function.
  const uint8_t* Elements() const {
    MOZ_DIAGNOSTIC_ASSERT(mStart < mBuffer->Length());
    return mBuffer->Elements() + mStart;
  }

  size_t Length() const { return mLength; }

  uint8_t operator[](size_t aIndex) const {
    MOZ_DIAGNOSTIC_ASSERT(aIndex < Length());
    return (*mBuffer)[mStart + aIndex];
  }

  bool Append(const MediaSpan& aBuffer) { return Append(aBuffer.Buffer()); }

  bool Append(MediaByteBuffer* aBuffer) {
    if (!aBuffer) {
      return true;
    }
    if (mStart + mLength < mBuffer->Length()) {
      // This MediaSpan finishes before the end of its buffer. The buffer
      // could be shared with another MediaSpan. So we can't just append to
      // the underlying buffer without risking damaging other MediaSpans' data.
      // So we must reallocate a new buffer, copy our old data into it, and
      // append the new data into it.
      RefPtr<MediaByteBuffer> buffer =
          new MediaByteBuffer(mLength + aBuffer->Length());
      if (!buffer->AppendElements(Elements(), Length(), fallible) ||
          !buffer->AppendElements(*aBuffer, fallible)) {
        return false;
      }
      mBuffer = buffer;
      mLength += aBuffer->Length();
      return true;
    }
    if (!mBuffer->AppendElements(*aBuffer, fallible)) {
      return false;
    }
    mLength += aBuffer->Length();
    return true;
  }

  // Returns a new MediaSpan, spanning from the start of this span,
  // up until aEnd.
  MediaSpan To(size_t aEnd) const {
    MOZ_DIAGNOSTIC_ASSERT(aEnd <= Length());
    return MediaSpan(mBuffer, mStart, aEnd);
  }

  // Returns a new MediaSpan, spanning from aStart bytes offset from
  // the start of this span, until the end of this span.
  MediaSpan From(size_t aStart) const {
    MOZ_DIAGNOSTIC_ASSERT(aStart <= Length());
    return MediaSpan(mBuffer, mStart + aStart, Length() - aStart);
  }

  void RemoveFront(size_t aNumBytes) {
    MOZ_DIAGNOSTIC_ASSERT(aNumBytes <= Length());
    mStart += aNumBytes;
    mLength -= aNumBytes;
  }

  MediaByteBuffer* Buffer() const { return mBuffer; }

 private:
  MediaSpan(MediaByteBuffer* aBuffer, size_t aStart, size_t aLength)
      : mBuffer(aBuffer), mStart(aStart), mLength(aLength) {
    MOZ_DIAGNOSTIC_ASSERT(mStart + mLength <= mBuffer->Length());
  }

  RefPtr<MediaByteBuffer> mBuffer;
  size_t mStart = 0;
  size_t mLength = 0;
};

}  // namespace mozilla

#endif  // MediaSpan_h
