/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrashReporterMetadataShmem.h"
#include "mozilla/Attributes.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace ipc {

enum class EntryType : uint8_t {
  None,
  Annotation,
};

CrashReporterMetadataShmem::CrashReporterMetadataShmem(const Shmem& aShmem)
 : mShmem(aShmem)
{
  MOZ_COUNT_CTOR(CrashReporterMetadataShmem);
}

CrashReporterMetadataShmem::~CrashReporterMetadataShmem()
{
  MOZ_COUNT_DTOR(CrashReporterMetadataShmem);
}

void
CrashReporterMetadataShmem::AnnotateCrashReport(const nsCString& aKey, const nsCString& aData)
{
  mNotes.Put(aKey, aData);
  SyncNotesToShmem();
}

void
CrashReporterMetadataShmem::AppendAppNotes(const nsCString& aData)
{
  mAppNotes.Append(aData);
  mNotes.Put(NS_LITERAL_CSTRING("Notes"), mAppNotes);
  SyncNotesToShmem();
}

class MOZ_STACK_CLASS MetadataShmemWriter
{
public:
  explicit MetadataShmemWriter(const Shmem& aShmem)
   : mCursor(aShmem.get<uint8_t>()),
     mEnd(mCursor + aShmem.Size<uint8_t>())
  {
    *mCursor = uint8_t(EntryType::None);
  }

  MOZ_MUST_USE bool WriteAnnotation(const nsCString& aKey, const nsCString& aValue) {
    // This shouldn't happen because Commit() guarantees mCursor < mEnd. But
    // we might as well be safe.
    if (mCursor >= mEnd) {
      return false;
    }

    // Save the current position so we can write the entry type if the entire
    // entry fits.
    uint8_t* start = mCursor++;
    if (!Write(aKey) || !Write(aValue)) {
      return false;
    }
    return Commit(start, EntryType::Annotation);
  }

private:
  // On success, append a new terminal byte. On failure, rollback the cursor.
  MOZ_MUST_USE bool Commit(uint8_t* aStart, EntryType aType) {
    MOZ_ASSERT(aStart < mEnd);
    MOZ_ASSERT(EntryType(*aStart) == EntryType::None);

    if (mCursor >= mEnd) {
      // No room for a terminating byte - rollback.
      mCursor = aStart;
      return false;
    }

    // Commit the entry and write a new terminal byte.
    *aStart = uint8_t(aType);
    *mCursor = uint8_t(EntryType::None);
    return true;
  }

  MOZ_MUST_USE bool Write(const nsCString& aString) {
    // 32-bit length is okay since our shmems are very small (16K),
    // a huge write would fail anyway.
    return Write(static_cast<uint32_t>(aString.Length())) &&
           Write(aString.get(), aString.Length());
  }

  template <typename T>
  MOZ_MUST_USE bool Write(const T& aT) {
    return Write(&aT, sizeof(T));
  }

  MOZ_MUST_USE bool Write(const void* aData, size_t aLength) {
    if (size_t(mEnd - mCursor) < aLength) {
      return false;
    }
    memcpy(mCursor, aData, aLength);
    mCursor += aLength;
    return true;
  }

 private:
  // The cursor (beginning at start) always points to a single byte
  // representing the next EntryType. An EntryType is either None,
  // indicating there are no more entries, or Annotation, meaning
  // two strings follow.
  //
  // Strings are written as a 32-bit length and byte sequence. After each new
  // entry, a None entry is always appended, and a subsequent entry will
  // overwrite this byte.
  uint8_t* mCursor;
  uint8_t* mEnd;
};

void
CrashReporterMetadataShmem::SyncNotesToShmem()
{
  MetadataShmemWriter writer(mShmem);

  for (auto it = mNotes.Iter(); !it.Done(); it.Next()) {
    nsCString key = nsCString(it.Key());
    nsCString value = nsCString(it.Data());
    if (!writer.WriteAnnotation(key, value)) {
      return;
    }
  }
}

// Helper class to iterate over metadata entries encoded in shmem.
class MOZ_STACK_CLASS MetadataShmemReader
{
public:
  explicit MetadataShmemReader(const Shmem& aShmem)
   : mEntryType(EntryType::None)
  {
    mCursor = aShmem.get<uint8_t>();
    mEnd = mCursor + aShmem.Size<uint8_t>();

    // Advance to the first item, if any.
    Next();
  }

  bool Done() const {
    return mCursor >= mEnd || Type() == EntryType::None;
  }
  EntryType Type() const {
    return mEntryType;
  }
  void Next() {
    if (mCursor < mEnd) {
      mEntryType = EntryType(*mCursor++);
    } else {
      mEntryType = EntryType::None;
    }
  }

  bool Read(nsCString& aOut) {
    uint32_t length = 0;
    if (!Read(&length)) {
      return false;
    }

    const uint8_t* src = Read(length);
    if (!src) {
      return false;
    }

    aOut.Assign((const char *)src, length);
    return true;
  }

private:
  template <typename T>
  bool Read(T* aOut) {
    return Read(aOut, sizeof(T));
  }
  bool Read(void* aOut, size_t aLength) {
    const uint8_t* src = Read(aLength);
    if (!src) {
      return false;
    }
    memcpy(aOut, src, aLength);
    return true;
  }

  // If buffer has |aLength| bytes, return cursor and then advance it.
  // Otherwise, return null.
  const uint8_t* Read(size_t aLength) {
    if (size_t(mEnd - mCursor) < aLength) {
      return nullptr;
    }
    const uint8_t* result = mCursor;
    mCursor += aLength;
    return result;
  }

private:
  const uint8_t* mCursor;
  const uint8_t* mEnd;
  EntryType mEntryType;
};

void
CrashReporterMetadataShmem::ReadAppNotes(const Shmem& aShmem, CrashReporter::AnnotationTable* aNotes)
{
  for (MetadataShmemReader reader(aShmem); !reader.Done(); reader.Next()) {
    switch (reader.Type()) {
      case EntryType::Annotation: {
        nsCString key, value;
        if (!reader.Read(key) || !reader.Read(value)) {
          return;
        }

        aNotes->Put(key, value);
        break;
      }
      default:
        NS_ASSERTION(false, "Unknown metadata entry type");
        break;
    }
  }
}

} // namespace ipc
} // namespace mozilla
