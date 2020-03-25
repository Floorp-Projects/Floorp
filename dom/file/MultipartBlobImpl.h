/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MultipartBlobImpl_h
#define mozilla_dom_MultipartBlobImpl_h

#include <utility>

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/BaseBlobImpl.h"

namespace mozilla {
namespace dom {

// This is just a sentinel value to be sure that we don't call
// SetLengthAndModifiedDate more than once.
constexpr int64_t MULTIPARTBLOBIMPL_UNKNOWN_LAST_MODIFIED = INT64_MAX;
constexpr uint64_t MULTIPARTBLOBIMPL_UNKNOWN_LENGTH = UINT64_MAX;

class MultipartBlobImpl final : public BaseBlobImpl {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(MultipartBlobImpl, BaseBlobImpl)

  // Create as a file
  static already_AddRefed<MultipartBlobImpl> Create(
      nsTArray<RefPtr<BlobImpl>>&& aBlobImpls, const nsAString& aName,
      const nsAString& aContentType, bool aCrossOriginIsolated,
      ErrorResult& aRv);

  // Create as a blob
  static already_AddRefed<MultipartBlobImpl> Create(
      nsTArray<RefPtr<BlobImpl>>&& aBlobImpls, const nsAString& aContentType,
      ErrorResult& aRv);

  // Create as a file to be later initialized
  explicit MultipartBlobImpl(const nsAString& aName)
      : BaseBlobImpl(aName, EmptyString(), MULTIPARTBLOBIMPL_UNKNOWN_LENGTH,
                     MULTIPARTBLOBIMPL_UNKNOWN_LAST_MODIFIED) {}

  // Create as a blob to be later initialized
  MultipartBlobImpl()
      : BaseBlobImpl(EmptyString(), MULTIPARTBLOBIMPL_UNKNOWN_LENGTH) {}

  void InitializeBlob(bool aCrossOriginIsolated, ErrorResult& aRv);

  void InitializeBlob(const Sequence<Blob::BlobPart>& aData,
                      const nsAString& aContentType, bool aNativeEOL,
                      bool aCrossOriginIsolated, ErrorResult& aRv);

  already_AddRefed<BlobImpl> CreateSlice(uint64_t aStart, uint64_t aLength,
                                         const nsAString& aContentType,
                                         ErrorResult& aRv) override;

  uint64_t GetSize(ErrorResult& aRv) override { return mLength; }

  void CreateInputStream(nsIInputStream** aInputStream,
                         ErrorResult& aRv) override;

  const nsTArray<RefPtr<BlobImpl>>* GetSubBlobImpls() const override {
    return mBlobImpls.Length() ? &mBlobImpls : nullptr;
  }

  void SetName(const nsAString& aName) { mName = aName; }

  size_t GetAllocationSize() const override;
  size_t GetAllocationSize(
      FallibleTArray<BlobImpl*>& aVisitedBlobImpls) const override;

  void GetBlobImplType(nsAString& aBlobImplType) const override;

  void SetLastModified(int64_t aLastModified);

 protected:
  // File constructor.
  MultipartBlobImpl(nsTArray<RefPtr<BlobImpl>>&& aBlobImpls,
                    const nsAString& aName, const nsAString& aContentType)
      : BaseBlobImpl(aName, aContentType, MULTIPARTBLOBIMPL_UNKNOWN_LENGTH,
                     MULTIPARTBLOBIMPL_UNKNOWN_LAST_MODIFIED),
        mBlobImpls(std::move(aBlobImpls)) {}

  // Blob constructor.
  MultipartBlobImpl(nsTArray<RefPtr<BlobImpl>>&& aBlobImpls,
                    const nsAString& aContentType)
      : BaseBlobImpl(aContentType, MULTIPARTBLOBIMPL_UNKNOWN_LENGTH),
        mBlobImpls(std::move(aBlobImpls)) {}

  ~MultipartBlobImpl() = default;

  void SetLengthAndModifiedDate(const Maybe<bool>& aCrossOriginIsolated,
                                ErrorResult& aRv);

  nsTArray<RefPtr<BlobImpl>> mBlobImpls;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MultipartBlobImpl_h
