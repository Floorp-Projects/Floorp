/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddatabase_h__
#define mozilla_dom_indexeddatabase_h__

#include "js/StructuredClone.h"
#include "mozilla/InitializedOnce.h"
#include "mozilla/Variant.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "FileInfoFwd.h"
#include "SafeRefPtr.h"

namespace mozilla {
namespace dom {

class Blob;
class IDBDatabase;
class IDBMutableFile;

namespace indexedDB {

class SerializedStructuredCloneReadInfo;

struct StructuredCloneFileBase {
  enum FileType {
    eBlob,
    eMutableFile,
    eStructuredClone,
    eWasmBytecode,
    eWasmCompiled,
    eEndGuard
  };

  FileType Type() const { return mType; }

 protected:
  explicit StructuredCloneFileBase(FileType aType) : mType{aType} {}

  FileType mType;
};

struct StructuredCloneFileChild : StructuredCloneFileBase {
  StructuredCloneFileChild(const StructuredCloneFileChild&) = delete;
  StructuredCloneFileChild& operator=(const StructuredCloneFileChild&) = delete;
#ifdef NS_BUILD_REFCNT_LOGGING
  // In IndexedDatabaseInlines.h
  StructuredCloneFileChild(StructuredCloneFileChild&&);
#else
  StructuredCloneFileChild(StructuredCloneFileChild&&) = default;
#endif
  StructuredCloneFileChild& operator=(StructuredCloneFileChild&&) = delete;

  // In IndexedDatabaseInlines.h
  ~StructuredCloneFileChild();

  // In IndexedDatabaseInlines.h
  explicit StructuredCloneFileChild(FileType aType);

  // In IndexedDatabaseInlines.h
  StructuredCloneFileChild(FileType aType, RefPtr<Blob> aBlob);

  // In IndexedDatabaseInlines.h
  explicit StructuredCloneFileChild(RefPtr<IDBMutableFile> aMutableFile);

  const dom::Blob& Blob() const { return *mContents->as<RefPtr<dom::Blob>>(); }

  // XXX This is currently used for a number of reasons. Bug 1620560 will remove
  // the need for one of them, but the uses of do_GetWeakReference in
  // IDBDatabase::GetOrCreateFileActorForBlob and WrapAsJSObject in
  // CopyingStructuredCloneReadCallback are probably harder to change.
  dom::Blob& MutableBlob() const { return *mContents->as<RefPtr<dom::Blob>>(); }

  // In IndexedDatabaseInlines.h
  RefPtr<dom::Blob> BlobPtr() const;

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
      const Variant<Nothing, RefPtr<dom::Blob>, RefPtr<IDBMutableFile>>>
      mContents;
};

struct StructuredCloneFileParent : StructuredCloneFileBase {
  StructuredCloneFileParent(const StructuredCloneFileParent&) = delete;
  StructuredCloneFileParent& operator=(const StructuredCloneFileParent&) =
      delete;
#ifdef NS_BUILD_REFCNT_LOGGING
  // In IndexedDatabaseInlines.h
  StructuredCloneFileParent(StructuredCloneFileParent&&);
#else
  StructuredCloneFileParent(StructuredCloneFileParent&&) = default;
#endif
  StructuredCloneFileParent& operator=(StructuredCloneFileParent&&) = delete;

  // In IndexedDatabaseInlines.h
  StructuredCloneFileParent(FileType aType, SafeRefPtr<FileInfo> aFileInfo);

  // In IndexedDatabaseInlines.h
  ~StructuredCloneFileParent();

  // XXX This is used for a schema upgrade hack in UpgradeSchemaFrom19_0To20_0.
  // When this is eventually removed, this function can be removed, and mType
  // can be declared const in the base class.
  void MutateType(FileType aNewType) { mType = aNewType; }

  const indexedDB::FileInfo& FileInfo() const { return ***mContents; }

  // In IndexedDatabaseInlines.h
  SafeRefPtr<indexedDB::FileInfo> FileInfoPtr() const;

 private:
  InitializedOnce<const Maybe<SafeRefPtr<indexedDB::FileInfo>>> mContents;
};

struct StructuredCloneReadInfoBase {
  // In IndexedDatabaseInlines.h
  explicit StructuredCloneReadInfoBase(JSStructuredCloneData&& aData)
      : mData{std::move(aData)} {}

  const JSStructuredCloneData& Data() const { return mData; }
  JSStructuredCloneData ReleaseData() { return std::move(mData); }

 private:
  JSStructuredCloneData mData;
};

template <typename StructuredCloneFileT>
struct StructuredCloneReadInfo : StructuredCloneReadInfoBase {
  using StructuredCloneFile = StructuredCloneFileT;

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
  nsTArray<StructuredCloneFile> mFiles;
};

struct StructuredCloneReadInfoChild
    : StructuredCloneReadInfo<StructuredCloneFileChild> {
  inline StructuredCloneReadInfoChild(JSStructuredCloneData&& aData,
                                      nsTArray<StructuredCloneFileChild> aFiles,
                                      IDBDatabase* aDatabase);

  IDBDatabase* Database() const { return mDatabase; }

 private:
  IDBDatabase* mDatabase;
};

// This is only defined in the header file to satisfy the clang-plugin static
// analysis, it could be placed in ActorsParent.cpp otherwise.
struct StructuredCloneReadInfoParent
    : StructuredCloneReadInfo<StructuredCloneFileParent> {
  StructuredCloneReadInfoParent(JSStructuredCloneData&& aData,
                                nsTArray<StructuredCloneFileParent> aFiles,
                                bool aHasPreprocessInfo)
      : StructuredCloneReadInfo{std::move(aData), std::move(aFiles)},
        mHasPreprocessInfo{aHasPreprocessInfo} {}

  bool HasPreprocessInfo() const { return mHasPreprocessInfo; }

 private:
  bool mHasPreprocessInfo;
};

template <typename StructuredCloneReadInfo>
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

MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(
    mozilla::dom::indexedDB::StructuredCloneReadInfo<
        mozilla::dom::indexedDB::StructuredCloneFileChild>);
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(
    mozilla::dom::indexedDB::StructuredCloneReadInfo<
        mozilla::dom::indexedDB::StructuredCloneFileParent>);
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(
    mozilla::dom::indexedDB::StructuredCloneReadInfoChild);
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(
    mozilla::dom::indexedDB::StructuredCloneReadInfoParent);

#endif  // mozilla_dom_indexeddatabase_h__
