/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/unused.h"
#include "nsThreadUtils.h"
#include "NuwaParent.h"

using namespace mozilla::ipc;
using namespace mozilla::dom;
using namespace IPC;

namespace mozilla {
namespace dom {

/*static*/ NuwaParent*
NuwaParent::Alloc() {
  RefPtr<NuwaParent> actor = new NuwaParent();
  return actor.forget().take();
}

/*static*/ bool
NuwaParent::ActorConstructed(mozilla::dom::PNuwaParent *aActor)
{
  NuwaParent* actor = static_cast<NuwaParent*>(aActor);
  actor->ActorConstructed();

  return true;
}

/*static*/ bool
NuwaParent::Dealloc(mozilla::dom::PNuwaParent *aActor)
{
  RefPtr<NuwaParent> actor = dont_AddRef(static_cast<NuwaParent*>(aActor));
  return true;
}

NuwaParent::NuwaParent()
  : mBlocked(false)
  , mMonitor("NuwaParent")
  , mClonedActor(nullptr)
  , mWorkerThread(do_GetCurrentThread())
  , mNewProcessPid(0)
{
  AssertIsOnBackgroundThread();
}

NuwaParent::~NuwaParent()
{
  // Both the worker thread and the main thread (ContentParent) hold a ref to
  // this. The instance may be destroyed on either thread.
  MOZ_ASSERT(!mContentParent);
}

inline void
NuwaParent::AssertIsOnWorkerThread()
{
  nsCOMPtr<nsIThread> currentThread = do_GetCurrentThread();
  MOZ_ASSERT(currentThread ==  mWorkerThread);
}

bool
NuwaParent::ActorConstructed()
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(Manager());
  MOZ_ASSERT(!mContentParent);

  mContentParent = BackgroundParent::GetContentParent(Manager());
  if (!mContentParent) {
    return false;
  }

  // mContentParent is guaranteed to be alive. It's safe to set its backward ref
  // to this.
  mContentParent->SetNuwaParent(this);
  return true;
}

mozilla::ipc::IProtocol*
NuwaParent::CloneProtocol(Channel* aChannel,
                          ProtocolCloneContext* aCtx)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<NuwaParent> self = this;

  MonitorAutoLock lock(mMonitor);

  // Alloc NuwaParent on the worker thread.
  nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([self] () -> void
  {
    MonitorAutoLock lock(self->mMonitor);
    // XXX Calling NuwaParent::Alloc() leads to a compilation error. Use
    // self->Alloc() as a workaround.
    self->mClonedActor = self->Alloc();
    lock.Notify();
  });
  MOZ_ASSERT(runnable);
  MOZ_ALWAYS_SUCCEEDS(mWorkerThread->Dispatch(runnable, NS_DISPATCH_NORMAL));

  while (!mClonedActor) {
    lock.Wait();
  }
  RefPtr<NuwaParent> actor = mClonedActor;
  mClonedActor = nullptr;

  // mManager of the cloned actor is assigned after returning from this method.
  // We can't call ActorConstructed() right after Alloc() in the above runnable.
  // To be safe we dispatch a runnable to the current thread to do it.
  runnable = NS_NewRunnableFunction([actor] () -> void
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIRunnable> nested = NS_NewRunnableFunction([actor] () -> void
    {
      AssertIsOnBackgroundThread();

      // Call NuwaParent::ActorConstructed() on the worker thread.
      actor->ActorConstructed();

      // The actor can finally be deleted after fully constructed.
      mozilla::Unused << actor->Send__delete__(actor);
    });
    MOZ_ASSERT(nested);
    MOZ_ALWAYS_SUCCEEDS(actor->mWorkerThread->Dispatch(nested, NS_DISPATCH_NORMAL));
  });

  MOZ_ASSERT(runnable);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable));

  return actor;
}

void
NuwaParent::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnWorkerThread();

  RefPtr<NuwaParent> self = this;
  nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([self] () -> void
  {
    // These extra nsRefPtr serve as kungFuDeathGrip to keep both objects from
    // deletion in breaking the ref cycle.
    RefPtr<ContentParent> contentParent = self->mContentParent;

    contentParent->SetNuwaParent(nullptr);
    // Need to clear the ref to ContentParent on the main thread.
    self->mContentParent = nullptr;
  });
  MOZ_ASSERT(runnable);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable));
}

bool
NuwaParent::RecvNotifyReady()
{
#ifdef MOZ_NUWA_PROCESS
  if (!mContentParent || !mContentParent->IsNuwaProcess()) {
    NS_ERROR("Received NotifyReady() message from a non-Nuwa process.");
    return false;
  }

  // Creating a NonOwningRunnableMethod here is safe because refcount changes of
  // mContentParent have to go the the main thread. The mContentParent will
  // be alive when the runnable runs.
  nsCOMPtr<nsIRunnable> runnable =
    NewNonOwningRunnableMethod(mContentParent.get(),
                                  &ContentParent::OnNuwaReady);
  MOZ_ASSERT(runnable);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable));

  return true;
#else
  NS_ERROR("NuwaParent::RecvNotifyReady() not implemented!");
  return false;
#endif
}

bool
NuwaParent::RecvAddNewProcess(const uint32_t& aPid,
                              nsTArray<ProtocolFdMapping>&& aFds)
{
#ifdef MOZ_NUWA_PROCESS
  if (!mContentParent || !mContentParent->IsNuwaProcess()) {
    NS_ERROR("Received AddNewProcess() message from a non-Nuwa process.");
    return false;
  }

  mNewProcessPid = aPid;
  mNewProcessFds->SwapElements(aFds);
  MonitorAutoLock lock(mMonitor);
  if (mBlocked) {
    // Unblock ForkNewProcess().
    mMonitor.Notify();
    mBlocked = false;
  } else {
    nsCOMPtr<nsIRunnable> runnable =
      NewNonOwningRunnableMethod<
        uint32_t,
        UniquePtr<nsTArray<ProtocolFdMapping>>&& >(
          mContentParent.get(),
          &ContentParent::OnNewProcessCreated,
          mNewProcessPid,
          Move(mNewProcessFds));
    MOZ_ASSERT(runnable);
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable));
  }
  return true;
#else
  NS_ERROR("NuwaParent::RecvAddNewProcess() not implemented!");
  return false;
#endif
}

bool
NuwaParent::ForkNewProcess(uint32_t& aPid,
                           UniquePtr<nsTArray<ProtocolFdMapping>>&& aFds,
                           bool aBlocking)
{
  MOZ_ASSERT(mWorkerThread);
  MOZ_ASSERT(NS_IsMainThread());

  mNewProcessFds = Move(aFds);

  RefPtr<NuwaParent> self = this;
  nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([self] () -> void
  {
    mozilla::Unused << self->SendFork();
  });
  MOZ_ASSERT(runnable);
  MOZ_ALWAYS_SUCCEEDS(mWorkerThread->Dispatch(runnable, NS_DISPATCH_NORMAL));
  if (!aBlocking) {
    return false;
  }

  MonitorAutoLock lock(mMonitor);
  mBlocked = true;
  while (mBlocked) {
    // This will be notified in NuwaParent::RecvAddNewProcess().
    lock.Wait();
  }

  if (!mNewProcessPid) {
    return false;
  }

  aPid = mNewProcessPid;
  aFds = Move(mNewProcessFds);

  mNewProcessPid = 0;
  return true;
}

} // namespace dom
} // namespace mozilla
