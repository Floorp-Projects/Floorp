/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientSourceParent.h"

#include "ClientHandleParent.h"
#include "ClientManagerService.h"
#include "ClientSourceOpParent.h"
#include "ClientValidation.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/PClientManagerParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::AssertIsOnBackgroundThread;
using mozilla::ipc::BackgroundParent;
using mozilla::ipc::IPCResult;
using mozilla::ipc::PrincipalInfo;

namespace {

// It would be nice to use a lambda instead of this class, but we cannot
// move capture in lambdas yet and ContentParent cannot be AddRef'd off
// the main thread.
class KillContentParentRunnable final : public Runnable
{
  RefPtr<ContentParent> mContentParent;

public:
  explicit KillContentParentRunnable(RefPtr<ContentParent>&& aContentParent)
    : Runnable("KillContentParentRunnable")
    , mContentParent(Move(aContentParent))
  {
    MOZ_ASSERT(mContentParent);
  }

  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mContentParent->KillHard("invalid ClientSourceParent actor");
    mContentParent = nullptr;
    return NS_OK;
  }
};

} // anonymous namespace

void
ClientSourceParent::KillInvalidChild()
{
  // Try to get the content process before we destroy the actor below.
  RefPtr<ContentParent> process =
    BackgroundParent::GetContentParent(Manager()->Manager());

  // First, immediately teardown the ClientSource actor.  No matter what
  // we want to start this process as soon as possible.
  Unused << ClientSourceParent::Send__delete__(this);

  // If we are running in non-e10s, then there is nothing else to do here.
  // There is no child process and we don't want to crash the entire browser
  // in release builds.  In general, though, this should not happen in non-e10s
  // so we do assert this condition.
  if (!process) {
    MOZ_DIAGNOSTIC_ASSERT(false, "invalid ClientSourceParent in non-e10s");
    return;
  }

  // In e10s mode we also want to kill the child process.  Validation failures
  // typically mean someone sent us bogus data over the IPC link.  We can't
  // trust that process any more.  We have to do this on the main thread, so
  // there is a small window of time before we kill the process.  This is why
  // we start the actor destruction immediately above.
  nsCOMPtr<nsIRunnable> r = new KillContentParentRunnable(Move(process));
  MOZ_ALWAYS_SUCCEEDS(SystemGroup::Dispatch(TaskCategory::Other, r.forget()));
}

mozilla::ipc::IPCResult
ClientSourceParent::RecvWorkerSyncPing()
{
  AssertIsOnBackgroundThread();
  // Do nothing here.  This is purely a sync message allowing the child to
  // confirm that the actor has been created on the parent process.
  return IPC_OK();
}

IPCResult
ClientSourceParent::RecvTeardown()
{
  Unused << Send__delete__(this);
  return IPC_OK();
}

IPCResult
ClientSourceParent::RecvExecutionReady(const ClientSourceExecutionReadyArgs& aArgs)
{
  // Now that we have the creation URL for the Client we can do some validation
  // to make sure the child actor is not giving us garbage.  Since we validate
  // on the child side as well we treat a failure here as fatal.
  if (!ClientIsValidCreationURL(mClientInfo.PrincipalInfo(), aArgs.url())) {
    KillInvalidChild();
    return IPC_OK();
  }

  mClientInfo.SetURL(aArgs.url());
  mClientInfo.SetFrameType(aArgs.frameType());
  mExecutionReady = true;

  for (ClientHandleParent* handle : mHandleList) {
    Unused << handle->SendExecutionReady(mClientInfo.ToIPC());
  }

  return IPC_OK();
};

IPCResult
ClientSourceParent::RecvFreeze()
{
  MOZ_DIAGNOSTIC_ASSERT(!mFrozen);
  mFrozen = true;

  // Frozen clients should not be observable.  Act as if the client has
  // been destroyed.
  nsTArray<ClientHandleParent*> handleList(mHandleList);
  for (ClientHandleParent* handle : handleList) {
    Unused << ClientHandleParent::Send__delete__(handle);
  }

  return IPC_OK();
}

