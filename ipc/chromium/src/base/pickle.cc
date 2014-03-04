// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pickle.h"

#include "mozilla/Alignment.h"
#include "mozilla/Endian.h"
#include "mozilla/TypeTraits.h"

#include <stdlib.h>

#include <limits>
#include <string>
#include <algorithm>

#include "nsDebug.h"

//------------------------------------------------------------------------------

static_assert(MOZ_ALIGNOF(Pickle::memberAlignmentType) >= MOZ_ALIGNOF(uint32_t),
              "Insufficient alignment");

// static
const int Pickle::kPayloadUnit = 64;

// We mark a read only pickle with a special capacity_.
static const uint32_t kCapacityReadOnly = (uint32_t) -1;

static const char kBytePaddingMarker = char(0xbf);

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
  static void Copy(T* dest, void** iter) {
    memcpy(dest, *iter, sizeof(T));
  }
};

// Copying 64-bit quantities happens often enough and can easily be made
// worthwhile on 32-bit platforms, so handle it specially.  Only do it
// if 64-bit types aren't sufficiently aligned; the alignment
// requirements for them vary between 32-bit platforms.
#ifndef HAVE_64BIT_OS
template<typename T>
struct Copier<T, sizeof(uint64_t), false>
{
  static void Copy(T* dest, void** iter) {
#if MOZ_LITTLE_ENDIAN
    static const int loIndex = 0, hiIndex = 1;
#else
    static const int loIndex = 1, hiIndex = 0;
#endif
    static_assert(MOZ_ALIGNOF(uint32_t*) == MOZ_ALIGNOF(void*),
                  "Pointers have different alignments");
    uint32_t* src = *reinterpret_cast<uint32_t**>(iter);
    uint32_t* uint32dest = reinterpret_cast<uint32_t*>(dest);
    uint32dest[loIndex] = src[loIndex];
    uint32dest[hiIndex] = src[hiIndex];
  }
};
#endif

template<typename T, size_t size>
struct Copier<T, size, true>
{
  static void Copy(T* dest, void** iter) {
    // The reinterpret_cast is only safe if two conditions hold:
    // (1) If the alignment of T* is the same as void*;
    // (2) The alignment of the data in *iter is at least as
    //     big as MOZ_ALIGNOF(T).
    // Check the first condition, as the second condition is already
    // known to be true, or we wouldn't be here.
    static_assert(MOZ_ALIGNOF(T*) == MOZ_ALIGNOF(void*),
                  "Pointers have different alignments");
    *dest = *(*reinterpret_cast<T**>(iter));
  }
};

template<typename T>
void CopyFromIter(T* dest, void** iter) {
  static_assert(mozilla::IsPod<T>::value, "Copied type must be a POD type");
  Copier<T, sizeof(T), (MOZ_ALIGNOF(T) <= sizeof(Pickle::memberAlignmentType))>::Copy(dest, iter);
}

} // anonymous namespace

// Payload is sizeof(Pickle::memberAlignmentType) aligned.

Pickle::Pickle()
    : header_(NULL),
      header_size_(sizeof(Header)),
      capacity_(0),
      variable_buffer_offset_(0) {
  Resize(kPayloadUnit);
  header_->payload_size = 0;
}

Pickle::Pickle(int header_size)
    : header_(NULL),
      header_size_(AlignInt(header_size)),
      capacity_(0),
      variable_buffer_offset_(0) {
  DCHECK(static_cast<memberAlignmentType>(header_size) >= sizeof(Header));
  DCHECK(header_size <= kPayloadUnit);
  Resize(kPayloadUnit);
  if (!header_) {
    NS_ABORT_OOM(kPayloadUnit);
  }
  header_->payload_size = 0;
}

Pickle::Pickle(const char* data, int data_len)
    : header_(reinterpret_cast<Header*>(const_cast<char*>(data))),
      header_size_(data_len - header_->payload_size),
      capacity_(kCapacityReadOnly),
      variable_buffer_offset_(0) {
  DCHECK(header_size_ >= sizeof(Header));
  DCHECK(header_size_ == AlignInt(header_size_));
}

