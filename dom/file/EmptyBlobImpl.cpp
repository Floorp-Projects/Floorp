/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmptyBlobImpl.h"
#include "nsStringStream.h"

namespace mozilla::dom {

already_AddRefed<BlobImpl> EmptyBlobImpl::CreateSlice(
    uint64_t aStart, uint64_t aLength, const nsAString& aContentType,
    ErrorResult& aRv) const {
  MOZ_ASSERT(!aStart && !aLength);
  RefPtr<BlobImpl> impl = new EmptyBlobImpl(aContentType);
  return impl.forget();
}

void EmptyBlobImpl::CreateInputStream(nsIInputStream** aStream,
                                      ErrorResult& aRv) const {
  if (NS_WARN_IF(!aStream)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsresult rv = NS_NewCStringInputStream(aStream, ""_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
}

}  // namespace mozilla::dom