IPCResult
ClientSourceParent::RecvThaw()
{
  MOZ_DIAGNOSTIC_ASSERT(mFrozen);
  mFrozen = false;
  return IPC_OK();
}

void
ClientSourceParent::ActorDestroy(ActorDestroyReason aReason)
{
  DebugOnly<bool> removed = mService->RemoveSource(this);
  MOZ_ASSERT(removed);

  nsTArray<ClientHandleParent*> handleList(mHandleList);
  for (ClientHandleParent* handle : handleList) {
    // This should trigger DetachHandle() to be called removing
    // the entry from the mHandleList.
    Unused << ClientHandleParent::Send__delete__(handle);
  }
  MOZ_DIAGNOSTIC_ASSERT(mHandleList.IsEmpty());
}

PClientSourceOpParent*
ClientSourceParent::AllocPClientSourceOpParent(const ClientOpConstructorArgs& aArgs)
{
  MOZ_ASSERT_UNREACHABLE("ClientSourceOpParent should be explicitly constructed.");
  return nullptr;
}

bool
ClientSourceParent::DeallocPClientSourceOpParent(PClientSourceOpParent* aActor)
{
  delete aActor;
  return true;
}

ClientSourceParent::ClientSourceParent(const ClientSourceConstructorArgs& aArgs)
  : mClientInfo(aArgs.id(), aArgs.type(), aArgs.principalInfo(), aArgs.creationTime())
  , mService(ClientManagerService::GetOrCreateInstance())
  , mExecutionReady(false)
  , mFrozen(false)
{
}

ClientSourceParent::~ClientSourceParent()
{
  MOZ_DIAGNOSTIC_ASSERT(mHandleList.IsEmpty());
}

void
ClientSourceParent::Init()
{
  // Ensure the principal is reasonable before adding ourself to the service.
  // Since we validate the principal on the child side as well, any failure
  // here is treated as fatal.
  if (NS_WARN_IF(!ClientIsValidPrincipalInfo(mClientInfo.PrincipalInfo()))) {
    KillInvalidChild();
    return;
  }

  // Its possible for AddSource() to fail if there is already an entry for
  // our UUID.  This should not normally happen, but could if someone is
  // spoofing IPC messages.
  if (NS_WARN_IF(!mService->AddSource(this))) {
    KillInvalidChild();
    return;
  }
}

const ClientInfo&
ClientSourceParent::Info() const
{
  return mClientInfo;
}

bool
ClientSourceParent::IsFrozen() const
{
  return mFrozen;
}

bool
ClientSourceParent::ExecutionReady() const
{
  return mExecutionReady;
}

void
ClientSourceParent::AttachHandle(ClientHandleParent* aClientHandle)
{
  MOZ_DIAGNOSTIC_ASSERT(aClientHandle);
  MOZ_DIAGNOSTIC_ASSERT(!mFrozen);
  MOZ_ASSERT(!mHandleList.Contains(aClientHandle));
  mHandleList.AppendElement(aClientHandle);
}

void
ClientSourceParent::DetachHandle(ClientHandleParent* aClientHandle)
{
  MOZ_DIAGNOSTIC_ASSERT(aClientHandle);
  MOZ_ASSERT(mHandleList.Contains(aClientHandle));
  mHandleList.RemoveElement(aClientHandle);
}

RefPtr<ClientOpPromise>
ClientSourceParent::StartOp(const ClientOpConstructorArgs& aArgs)
{
  RefPtr<ClientOpPromise::Private> promise =
    new ClientOpPromise::Private(__func__);

  // If we are being controlled, remember that data before propagating
  // on to the ClientSource.
  if (aArgs.type() == ClientOpConstructorArgs::TClientControlledArgs) {
    mController.reset();
    mController.emplace(aArgs.get_ClientControlledArgs().serviceWorker());
  }

  // Constructor failure will reject the promise via ActorDestroy().
  ClientSourceOpParent* actor = new ClientSourceOpParent(aArgs, promise);
  Unused << SendPClientSourceOpConstructor(actor, aArgs);

  return promise.forget();
}

} // namespace dom
} // namespace mozilla
