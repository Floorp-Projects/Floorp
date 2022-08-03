/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XMLHttpRequestString.h"
#include "nsISupportsImpl.h"
#include "mozilla/dom/DOMString.h"

namespace mozilla::dom {

class XMLHttpRequestStringBuffer final {
  friend class XMLHttpRequestStringWriterHelper;
  friend class XMLHttpRequestStringSnapshotReaderHelper;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(XMLHttpRequestStringBuffer)
  NS_DECL_OWNINGTHREAD

  XMLHttpRequestStringBuffer() : mMutex("XMLHttpRequestStringBuffer::mMutex") {}

  uint32_t Length() {
    MutexAutoLock lock(mMutex);
    return mData.Length();
  }

  uint32_t UnsafeLength() const MOZ_NO_THREAD_SAFETY_ANALYSIS {
    return mData.Length();
  }

  mozilla::Result<mozilla::BulkWriteHandle<char16_t>, nsresult> UnsafeBulkWrite(
      uint32_t aCapacity) MOZ_NO_THREAD_SAFETY_ANALYSIS {
    return mData.BulkWrite(aCapacity, UnsafeLength(), false);
  }

  void Append(const nsAString& aString) {
    NS_ASSERT_OWNINGTHREAD(XMLHttpRequestStringBuffer);

    MutexAutoLock lock(mMutex);
    mData.Append(aString);
  }

  [[nodiscard]] bool GetAsString(nsAString& aString) {
    MutexAutoLock lock(mMutex);
    return aString.Assign(mData, mozilla::fallible);
  }

  size_t SizeOfThis(MallocSizeOf aMallocSizeOf) {
    MutexAutoLock lock(mMutex);
    return mData.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  [[nodiscard]] bool GetAsString(DOMString& aString, uint32_t aLength) {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(aLength <= mData.Length());

    // XXX: Bug 1408793 suggests encapsulating the following sequence within
    //      DOMString.
    nsStringBuffer* buf = nsStringBuffer::FromString(mData);
    if (buf) {
      // We have to use SetStringBuffer, because once we release our mutex mData
      // can get mutated from some other thread while the DOMString is still
      // alive.
      aString.SetStringBuffer(buf, aLength);
      return true;
    }

    // We can get here if mData is empty.  In that case it won't have an
    // nsStringBuffer....
    MOZ_ASSERT(mData.IsEmpty());
    return aString.AsAString().Assign(mData.BeginReading(), aLength,
                                      mozilla::fallible);
  }

  void CreateSnapshot(XMLHttpRequestStringSnapshot& aSnapshot) {
    MutexAutoLock lock(mMutex);
    aSnapshot.Set(this, mData.Length());
  }

 private:
  ~XMLHttpRequestStringBuffer() = default;

  nsString& UnsafeData() { return mData; }

  Mutex mMutex;

  // The following member variable is protected by mutex.
  nsString mData MOZ_GUARDED_BY(mMutex);
};

// ---------------------------------------------------------------------------
// XMLHttpRequestString

XMLHttpRequestString::XMLHttpRequestString()
    : mBuffer(new XMLHttpRequestStringBuffer()) {}

XMLHttpRequestString::~XMLHttpRequestString() = default;

void XMLHttpRequestString::Truncate() {
  mBuffer = new XMLHttpRequestStringBuffer();
}

uint32_t XMLHttpRequestString::Length() const { return mBuffer->Length(); }

void XMLHttpRequestString::Append(const nsAString& aString) {
  mBuffer->Append(aString);
}

bool XMLHttpRequestString::GetAsString(nsAString& aString) const {
  return mBuffer->GetAsString(aString);
}

size_t XMLHttpRequestString::SizeOfThis(MallocSizeOf aMallocSizeOf) const {
  return mBuffer->SizeOfThis(aMallocSizeOf);
}

bool XMLHttpRequestString::IsEmpty() const { return !mBuffer->Length(); }

void XMLHttpRequestString::CreateSnapshot(
    XMLHttpRequestStringSnapshot& aSnapshot) {
  mBuffer->CreateSnapshot(aSnapshot);
}

// ---------------------------------------------------------------------------
// XMLHttpRequestStringSnapshot

XMLHttpRequestStringSnapshot::XMLHttpRequestStringSnapshot()
    : mLength(0), mVoid(false) {}

XMLHttpRequestStringSnapshot::~XMLHttpRequestStringSnapshot() = default;

void XMLHttpRequestStringSnapshot::ResetInternal(bool aIsVoid) {
  mBuffer = nullptr;
  mLength = 0;
  mVoid = aIsVoid;
}

void XMLHttpRequestStringSnapshot::Set(XMLHttpRequestStringBuffer* aBuffer,
                                       uint32_t aLength) {
  MOZ_ASSERT(aBuffer);
  MOZ_ASSERT(aLength <= aBuffer->UnsafeLength());

  mBuffer = aBuffer;
  mLength = aLength;
  mVoid = false;
}

bool XMLHttpRequestStringSnapshot::GetAsString(DOMString& aString) const {
  if (mBuffer) {
    MOZ_ASSERT(!mVoid);
    return mBuffer->GetAsString(aString, mLength);
  }

  if (mVoid) {
    aString.SetNull();
  }

  return true;
}

// ---------------------------------------------------------------------------
// XMLHttpRequestStringWriterHelper

XMLHttpRequestStringWriterHelper::XMLHttpRequestStringWriterHelper(
    XMLHttpRequestString& aString)
    : mBuffer(aString.mBuffer), mLock(aString.mBuffer->mMutex) {}

XMLHttpRequestStringWriterHelper::~XMLHttpRequestStringWriterHelper() = default;

uint32_t XMLHttpRequestStringWriterHelper::Length() const {
  return mBuffer->UnsafeLength();
}

mozilla::Result<mozilla::BulkWriteHandle<char16_t>, nsresult>
XMLHttpRequestStringWriterHelper::BulkWrite(uint32_t aCapacity) {
  return mBuffer->UnsafeBulkWrite(aCapacity);
}

// ---------------------------------------------------------------------------
// XMLHttpRequestStringReaderHelper

XMLHttpRequestStringSnapshotReaderHelper::
    XMLHttpRequestStringSnapshotReaderHelper(
        XMLHttpRequestStringSnapshot& aSnapshot)
    : mBuffer(aSnapshot.mBuffer), mLock(aSnapshot.mBuffer->mMutex) {}

XMLHttpRequestStringSnapshotReaderHelper::
    ~XMLHttpRequestStringSnapshotReaderHelper() = default;

const char16_t* XMLHttpRequestStringSnapshotReaderHelper::Buffer() const {
  return mBuffer->UnsafeData().BeginReading();
}

uint32_t XMLHttpRequestStringSnapshotReaderHelper::Length() const {
  return mBuffer->UnsafeLength();
}

}  // namespace mozilla::dom
