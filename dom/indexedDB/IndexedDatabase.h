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

namespace mozilla {
namespace dom {

class Blob;
class IDBDatabase;
class IDBMutableFile;

namespace indexedDB {

class FileInfo;
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
  inline ~StructuredCloneFile();

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
  JSStructuredCloneData mData;
  nsTArray<StructuredCloneFile> mFiles;
  IDBDatabase* mDatabase;
  bool mHasPreprocessInfo;

  // In IndexedDatabaseInlines.h
  inline explicit StructuredCloneReadInfo(JS::StructuredCloneScope aScope);

  // In IndexedDatabaseInlines.h
  inline StructuredCloneReadInfo();

  // In IndexedDatabaseInlines.h
  inline StructuredCloneReadInfo(JSStructuredCloneData&& aData,
                                 nsTArray<StructuredCloneFile> aFiles,
                                 IDBDatabase* aDatabase,
                                 bool aHasPreprocessInfo);

#ifdef NS_BUILD_REFCNT_LOGGING
  // In IndexedDatabaseInlines.h
  inline ~StructuredCloneReadInfo();

  // In IndexedDatabaseInlines.h
  //
  // This custom implementation of the move ctor is only necessary because of
  // MOZ_COUNT_CTOR. It is less efficient as the compiler-generated move ctor,
  // since it unnecessarily clears elements on the source.
  inline StructuredCloneReadInfo(StructuredCloneReadInfo&& aOther) noexcept;

  // In IndexedDatabaseInlines.h
  //
  // This custom implementation of the move assignment operator is only there
  // for symmetry with the move ctor. It is less efficient as the
  // compiler-generated move assignment operator, since it unnecessarily clears
  // elements on the source.
  inline StructuredCloneReadInfo& operator=(
      StructuredCloneReadInfo&& aOther) noexcept;
#else
  StructuredCloneReadInfo(StructuredCloneReadInfo&& aOther) = default;
  StructuredCloneReadInfo& operator=(StructuredCloneReadInfo&& aOther) =
      default;
#endif

  StructuredCloneReadInfo(const StructuredCloneReadInfo& aOther) = delete;
  StructuredCloneReadInfo& operator=(const StructuredCloneReadInfo& aOther) =
      delete;

  // In IndexedDatabaseInlines.h
  inline size_t Size() const;
};

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_indexeddatabase_h__
