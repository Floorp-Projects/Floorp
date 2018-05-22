/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IndexedDatabaseInlines_h
#define IndexedDatabaseInlines_h

#ifndef mozilla_dom_indexeddatabase_h__
#error Must include IndexedDatabase.h first
#endif

#include "FileInfo.h"
#include "IDBMutableFile.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBSharedTypes.h"
#include "mozilla/dom/File.h"
#include "nsIInputStream.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

inline
StructuredCloneFile::StructuredCloneFile()
  : mType(eBlob)
{
  MOZ_COUNT_CTOR(StructuredCloneFile);
}

inline
StructuredCloneFile::~StructuredCloneFile()
{
  MOZ_COUNT_DTOR(StructuredCloneFile);
}

inline
bool
StructuredCloneFile::operator==(const StructuredCloneFile& aOther) const
{
  return this->mBlob == aOther.mBlob &&
         this->mMutableFile == aOther.mMutableFile &&
         this->mFileInfo == aOther.mFileInfo &&
         this->mType == aOther.mType;
}

inline
StructuredCloneReadInfo::StructuredCloneReadInfo(JS::StructuredCloneScope aScope)
  : mData(aScope)
  , mDatabase(nullptr)
  , mHasPreprocessInfo(false)
{
  MOZ_COUNT_CTOR(StructuredCloneReadInfo);
}

inline
StructuredCloneReadInfo::StructuredCloneReadInfo()
 : StructuredCloneReadInfo(JS::StructuredCloneScope::DifferentProcessForIndexedDB)
{
}

inline
StructuredCloneReadInfo::StructuredCloneReadInfo(
                             StructuredCloneReadInfo&& aCloneReadInfo)
  : mData(Move(aCloneReadInfo.mData))
{
  MOZ_ASSERT(&aCloneReadInfo != this);
  MOZ_COUNT_CTOR(StructuredCloneReadInfo);

  mFiles.Clear();
  mFiles.SwapElements(aCloneReadInfo.mFiles);
  mDatabase = aCloneReadInfo.mDatabase;
  aCloneReadInfo.mDatabase = nullptr;
  mHasPreprocessInfo = aCloneReadInfo.mHasPreprocessInfo;
  aCloneReadInfo.mHasPreprocessInfo = false;
}

inline
StructuredCloneReadInfo::StructuredCloneReadInfo(
                             SerializedStructuredCloneReadInfo&& aCloneReadInfo)
  : mData(Move(aCloneReadInfo.data().data))
  , mDatabase(nullptr)
  , mHasPreprocessInfo(aCloneReadInfo.hasPreprocessInfo())
{
  MOZ_COUNT_CTOR(StructuredCloneReadInfo);
}

inline
StructuredCloneReadInfo::~StructuredCloneReadInfo()
{
  MOZ_COUNT_DTOR(StructuredCloneReadInfo);
}

inline StructuredCloneReadInfo&
StructuredCloneReadInfo::operator=(StructuredCloneReadInfo&& aCloneReadInfo)
{
  MOZ_ASSERT(&aCloneReadInfo != this);

  mData = Move(aCloneReadInfo.mData);
  mFiles.Clear();
  mFiles.SwapElements(aCloneReadInfo.mFiles);
  mDatabase = aCloneReadInfo.mDatabase;
  aCloneReadInfo.mDatabase = nullptr;
  mHasPreprocessInfo = aCloneReadInfo.mHasPreprocessInfo;
  aCloneReadInfo.mHasPreprocessInfo = false;
  return *this;
}

inline size_t
StructuredCloneReadInfo::Size() const
{
  size_t size = mData.Size();

  for (uint32_t i = 0, count = mFiles.Length(); i < count; ++i) {
    // We don't want to calculate the size of files and so on, because are mainly
    // file descriptors.
    size += sizeof(uint64_t);
  }

  return size;
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // IndexedDatabaseInlines_h
