/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_domarchivefile_h__
#define mozilla_dom_file_domarchivefile_h__

#include "nsDOMFile.h"

#include "ArchiveReader.h"

#include "FileCommon.h"
#include "zipstruct.h"

BEGIN_FILE_NAMESPACE

class ArchiveZipFile : public nsDOMFileCC
{
public:
  ArchiveZipFile(const nsAString& aName,
                 const nsAString& aContentType,
                 PRUint64 aLength,
                 ZipCentral& aCentral,
                 ArchiveReader* aReader)
  : nsDOMFileCC(aName, aContentType, aLength),
    mCentral(aCentral),
    mArchiveReader(aReader),
    mFilename(aName)
  {
    NS_ASSERTION(mArchiveReader, "must have a reader");
  }

  ArchiveZipFile(const nsAString& aName,
                 const nsAString& aContentType,
                 PRUint64 aStart,
                 PRUint64 aLength,
                 ZipCentral& aCentral,
                 ArchiveReader* aReader)
  : nsDOMFileCC(aContentType, aStart, aLength),
    mCentral(aCentral),
    mArchiveReader(aReader),
    mFilename(aName)
  {
    NS_ASSERTION(mArchiveReader, "must have a reader");
  }

  // Overrides:
  NS_IMETHOD GetInternalStream(nsIInputStream**);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(ArchiveZipFile, nsIDOMFile)

protected:
  virtual already_AddRefed<nsIDOMBlob> CreateSlice(PRUint64 aStart,
                                                   PRUint64 aLength,
                                                   const nsAString& aContentType);

private: // Data
  ZipCentral mCentral;
  nsRefPtr<ArchiveReader> mArchiveReader;

  nsString mFilename;
};

END_FILE_NAMESPACE

#endif // mozilla_dom_file_domarchivefile_h__
