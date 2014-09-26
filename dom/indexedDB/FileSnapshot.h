/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_filesnapshot_h__
#define mozilla_dom_indexeddb_filesnapshot_h__

#include "mozilla/Attributes.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsDOMFile.h"

namespace mozilla {
namespace dom {

class MetadataParameters;

namespace indexedDB {

class IDBFileHandle;

class FileImplSnapshot MOZ_FINAL
  : public DOMFileImplBase
{
  typedef mozilla::dom::MetadataParameters MetadataParameters;

  nsCOMPtr<nsIFile> mFile;
  nsRefPtr<IDBFileHandle> mFileHandle;

  bool mWholeFile;

public:
  // Create as a stored file
  FileImplSnapshot(const nsAString& aName,
                   const nsAString& aContentType,
                   MetadataParameters* aMetadataParams,
                   nsIFile* aFile,
                   IDBFileHandle* aFileHandle,
                   FileInfo* aFileInfo);

  NS_DECL_ISUPPORTS_INHERITED

private:
  // Create slice
  FileImplSnapshot(const FileImplSnapshot* aOther,
                   uint64_t aStart,
                   uint64_t aLength,
                   const nsAString& aContentType);

  ~FileImplSnapshot();

  static void
  AssertSanity()
#ifdef DEBUG
  ;
#else
  { }
#endif

  virtual nsresult
  GetMozFullPathInternal(nsAString& aFullPath) MOZ_OVERRIDE;

  virtual nsresult
  GetInternalStream(nsIInputStream** aStream) MOZ_OVERRIDE;

  virtual void
  Unlink() MOZ_OVERRIDE;

  virtual void
  Traverse(nsCycleCollectionTraversalCallback &aCb) MOZ_OVERRIDE;

  virtual bool
  IsCCed() const MOZ_OVERRIDE;

  virtual already_AddRefed<DOMFileImpl>
  CreateSlice(uint64_t aStart,
              uint64_t aLength,
              const nsAString& aContentType) MOZ_OVERRIDE;

  virtual bool
  IsStoredFile() const MOZ_OVERRIDE;

  virtual bool
  IsWholeFile() const MOZ_OVERRIDE;

  virtual bool
  IsSnapshot() const MOZ_OVERRIDE;
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_filesnapshot_h__
