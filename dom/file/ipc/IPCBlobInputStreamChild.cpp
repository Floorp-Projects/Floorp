/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCBlobInputStreamChild.h"

namespace mozilla {
namespace dom {

namespace {

// This runnable is used in case the last stream is forgotten on the 'wrong'
// thread.
class DeleteRunnable final : public Runnable
{
public:
  explicit DeleteRunnable(IPCBlobInputStreamChild* aActor)
    : mActor(aActor)
  {}

  NS_IMETHOD
  Run() override
  {
    mActor->Send__delete__(mActor);
    return NS_OK;
  }

private:
  RefPtr<IPCBlobInputStreamChild> mActor;
};

} // anonymous

IPCBlobInputStreamChild::IPCBlobInputStreamChild(const nsID& aID,
                                                 uint64_t aSize)
  : mMutex("IPCBlobInputStreamChild::mMutex")
  , mID(aID)
  , mSize(aSize)
  , mActorAlive(true)
  , mOwningThread(NS_GetCurrentThread())
{}

IPCBlobInputStreamChild::~IPCBlobInputStreamChild()
{}

void
IPCBlobInputStreamChild::ActorDestroy(IProtocol::ActorDestroyReason aReason)
{
  mActorAlive = false;
}

already_AddRefed<nsIInputStream>
IPCBlobInputStreamChild::CreateStream()
{
  MutexAutoLock lock(mMutex);

  RefPtr<IPCBlobInputStream> stream = new IPCBlobInputStream(this);
  mStreams.AppendElement(stream);
  return stream.forget();
}

void
IPCBlobInputStreamChild::ForgetStream(IPCBlobInputStream* aStream)
{
  MOZ_ASSERT(aStream);

  RefPtr<IPCBlobInputStreamChild> kungFoDeathGrip = this;

  {
    MutexAutoLock lock(mMutex);
    mStreams.RemoveElement(aStream);

    if (!mStreams.IsEmpty() || !mActorAlive) {
      return;
    }
  }

  if (mOwningThread == NS_GetCurrentThread()) {
    Send__delete__(this);
    return;
  }

  RefPtr<DeleteRunnable> runnable = new DeleteRunnable(this);
  mOwningThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

} // namespace dom
} // namespace mozilla
