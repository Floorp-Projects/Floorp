/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pickle.h"

#include "mozilla/Alignment.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/Telemetry.h"
#include "mozilla/ipc/ProtocolUtils.h"

#include <stdlib.h>

#include <limits>
#include <string>
#include <algorithm>

#include "nsDebug.h"

//------------------------------------------------------------------------------

static_assert(MOZ_ALIGNOF(Pickle::memberAlignmentType) >= MOZ_ALIGNOF(uint32_t),
              "Insufficient alignment");

#ifndef MOZ_TASK_TRACER
static const uint32_t kHeaderSegmentCapacity = 64;
#else
// TaskTracer would add extra fields to the header to carry task ID and
// other information.
// \see class Message::HeaderTaskTracer
static const uint32_t kHeaderSegmentCapacity = 128;
#endif

static const uint32_t kDefaultSegmentCapacity = 4096;

static const char kBytePaddingMarker = char(0xbf);

// Note: we round the time to the nearest millisecond. So a min value of 1 ms
// actually captures from 500us and above.
static const uint32_t kMinTelemetryIPCReadLatencyMs = 1;

namespace {

// We want to copy data to our payload as efficiently as possible.
// memcpy fits the bill for copying, but not all compilers or
// architectures support inlining memcpy from void*, which has unknown
// static alignment.  However, we know that all the members of our
// payload will be aligned on memberAlignmentType boundaries.  We
// therefore use that knowledge to construct a copier that will copy
// efficiently (via standard C++ assignment mechanisms) if the datatype
// needs that alignment or less, and memcpy otherwise.  (The compiler
// may still inline memcpy, of course.)

template<typename T, size_t size, bool hasSufficientAlignment>
struct Copier
{
  static void Copy(T* dest, const char* iter) {
    memcpy(dest, iter, sizeof(T));
  }
};

// Copying 64-bit quantities happens often enough and can easily be made
// worthwhile on 32-bit platforms, so handle it specially.  Only do it
// if 64-bit types aren't sufficiently aligned; the alignment
// requirements for them vary between 32-bit platforms.
#ifndef HAVE_64BIT_BUILD
template<typename T>
struct Copier<T, sizeof(uint64_t), false>
{
  static void Copy(T* dest, const char* iter) {
#if MOZ_LITTLE_ENDIAN
    static const int loIndex = 0, hiIndex = 1;
#else
    static const int loIndex = 1, hiIndex = 0;
#endif
    static_assert(MOZ_ALIGNOF(uint32_t*) == MOZ_ALIGNOF(void*),
                  "Pointers have different alignments");
    const uint32_t* src = reinterpret_cast<const uint32_t*>(iter);
    uint32_t* uint32dest = reinterpret_cast<uint32_t*>(dest);
    uint32dest[loIndex] = src[loIndex];
    uint32dest[hiIndex] = src[hiIndex];
  }
};
#endif

template<typename T, size_t size>
struct Copier<T, size, true>
{
  static void Copy(T* dest, const char* iter) {
    *dest = *reinterpret_cast<const T*>(iter);
  }
};

} // anonymous namespace

PickleIterator::PickleIterator(const Pickle& pickle)
   : iter_(pickle.buffers_.Iter())
   , start_(mozilla::TimeStamp::Now()) {
  iter_.Advance(pickle.buffers_, pickle.header_size_);
}

template<typename T>
void
PickleIterator::CopyInto(T* dest) {
  static_assert(mozilla::IsPod<T>::value, "Copied type must be a POD type");
  Copier<T, sizeof(T), (MOZ_ALIGNOF(T) <= sizeof(Pickle::memberAlignmentType))>::Copy(dest, iter_.Data());
}

bool Pickle::IteratorHasRoomFor(const PickleIterator& iter, uint32_t len) const {
  // Make sure we don't get into trouble where AlignInt(len) == 0.
  MOZ_RELEASE_ASSERT(len < 64);

  return iter.iter_.HasRoomFor(AlignInt(len));
}

