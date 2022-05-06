/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EmptyBlobImpl_h
#define mozilla_dom_EmptyBlobImpl_h

#include "BaseBlobImpl.h"

namespace mozilla::dom {

class EmptyBlobImpl final : public BaseBlobImpl {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(EmptyBlobImpl, BaseBlobImpl)

  // Blob constructor.
  explicit EmptyBlobImpl(const nsAString& aContentType)
      : BaseBlobImpl(aContentType, 0 /* aLength */) {}

  void CreateInputStream(nsIInputStream** aStream,
                         ErrorResult& aRv) const override;

  already_AddRefed<BlobImpl> CreateSlice(uint64_t aStart, uint64_t aLength,
                                         const nsAString& aContentType,
                                         ErrorResult& aRv) const override;

  bool IsMemoryFile() const override { return true; }

  void GetBlobImplType(nsAString& aBlobImplType) const override {
    aBlobImplType = u"EmptyBlobImpl"_ns;
  }

 private:
  ~EmptyBlobImpl() override = default;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_EmptyBlobImpl_h
