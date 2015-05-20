/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

class PIBlobImplSnapshot : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(FILEIMPLSNAPSHOT_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(PIBlobImplSnapshot, FILEIMPLSNAPSHOT_IID)

namespace mozilla {
namespace dom {

class MetadataParameters;

namespace indexedDB {

class IDBFileHandle;

class BlobImplSnapshot final
  : public BlobImplBase
  , public PIBlobImplSnapshot
{
  typedef mozilla::dom::MetadataParameters MetadataParameters;

  nsCOMPtr<nsIFile> mFile;
  nsWeakPtr mFileHandle;

  bool mWholeFile;

public:
  // Create as a stored file
  BlobImplSnapshot(const nsAString& aName,
                   const nsAString& aContentType,
                   MetadataParameters* aMetadataParams,
                   nsIFile* aFile,
                   IDBFileHandle* aFileHandle,
                   FileInfo* aFileInfo);

  NS_DECL_ISUPPORTS_INHERITED

private:
  // Create slice
  BlobImplSnapshot(const BlobImplSnapshot* aOther,
                   uint64_t aStart,
                   uint64_t aLength,
                   const nsAString& aContentType);

  ~BlobImplSnapshot();

  static void
  AssertSanity()
#ifdef DEBUG
  ;
#else
  { }
#endif

  virtual void
  GetMozFullPathInternal(nsAString& aFullPath, ErrorResult& aRv) override;

  virtual void
  GetInternalStream(nsIInputStream** aStream, ErrorResult& aRv) override;

  virtual bool MayBeClonedToOtherThreads() const override
  {
    return false;
  }

  virtual already_AddRefed<BlobImpl>
  CreateSlice(uint64_t aStart,
              uint64_t aLength,
              const nsAString& aContentType,
              ErrorResult& aRv) override;

  virtual bool
  IsStoredFile() const override;

  virtual bool
  IsWholeFile() const override;

  virtual bool
  IsSnapshot() const override;
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_filesnapshot_h__
