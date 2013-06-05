/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_file_h__
#define mozilla_dom_file_file_h__

#include "mozilla/Attributes.h"
#include "FileCommon.h"

#include "nsDOMFile.h"

#include "LockedFile.h"

BEGIN_FILE_NAMESPACE

class File : public nsDOMFileCC
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(File, nsDOMFileCC)

  // Create as a file
  File(const nsAString& aName, const nsAString& aContentType,
       uint64_t aLength, nsIFile* aFile, LockedFile* aLockedFile)
  : nsDOMFileCC(aName, aContentType, aLength),
    mFile(aFile), mLockedFile(aLockedFile),
    mWholeFile(true), mStoredFile(false)
  {
    NS_ASSERTION(mFile, "Null file!");
    NS_ASSERTION(mLockedFile, "Null locked file!");
  }

  // Create as a stored file
  File(const nsAString& aName, const nsAString& aContentType,
       uint64_t aLength, nsIFile* aFile, LockedFile* aLockedFile,
       FileInfo* aFileInfo)
  : nsDOMFileCC(aName, aContentType, aLength),
    mFile(aFile), mLockedFile(aLockedFile),
    mWholeFile(true), mStoredFile(true)
  {
    NS_ASSERTION(mFile, "Null file!");
    NS_ASSERTION(mLockedFile, "Null locked file!");
    mFileInfos.AppendElement(aFileInfo);
  }

  // Overrides
  NS_IMETHOD
  GetMozFullPathInternal(nsAString& aFullPath) MOZ_OVERRIDE;

  NS_IMETHOD
  GetInternalStream(nsIInputStream** aStream) MOZ_OVERRIDE;

protected:
  // Create slice
  File(const File* aOther, uint64_t aStart, uint64_t aLength,
       const nsAString& aContentType);

  virtual ~File()
  { }

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
  nsRefPtr<LockedFile> mLockedFile;

  bool mWholeFile;
  bool mStoredFile;
};

END_FILE_NAMESPACE

#endif // mozilla_dom_file_file_h__