Pickle::Pickle(const Pickle& other)
    : header_(NULL),
      header_size_(other.header_size_),
      capacity_(0),
      variable_buffer_offset_(other.variable_buffer_offset_) {
  uint32_t payload_size = header_size_ + other.header_->payload_size;
  bool resized = Resize(payload_size);
  if (!resized) {
    NS_ABORT_OOM(payload_size);
  }
  memcpy(header_, other.header_, payload_size);
}

Pickle::~Pickle() {
  if (capacity_ != kCapacityReadOnly)
    free(header_);
}

Pickle& Pickle::operator=(const Pickle& other) {
  if (header_size_ != other.header_size_ && capacity_ != kCapacityReadOnly) {
    free(header_);
    header_ = NULL;
    header_size_ = other.header_size_;
  }
  bool resized = Resize(other.header_size_ + other.header_->payload_size);
  if (!resized) {
    NS_ABORT_OOM(other.header_size_ + other.header_->payload_size);
  }
  memcpy(header_, other.header_, header_size_ + other.header_->payload_size);
  variable_buffer_offset_ = other.variable_buffer_offset_;
  return *this;
}

bool Pickle::ReadBool(void** iter, bool* result) const {
  DCHECK(iter);

  int tmp;
  if (!ReadInt(iter, &tmp))
    return false;
  DCHECK(0 == tmp || 1 == tmp);
  *result = tmp ? true : false;
  return true;
}

