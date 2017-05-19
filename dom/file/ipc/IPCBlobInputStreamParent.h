/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_IPCBlobInputStreamParent_h
#define mozilla_dom_ipc_IPCBlobInputStreamParent_h

#include "mozilla/ipc/PIPCBlobInputStreamParent.h"

class nsIInputStream;

namespace mozilla {
namespace dom {

class IPCBlobInputStreamParent final
  : public mozilla::ipc::PIPCBlobInputStreamParent
{
public:
  // The size of the inputStream must be passed as argument in order to avoid
  // the use of nsIInputStream::Available() which could open a fileDescriptor in
  // case the stream is a nsFileStream.
  template<typename M>
  static IPCBlobInputStreamParent*
  Create(nsIInputStream* aInputStream, uint64_t aSize, nsresult* aRv,
         M* aManager);

  void
  ActorDestroy(IProtocol::ActorDestroyReason aReason) override;

  const nsID&
  ID() const
  {
    return mID;
  }

  uint64_t
  Size() const
  {
    return mSize;
  }

  mozilla::ipc::IPCResult
  RecvStreamNeeded() override;

private:
  IPCBlobInputStreamParent(const nsID& aID, uint64_t aSize,
                           nsIContentParent* aManager);

  IPCBlobInputStreamParent(const nsID& aID, uint64_t aSize,
                           mozilla::ipc::PBackgroundParent* aManager);

  const nsID mID;
  const uint64_t mSize;

  // Only 1 of these 2 is set. Raw pointer because these 2 managers are keeping
  // the parent actor alive. The pointers will be nullified in ActorDestroyed.
  nsIContentParent* mContentManager;
  mozilla::ipc::PBackgroundParent* mPBackgroundManager;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ipc_IPCBlobInputStreamParent_h
