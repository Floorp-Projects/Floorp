/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_IPCBlobInputStreamChild_h
#define mozilla_dom_ipc_IPCBlobInputStreamChild_h

#include "mozilla/ipc/PIPCBlobInputStreamChild.h"
#include "mozilla/Mutex.h"
#include "nsIThread.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class IPCBlobInputStream;

class IPCBlobInputStreamChild final
  : public mozilla::ipc::PIPCBlobInputStreamChild
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(IPCBlobInputStreamChild)

  IPCBlobInputStreamChild(const nsID& aID, uint64_t aSize);

  void
  ActorDestroy(IProtocol::ActorDestroyReason aReason) override;

  already_AddRefed<nsIInputStream>
  CreateStream();

  void
  ForgetStream(IPCBlobInputStream* aStream);

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

private:
  ~IPCBlobInputStreamChild();

  // Raw pointers because these streams keep this actor alive. When the last
  // stream is unregister, the actor will be deleted. This list is protected by
  // mutex.
  nsTArray<IPCBlobInputStream*> mStreams;

  // This mutex protects mStreams because that can be touched in any thread.
  Mutex mMutex;

  const nsID mID;
  const uint64_t mSize;

  // false when ActorDestroy() is called.
  bool mActorAlive;

  nsCOMPtr<nsIThread> mOwningThread;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ipc_IPCBlobInputStreamChild_h
