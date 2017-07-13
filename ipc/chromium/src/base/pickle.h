/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PICKLE_H__
#define BASE_PICKLE_H__

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string16.h"

#include "mozilla/Attributes.h"
#include "mozilla/BufferList.h"
#include "mozilla/mozalloc.h"
#include "mozilla/TimeStamp.h"

#ifdef FUZZING
#include "base/singleton.h"
#include "mozilla/ipc/Faulty.h"
#endif

#if !defined(RELEASE_OR_BETA) || defined(DEBUG)
#define MOZ_PICKLE_SENTINEL_CHECKING
#endif

class Pickle;

class PickleIterator {
public:
  explicit PickleIterator(const Pickle& pickle);

private:
  friend class Pickle;

  mozilla::BufferList<InfallibleAllocPolicy>::IterImpl iter_;
  mozilla::TimeStamp start_;

  template<typename T>
  void CopyInto(T* dest);
};

// This class provides facilities for basic binary value packing and unpacking.
//
// The Pickle class supports appending primitive values (ints, strings, etc.)
// to a pickle instance.  The Pickle instance grows its internal memory buffer
// dynamically to hold the sequence of primitive values.   The internal memory
// buffer is exposed as the "data" of the Pickle.  This "data" can be passed
// to a Pickle object to initialize it for reading.
//
// When reading from a Pickle object, it is important for the consumer to know
// what value types to read and in what order to read them as the Pickle does
// not keep track of the type of data written to it.
//
// The Pickle's data has a header which contains the size of the Pickle's
// payload.  It can optionally support additional space in the header.  That
// space is controlled by the header_size parameter passed to the Pickle
// constructor.
//
class Pickle {
 public:
  ~Pickle();

  Pickle() = delete;

  // Initialize a Pickle object with the specified header size in bytes, which
  // must be greater-than-or-equal-to sizeof(Pickle::Header).  The header size
  // will be rounded up to ensure that the header size is 32bit-aligned.
  explicit Pickle(uint32_t header_size, size_t segment_capacity = 0);

  Pickle(uint32_t header_size, const char* data, uint32_t length);

  Pickle(const Pickle& other) = delete;

  Pickle(Pickle&& other);

  // Performs a deep copy.
  Pickle& operator=(const Pickle& other) = delete;

  Pickle& operator=(Pickle&& other);

  // Returns the size of the Pickle's data.
  uint32_t size() const { return header_size_ + header_->payload_size; }

  typedef mozilla::BufferList<InfallibleAllocPolicy> BufferList;

  const BufferList& Buffers() const { return buffers_; }

  uint32_t CurrentSize() const { return buffers_.Size(); }

  // Methods for reading the payload of the Pickle.  To read from the start of
  // the Pickle, initialize *iter to NULL.  If successful, these methods return
  // true.  Otherwise, false is returned to indicate that the result could not
  // be extracted.
  MOZ_MUST_USE bool ReadBool(PickleIterator* iter, bool* result) const;
  MOZ_MUST_USE bool ReadInt16(PickleIterator* iter, int16_t* result) const;
  MOZ_MUST_USE bool ReadUInt16(PickleIterator* iter, uint16_t* result) const;
  MOZ_MUST_USE bool ReadShort(PickleIterator* iter, short* result) const;
  MOZ_MUST_USE bool ReadInt(PickleIterator* iter, int* result) const;
  MOZ_MUST_USE bool ReadLong(PickleIterator* iter, long* result) const;
  MOZ_MUST_USE bool ReadULong(PickleIterator* iter, unsigned long* result) const;
  MOZ_MUST_USE bool ReadSize(PickleIterator* iter, size_t* result) const;
  MOZ_MUST_USE bool ReadInt32(PickleIterator* iter, int32_t* result) const;
  MOZ_MUST_USE bool ReadUInt32(PickleIterator* iter, uint32_t* result) const;
  MOZ_MUST_USE bool ReadInt64(PickleIterator* iter, int64_t* result) const;
  MOZ_MUST_USE bool ReadUInt64(PickleIterator* iter, uint64_t* result) const;
  MOZ_MUST_USE bool ReadDouble(PickleIterator* iter, double* result) const;
  MOZ_MUST_USE bool ReadIntPtr(PickleIterator* iter, intptr_t* result) const;
  MOZ_MUST_USE bool ReadUnsignedChar(PickleIterator* iter, unsigned char* result) const;
  MOZ_MUST_USE bool ReadString(PickleIterator* iter, std::string* result) const;
  MOZ_MUST_USE bool ReadWString(PickleIterator* iter, std::wstring* result) const;
  MOZ_MUST_USE bool ReadBytesInto(PickleIterator* iter, void* data, uint32_t length) const;
  MOZ_MUST_USE bool ExtractBuffers(PickleIterator* iter, size_t length, BufferList* buffers,
                                   uint32_t alignment = sizeof(memberAlignmentType)) const;

