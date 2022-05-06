/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StringBlobImpl_h
#define mozilla_dom_StringBlobImpl_h

#include "BaseBlobImpl.h"
#include "nsIMemoryReporter.h"

namespace mozilla::dom {

class StringBlobImpl final : public BaseBlobImpl, public nsIMemoryReporter {
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIMEMORYREPORTER

  static already_AddRefed<StringBlobImpl> Create(const nsACString& aData,
                                                 const nsAString& aContentType);

  void CreateInputStream(nsIInputStream** aStream,
                         ErrorResult& aRv) const override;

  already_AddRefed<BlobImpl> CreateSlice(uint64_t aStart, uint64_t aLength,
                                         const nsAString& aContentType,
                                         ErrorResult& aRv) const override;

  size_t GetAllocationSize() const override { return mData.Length(); }

  size_t GetAllocationSize(
      FallibleTArray<BlobImpl*>& aVisitedBlobImpls) const override {
    return GetAllocationSize();
  }

  void GetBlobImplType(nsAString& aBlobImplType) const override {
    aBlobImplType = u"StringBlobImpl"_ns;
  }

 private:
  StringBlobImpl(const nsACString& aData, const nsAString& aContentType);

  ~StringBlobImpl() override;

  nsCString mData;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_StringBlobImpl_h
