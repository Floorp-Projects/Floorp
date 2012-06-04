/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_file_h__
#define mozilla_dom_file_file_h__

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
       PRUint64 aLength, nsIFile* aFile, LockedFile* aLockedFile)
  : nsDOMFileCC(aName, aContentType, aLength),
    mFile(aFile), mLockedFile(aLockedFile),
    mWholeFile(true), mStoredFile(false)
  {
    NS_ASSERTION(mFile, "Null file!");
    NS_ASSERTION(mLockedFile, "Null locked file!");
  }

  // Create as a stored file
  File(const nsAString& aName, const nsAString& aContentType,
       PRUint64 aLength, nsIFile* aFile, LockedFile* aLockedFile,
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
  GetMozFullPathInternal(nsAString& aFullPath);

  NS_IMETHOD
  GetInternalStream(nsIInputStream** aStream);

protected:
  // Create slice
  File(const File* aOther, PRUint64 aStart, PRUint64 aLength,
       const nsAString& aContentType);

  virtual ~File()
  { }

  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(PRUint64 aStart, PRUint64 aLength,
              const nsAString& aContentType);

  virtual bool
  IsStoredFile() const
  {
    return mStoredFile;
  }

  virtual bool
  IsWholeFile() const
  {
    return mWholeFile;
  }

  virtual bool
  IsSnapshot() const
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
