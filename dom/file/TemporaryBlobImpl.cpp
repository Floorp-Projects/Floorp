/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TemporaryBlobImpl.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_INHERITED0(TemporaryBlobImpl, BlobImpl)

TemporaryBlobImpl::TemporaryBlobImpl(PRFileDesc* aFD, uint64_t aStartPos,
                                     uint64_t aLength,
                                     const nsAString& aContentType)
  : BaseBlobImpl(aContentType, aLength)
  , mStartPos(aStartPos)
{
  mFileDescOwner = new nsTemporaryFileInputStream::FileDescOwner(aFD);
}

TemporaryBlobImpl::TemporaryBlobImpl(const TemporaryBlobImpl* aOther,
                                     uint64_t aStart, uint64_t aLength,
                                     const nsAString& aContentType)
  : BaseBlobImpl(aContentType, aLength)
  , mStartPos(aStart)
  , mFileDescOwner(aOther->mFileDescOwner)
{}

already_AddRefed<BlobImpl>
TemporaryBlobImpl::CreateSlice(uint64_t aStart, uint64_t aLength,
                               const nsAString& aContentType,
                               ErrorResult& aRv)
{
  if (aStart + aLength > mLength) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<BlobImpl> impl =
    new TemporaryBlobImpl(this, aStart + mStartPos,
                              aLength, aContentType);
  return impl.forget();
}

void
TemporaryBlobImpl::GetInternalStream(nsIInputStream** aStream,
                                     ErrorResult& aRv)
{
  nsCOMPtr<nsIInputStream> stream =
    new nsTemporaryFileInputStream(mFileDescOwner, mStartPos,
                                   mStartPos + mLength);
  stream.forget(aStream);
}

} // namespace dom
} // namespace mozilla
