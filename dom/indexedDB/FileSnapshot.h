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
#include "nsISupports.h"
#include "nsWeakPtr.h"

#define FILEIMPLSNAPSHOT_IID \
  {0x0dfc11b1, 0x75d3, 0x473b, {0x8c, 0x67, 0xb7, 0x23, 0xf4, 0x67, 0xd6, 0x73}}

class PIBlobImplSnapshot : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(FILEIMPLSNAPSHOT_IID)

  virtual mozilla::dom::BlobImpl*
  GetBlobImpl() const = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(PIBlobImplSnapshot, FILEIMPLSNAPSHOT_IID)

namespace mozilla {
namespace dom {

class IDBFileHandle;

namespace indexedDB {

class BlobImplSnapshot final
  : public BlobImpl
  , public PIBlobImplSnapshot
{
  RefPtr<BlobImpl> mBlobImpl;
  nsWeakPtr mFileHandle;

public:
  BlobImplSnapshot(BlobImpl* aImpl,
                   IDBFileHandle* aFileHandle);

  NS_DECL_ISUPPORTS_INHERITED

private:
  BlobImplSnapshot(BlobImpl* aImpl,
                   nsIWeakReference* aFileHandle);

  ~BlobImplSnapshot();

  // BlobImpl
  virtual void
  GetName(nsAString& aName) const override
  {
    mBlobImpl->GetName(aName);
  }

  virtual void
  GetPath(nsAString& aPath) const override
  {
    mBlobImpl->GetPath(aPath);
  }

  virtual void
  SetPath(const nsAString& aPath) override
  {
    mBlobImpl->SetPath(aPath);
  }

  virtual int64_t
  GetLastModified(ErrorResult& aRv) override
  {
    return mBlobImpl->GetLastModified(aRv);
  }

  virtual void
  SetLastModified(int64_t aLastModified) override
  {
    mBlobImpl->SetLastModified(aLastModified);
  }

  virtual void
  GetMozFullPath(nsAString& aName, ErrorResult& aRv) const override
  {
    mBlobImpl->GetMozFullPath(aName, aRv);
  }

  virtual void
  GetMozFullPathInternal(nsAString& aFileName, ErrorResult& aRv) const override
  {
    mBlobImpl->GetMozFullPathInternal(aFileName, aRv);
  }

  virtual uint64_t
  GetSize(ErrorResult& aRv) override
  {
    return mBlobImpl->GetSize(aRv);
  }

  virtual void
  GetType(nsAString& aType) override
  {
    mBlobImpl->GetType(aType);
  }

  virtual uint64_t
  GetSerialNumber() const override
  {
    return mBlobImpl->GetSerialNumber();
  }

  virtual already_AddRefed<BlobImpl>
  CreateSlice(uint64_t aStart,
              uint64_t aLength,
              const nsAString& aContentType,
              ErrorResult& aRv) override;

  virtual const nsTArray<RefPtr<BlobImpl>>*
  GetSubBlobImpls() const override
  {
    return mBlobImpl->GetSubBlobImpls();
  }

  virtual void
  GetInternalStream(nsIInputStream** aStream,
                    ErrorResult& aRv) override;

  virtual int64_t
  GetFileId() override
  {
    return mBlobImpl->GetFileId();
  }

  virtual nsresult
  GetSendInfo(nsIInputStream** aBody,
              uint64_t* aContentLength,
              nsACString& aContentType,
              nsACString& aCharset) override
  {
    return mBlobImpl->GetSendInfo(aBody,
                                  aContentLength,
                                  aContentType,
                                  aCharset);
  }

  virtual nsresult
  GetMutable(bool* aMutable) const override
  {
    return mBlobImpl->GetMutable(aMutable);
  }

  virtual nsresult
  SetMutable(bool aMutable) override
  {
    return mBlobImpl->SetMutable(aMutable);
  }

  virtual void
  SetLazyData(const nsAString& aName,
              const nsAString& aContentType,
              uint64_t aLength,
              int64_t aLastModifiedDate) override
  {
    MOZ_CRASH("This should never be called!");
  }

  virtual bool
  IsMemoryFile() const override
  {
    return mBlobImpl->IsMemoryFile();
  }

  virtual bool
  IsSizeUnknown() const override
  {
    return mBlobImpl->IsSizeUnknown();
  }

  virtual bool
  IsDateUnknown() const override
  {
    return mBlobImpl->IsDateUnknown();
  }

  virtual bool
  IsFile() const override
  {
    return mBlobImpl->IsFile();
  }

  virtual bool
  MayBeClonedToOtherThreads() const override
  {
    return false;
  }

  // PIBlobImplSnapshot
  virtual BlobImpl*
  GetBlobImpl() const override;
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_filesnapshot_h__
