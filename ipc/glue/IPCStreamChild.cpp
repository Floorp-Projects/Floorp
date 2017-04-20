/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCStreamDestination.h"
#include "IPCStreamSource.h"

#include "mozilla/Unused.h"
#include "mozilla/dom/nsIContentChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PChildToParentStreamChild.h"
#include "mozilla/ipc/PParentToChildStreamChild.h"

namespace mozilla {
namespace ipc {

// Child to Parent implementation
// ----------------------------------------------------------------------------

namespace {

class IPCStreamSourceChild final : public PChildToParentStreamChild
                                 , public IPCStreamSource
{
public:
  static IPCStreamSourceChild*
  Create(nsIAsyncInputStream* aInputStream)
  {
    MOZ_ASSERT(aInputStream);

    IPCStreamSourceChild* source = new IPCStreamSourceChild(aInputStream);
    if (!source->Initialize()) {
      delete source;
      return nullptr;
    }

    return source;
  }

  // PChildToParentStreamChild methods

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
  explicit IPCStreamSourceChild(nsIAsyncInputStream* aInputStream)
    :IPCStreamSource(aInputStream)
  {}
};

} // anonymous namespace

/* static */ PChildToParentStreamChild*
IPCStreamSource::Create(nsIAsyncInputStream* aInputStream,
                        dom::nsIContentChild* aManager)
{
  MOZ_ASSERT(aInputStream);
  MOZ_ASSERT(aManager);

  // PContent can only be used on the main thread
  MOZ_ASSERT(NS_IsMainThread());

  IPCStreamSourceChild* source = IPCStreamSourceChild::Create(aInputStream);
  if (!source) {
    return nullptr;
  }

  if (!aManager->SendPChildToParentStreamConstructor(source)) {
    return nullptr;
  }

  source->ActorConstructed();
  return source;
}

/* static */ PChildToParentStreamChild*
IPCStreamSource::Create(nsIAsyncInputStream* aInputStream,
                        PBackgroundChild* aManager)
{
  MOZ_ASSERT(aInputStream);
  MOZ_ASSERT(aManager);

  IPCStreamSourceChild* source = IPCStreamSourceChild::Create(aInputStream);
  if (!source) {
    return nullptr;
  }

  if (!aManager->SendPChildToParentStreamConstructor(source)) {
    return nullptr;
  }

  source->ActorConstructed();
  return source;
}

/* static */ IPCStreamSource*
IPCStreamSource::Cast(PChildToParentStreamChild* aActor)
{
  MOZ_ASSERT(aActor);
  return static_cast<IPCStreamSourceChild*>(aActor);
}

// Parent to Child implementation
// ----------------------------------------------------------------------------

namespace {

class IPCStreamDestinationChild final : public PParentToChildStreamChild
                                      , public IPCStreamDestination
{
public:
  nsresult Initialize()
  {
    return IPCStreamDestination::Initialize();
  }

  ~IPCStreamDestinationChild()
  {}

private:
  // PParentToChildStreamChild methods

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

PParentToChildStreamChild*
AllocPParentToChildStreamChild()
{
  IPCStreamDestinationChild* actor = new IPCStreamDestinationChild();

  if (NS_WARN_IF(NS_FAILED(actor->Initialize()))) {
    delete actor;
    actor = nullptr;
  }

  return actor;
}

void
DeallocPParentToChildStreamChild(PParentToChildStreamChild* aActor)
{
  delete aActor;
}

/* static */ IPCStreamDestination*
IPCStreamDestination::Cast(PParentToChildStreamChild* aActor)
{
  MOZ_ASSERT(aActor);
  return static_cast<IPCStreamDestinationChild*>(aActor);
}

} // namespace ipc
} // namespace mozilla
