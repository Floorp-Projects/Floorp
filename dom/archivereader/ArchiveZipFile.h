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
 * ArchiveZipFileImpl to DOMFileImpl
 */
class ArchiveZipFileImpl : public DOMFileImplBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  ArchiveZipFileImpl(const nsAString& aName,
                     const nsAString& aContentType,
                     uint64_t aLength,
                     ZipCentral& aCentral,
                     ArchiveReader* aReader)
  : DOMFileImplBase(aName, aContentType, aLength),
    mCentral(aCentral),
    mArchiveReader(aReader),
    mFilename(aName)
  {
    NS_ASSERTION(mArchiveReader, "must have a reader");
    MOZ_COUNT_CTOR(ArchiveZipFileImpl);
  }

  ArchiveZipFileImpl(const nsAString& aName,
                     const nsAString& aContentType,
                     uint64_t aStart,
                     uint64_t aLength,
                     ZipCentral& aCentral,
                     ArchiveReader* aReader)
  : DOMFileImplBase(aContentType, aStart, aLength),
    mCentral(aCentral),
    mArchiveReader(aReader),
    mFilename(aName)
  {
    NS_ASSERTION(mArchiveReader, "must have a reader");
    MOZ_COUNT_CTOR(ArchiveZipFileImpl);
  }

  // Overrides:
  virtual nsresult GetInternalStream(nsIInputStream**) MOZ_OVERRIDE;

  virtual void Unlink() MOZ_OVERRIDE;
  virtual void Traverse(nsCycleCollectionTraversalCallback &aCb) MOZ_OVERRIDE;

  virtual bool IsCCed() const MOZ_OVERRIDE
  {
    return true;
  }

protected:
  virtual ~ArchiveZipFileImpl()
  {
    MOZ_COUNT_DTOR(ArchiveZipFileImpl);
  }

  virtual already_AddRefed<DOMFileImpl>
  CreateSlice(uint64_t aStart, uint64_t aLength, const nsAString& aContentType,
              ErrorResult& aRv) MOZ_OVERRIDE;

private: // Data
  ZipCentral mCentral;
  nsRefPtr<ArchiveReader> mArchiveReader;

  nsString mFilename;
};

END_ARCHIVEREADER_NAMESPACE

#endif // mozilla_dom_archivereader_domarchivefile_h__
