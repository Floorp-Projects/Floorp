/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BaseBlobImpl.h"
#include "nsRFPService.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

void
BaseBlobImpl::GetName(nsAString& aName) const
{
  MOZ_ASSERT(mIsFile, "Should only be called on files");
  aName = mName;
}

void
BaseBlobImpl::GetDOMPath(nsAString& aPath) const
{
  MOZ_ASSERT(mIsFile, "Should only be called on files");
  aPath = mPath;
}

void
BaseBlobImpl::SetDOMPath(const nsAString& aPath)
{
  MOZ_ASSERT(mIsFile, "Should only be called on files");
  mPath = aPath;
}

void
BaseBlobImpl::GetMozFullPath(nsAString& aFileName,
                             SystemCallerGuarantee /* unused */,
                             ErrorResult& aRv) const
{
  MOZ_ASSERT(mIsFile, "Should only be called on files");

  GetMozFullPathInternal(aFileName, aRv);
}

void
BaseBlobImpl::GetMozFullPathInternal(nsAString& aFileName, ErrorResult& aRv) const
{
  if (!mIsFile) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  aFileName.Truncate();
}

void
BaseBlobImpl::GetType(nsAString& aType)
{
  aType = mContentType;
}

int64_t
BaseBlobImpl::GetLastModified(ErrorResult& aRv)
{
  MOZ_ASSERT(mIsFile, "Should only be called on files");
  if (IsDateUnknown()) {
    mLastModificationDate = nsRFPService::ReduceTimePrecisionAsUSecs(PR_Now());
  }

  return mLastModificationDate / PR_USEC_PER_MSEC;
}

void
BaseBlobImpl::SetLastModified(int64_t aLastModified)
{
  mLastModificationDate = aLastModified * PR_USEC_PER_MSEC;
}

int64_t
BaseBlobImpl::GetFileId()
{
  return -1;
}

nsresult
BaseBlobImpl::GetSendInfo(nsIInputStream** aBody, uint64_t* aContentLength,
                          nsACString& aContentType, nsACString& aCharset)
{
  MOZ_ASSERT(aContentLength);

  ErrorResult rv;

  nsCOMPtr<nsIInputStream> stream;
  GetInternalStream(getter_AddRefs(stream), rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  *aContentLength = GetSize(rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  nsAutoString contentType;
  GetType(contentType);

  if (contentType.IsEmpty()) {
    aContentType.SetIsVoid(true);
  } else {
    CopyUTF16toUTF8(contentType, aContentType);
  }

  aCharset.Truncate();

  stream.forget(aBody);
  return NS_OK;
}

nsresult
BaseBlobImpl::GetMutable(bool* aMutable) const
{
  *aMutable = !mImmutable;
  return NS_OK;
}

nsresult
BaseBlobImpl::SetMutable(bool aMutable)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG(!mImmutable || !aMutable);

  if (!mImmutable && !aMutable) {
    // Force the content type and size to be cached
    nsAutoString dummyString;
    GetType(dummyString);

    ErrorResult error;
    GetSize(error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  mImmutable = !aMutable;
  return rv;
}

/* static */ uint64_t
BaseBlobImpl::NextSerialNumber()
{
  static Atomic<uint64_t> nextSerialNumber;
  return nextSerialNumber++;
}

} // namespace dom
} // namespace mozilla
