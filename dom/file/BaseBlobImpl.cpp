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

void BaseBlobImpl::GetName(nsAString& aName) const {
  MOZ_ASSERT(mIsFile, "Should only be called on files");
  aName = mName;
}

void BaseBlobImpl::GetDOMPath(nsAString& aPath) const {
  MOZ_ASSERT(mIsFile, "Should only be called on files");
  aPath = mPath;
}

void BaseBlobImpl::SetDOMPath(const nsAString& aPath) {
  MOZ_ASSERT(mIsFile, "Should only be called on files");
  mPath = aPath;
}

void BaseBlobImpl::GetMozFullPath(nsAString& aFileName,
                                  SystemCallerGuarantee /* unused */,
                                  ErrorResult& aRv) {
  MOZ_ASSERT(mIsFile, "Should only be called on files");

  GetMozFullPathInternal(aFileName, aRv);
}

void BaseBlobImpl::GetMozFullPathInternal(nsAString& aFileName,
                                          ErrorResult& aRv) {
  if (!mIsFile) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  aFileName.Truncate();
}

void BaseBlobImpl::GetType(nsAString& aType) { aType = mContentType; }

int64_t BaseBlobImpl::GetLastModified(ErrorResult& aRv) {
  MOZ_ASSERT(mIsFile, "Should only be called on files");
  return mLastModificationDate / PR_USEC_PER_MSEC;
}

int64_t BaseBlobImpl::GetFileId() { return -1; }

/* static */
uint64_t BaseBlobImpl::NextSerialNumber() {
  static Atomic<uint64_t> nextSerialNumber;
  return nextSerialNumber++;
}

void BaseBlobImpl::SetLastModificationDatePrecisely(int64_t aDate) {
  MOZ_ASSERT(mIsFile, "Should only be called on files");
  mLastModificationDate = aDate;
}

void BaseBlobImpl::SetLastModificationDate(bool aCrossOriginIsolated,
                                           int64_t aDate) {
  return SetLastModificationDatePrecisely(
      nsRFPService::ReduceTimePrecisionAsUSecs(aDate, 0,
                                               /* aIsSystemPrincipal */ false,
                                               aCrossOriginIsolated));
  // mLastModificationDate is an absolute timestamp so we supply a zero
  // context mix-in
}

}  // namespace dom
}  // namespace mozilla
