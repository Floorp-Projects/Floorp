/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IndexedDatabaseInlines_h
#define IndexedDatabaseInlines_h

#ifndef mozilla_dom_indexeddb_indexeddatabase_h__
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
  : mMutable(false)
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
         this->mMutable == aOther.mMutable;
}

inline
StructuredCloneReadInfo::StructuredCloneReadInfo()
  : mDatabase(nullptr)
{
  MOZ_COUNT_CTOR(StructuredCloneReadInfo);
}

inline
StructuredCloneReadInfo::StructuredCloneReadInfo(
                             SerializedStructuredCloneReadInfo&& aCloneReadInfo)
  : mData(Move(aCloneReadInfo.data()))
  , mDatabase(nullptr)
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
  mCloneBuffer = Move(aCloneReadInfo.mCloneBuffer);
  mFiles.Clear();
  mFiles.SwapElements(aCloneReadInfo.mFiles);
  mDatabase = aCloneReadInfo.mDatabase;
  aCloneReadInfo.mDatabase = nullptr;
  return *this;
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // IndexedDatabaseInlines_h
