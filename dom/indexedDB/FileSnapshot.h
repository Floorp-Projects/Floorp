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
#include "nsCycleCollectionParticipant.h"
#include "nsDOMFile.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

class IDBFileHandle;

class FileImplSnapshot : public DOMFileImplBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  // Create as a stored file
  FileImplSnapshot(const nsAString& aName, const nsAString& aContentType,
                   uint64_t aLength, nsIFile* aFile, IDBFileHandle* aFileHandle,
                   FileInfo* aFileInfo);

  // Overrides
  virtual nsresult
  GetMozFullPathInternal(nsAString& aFullPath) MOZ_OVERRIDE;

  virtual nsresult
  GetInternalStream(nsIInputStream** aStream) MOZ_OVERRIDE;

  virtual void
  Unlink() MOZ_OVERRIDE;

  virtual void
  Traverse(nsCycleCollectionTraversalCallback &aCb) MOZ_OVERRIDE;

  virtual bool
  IsCCed() const MOZ_OVERRIDE
  {
    return true;
  }

protected:
  // Create slice
  FileImplSnapshot(const FileImplSnapshot* aOther, uint64_t aStart,
                   uint64_t aLength, const nsAString& aContentType);

  virtual ~FileImplSnapshot();

  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType) MOZ_OVERRIDE;

  virtual bool
  IsStoredFile() const MOZ_OVERRIDE
  {
    return true;
  }

  virtual bool
  IsWholeFile() const MOZ_OVERRIDE
  {
    return mWholeFile;
  }

  virtual bool
  IsSnapshot() const MOZ_OVERRIDE
  {
    return true;
  }

private:
  nsCOMPtr<nsIFile> mFile;
  nsRefPtr<IDBFileHandle> mFileHandle;

  bool mWholeFile;
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_filesnapshot_h__
