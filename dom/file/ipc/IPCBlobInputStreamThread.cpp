/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCBlobInputStreamThread.h"

#include "mozilla/StaticMutex.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsIIPCBackgroundChildCreateCallback.h"
#include "nsXPCOMPrivate.h"

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

StaticMutex gIPCBlobThreadMutex;
StaticRefPtr<IPCBlobInputStreamThread> gIPCBlobThread;
bool gShutdownHasStarted = false;

class ThreadInitializeRunnable final : public Runnable
{
public:
  NS_IMETHOD
  Run() override
  {
     mozilla::StaticMutexAutoLock lock(gIPCBlobThreadMutex);
     MOZ_ASSERT(gIPCBlobThread);
     gIPCBlobThread->Initialize();
     return NS_OK;
  }
};

class MigrateActorRunnable final : public Runnable
                                 , public nsIIPCBackgroundChildCreateCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  explicit MigrateActorRunnable(IPCBlobInputStreamChild* aActor)
    : mActor(aActor)
  {
    MOZ_ASSERT(mActor);
  }

  NS_IMETHOD
  Run() override
  {
    BackgroundChild::GetOrCreateForCurrentThread(this);
    return NS_OK;
  }

  void
  ActorFailed() override
  {
    // We cannot continue. We are probably shutting down.
  }

  void
  ActorCreated(mozilla::ipc::PBackgroundChild* aActor) override
  {
    MOZ_ASSERT(mActor->State() == IPCBlobInputStreamChild::eInactiveMigrating);

    if (aActor->SendPIPCBlobInputStreamConstructor(mActor, mActor->ID(),
                                                   mActor->Size())) {
      // We need manually to increase the reference for this actor because the
      // IPC allocator method is not triggered. The Release() is called by IPDL
      // when the actor is deleted.
      mActor.get()->AddRef();
      mActor->Migrated();
    }
  }

private:
  ~MigrateActorRunnable() = default;

  RefPtr<IPCBlobInputStreamChild> mActor;
};

NS_IMPL_ISUPPORTS_INHERITED(MigrateActorRunnable, Runnable,
                  nsIIPCBackgroundChildCreateCallback)

} // anonymous

NS_IMPL_ISUPPORTS(IPCBlobInputStreamThread, nsIObserver)

/* static */ bool
IPCBlobInputStreamThread::IsOnFileEventTarget(nsIEventTarget* aEventTarget)
{
  MOZ_ASSERT(aEventTarget);

  mozilla::StaticMutexAutoLock lock(gIPCBlobThreadMutex);
  return gIPCBlobThread && aEventTarget == gIPCBlobThread->mThread;
}

/* static */ IPCBlobInputStreamThread*
IPCBlobInputStreamThread::GetOrCreate()
{
  mozilla::StaticMutexAutoLock lock(gIPCBlobThreadMutex);

  if (gShutdownHasStarted) {
    return nullptr;
  }

  if (!gIPCBlobThread) {
    gIPCBlobThread = new IPCBlobInputStreamThread();
    gIPCBlobThread->Initialize();
  }

  return gIPCBlobThread;
}

void
IPCBlobInputStreamThread::Initialize()
{
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(new ThreadInitializeRunnable());
    return;
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return;
  }

  nsresult rv =
    obs->AddObserver(this, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIThread> thread;
  rv = NS_NewNamedThread("DOM File", getter_AddRefs(thread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  mThread = thread;

  if (!mPendingActors.IsEmpty()) {
    for (uint32_t i = 0; i < mPendingActors.Length(); ++i) {
      MigrateActorInternal(mPendingActors[i]);
    }

    mPendingActors.Clear();
  }
}

NS_IMETHODIMP
IPCBlobInputStreamThread::Observe(nsISupports* aSubject,
                                  const char* aTopic,
                                  const char16_t* aData)
{
  MOZ_ASSERT(!strcmp(aTopic, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID));

  mozilla::StaticMutexAutoLock lock(gIPCBlobThreadMutex);

  if (mThread) {
    mThread->Shutdown();
    mThread = nullptr;
  }

  gShutdownHasStarted = true;
  gIPCBlobThread = nullptr;

  return NS_OK;
}

void
IPCBlobInputStreamThread::MigrateActor(IPCBlobInputStreamChild* aActor)
{
  MOZ_ASSERT(aActor->State() == IPCBlobInputStreamChild::eInactiveMigrating);

  mozilla::StaticMutexAutoLock lock(gIPCBlobThreadMutex);

  if (gShutdownHasStarted) {
    return;
  }

  if (!mThread) {
    // The thread is not initialized yet.
    mPendingActors.AppendElement(aActor);
    return;
  }

  MigrateActorInternal(aActor);
}

void
IPCBlobInputStreamThread::MigrateActorInternal(IPCBlobInputStreamChild* aActor)
{
  RefPtr<Runnable> runnable = new MigrateActorRunnable(aActor);
  mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

} // dom namespace
} // mozilla namespace
