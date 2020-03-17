/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddatabase_h__
#define mozilla_dom_indexeddatabase_h__

#include "js/StructuredClone.h"
#include "mozilla/Variant.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "InitializedOnce.h"
#include "FileInfoFwd.h"

namespace mozilla {
namespace dom {

class Blob;
class IDBDatabase;
class IDBMutableFile;

namespace indexedDB {

class SerializedStructuredCloneReadInfo;

struct StructuredCloneFile {
  enum FileType {
    eBlob,
    eMutableFile,
    eStructuredClone,
    eWasmBytecode,
    eWasmCompiled,
    eEndGuard
  };

  StructuredCloneFile(const StructuredCloneFile&) = delete;
  StructuredCloneFile& operator=(const StructuredCloneFile&) = delete;
#ifdef NS_BUILD_REFCNT_LOGGING
  // In IndexedDatabaseInlines.h
  StructuredCloneFile(StructuredCloneFile&&);
#else
  StructuredCloneFile(StructuredCloneFile&&) = default;
#endif
  StructuredCloneFile& operator=(StructuredCloneFile&&) = delete;

  // In IndexedDatabaseInlines.h
  inline explicit StructuredCloneFile(FileType aType);

  // In IndexedDatabaseInlines.h
  inline StructuredCloneFile(FileType aType, RefPtr<Blob> aBlob);

  // In IndexedDatabaseInlines.h
  inline StructuredCloneFile(FileType aType, RefPtr<FileInfo> aFileInfo);

  // In IndexedDatabaseInlines.h
  inline explicit StructuredCloneFile(RefPtr<IDBMutableFile> aMutableFile);

  // In IndexedDatabaseInlines.h
  ~StructuredCloneFile();

  // In IndexedDatabaseInlines.h
  inline bool operator==(const StructuredCloneFile& aOther) const;

  // XXX This is only needed for a schema upgrade (UpgradeSchemaFrom19_0To20_0).
  // If support for older schemas is dropped, we can probably remove this method
  // and make mType const.
  void MutateType(FileType aNewType) { mType = aNewType; }

  FileType Type() const { return mType; }

  const indexedDB::FileInfo& FileInfo() const {
    return *mContents->as<RefPtr<indexedDB::FileInfo>>();
  }

  // In IndexedDatabaseInlines.h
  RefPtr<indexedDB::FileInfo> FileInfoPtr() const;

  const dom::Blob& Blob() const { return *mContents->as<RefPtr<dom::Blob>>(); }

  // XXX This is currently used for a number of reasons. Bug 1620560 will remove
  // the need for one of them, but the uses of do_GetWeakReference in
  // IDBDatabase::GetOrCreateFileActorForBlob and WrapAsJSObject in
  // CopyingStructuredCloneReadCallback are probably harder to change.
  dom::Blob& MutableBlob() const { return *mContents->as<RefPtr<dom::Blob>>(); }

  // In IndexedDatabaseInlines.h
  inline RefPtr<dom::Blob> BlobPtr() const;

  bool HasBlob() const { return mContents->is<RefPtr<dom::Blob>>(); }

  const IDBMutableFile& MutableFile() const {
    return *mContents->as<RefPtr<IDBMutableFile>>();
  }

  IDBMutableFile& MutableMutableFile() const {
    return *mContents->as<RefPtr<IDBMutableFile>>();
  }

  bool HasMutableFile() const {
    return mContents->is<RefPtr<IDBMutableFile>>();
  }

 private:
  InitializedOnce<
      const Variant<Nothing, RefPtr<dom::Blob>, RefPtr<IDBMutableFile>,
                    RefPtr<indexedDB::FileInfo>>>
      mContents;
  FileType mType;
};

struct StructuredCloneReadInfo {
  // In IndexedDatabaseInlines.h
  explicit StructuredCloneReadInfo(JS::StructuredCloneScope aScope);

  // In IndexedDatabaseInlines.h
  StructuredCloneReadInfo();

