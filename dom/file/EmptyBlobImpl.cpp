/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmptyBlobImpl.h"
#include "nsStringStream.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_INHERITED0(EmptyBlobImpl, BlobImpl)

already_AddRefed<BlobImpl>
EmptyBlobImpl::CreateSlice(uint64_t aStart, uint64_t aLength,
                           const nsAString& aContentType,
                           ErrorResult& aRv)
{
  MOZ_ASSERT(!aStart && !aLength);
  RefPtr<BlobImpl> impl = new EmptyBlobImpl(aContentType);

  DebugOnly<bool> isMutable;
  MOZ_ASSERT(NS_SUCCEEDED(impl->GetMutable(&isMutable)));
  MOZ_ASSERT(!isMutable);

  return impl.forget();
}

void
EmptyBlobImpl::CreateInputStream(nsIInputStream** aStream,
                                 ErrorResult& aRv)
{
  if (NS_WARN_IF(!aStream)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsresult rv = NS_NewCStringInputStream(aStream, EmptyCString());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
}

} // namespace dom
} // namespace mozilla
