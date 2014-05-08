/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_archivereader_domarchivefile_h__
#define mozilla_dom_archivereader_domarchivefile_h__

#include "mozilla/Attributes.h"
#include "nsDOMFile.h"

#include "ArchiveReader.h"

#include "ArchiveReaderCommon.h"
#include "zipstruct.h"

BEGIN_ARCHIVEREADER_NAMESPACE

/**
 * ZipFile to DOMFileCC
 */
class ArchiveZipFile : public nsDOMFileCC
{
public:
  ArchiveZipFile(const nsAString& aName,
                 const nsAString& aContentType,
                 uint64_t aLength,
                 ZipCentral& aCentral,
                 ArchiveReader* aReader)
  : nsDOMFileCC(aName, aContentType, aLength),
    mCentral(aCentral),
    mArchiveReader(aReader),
    mFilename(aName)
  {
    NS_ASSERTION(mArchiveReader, "must have a reader");
    MOZ_COUNT_CTOR(ArchiveZipFile);
  }

  ArchiveZipFile(const nsAString& aName,
                 const nsAString& aContentType,
                 uint64_t aStart,
                 uint64_t aLength,
                 ZipCentral& aCentral,
                 ArchiveReader* aReader)
  : nsDOMFileCC(aContentType, aStart, aLength),
    mCentral(aCentral),
    mArchiveReader(aReader),
    mFilename(aName)
  {
    NS_ASSERTION(mArchiveReader, "must have a reader");
    MOZ_COUNT_CTOR(ArchiveZipFile);
  }

  virtual ~ArchiveZipFile()
  {
    MOZ_COUNT_DTOR(ArchiveZipFile);
  }

  // Overrides:
  NS_IMETHOD GetInternalStream(nsIInputStream**) MOZ_OVERRIDE;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ArchiveZipFile, nsDOMFileCC)

protected:
  virtual already_AddRefed<nsIDOMBlob> CreateSlice(uint64_t aStart,
                                                   uint64_t aLength,
                                                   const nsAString& aContentType) MOZ_OVERRIDE;

private: // Data
  ZipCentral mCentral;
  nsRefPtr<ArchiveReader> mArchiveReader;

  nsString mFilename;
};

END_ARCHIVEREADER_NAMESPACE

#endif // mozilla_dom_archivereader_domarchivefile_h__
