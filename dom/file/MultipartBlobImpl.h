/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MultipartBlobImpl_h
#define mozilla_dom_MultipartBlobImpl_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Move.h"
#include "mozilla/dom/BaseBlobImpl.h"

namespace mozilla {
namespace dom {

class MultipartBlobImpl final : public BaseBlobImpl
{
public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(MultipartBlobImpl, BaseBlobImpl)

  // Create as a file
  static already_AddRefed<MultipartBlobImpl>
  Create(nsTArray<RefPtr<BlobImpl>>&& aBlobImpls,
         const nsAString& aName,
         const nsAString& aContentType,
         ErrorResult& aRv);

  // Create as a blob
  static already_AddRefed<MultipartBlobImpl>
  Create(nsTArray<RefPtr<BlobImpl>>&& aBlobImpls,
         const nsAString& aContentType,
         ErrorResult& aRv);

  // Create as a file to be later initialized
  explicit MultipartBlobImpl(const nsAString& aName)
    : BaseBlobImpl(aName, EmptyString(), UINT64_MAX),
      mIsFromNsIFile(false)
  {
  }

  // Create as a blob to be later initialized
  MultipartBlobImpl()
    : BaseBlobImpl(EmptyString(), UINT64_MAX),
      mIsFromNsIFile(false)
  {
  }

  void InitializeBlob(ErrorResult& aRv);

  void InitializeBlob(const Sequence<Blob::BlobPart>& aData,
                      const nsAString& aContentType,
                      bool aNativeEOL,
                      ErrorResult& aRv);

  nsresult InitializeChromeFile(nsIFile* aData,
                                const nsAString& aType,
                                const nsAString& aName,
                                bool aLastModifiedPassed,
                                int64_t aLastModified,
                                bool aIsFromNsIFile);

  virtual already_AddRefed<BlobImpl>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType,
              ErrorResult& aRv) override;

  virtual uint64_t GetSize(ErrorResult& aRv) override
  {
    return mLength;
  }

  virtual void CreateInputStream(nsIInputStream** aInputStream,
                                 ErrorResult& aRv) override;

  virtual const nsTArray<RefPtr<BlobImpl>>* GetSubBlobImpls() const override
  {
    return mBlobImpls.Length() ? &mBlobImpls : nullptr;
  }

  virtual void GetMozFullPathInternal(nsAString& aFullPath,
                                      ErrorResult& aRv) const override;

  virtual nsresult
  SetMutable(bool aMutable) override;

  void SetName(const nsAString& aName)
  {
    mName = aName;
  }

  virtual bool MayBeClonedToOtherThreads() const override;

  size_t GetAllocationSize() const override;
  size_t GetAllocationSize(FallibleTArray<BlobImpl*>& aVisitedBlobImpls) const override;

protected:
  MultipartBlobImpl(nsTArray<RefPtr<BlobImpl>>&& aBlobImpls,
                    const nsAString& aName,
                    const nsAString& aContentType)
    : BaseBlobImpl(aName, aContentType, UINT64_MAX),
      mBlobImpls(std::move(aBlobImpls)),
      mIsFromNsIFile(false)
  {
  }

  MultipartBlobImpl(nsTArray<RefPtr<BlobImpl>>&& aBlobImpls,
                    const nsAString& aContentType)
    : BaseBlobImpl(aContentType, UINT64_MAX),
      mBlobImpls(std::move(aBlobImpls)),
      mIsFromNsIFile(false)
  {
  }

  virtual ~MultipartBlobImpl() {}

  void SetLengthAndModifiedDate(ErrorResult& aRv);

  nsTArray<RefPtr<BlobImpl>> mBlobImpls;
  bool mIsFromNsIFile;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_MultipartBlobImpl_h
