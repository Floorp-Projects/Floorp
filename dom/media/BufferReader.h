/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BUFFER_READER_H_
#define BUFFER_READER_H_

#include "mozilla/EndianUtils.h"
#include "nscore.h"
#include "nsTArray.h"
#include "MediaData.h"
#include "MediaSpan.h"
#include "mozilla/Logging.h"
#include "mozilla/Result.h"

namespace mozilla {

extern mozilla::LazyLogModule gMP4MetadataLog;

class MOZ_RAII BufferReader {
 public:
  BufferReader() : mPtr(nullptr), mRemaining(0), mLength(0) {}
  BufferReader(const uint8_t* aData, size_t aSize)
      : mPtr(aData), mRemaining(aSize), mLength(aSize) {}
  template <size_t S>
  explicit BufferReader(const AutoTArray<uint8_t, S>& aData)
      : mPtr(aData.Elements()),
        mRemaining(aData.Length()),
        mLength(aData.Length()) {}
  explicit BufferReader(const nsTArray<uint8_t>& aData)
      : mPtr(aData.Elements()),
        mRemaining(aData.Length()),
        mLength(aData.Length()) {}
  explicit BufferReader(const mozilla::MediaByteBuffer* aData)
      : mPtr(aData->Elements()),
        mRemaining(aData->Length()),
        mLength(aData->Length()) {}
  explicit BufferReader(const mozilla::MediaSpan& aData)
      : mPtr(aData.Elements()),
        mRemaining(aData.Length()),
        mLength(aData.Length()) {}

  void SetData(const nsTArray<uint8_t>& aData) {
    MOZ_ASSERT(!mPtr && !mRemaining);
    mPtr = aData.Elements();
    mRemaining = aData.Length();
    mLength = mRemaining;
  }

  ~BufferReader() = default;

  size_t Offset() const { return mLength - mRemaining; }

  size_t Remaining() const { return mRemaining; }

  mozilla::Result<uint8_t, nsresult> ReadU8() {
    auto ptr = Read(1);
    if (!ptr) {
      MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Error,
              ("%s: failure", __func__));
      return mozilla::Err(NS_ERROR_FAILURE);
    }
    return *ptr;
  }

