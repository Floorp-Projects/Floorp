/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_filesnapshot_h__
#define mozilla_dom_indexeddb_filesnapshot_h__

#include "mozilla/Attributes.h"
#include "mozilla/dom/File.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWeakPtr.h"

#define FILEIMPLSNAPSHOT_IID \
  {0x0dfc11b1, 0x75d3, 0x473b, {0x8c, 0x67, 0xb7, 0x23, 0xf4, 0x67, 0xd6, 0x73}}

class PIFileImplSnapshot : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(FILEIMPLSNAPSHOT_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(PIFileImplSnapshot, FILEIMPLSNAPSHOT_IID)

namespace mozilla {
namespace dom {

class MetadataParameters;

namespace indexedDB {

class IDBFileHandle;

class FileImplSnapshot MOZ_FINAL
  : public FileImplBase
  , public PIFileImplSnapshot
{
  typedef mozilla::dom::MetadataParameters MetadataParameters;

  nsCOMPtr<nsIFile> mFile;
  nsWeakPtr mFileHandle;

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

  virtual void
  GetMozFullPathInternal(nsAString& aFullPath, ErrorResult& aRv) MOZ_OVERRIDE;

  virtual nsresult
  GetInternalStream(nsIInputStream** aStream) MOZ_OVERRIDE;

  virtual bool MayBeClonedToOtherThreads() const MOZ_OVERRIDE
  {
    return false;
  }

  virtual already_AddRefed<FileImpl>
  CreateSlice(uint64_t aStart,
              uint64_t aLength,
              const nsAString& aContentType,
              ErrorResult& aRv) MOZ_OVERRIDE;

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
