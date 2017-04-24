/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCBlobInputStreamParent.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

/* static */ IPCBlobInputStreamParent*
IPCBlobInputStreamParent::Create(nsIInputStream* aInputStream, uint64_t aSize,
                                 nsresult* aRv)
{
  MOZ_ASSERT(aInputStream);
  MOZ_ASSERT(aRv);

  nsID id;
  *aRv = nsContentUtils::GenerateUUIDInPlace(id);
  if (NS_WARN_IF(NS_FAILED(*aRv))) {
    return nullptr;
  }

  // TODO: register to a service.

  return new IPCBlobInputStreamParent(id, aSize);
}

IPCBlobInputStreamParent::IPCBlobInputStreamParent(const nsID& aID,
                                                   uint64_t aSize)
  : mID(aID)
  , mSize(aSize)
{}

void
IPCBlobInputStreamParent::ActorDestroy(IProtocol::ActorDestroyReason aReason)
{
  // TODO: unregister
}

} // namespace dom
} // namespace mozilla