  mozilla::Result<uint16_t, nsresult> ReadU16() {
    auto ptr = Read(2);
    if (!ptr) {
      MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Error,
              ("%s: failure", __func__));
      return mozilla::Err(NS_ERROR_FAILURE);
    }
    return mozilla::BigEndian::readUint16(ptr);
  }

  mozilla::Result<int16_t, nsresult> ReadLE16() {
    auto ptr = Read(2);
    if (!ptr) {
      MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Error,
              ("%s: failure", __func__));
      return mozilla::Err(NS_ERROR_FAILURE);
    }
    return mozilla::LittleEndian::readInt16(ptr);
  }

  mozilla::Result<uint32_t, nsresult> ReadU24() {
    auto ptr = Read(3);
    if (!ptr) {
      MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Error,
              ("%s: failure", __func__));
      return mozilla::Err(NS_ERROR_FAILURE);
    }
    return ptr[0] << 16 | ptr[1] << 8 | ptr[2];
  }

  mozilla::Result<int32_t, nsresult> Read24() {
    return ReadU24().map([](uint32_t x) { return (int32_t)x; });
  }

  mozilla::Result<int32_t, nsresult> ReadLE24() {
    auto ptr = Read(3);
    if (!ptr) {
      MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Error,
              ("%s: failure", __func__));
      return mozilla::Err(NS_ERROR_FAILURE);
    }
    int32_t result = int32_t(ptr[2] << 16 | ptr[1] << 8 | ptr[0]);
    if (result & 0x00800000u) {
      result -= 0x1000000;
    }
    return result;
  }

  mozilla::Result<uint32_t, nsresult> ReadU32() {
    auto ptr = Read(4);
    if (!ptr) {
      MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Error,
              ("%s: failure", __func__));
      return mozilla::Err(NS_ERROR_FAILURE);
    }
    return mozilla::BigEndian::readUint32(ptr);
  }

  mozilla::Result<int32_t, nsresult> Read32() {
    auto ptr = Read(4);
    if (!ptr) {
      MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Error,
              ("%s: failure", __func__));
      return mozilla::Err(NS_ERROR_FAILURE);
    }
    return mozilla::BigEndian::readInt32(ptr);
  }

  mozilla::Result<uint32_t, nsresult> ReadLEU32() {
    auto ptr = Read(4);
    if (!ptr) {
      MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Error,
              ("%s: failure", __func__));
      return mozilla::Err(NS_ERROR_FAILURE);
    }
    return mozilla::LittleEndian::readUint32(ptr);
  }

  mozilla::Result<uint64_t, nsresult> ReadU64() {
    auto ptr = Read(8);
    if (!ptr) {
      MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Error,
              ("%s: failure", __func__));
      return mozilla::Err(NS_ERROR_FAILURE);
    }
    return mozilla::BigEndian::readUint64(ptr);
  }

  mozilla::Result<int64_t, nsresult> Read64() {
    auto ptr = Read(8);
    if (!ptr) {
      MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Error,
              ("%s: failure", __func__));
      return mozilla::Err(NS_ERROR_FAILURE);
    }
    return mozilla::BigEndian::readInt64(ptr);
  }

  const uint8_t* Read(size_t aCount) {
    if (aCount > mRemaining) {
      mPtr += mRemaining;
      mRemaining = 0;
      return nullptr;
    }
    mRemaining -= aCount;

    const uint8_t* result = mPtr;
    mPtr += aCount;

    return result;
  }

  const uint8_t* Rewind(size_t aCount) {
    MOZ_ASSERT(aCount <= Offset());
    size_t rewind = Offset();
    if (aCount < rewind) {
      rewind = aCount;
    }
    mRemaining += rewind;
    mPtr -= rewind;
    return mPtr;
  }

  mozilla::Result<uint8_t, nsresult> PeekU8() const {
    auto ptr = Peek(1);
    if (!ptr) {
      MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Error,
              ("%s: failure", __func__));
      return mozilla::Err(NS_ERROR_FAILURE);
    }
    return *ptr;
  }

  mozilla::Result<uint16_t, nsresult> PeekU16() const {
    auto ptr = Peek(2);
    if (!ptr) {
      MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Error,
              ("%s: failure", __func__));
      return mozilla::Err(NS_ERROR_FAILURE);
    }
    return mozilla::BigEndian::readUint16(ptr);
  }

  mozilla::Result<uint32_t, nsresult> PeekU24() const {
    auto ptr = Peek(3);
    if (!ptr) {
      MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Error,
              ("%s: failure", __func__));
      return mozilla::Err(NS_ERROR_FAILURE);
    }
    return ptr[0] << 16 | ptr[1] << 8 | ptr[2];
  }

  mozilla::Result<int32_t, nsresult> Peek24() const {
    return PeekU24().map([](uint32_t x) { return (int32_t)x; });
  }

  mozilla::Result<uint32_t, nsresult> PeekU32() {
    auto ptr = Peek(4);
    if (!ptr) {
      MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Error,
              ("%s: failure", __func__));
      return mozilla::Err(NS_ERROR_FAILURE);
    }
    return mozilla::BigEndian::readUint32(ptr);
  }

  const uint8_t* Peek(size_t aCount) const {
    if (aCount > mRemaining) {
      return nullptr;
    }
    return mPtr;
  }

  const uint8_t* Seek(size_t aOffset) {
    if (aOffset >= mLength) {
      MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Error,
              ("%s: failure, offset: %zu", __func__, aOffset));
      return nullptr;
    }

    mPtr = mPtr - Offset() + aOffset;
    mRemaining = mLength - aOffset;
    return mPtr;
  }

  const uint8_t* Reset() {
    mPtr -= Offset();
    mRemaining = mLength;
    return mPtr;
  }

  uint32_t Align() const { return 4 - ((intptr_t)mPtr & 3); }

  template <typename T>
  bool CanReadType() const {
    return mRemaining >= sizeof(T);
  }

  template <typename T>
  T ReadType() {
    auto ptr = Read(sizeof(T));
    if (!ptr) {
      MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Error,
              ("%s: failure", __func__));
      return 0;
    }
    return *reinterpret_cast<const T*>(ptr);
  }

  template <typename T>
  [[nodiscard]] bool ReadArray(nsTArray<T>& aDest, size_t aLength) {
    auto ptr = Read(aLength * sizeof(T));
    if (!ptr) {
      MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Error,
              ("%s: failure", __func__));
      return false;
    }

    aDest.Clear();
    aDest.AppendElements(reinterpret_cast<const T*>(ptr), aLength);
    return true;
  }

  template <typename T>
  [[nodiscard]] bool ReadArray(FallibleTArray<T>& aDest, size_t aLength) {
    auto ptr = Read(aLength * sizeof(T));
    if (!ptr) {
      MOZ_LOG(gMP4MetadataLog, mozilla::LogLevel::Error,
              ("%s: failure", __func__));
      return false;
    }

    aDest.Clear();
    if (!aDest.SetCapacity(aLength, mozilla::fallible)) {
      return false;
    }
    MOZ_ALWAYS_TRUE(aDest.AppendElements(reinterpret_cast<const T*>(ptr),
                                         aLength, mozilla::fallible));
    return true;
  }

 private:
  const uint8_t* mPtr;
  size_t mRemaining;
  size_t mLength;
};

}  // namespace mozilla

#endif
