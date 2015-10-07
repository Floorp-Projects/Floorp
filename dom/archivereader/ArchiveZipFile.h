/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_archivereader_domarchivefile_h__
#define mozilla_dom_archivereader_domarchivefile_h__

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/File.h"

#include "ArchiveReader.h"

#include "ArchiveReaderCommon.h"
#include "zipstruct.h"

BEGIN_ARCHIVEREADER_NAMESPACE

/**
 * ArchiveZipBlobImpl to BlobImpl
 */
class ArchiveZipBlobImpl : public BlobImplBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  ArchiveZipBlobImpl(const nsAString& aName,
                     const nsAString& aContentType,
                     uint64_t aLength,
                     ZipCentral& aCentral,
                     BlobImpl* aBlobImpl)
  : BlobImplBase(aName, aContentType, aLength),
    mCentral(aCentral),
    mBlobImpl(aBlobImpl),
    mFilename(aName)
  {
    MOZ_ASSERT(mBlobImpl);
    MOZ_COUNT_CTOR(ArchiveZipBlobImpl);
  }

  ArchiveZipBlobImpl(const nsAString& aName,
                     const nsAString& aContentType,
                     uint64_t aStart,
                     uint64_t aLength,
                     ZipCentral& aCentral,
                     BlobImpl* aBlobImpl)
  : BlobImplBase(aContentType, aStart, aLength),
    mCentral(aCentral),
    mBlobImpl(aBlobImpl),
    mFilename(aName)
  {
    MOZ_ASSERT(mBlobImpl);
    MOZ_COUNT_CTOR(ArchiveZipBlobImpl);
  }

  // Overrides:
  virtual void GetInternalStream(nsIInputStream** aInputStream,
                                 ErrorResult& aRv) override;

protected:
  virtual ~ArchiveZipBlobImpl()
  {
    MOZ_COUNT_DTOR(ArchiveZipBlobImpl);
  }

  virtual already_AddRefed<BlobImpl>
  CreateSlice(uint64_t aStart, uint64_t aLength, const nsAString& aContentType,
              mozilla::ErrorResult& aRv) override;

private: // Data
  ZipCentral mCentral;
  nsRefPtr<BlobImpl> mBlobImpl;

  nsString mFilename;
};

END_ARCHIVEREADER_NAMESPACE

#endif // mozilla_dom_archivereader_domarchivefile_h__