bool Pickle::HasBytesAvailable(const PickleIterator* iter, uint32_t len) const {
  return iter->iter_.HasBytesAvailable(buffers_, len);
}

void Pickle::UpdateIter(PickleIterator* iter, uint32_t bytes) const {
  // Make sure we don't get into trouble where AlignInt(bytes) == 0.
  MOZ_RELEASE_ASSERT(bytes < 64);

  iter->iter_.Advance(buffers_, AlignInt(bytes));
}

// Payload is sizeof(Pickle::memberAlignmentType) aligned.

Pickle::Pickle(uint32_t header_size, size_t segment_capacity)
    : buffers_(AlignInt(header_size),
               segment_capacity ? segment_capacity : kHeaderSegmentCapacity,
               segment_capacity ? segment_capacity : kDefaultSegmentCapacity),
      header_(nullptr),
      header_size_(AlignInt(header_size)) {
  DCHECK(static_cast<memberAlignmentType>(header_size) >= sizeof(Header));
  DCHECK(header_size_ <= kHeaderSegmentCapacity);
  header_ = reinterpret_cast<Header*>(buffers_.Start());
  header_->payload_size = 0;
}

Pickle::Pickle(uint32_t header_size, const char* data, uint32_t length)
    : buffers_(length, AlignCapacity(length), kDefaultSegmentCapacity),
      header_(nullptr),
      header_size_(AlignInt(header_size)) {
  DCHECK(static_cast<memberAlignmentType>(header_size) >= sizeof(Header));
  DCHECK(header_size <= kHeaderSegmentCapacity);
  MOZ_RELEASE_ASSERT(header_size <= length);

  header_ = reinterpret_cast<Header*>(buffers_.Start());
  memcpy(header_, data, length);
}

Pickle::Pickle(Pickle&& other)
   : buffers_(mozilla::Move(other.buffers_)),
     header_(other.header_),
     header_size_(other.header_size_) {
  other.header_ = nullptr;
}

Pickle::~Pickle() {
}

Pickle& Pickle::operator=(Pickle&& other) {
  BufferList tmp = mozilla::Move(other.buffers_);
  other.buffers_ = mozilla::Move(buffers_);
  buffers_ = mozilla::Move(tmp);

  //std::swap(buffers_, other.buffers_);
  std::swap(header_, other.header_);
  std::swap(header_size_, other.header_size_);
  return *this;
}

bool Pickle::ReadBool(PickleIterator* iter, bool* result) const {
  DCHECK(iter);

  int tmp;
  if (!ReadInt(iter, &tmp))
    return false;
  DCHECK(0 == tmp || 1 == tmp);
  *result = tmp ? true : false;
  return true;
}

