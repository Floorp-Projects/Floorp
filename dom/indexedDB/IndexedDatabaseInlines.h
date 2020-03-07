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

inline StructuredCloneFile::StructuredCloneFile(FileType aType)
    : mContents{Nothing()}, mType{aType} {
  MOZ_COUNT_CTOR(StructuredCloneFile);
}

inline StructuredCloneFile::StructuredCloneFile(FileType aType,
                                                RefPtr<dom::Blob> aBlob)
    : mContents{std::move(aBlob)}, mType{aType} {
  MOZ_ASSERT(eBlob == aType || eStructuredClone == aType);
  MOZ_ASSERT(mContents->as<RefPtr<dom::Blob>>());
  MOZ_COUNT_CTOR(StructuredCloneFile);
}

inline StructuredCloneFile::StructuredCloneFile(
    FileType aType, RefPtr<indexedDB::FileInfo> aFileInfo)
    : mContents{std::move(aFileInfo)}, mType{aType} {
  MOZ_ASSERT(mContents->as<RefPtr<indexedDB::FileInfo>>());
  MOZ_COUNT_CTOR(StructuredCloneFile);
}

inline StructuredCloneFile::StructuredCloneFile(
    RefPtr<IDBMutableFile> aMutableFile)
    : mContents{std::move(aMutableFile)}, mType{eMutableFile} {
  MOZ_ASSERT(mContents->as<RefPtr<IDBMutableFile>>());
  MOZ_COUNT_CTOR(StructuredCloneFile);
}

#ifdef NS_BUILD_REFCNT_LOGGING
inline StructuredCloneFile::StructuredCloneFile(StructuredCloneFile&& aOther)
    : mContents{std::move(aOther.mContents)}, mType{aOther.mType} {
  MOZ_COUNT_CTOR(StructuredCloneFile);
}
#endif

inline StructuredCloneFile::~StructuredCloneFile() {
  MOZ_COUNT_DTOR(StructuredCloneFile);
}

inline RefPtr<indexedDB::FileInfo> StructuredCloneFile::FileInfoPtr() const {
  return mContents->as<RefPtr<indexedDB::FileInfo>>();
}

inline RefPtr<dom::Blob> StructuredCloneFile::BlobPtr() const {
  return mContents->as<RefPtr<dom::Blob>>();
}

inline bool StructuredCloneFile::operator==(
    const StructuredCloneFile& aOther) const {
  return this->mType == aOther.mType && *this->mContents == *aOther.mContents;
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
