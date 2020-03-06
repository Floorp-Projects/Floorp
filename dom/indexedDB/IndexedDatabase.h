/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddatabase_h__
#define mozilla_dom_indexeddatabase_h__

#include "js/StructuredClone.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

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

  RefPtr<Blob> mBlob;
  RefPtr<IDBMutableFile> mMutableFile;
  RefPtr<FileInfo> mFileInfo;
  FileType mType;

  // In IndexedDatabaseInlines.h
  inline explicit StructuredCloneFile(FileType aType, RefPtr<Blob> aBlob = {});

  // In IndexedDatabaseInlines.h
  inline explicit StructuredCloneFile(RefPtr<IDBMutableFile> aMutableFile);

  // In IndexedDatabaseInlines.h
  inline ~StructuredCloneFile();

  // In IndexedDatabaseInlines.h
  inline bool operator==(const StructuredCloneFile& aOther) const;
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