bool Pickle::ReadInt16(PickleIterator* iter, int16_t* result) const {
  DCHECK(iter);

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return ReadBytesInto(iter, result, sizeof(*result));

  iter->CopyInto(result);

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadUInt16(PickleIterator* iter, uint16_t* result) const {
  DCHECK(iter);

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return ReadBytesInto(iter, result, sizeof(*result));

  iter->CopyInto(result);

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadInt(PickleIterator* iter, int* result) const {
  DCHECK(iter);

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return ReadBytesInto(iter, result, sizeof(*result));

  iter->CopyInto(result);

  UpdateIter(iter, sizeof(*result));
  return true;
}

// Always written as a 64-bit value since the size for this type can
// differ between architectures.
bool Pickle::ReadLong(PickleIterator* iter, long* result) const {
  DCHECK(iter);

  int64_t big_result = 0;
  if (IteratorHasRoomFor(*iter, sizeof(big_result))) {
    iter->CopyInto(&big_result);
    UpdateIter(iter, sizeof(big_result));
  } else {
    if (!ReadBytesInto(iter, &big_result, sizeof(big_result))) {
      return false;
    }
  }
  DCHECK(big_result <= LONG_MAX && big_result >= LONG_MIN);
  *result = static_cast<long>(big_result);

  return true;
}

// Always written as a 64-bit value since the size for this type can
// differ between architectures.
bool Pickle::ReadULong(PickleIterator* iter, unsigned long* result) const {
  DCHECK(iter);

  uint64_t big_result = 0;
  if (IteratorHasRoomFor(*iter, sizeof(big_result))) {
    iter->CopyInto(&big_result);
    UpdateIter(iter, sizeof(big_result));
  } else {
    if (!ReadBytesInto(iter, &big_result, sizeof(big_result))) {
      return false;
    }
  }
  DCHECK(big_result <= ULONG_MAX);
  *result = static_cast<unsigned long>(big_result);

  return true;
}

bool Pickle::ReadLength(PickleIterator* iter, int* result) const {
  if (!ReadInt(iter, result))
    return false;
  return ((*result) >= 0);
}

// Always written as a 64-bit value since the size for this type can
// differ between architectures.
bool Pickle::ReadSize(PickleIterator* iter, size_t* result) const {
  DCHECK(iter);

  uint64_t big_result = 0;
  if (IteratorHasRoomFor(*iter, sizeof(big_result))) {
    iter->CopyInto(&big_result);
    UpdateIter(iter, sizeof(big_result));
  } else {
    if (!ReadBytesInto(iter, &big_result, sizeof(big_result))) {
      return false;
    }
  }
  DCHECK(big_result <= std::numeric_limits<size_t>::max());
  *result = static_cast<size_t>(big_result);

  return true;
}

bool Pickle::ReadInt32(PickleIterator* iter, int32_t* result) const {
  DCHECK(iter);

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return ReadBytesInto(iter, result, sizeof(*result));

  iter->CopyInto(result);

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadUInt32(PickleIterator* iter, uint32_t* result) const {
  DCHECK(iter);

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return ReadBytesInto(iter, result, sizeof(*result));

  iter->CopyInto(result);

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadInt64(PickleIterator* iter, int64_t* result) const {
  DCHECK(iter);

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return ReadBytesInto(iter, result, sizeof(*result));

  iter->CopyInto(result);

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadUInt64(PickleIterator* iter, uint64_t* result) const {
  DCHECK(iter);

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return ReadBytesInto(iter, result, sizeof(*result));

  iter->CopyInto(result);

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadDouble(PickleIterator* iter, double* result) const {
  DCHECK(iter);

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return ReadBytesInto(iter, result, sizeof(*result));

  iter->CopyInto(result);

  UpdateIter(iter, sizeof(*result));
  return true;
}

// Always written as a 64-bit value since the size for this type can
// differ between architectures.
bool Pickle::ReadIntPtr(PickleIterator* iter, intptr_t* result) const {
  DCHECK(iter);

  int64_t big_result = 0;
  if (IteratorHasRoomFor(*iter, sizeof(big_result))) {
    iter->CopyInto(&big_result);
    UpdateIter(iter, sizeof(big_result));
  } else {
    if (!ReadBytesInto(iter, &big_result, sizeof(big_result))) {
      return false;
    }
  }

  DCHECK(big_result <= std::numeric_limits<intptr_t>::max() && big_result >= std::numeric_limits<intptr_t>::min());
  *result = static_cast<intptr_t>(big_result);

  return true;
}

bool Pickle::ReadUnsignedChar(PickleIterator* iter, unsigned char* result) const {
  DCHECK(iter);

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return ReadBytesInto(iter, result, sizeof(*result));

  iter->CopyInto(result);

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadString(PickleIterator* iter, std::string* result) const {
  DCHECK(iter);

  int len;
  if (!ReadLength(iter, &len))
    return false;

  auto chars = mozilla::MakeUnique<char[]>(len);
  if (!ReadBytesInto(iter, chars.get(), len)) {
    return false;
  }
  result->assign(chars.get(), len);

  return true;
}

bool Pickle::ReadWString(PickleIterator* iter, std::wstring* result) const {
  DCHECK(iter);

  int len;
  if (!ReadLength(iter, &len))
    return false;
  // Avoid integer multiplication overflow.
  if (len > INT_MAX / static_cast<int>(sizeof(wchar_t)))
    return false;

  auto chars = mozilla::MakeUnique<wchar_t[]>(len);
  if (!ReadBytesInto(iter, chars.get(), len * sizeof(wchar_t))) {
    return false;
  }
  result->assign(chars.get(), len);

  return true;
}

bool Pickle::ExtractBuffers(PickleIterator* iter, size_t length, BufferList* buffers,
                            uint32_t alignment) const
{
  DCHECK(iter);
  DCHECK(buffers);
  DCHECK(alignment == 4 || alignment == 8);
  DCHECK(intptr_t(header_) % alignment == 0);

  if (AlignInt(length) < length || iter->iter_.Done()) {
    return false;
  }

  uint32_t padding_len = intptr_t(iter->iter_.Data()) % alignment;
  if (!iter->iter_.AdvanceAcrossSegments(buffers_, padding_len)) {
    return false;
  }

  bool success;
  *buffers = const_cast<BufferList*>(&buffers_)->Extract(iter->iter_, length, &success);
  if (!success) {
    return false;
  }

  return iter->iter_.AdvanceAcrossSegments(buffers_, AlignInt(length) - length);
}

bool Pickle::ReadBytesInto(PickleIterator* iter, void* data, uint32_t length) const {
  if (AlignInt(length) < length) {
    return false;
  }

  if (!buffers_.ReadBytes(iter->iter_, reinterpret_cast<char*>(data), length)) {
    return false;
  }

  return iter->iter_.AdvanceAcrossSegments(buffers_, AlignInt(length) - length);
}

#ifdef MOZ_PICKLE_SENTINEL_CHECKING
MOZ_NEVER_INLINE
bool Pickle::ReadSentinel(PickleIterator* iter, uint32_t sentinel) const {
  uint32_t found;
  if (!ReadUInt32(iter, &found)) {
    return false;
  }
  return found == sentinel;
}

bool Pickle::IgnoreSentinel(PickleIterator* iter) const {
  uint32_t found;
  return ReadUInt32(iter, &found);
}

bool Pickle::WriteSentinel(uint32_t sentinel) {
  return WriteUInt32(sentinel);
}
#endif

void Pickle::EndRead(PickleIterator& iter, uint32_t ipcMsgType) const {
  DCHECK(iter.iter_.Done());

  if (NS_IsMainThread() && ipcMsgType != 0) {
    uint32_t latencyMs = round((mozilla::TimeStamp::Now() - iter.start_).ToMilliseconds());
    if (latencyMs >= kMinTelemetryIPCReadLatencyMs) {
      mozilla::Telemetry::Accumulate(mozilla::Telemetry::IPC_READ_MAIN_THREAD_LATENCY_MS,
                                     nsDependentCString(IPC::StringFromIPCMessageType(ipcMsgType)),
                                     latencyMs);
    }
  }
}

void Pickle::BeginWrite(uint32_t length, uint32_t alignment) {
  DCHECK(alignment % 4 == 0) << "Must be at least 32-bit aligned!";

  // write at an alignment-aligned offset from the beginning of the header
  uint32_t offset = AlignInt(header_->payload_size);
  uint32_t padding = (header_size_ + offset) % alignment;
  uint32_t new_size = offset + padding + AlignInt(length);
  MOZ_RELEASE_ASSERT(new_size >= header_->payload_size);

  DCHECK(intptr_t(header_) % alignment == 0);

#ifdef ARCH_CPU_64_BITS
  DCHECK_LE(length, std::numeric_limits<uint32_t>::max());
#endif

  if (padding) {
    MOZ_RELEASE_ASSERT(padding <= 8);
    static const char padding_data[8] = {
      kBytePaddingMarker, kBytePaddingMarker, kBytePaddingMarker, kBytePaddingMarker,
      kBytePaddingMarker, kBytePaddingMarker, kBytePaddingMarker, kBytePaddingMarker,
    };
    buffers_.WriteBytes(padding_data, padding);
  }

  DCHECK((header_size_ + header_->payload_size + padding) % alignment == 0);

  header_->payload_size = new_size;
}

void Pickle::EndWrite(uint32_t length) {
  // Zero-pad to keep tools like purify from complaining about uninitialized
  // memory.
  uint32_t padding = AlignInt(length) - length;
  if (padding) {
    MOZ_RELEASE_ASSERT(padding <= 4);
    static const char padding_data[4] = {
      kBytePaddingMarker, kBytePaddingMarker, kBytePaddingMarker, kBytePaddingMarker,
    };
    buffers_.WriteBytes(padding_data, padding);
  }
}

bool Pickle::WriteBool(bool value) {
#ifdef FUZZING
  Singleton<mozilla::ipc::Faulty>::get()->FuzzBool(&value);
#endif
  return WriteInt(value ? 1 : 0);
}

bool Pickle::WriteInt16(int16_t value) {
#ifdef FUZZING
  Singleton<mozilla::ipc::Faulty>::get()->FuzzInt16(&value);
#endif
  return WriteBytes(&value, sizeof(value));
}

bool Pickle::WriteUInt16(uint16_t value) {
#ifdef FUZZING
  Singleton<mozilla::ipc::Faulty>::get()->FuzzUInt16(&value);
#endif
  return WriteBytes(&value, sizeof(value));
}

bool Pickle::WriteInt(int value) {
#ifdef FUZZING
  Singleton<mozilla::ipc::Faulty>::get()->FuzzInt(&value);
#endif
  return WriteBytes(&value, sizeof(value));
}

bool Pickle::WriteLong(long value) {
  // Always written as a 64-bit value since the size for this type can
  // differ between architectures.
#ifdef FUZZING
  Singleton<mozilla::ipc::Faulty>::get()->FuzzLong(&value);
#endif
  return WriteInt64(int64_t(value));
}

bool Pickle::WriteULong(unsigned long value) {
  // Always written as a 64-bit value since the size for this type can
  // differ between architectures.
#ifdef FUZZING
  Singleton<mozilla::ipc::Faulty>::get()->FuzzULong(&value);
#endif
  return WriteUInt64(uint64_t(value));
}

bool Pickle::WriteSize(size_t value) {
  // Always written as a 64-bit value since the size for this type can
  // differ between architectures.
#ifdef FUZZING
  Singleton<mozilla::ipc::Faulty>::get()->FuzzSize(&value);
#endif
  return WriteUInt64(uint64_t(value));
}

bool Pickle::WriteInt32(int32_t value) {
#ifdef FUZZING
  Singleton<mozilla::ipc::Faulty>::get()->FuzzInt(&value);
#endif
  return WriteBytes(&value, sizeof(value));
}

bool Pickle::WriteUInt32(uint32_t value) {
#ifdef FUZZING
  Singleton<mozilla::ipc::Faulty>::get()->FuzzUInt32(&value);
#endif
  return WriteBytes(&value, sizeof(value));
}

bool Pickle::WriteInt64(int64_t value) {
#ifdef FUZZING
  Singleton<mozilla::ipc::Faulty>::get()->FuzzInt64(&value);
#endif
  return WriteBytes(&value, sizeof(value));
}

bool Pickle::WriteUInt64(uint64_t value) {
#ifdef FUZZING
  Singleton<mozilla::ipc::Faulty>::get()->FuzzUInt64(&value);
#endif
  return WriteBytes(&value, sizeof(value));
}

bool Pickle::WriteDouble(double value) {
#ifdef FUZZING
  Singleton<mozilla::ipc::Faulty>::get()->FuzzDouble(&value);
#endif
  return WriteBytes(&value, sizeof(value));
}

bool Pickle::WriteIntPtr(intptr_t value) {
  // Always written as a 64-bit value since the size for this type can
  // differ between architectures.
  return WriteInt64(int64_t(value));
}

bool Pickle::WriteUnsignedChar(unsigned char value) {
#ifdef FUZZING
  Singleton<mozilla::ipc::Faulty>::get()->FuzzUChar(&value);
#endif
  return WriteBytes(&value, sizeof(value));
}

bool Pickle::WriteBytesZeroCopy(void* data, uint32_t data_len, uint32_t capacity) {

  BeginWrite(data_len, sizeof(memberAlignmentType));

  buffers_.WriteBytesZeroCopy(reinterpret_cast<char*>(data), data_len, capacity);

  EndWrite(data_len);
  return true;
}

bool Pickle::WriteBytes(const void* data, uint32_t data_len, uint32_t alignment) {
  DCHECK(alignment == 4 || alignment == 8);
  DCHECK(intptr_t(header_) % alignment == 0);

  BeginWrite(data_len, alignment);

  buffers_.WriteBytes(reinterpret_cast<const char*>(data), data_len);

  EndWrite(data_len);
  return true;
}

bool Pickle::WriteString(const std::string& value) {
#ifdef FUZZING
  std::string v(value);
  Singleton<mozilla::ipc::Faulty>::get()->FuzzString(v);
  if (!WriteInt(static_cast<int>(v.size())))
    return false;

  return WriteBytes(v.data(), static_cast<int>(v.size()));
#else
  if (!WriteInt(static_cast<int>(value.size())))
    return false;

  return WriteBytes(value.data(), static_cast<int>(value.size()));
#endif
}

bool Pickle::WriteWString(const std::wstring& value) {
#ifdef FUZZING
  std::wstring v(value);
  Singleton<mozilla::ipc::Faulty>::get()->FuzzWString(v);
  if (!WriteInt(static_cast<int>(v.size())))
    return false;

  return WriteBytes(v.data(),
                    static_cast<int>(v.size() * sizeof(wchar_t)));
#else
  if (!WriteInt(static_cast<int>(value.size())))
    return false;

  return WriteBytes(value.data(),
                    static_cast<int>(value.size() * sizeof(wchar_t)));
#endif
}

bool Pickle::WriteData(const char* data, uint32_t length) {
   return WriteInt(length) && WriteBytes(data, length);
}

void Pickle::InputBytes(const char* data, uint32_t length) {
  buffers_.WriteBytes(data, length);
}

int32_t* Pickle::GetInt32PtrForTest(uint32_t offset) {
  size_t pos = buffers_.Size() - offset;
  BufferList::IterImpl iter(buffers_);
  MOZ_RELEASE_ASSERT(iter.AdvanceAcrossSegments(buffers_, pos));
  return reinterpret_cast<int32_t*>(iter.Data());
}

// static
uint32_t Pickle::MessageSize(uint32_t header_size,
                             const char* start,
                             const char* end) {
  DCHECK(header_size == AlignInt(header_size));
  DCHECK(header_size <= static_cast<memberAlignmentType>(kHeaderSegmentCapacity));

  if (end < start)
    return 0;
  size_t length = static_cast<size_t>(end - start);
  if (length < sizeof(Header))
    return 0;

  const Header* hdr = reinterpret_cast<const Header*>(start);
  if (length < header_size)
    return 0;

  mozilla::CheckedInt<uint32_t> sum(header_size);
  sum += hdr->payload_size;

  if (!sum.isValid())
    return 0;

  return sum.value();
}
