/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_File_h
#define mozilla_dom_File_h

#include "mozilla/Attributes.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMFile.h"

namespace mozilla {
namespace dom {

class FileHandle;
class File;

class FileImpl : public DOMFileImplBase
{
  friend class File;

public:
  // Create as a file
  FileImpl(const nsAString& aName, const nsAString& aContentType,
           uint64_t aLength, nsIFile* aFile, FileHandle* aFileHandle);

  // Create as a stored file
  FileImpl(const nsAString& aName, const nsAString& aContentType,
           uint64_t aLength, nsIFile* aFile, FileHandle* aFileHandle,
           indexedDB::FileInfo* aFileInfo);

  // Overrides
  virtual nsresult GetMozFullPathInternal(nsAString& aFullPath) MOZ_OVERRIDE;

  virtual nsresult GetInternalStream(nsIInputStream** aStream) MOZ_OVERRIDE;

  virtual void Unlink() MOZ_OVERRIDE;
  virtual void Traverse(nsCycleCollectionTraversalCallback &aCb) MOZ_OVERRIDE;

protected:
  // Create slice
  FileImpl(const FileImpl* aOther, uint64_t aStart, uint64_t aLength,
           const nsAString& aContentType);

  virtual ~FileImpl();

  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType) MOZ_OVERRIDE;

  virtual bool
  IsStoredFile() const MOZ_OVERRIDE
  {
    return mStoredFile;
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
  nsRefPtr<FileHandle> mFileHandle;

  bool mWholeFile;
  bool mStoredFile;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_File_h
