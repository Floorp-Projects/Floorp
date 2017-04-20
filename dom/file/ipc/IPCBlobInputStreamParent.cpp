/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCBlobInputStreamParent.h"
#include "IPCBlobInputStreamStorage.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

/* static */ IPCBlobInputStreamParent*
IPCBlobInputStreamParent::Create(nsIInputStream* aInputStream, nsresult* aRv)
{
  MOZ_ASSERT(aInputStream);
  MOZ_ASSERT(aRv);

  nsID id;
  *aRv = nsContentUtils::GenerateUUIDInPlace(id);
  if (NS_WARN_IF(NS_FAILED(*aRv))) {
    return nullptr;
  }

  // We cannot fail because of the stream has a invalid size. This happens
  // normally for nsFileStreams pointing to non-existing files.
  uint64_t size;
  *aRv = aInputStream->Available(&size);
  if (NS_WARN_IF(NS_FAILED(*aRv))) {
    size = 0;
  }

  IPCBlobInputStreamStorage::Get()->AddStream(aInputStream, id);

  return new IPCBlobInputStreamParent(id, size);
}

IPCBlobInputStreamParent::IPCBlobInputStreamParent(const nsID& aID,
                                                   uint64_t aSize)
  : mID(aID)
  , mSize(aSize)
{}

void
IPCBlobInputStreamParent::ActorDestroy(IProtocol::ActorDestroyReason aReason)
{
  IPCBlobInputStreamStorage::Get()->ForgetStream(mID);
}

} // namespace dom
} // namespace mozilla