  // In IndexedDatabaseInlines.h
  StructuredCloneReadInfo(JSStructuredCloneData&& aData,
                          nsTArray<StructuredCloneFile> aFiles);

#ifdef NS_BUILD_REFCNT_LOGGING
  // In IndexedDatabaseInlines.h
  ~StructuredCloneReadInfo();

  // In IndexedDatabaseInlines.h
  //
  // This custom implementation of the move ctor is only necessary because of
  // MOZ_COUNT_CTOR. It is less efficient as the compiler-generated move ctor,
  // since it unnecessarily clears elements on the source.
  StructuredCloneReadInfo(StructuredCloneReadInfo&& aOther) noexcept;
#else
  StructuredCloneReadInfo(StructuredCloneReadInfo&& aOther) = default;
#endif
  StructuredCloneReadInfo& operator=(StructuredCloneReadInfo&& aOther) =
      default;

  StructuredCloneReadInfo(const StructuredCloneReadInfo& aOther) = delete;
  StructuredCloneReadInfo& operator=(const StructuredCloneReadInfo& aOther) =
      delete;

  // In IndexedDatabaseInlines.h
  size_t Size() const;

  const JSStructuredCloneData& Data() const { return mData; }
  JSStructuredCloneData ReleaseData() { return std::move(mData); }

  // XXX This is only needed for a schema upgrade (UpgradeSchemaFrom19_0To20_0).
  // If support for older schemas is dropped, we can probably remove this method
  // and make mFiles InitializedOnce.
  StructuredCloneFile& MutableFile(const size_t aIndex) {
    return mFiles[aIndex];
  }
  const nsTArray<StructuredCloneFile>& Files() const { return mFiles; }

  nsTArray<StructuredCloneFile> ReleaseFiles() { return std::move(mFiles); }

  bool HasFiles() const { return !mFiles.IsEmpty(); }

 private:
  JSStructuredCloneData mData;
  nsTArray<StructuredCloneFile> mFiles;
};

struct StructuredCloneReadInfoChild : StructuredCloneReadInfo {
  inline StructuredCloneReadInfoChild(JSStructuredCloneData&& aData,
                                      nsTArray<StructuredCloneFile> aFiles,
                                      IDBDatabase* aDatabase);

  IDBDatabase* Database() const { return mDatabase; }

 private:
  IDBDatabase* mDatabase;
};

// This is only defined in the header file to satisfy the clang-plugin static
// analysis, it could be placed in ActorsParent.cpp otherwise.
struct StructuredCloneReadInfoParent : StructuredCloneReadInfo {
  StructuredCloneReadInfoParent(JSStructuredCloneData&& aData,
                                nsTArray<StructuredCloneFile> aFiles,
                                bool aHasPreprocessInfo)
      : StructuredCloneReadInfo{std::move(aData), std::move(aFiles)},
        mHasPreprocessInfo{aHasPreprocessInfo} {}

  bool HasPreprocessInfo() const { return mHasPreprocessInfo; }

 private:
  bool mHasPreprocessInfo;
};

JSObject* CommonStructuredCloneReadCallback(
    JSContext* aCx, JSStructuredCloneReader* aReader,
    const JS::CloneDataPolicy& aCloneDataPolicy, uint32_t aTag, uint32_t aData,
    StructuredCloneReadInfo* aCloneReadInfo, IDBDatabase* aDatabase);

template <typename StructuredCloneReadInfoType>
JSObject* StructuredCloneReadCallback(
    JSContext* aCx, JSStructuredCloneReader* aReader,
    const JS::CloneDataPolicy& aCloneDataPolicy, uint32_t aTag, uint32_t aData,
    void* aClosure);

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

DECLARE_USE_COPY_CONSTRUCTORS(mozilla::dom::indexedDB::StructuredCloneReadInfo);
DECLARE_USE_COPY_CONSTRUCTORS(
    mozilla::dom::indexedDB::StructuredCloneReadInfoChild);
DECLARE_USE_COPY_CONSTRUCTORS(
    mozilla::dom::indexedDB::StructuredCloneReadInfoParent);

#endif  // mozilla_dom_indexeddatabase_h__
