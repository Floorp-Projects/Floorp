/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PendingIPCBlobChild.h"

namespace mozilla {
namespace dom {

PendingIPCBlobChild::PendingIPCBlobChild(const IPCBlob& aBlob)
{
  mBlobImpl = IPCBlobUtils::Deserialize(aBlob);
  MOZ_ASSERT(mBlobImpl);
}

PendingIPCBlobChild::~PendingIPCBlobChild()
{}

already_AddRefed<BlobImpl>
PendingIPCBlobChild::SetPendingInfoAndDeleteActor(const nsString& aName,
                                                  const nsString& aContentType,
                                                  uint64_t aLength,
                                                  int64_t aLastModifiedDate)
{
  RefPtr<BlobImpl> blobImpl;
  blobImpl.swap(mBlobImpl);

  blobImpl->SetLazyData(aName, aContentType, aLength, aLastModifiedDate);

  PendingIPCFileData fileData(nsString(aName), aLastModifiedDate);
  PendingIPCBlobData blobData(nsString(aContentType), aLength, fileData);
  Unused << Send__delete__(this, blobData);

  return blobImpl.forget();
}

already_AddRefed<BlobImpl>
PendingIPCBlobChild::SetPendingInfoAndDeleteActor(const nsString& aContentType,
                                                  uint64_t aLength)
{
  RefPtr<BlobImpl> blobImpl;
  blobImpl.swap(mBlobImpl);

  blobImpl->SetLazyData(NullString(), aContentType, aLength, INT64_MAX);

  PendingIPCBlobData data(nsString(aContentType), aLength, void_t());
  Unused << Send__delete__(this, data);

  return blobImpl.forget();
}

} // namespace dom
} // namespace mozilla