  // Safer version of ReadInt() checks for the result not being negative.
  // Use it for reading the object sizes.
  MOZ_MUST_USE bool ReadLength(PickleIterator* iter, int* result) const;

  MOZ_MUST_USE bool ReadSentinel(PickleIterator* iter, uint32_t sentinel) const
#ifdef MOZ_PICKLE_SENTINEL_CHECKING
    ;
#else
  {
    return true;
  }
#endif

  bool IgnoreSentinel(PickleIterator* iter) const
#ifdef MOZ_PICKLE_SENTINEL_CHECKING
    ;
#else
  {
    return true;
  }
#endif

  // NOTE: The message type optional parameter should _only_ be called from
  // generated IPDL code, as it is used to trigger the IPC_READ_LATENCY_MS
  // telemetry probe.
  void EndRead(PickleIterator& iter, uint32_t ipcMessageType = 0) const;

  // Methods for adding to the payload of the Pickle.  These values are
  // appended to the end of the Pickle's payload.  When reading values from a
  // Pickle, it is important to read them in the order in which they were added
  // to the Pickle.
  bool WriteBool(bool value) {
#ifdef FUZZING
    Singleton<mozilla::ipc::Faulty>::get()->FuzzBool(&value);
#endif
    return WriteInt(value ? 1 : 0);
  }
  bool WriteInt16(int16_t value) {
#ifdef FUZZING
    Singleton<mozilla::ipc::Faulty>::get()->FuzzInt16(&value);
#endif
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteUInt16(uint16_t value) {
#ifdef FUZZING
    Singleton<mozilla::ipc::Faulty>::get()->FuzzUInt16(&value);
#endif
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteInt(int value) {
#ifdef FUZZING
    Singleton<mozilla::ipc::Faulty>::get()->FuzzInt(&value);
#endif
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteLong(long value) {
    // Always written as a 64-bit value since the size for this type can
    // differ between architectures.
#ifdef FUZZING
    Singleton<mozilla::ipc::Faulty>::get()->FuzzLong(&value);
#endif
    return WriteInt64(int64_t(value));
  }
  bool WriteULong(unsigned long value) {
    // Always written as a 64-bit value since the size for this type can
    // differ between architectures.
#ifdef FUZZING
    Singleton<mozilla::ipc::Faulty>::get()->FuzzULong(&value);
#endif
    return WriteUInt64(uint64_t(value));
  }
  bool WriteSize(size_t value) {
    // Always written as a 64-bit value since the size for this type can
    // differ between architectures.
#ifdef FUZZING
    Singleton<mozilla::ipc::Faulty>::get()->FuzzSize(&value);
#endif
    return WriteUInt64(uint64_t(value));
  }
  bool WriteInt32(int32_t value) {
#ifdef FUZZING
    Singleton<mozilla::ipc::Faulty>::get()->FuzzInt(&value);
#endif
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteUInt32(uint32_t value) {
#ifdef FUZZING
    Singleton<mozilla::ipc::Faulty>::get()->FuzzUInt32(&value);
#endif
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteInt64(int64_t value) {
#ifdef FUZZING
    Singleton<mozilla::ipc::Faulty>::get()->FuzzInt64(&value);
#endif
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteUInt64(uint64_t value) {
#ifdef FUZZING
    Singleton<mozilla::ipc::Faulty>::get()->FuzzUInt64(&value);
#endif
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteDouble(double value) {
#ifdef FUZZING
    Singleton<mozilla::ipc::Faulty>::get()->FuzzDouble(&value);
#endif
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteIntPtr(intptr_t value) {
    // Always written as a 64-bit value since the size for this type can
    // differ between architectures.
    return WriteInt64(int64_t(value));
  }
  bool WriteUnsignedChar(unsigned char value) {
#ifdef FUZZING
    Singleton<mozilla::ipc::Faulty>::get()->FuzzUChar(&value);
#endif
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteString(const std::string& value);
  bool WriteWString(const std::wstring& value);
  bool WriteData(const char* data, uint32_t length);
  bool WriteBytes(const void* data, uint32_t data_len,
                  uint32_t alignment = sizeof(memberAlignmentType));

  bool WriteSentinel(uint32_t sentinel)
#ifdef MOZ_PICKLE_SENTINEL_CHECKING
    ;
#else
  {
    return true;
  }
#endif

  int32_t* GetInt32PtrForTest(uint32_t offset);

  void InputBytes(const char* data, uint32_t length);

  // Payload follows after allocation of Header (header size is customizable).
  struct Header {
    uint32_t payload_size;  // Specifies the size of the payload.
  };

  // Returns the header, cast to a user-specified type T.  The type T must be a
  // subclass of Header and its size must correspond to the header_size passed
  // to the Pickle constructor.
  template <class T>
  T* headerT() {
    DCHECK(sizeof(T) == header_size_);
    return static_cast<T*>(header_);
  }
  template <class T>
  const T* headerT() const {
    DCHECK(sizeof(T) == header_size_);
    return static_cast<const T*>(header_);
  }

  typedef uint32_t memberAlignmentType;

 protected:
  uint32_t payload_size() const { return header_->payload_size; }

  // Resizes the buffer for use when writing the specified amount of data. The
  // location that the data should be written at is returned, or NULL if there
  // was an error. Call EndWrite with the returned offset and the given length
  // to pad out for the next write.
  void BeginWrite(uint32_t length, uint32_t alignment);

  // Completes the write operation by padding the data with NULL bytes until it
  // is padded. Should be paired with BeginWrite, but it does not necessarily
  // have to be called after the data is written.
  void EndWrite(uint32_t length);

  // Round 'bytes' up to the next multiple of 'alignment'.  'alignment' must be
  // a power of 2.
  template<uint32_t alignment> struct ConstantAligner {
    static uint32_t align(int bytes) {
      static_assert((alignment & (alignment - 1)) == 0,
                    "alignment must be a power of two");
      return (bytes + (alignment - 1)) & ~static_cast<uint32_t>(alignment - 1);
    }
  };

  static uint32_t AlignInt(int bytes) {
    return ConstantAligner<sizeof(memberAlignmentType)>::align(bytes);
  }

  static uint32_t AlignCapacity(int bytes) {
    return ConstantAligner<kSegmentAlignment>::align(bytes);
  }

  // Returns true if the given iterator could point to data with the given
  // length. If there is no room for the given data before the end of the
  // payload, returns false.
  bool IteratorHasRoomFor(const PickleIterator& iter, uint32_t len) const;

  // Moves the iterator by the given number of bytes, making sure it is aligned.
  // Pointer (iterator) is NOT aligned, but the change in the pointer
  // is guaranteed to be a multiple of sizeof(memberAlignmentType).
  void UpdateIter(PickleIterator* iter, uint32_t bytes) const;

  // Figure out how big the message starting at range_start is. Returns 0 if
  // there's no enough data to determine (i.e., if [range_start, range_end) does
  // not contain enough of the message header to know the size).
  static uint32_t MessageSize(uint32_t header_size,
                              const char* range_start,
                              const char* range_end);

  // Segments capacities are aligned to 8 bytes to ensure that all reads/writes
  // at 8-byte aligned offsets will be on 8-byte aligned pointers.
  static const uint32_t kSegmentAlignment = 8;

 private:
  friend class PickleIterator;

  BufferList buffers_;
  Header* header_;
  uint32_t header_size_;
};

#endif  // BASE_PICKLE_H__
