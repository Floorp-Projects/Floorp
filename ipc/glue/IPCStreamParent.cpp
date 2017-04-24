/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCStreamDestination.h"
#include "mozilla/dom/nsIContentParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/PChildToParentStreamParent.h"
#include "mozilla/ipc/PParentToChildStreamParent.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace ipc {

// Child to Parent implementation
// ----------------------------------------------------------------------------

namespace {

class IPCStreamSourceParent final : public PParentToChildStreamParent
                                  , public IPCStreamSource
{
public:
  static IPCStreamSourceParent*
  Create(nsIAsyncInputStream* aInputStream)
  {
    MOZ_ASSERT(aInputStream);

    IPCStreamSourceParent* source = new IPCStreamSourceParent(aInputStream);
    if (!source->Initialize()) {
      delete source;
      return nullptr;
    }

    return source;
  }

  // PParentToChildStreamParent methods

  void
  ActorDestroy(ActorDestroyReason aReason) override
  {
    ActorDestroyed();
  }

  IPCResult
  RecvStartReading() override
  {
    Start();
    return IPC_OK();
  }

  IPCResult
  RecvRequestClose(const nsresult& aRv) override
  {
    OnEnd(aRv);
    return IPC_OK();
  }

  void
  Close(nsresult aRv) override
  {
    MOZ_ASSERT(IPCStreamSource::mState == IPCStreamSource::eClosed);
    Unused << SendClose(aRv);
  }

  void
  SendData(const nsCString& aBuffer) override
  {
    Unused << SendBuffer(aBuffer);
  }

private:
  explicit IPCStreamSourceParent(nsIAsyncInputStream* aInputStream)
    :IPCStreamSource(aInputStream)
  {}
};

} // anonymous namespace

/* static */ PParentToChildStreamParent*
IPCStreamSource::Create(nsIAsyncInputStream* aInputStream,
                        dom::nsIContentParent* aManager)
{
  MOZ_ASSERT(aInputStream);
  MOZ_ASSERT(aManager);

  // PContent can only be used on the main thread
  MOZ_ASSERT(NS_IsMainThread());

  IPCStreamSourceParent* source = IPCStreamSourceParent::Create(aInputStream);
  if (!source) {
    return nullptr;
  }

  if (!aManager->SendPParentToChildStreamConstructor(source)) {
    // no delete here, the manager will delete the actor for us.
    return nullptr;
  }

  source->ActorConstructed();
  return source;
}

/* static */ PParentToChildStreamParent*
IPCStreamSource::Create(nsIAsyncInputStream* aInputStream,
                        PBackgroundParent* aManager)
{
  MOZ_ASSERT(aInputStream);
  MOZ_ASSERT(aManager);

  IPCStreamSourceParent* source = IPCStreamSourceParent::Create(aInputStream);
  if (!source) {
    return nullptr;
  }

  if (!aManager->SendPParentToChildStreamConstructor(source)) {
    // no delete here, the manager will delete the actor for us.
    return nullptr;
  }

  source->ActorConstructed();
  return source;
}

/* static */ IPCStreamSource*
IPCStreamSource::Cast(PParentToChildStreamParent* aActor)
{
  MOZ_ASSERT(aActor);
  return static_cast<IPCStreamSourceParent*>(aActor);
}

// Child to Parent implementation
// ----------------------------------------------------------------------------

namespace {

class IPCStreamDestinationParent final : public PChildToParentStreamParent
                                       , public IPCStreamDestination
{
public:
  nsresult Initialize()
  {
    return IPCStreamDestination::Initialize();
  }

  ~IPCStreamDestinationParent()
  {}

private:
  // PChildToParentStreamParent methods

  void
  ActorDestroy(ActorDestroyReason aReason) override
  {
    ActorDestroyed();
  }

  IPCResult
  RecvBuffer(const nsCString& aBuffer) override
  {
    BufferReceived(aBuffer);
    return IPC_OK();
  }

  IPCResult
  RecvClose(const nsresult& aRv) override
  {
    CloseReceived(aRv);
    return IPC_OK();
  }

  // IPCStreamDestination methods

  void
  StartReading() override
  {
    MOZ_ASSERT(HasDelayedStart());
    Unused << SendStartReading();
  }

  void
  RequestClose(nsresult aRv) override
  {
    Unused << SendRequestClose(aRv);
  }

  void
  TerminateDestination() override
  {
    Unused << Send__delete__(this);
  }
};

} // anonymous namespace

PChildToParentStreamParent*
AllocPChildToParentStreamParent()
{
  IPCStreamDestinationParent* actor = new IPCStreamDestinationParent();

  if (NS_WARN_IF(NS_FAILED(actor->Initialize()))) {
    delete actor;
    actor = nullptr;
  }

  return actor;
}

void
DeallocPChildToParentStreamParent(PChildToParentStreamParent* aActor)
{
  delete aActor;
}

/* static */ IPCStreamDestination*
IPCStreamDestination::Cast(PChildToParentStreamParent* aActor)
{
  MOZ_ASSERT(aActor);
  return static_cast<IPCStreamDestinationParent*>(aActor);
}

} // namespace ipc
} // namespace mozilla