bool Pickle::ReadInt16(void** iter, int16_t* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  CopyFromIter(result, iter);

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadUInt16(void** iter, uint16_t* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  CopyFromIter(result, iter);

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadInt(void** iter, int* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  CopyFromIter(result, iter);

  UpdateIter(iter, sizeof(*result));
  return true;
}

// Always written as a 64-bit value since the size for this type can
// differ between architectures.
bool Pickle::ReadLong(void** iter, long* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  int64_t bigResult = 0;
  if (!IteratorHasRoomFor(*iter, sizeof(bigResult)))
    return false;

  CopyFromIter(&bigResult, iter);
  DCHECK(bigResult <= LONG_MAX && bigResult >= LONG_MIN);
  *result = static_cast<long>(bigResult);

  UpdateIter(iter, sizeof(bigResult));
  return true;
}

// Always written as a 64-bit value since the size for this type can
// differ between architectures.
bool Pickle::ReadULong(void** iter, unsigned long* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  uint64_t bigResult = 0;
  if (!IteratorHasRoomFor(*iter, sizeof(bigResult)))
    return false;

  CopyFromIter(&bigResult, iter);
  DCHECK(bigResult <= ULONG_MAX);
  *result = static_cast<unsigned long>(bigResult);

  UpdateIter(iter, sizeof(bigResult));
  return true;
}

bool Pickle::ReadLength(void** iter, int* result) const {
  if (!ReadInt(iter, result))
    return false;
  return ((*result) >= 0);
}

// Always written as a 64-bit value since the size for this type can
// differ between architectures.
bool Pickle::ReadSize(void** iter, size_t* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  uint64_t bigResult = 0;
  if (!IteratorHasRoomFor(*iter, sizeof(bigResult)))
    return false;

  CopyFromIter(&bigResult, iter);
  DCHECK(bigResult <= std::numeric_limits<size_t>::max());
  *result = static_cast<size_t>(bigResult);

  UpdateIter(iter, sizeof(bigResult));
  return true;
}

bool Pickle::ReadInt32(void** iter, int32_t* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  CopyFromIter(result, iter);

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadUInt32(void** iter, uint32_t* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  CopyFromIter(result, iter);

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadInt64(void** iter, int64_t* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  CopyFromIter(result, iter);

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadUInt64(void** iter, uint64_t* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  CopyFromIter(result, iter);

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadDouble(void** iter, double* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  CopyFromIter(result, iter);

  UpdateIter(iter, sizeof(*result));
  return true;
}

// Always written as a 64-bit value since the size for this type can
// differ between architectures.
bool Pickle::ReadIntPtr(void** iter, intptr_t* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  int64_t bigResult = 0;
  if (!IteratorHasRoomFor(*iter, sizeof(bigResult)))
    return false;

  CopyFromIter(&bigResult, iter);
  DCHECK(bigResult <= std::numeric_limits<intptr_t>::max() && bigResult >= std::numeric_limits<intptr_t>::min());
  *result = static_cast<intptr_t>(bigResult);

  UpdateIter(iter, sizeof(bigResult));
  return true;
}

bool Pickle::ReadUnsignedChar(void** iter, unsigned char* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!IteratorHasRoomFor(*iter, sizeof(*result)))
    return false;

  CopyFromIter(result, iter);

  UpdateIter(iter, sizeof(*result));
  return true;
}

bool Pickle::ReadString(void** iter, std::string* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  int len;
  if (!ReadLength(iter, &len))
    return false;
  if (!IteratorHasRoomFor(*iter, len))
    return false;

  char* chars = reinterpret_cast<char*>(*iter);
  result->assign(chars, len);

  UpdateIter(iter, len);
  return true;
}

bool Pickle::ReadWString(void** iter, std::wstring* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  int len;
  if (!ReadLength(iter, &len))
    return false;
  if (!IteratorHasRoomFor(*iter, len * sizeof(wchar_t)))
    return false;

  wchar_t* chars = reinterpret_cast<wchar_t*>(*iter);
  result->assign(chars, len);

  UpdateIter(iter, len * sizeof(wchar_t));
  return true;
}

bool Pickle::ReadString16(void** iter, string16* result) const {
  DCHECK(iter);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  int len;
  if (!ReadLength(iter, &len))
    return false;
  if (!IteratorHasRoomFor(*iter, len))
    return false;

  char16* chars = reinterpret_cast<char16*>(*iter);
  result->assign(chars, len);

  UpdateIter(iter, len * sizeof(char16));
  return true;
}

bool Pickle::ReadBytes(void** iter, const char** data, int length,
                       uint32_t alignment) const {
  DCHECK(iter);
  DCHECK(data);
  DCHECK(alignment == 4 || alignment == 8);
  DCHECK(intptr_t(header_) % alignment == 0);

  if (!*iter)
    *iter = const_cast<char*>(payload());

  uint32_t paddingLen = intptr_t(*iter) % alignment;
  if (paddingLen) {
#ifdef DEBUG
    {
      const char* padding = static_cast<const char*>(*iter);
      for (uint32_t i = 0; i < paddingLen; i++) {
        DCHECK(*(padding + i) == kBytePaddingMarker);
      }
    }
#endif
    length += paddingLen;
  }

  if (!IteratorHasRoomFor(*iter, length))
    return false;

  *data = static_cast<const char*>(*iter) + paddingLen;
  DCHECK(intptr_t(*data) % alignment == 0);

  UpdateIter(iter, length);
  return true;
}

bool Pickle::ReadData(void** iter, const char** data, int* length) const {
  DCHECK(iter);
  DCHECK(data);
  DCHECK(length);
  if (!*iter)
    *iter = const_cast<char*>(payload());

  if (!ReadLength(iter, length))
    return false;

  return ReadBytes(iter, data, *length);
}

char* Pickle::BeginWrite(uint32_t length, uint32_t alignment) {
  DCHECK(alignment % 4 == 0) << "Must be at least 32-bit aligned!";

  // write at an alignment-aligned offset from the beginning of the header
  uint32_t offset = AlignInt(header_->payload_size);
  uint32_t padding = (header_size_ + offset) %  alignment;
  uint32_t new_size = offset + padding + AlignInt(length);
  uint32_t needed_size = header_size_ + new_size;

  if (needed_size > capacity_ && !Resize(std::max(capacity_ * 2, needed_size)))
    return NULL;

  DCHECK(intptr_t(header_) % alignment == 0);

#ifdef ARCH_CPU_64_BITS
  DCHECK_LE(length, std::numeric_limits<uint32_t>::max());
#endif

  char* buffer = payload() + offset;

  if (padding) {
    memset(buffer, kBytePaddingMarker, padding);
    buffer += padding;
  }

  DCHECK(intptr_t(buffer) % alignment == 0);

  header_->payload_size = new_size;

#ifdef MOZ_VALGRIND
  // pad the trailing end as well, so that valgrind
  // doesn't complain when we write the buffer
  padding = AlignInt(length) - length;
  if (padding) {
    memset(buffer + length, kBytePaddingMarker, padding);
  }
#endif

  return buffer;
}

void Pickle::EndWrite(char* dest, int length) {
  // Zero-pad to keep tools like purify from complaining about uninitialized
  // memory.
  if (length % sizeof(memberAlignmentType))
    memset(dest + length, 0,
	   sizeof(memberAlignmentType) - (length % sizeof(memberAlignmentType)));
}

bool Pickle::WriteBytes(const void* data, int data_len, uint32_t alignment) {
  DCHECK(capacity_ != kCapacityReadOnly) << "oops: pickle is readonly";
  DCHECK(alignment == 4 || alignment == 8);
  DCHECK(intptr_t(header_) % alignment == 0);

  char* dest = BeginWrite(data_len, alignment);
  if (!dest)
    return false;

  memcpy(dest, data, data_len);

  EndWrite(dest, data_len);
  return true;
}

bool Pickle::WriteString(const std::string& value) {
  if (!WriteInt(static_cast<int>(value.size())))
    return false;

  return WriteBytes(value.data(), static_cast<int>(value.size()));
}

bool Pickle::WriteWString(const std::wstring& value) {
  if (!WriteInt(static_cast<int>(value.size())))
    return false;

  return WriteBytes(value.data(),
                    static_cast<int>(value.size() * sizeof(wchar_t)));
}

bool Pickle::WriteString16(const string16& value) {
  if (!WriteInt(static_cast<int>(value.size())))
    return false;

  return WriteBytes(value.data(),
                    static_cast<int>(value.size()) * sizeof(char16));
}

bool Pickle::WriteData(const char* data, int length) {
  return WriteInt(length) && WriteBytes(data, length);
}

char* Pickle::BeginWriteData(int length) {
  DCHECK_EQ(variable_buffer_offset_, 0U) <<
    "There can only be one variable buffer in a Pickle";

  if (!WriteInt(length))
    return NULL;

  char *data_ptr = BeginWrite(length, sizeof(memberAlignmentType));
  if (!data_ptr)
    return NULL;

  variable_buffer_offset_ =
      data_ptr - reinterpret_cast<char*>(header_) - sizeof(int);

  // EndWrite doesn't necessarily have to be called after the write operation,
  // so we call it here to pad out what the caller will eventually write.
  EndWrite(data_ptr, length);
  return data_ptr;
}

void Pickle::TrimWriteData(int new_length) {
  DCHECK(variable_buffer_offset_ != 0);

  // Fetch the the variable buffer size
  int* cur_length = reinterpret_cast<int*>(
      reinterpret_cast<char*>(header_) + variable_buffer_offset_);

  if (new_length < 0 || new_length > *cur_length) {
    NOTREACHED() << "Invalid length in TrimWriteData.";
    return;
  }

  // Update the payload size and variable buffer size
  header_->payload_size -= (*cur_length - new_length);
  *cur_length = new_length;
}

bool Pickle::Resize(uint32_t new_capacity) {
  new_capacity = ConstantAligner<kPayloadUnit>::align(new_capacity);

  void* p = realloc(header_, new_capacity);
  if (!p)
    return false;

  header_ = reinterpret_cast<Header*>(p);
  capacity_ = new_capacity;
  return true;
}

// static
const char* Pickle::FindNext(uint32_t header_size,
                             const char* start,
                             const char* end) {
  DCHECK(header_size == AlignInt(header_size));
  DCHECK(header_size <= static_cast<memberAlignmentType>(kPayloadUnit));

  const Header* hdr = reinterpret_cast<const Header*>(start);
  const char* payload_base = start + header_size;
  const char* payload_end = payload_base + hdr->payload_size;
  if (payload_end < payload_base)
    return NULL;

  return (payload_end > end) ? NULL : payload_end;
}
