/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IndexedDatabaseInlines_h
#define IndexedDatabaseInlines_h

#ifndef mozilla_dom_indexeddatabase_h__
#  error Must include IndexedDatabase.h first
#endif

#include "FileInfo.h"
#include "IDBMutableFile.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBSharedTypes.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/File.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

inline StructuredCloneFile::StructuredCloneFile(FileType aType,
                                                RefPtr<Blob> aBlob)
    : mBlob{std::move(aBlob)}, mType{aType} {
  MOZ_COUNT_CTOR(StructuredCloneFile);
}

inline StructuredCloneFile::StructuredCloneFile(
    RefPtr<IDBMutableFile> aMutableFile)
    : mMutableFile{std::move(aMutableFile)}, mType{eMutableFile} {
  MOZ_COUNT_CTOR(StructuredCloneFile);
}

inline StructuredCloneFile::~StructuredCloneFile() {
  MOZ_COUNT_DTOR(StructuredCloneFile);
}

inline bool StructuredCloneFile::operator==(
    const StructuredCloneFile& aOther) const {
  return this->mBlob == aOther.mBlob &&
         this->mMutableFile == aOther.mMutableFile &&
         this->mFileInfo == aOther.mFileInfo && this->mType == aOther.mType;
}

inline StructuredCloneReadInfo::StructuredCloneReadInfo(
    JS::StructuredCloneScope aScope)
    : mData(aScope), mDatabase(nullptr), mHasPreprocessInfo(false) {
  MOZ_COUNT_CTOR(StructuredCloneReadInfo);
}

inline StructuredCloneReadInfo::StructuredCloneReadInfo()
    : StructuredCloneReadInfo(
          JS::StructuredCloneScope::DifferentProcessForIndexedDB) {}

inline StructuredCloneReadInfo::StructuredCloneReadInfo(
    JSStructuredCloneData&& aData, nsTArray<StructuredCloneFile> aFiles,
    IDBDatabase* aDatabase, bool aHasPreprocessInfo)
    : mData{std::move(aData)},
      mFiles{std::move(aFiles)},
      mDatabase{aDatabase},
      mHasPreprocessInfo{aHasPreprocessInfo} {
  MOZ_COUNT_CTOR(StructuredCloneReadInfo);
}

#ifdef NS_BUILD_REFCNT_LOGGING
inline StructuredCloneReadInfo::StructuredCloneReadInfo(
    StructuredCloneReadInfo&& aOther) noexcept
    : mData(std::move(aOther.mData)) {
  MOZ_ASSERT(&aOther != this);
  MOZ_COUNT_CTOR(StructuredCloneReadInfo);

  mFiles.Clear();
  mFiles.SwapElements(aOther.mFiles);
  mDatabase = aOther.mDatabase;
  aOther.mDatabase = nullptr;
  mHasPreprocessInfo = aOther.mHasPreprocessInfo;
  aOther.mHasPreprocessInfo = false;
}

inline StructuredCloneReadInfo::~StructuredCloneReadInfo() {
  MOZ_COUNT_DTOR(StructuredCloneReadInfo);
}

inline StructuredCloneReadInfo& StructuredCloneReadInfo::operator=(
    StructuredCloneReadInfo&& aOther) noexcept {
  MOZ_ASSERT(&aOther != this);

  mData = std::move(aOther.mData);
  mFiles.Clear();
  mFiles.SwapElements(aOther.mFiles);
  mDatabase = aOther.mDatabase;
  aOther.mDatabase = nullptr;
  mHasPreprocessInfo = aOther.mHasPreprocessInfo;
  aOther.mHasPreprocessInfo = false;
  return *this;
}
#endif

inline size_t StructuredCloneReadInfo::Size() const {
  size_t size = mData.Size();

  for (uint32_t i = 0, count = mFiles.Length(); i < count; ++i) {
    // We don't want to calculate the size of files and so on, because are
    // mainly file descriptors.
    size += sizeof(uint64_t);
  }

  return size;
}

template <typename E, typename Map>
RefPtr<DOMStringList> CreateSortedDOMStringList(const nsTArray<E>& aArray,
                                                const Map& aMap) {
  auto list = MakeRefPtr<DOMStringList>();

  if (!aArray.IsEmpty()) {
    nsTArray<nsString>& mapped = list->StringArray();
    mapped.SetCapacity(aArray.Length());

    std::transform(aArray.cbegin(), aArray.cend(), MakeBackInserter(mapped),
                   aMap);

    mapped.Sort();
  }

  return list;
}

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#endif  // IndexedDatabaseInlines_h
