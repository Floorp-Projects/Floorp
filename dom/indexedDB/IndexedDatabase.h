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

struct StructuredCloneFile
{
  RefPtr<Blob> mBlob;
  RefPtr<IDBMutableFile> mMutableFile;
  RefPtr<FileInfo> mFileInfo;
  bool mMutable;

  // In IndexedDatabaseInlines.h
  inline
  StructuredCloneFile();

  // In IndexedDatabaseInlines.h
  inline
  ~StructuredCloneFile();

  // In IndexedDatabaseInlines.h
  inline bool
  operator==(const StructuredCloneFile& aOther) const;
};

struct StructuredCloneReadInfo
{
  JSStructuredCloneData mData;
  nsTArray<StructuredCloneFile> mFiles;
  IDBDatabase* mDatabase;

  // In IndexedDatabaseInlines.h
  inline
  StructuredCloneReadInfo();

  // In IndexedDatabaseInlines.h
  inline
  ~StructuredCloneReadInfo();

  // In IndexedDatabaseInlines.h
  inline StructuredCloneReadInfo&
  operator=(StructuredCloneReadInfo&& aOther);

  // In IndexedDatabaseInlines.h
  inline
  MOZ_IMPLICIT StructuredCloneReadInfo(SerializedStructuredCloneReadInfo&& aOther);
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddatabase_h__
