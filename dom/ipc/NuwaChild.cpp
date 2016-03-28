/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentChild.h"
#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#endif
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/ProtocolUtils.h"
#if defined(MOZ_CONTENT_SANDBOX)
#if defined(XP_LINUX)
#include "mozilla/Sandbox.h"
#include "mozilla/SandboxInfo.h"
#elif defined(XP_MACOSX)
#include "mozilla/Sandbox.h"
#endif
#endif
#include "mozilla/unused.h"
#include "nsXULAppAPI.h"
#include "NuwaChild.h"


using namespace mozilla::ipc;
using namespace mozilla::dom;

namespace mozilla {
namespace dom {

#ifdef MOZ_NUWA_PROCESS

namespace {

class CallNuwaSpawn: public nsRunnable
{
public:
  NS_IMETHOD Run()
  {
    NuwaSpawn();
    if (IsNuwaProcess()) {
      return NS_OK;
    }

    // In the new process.
    ContentChild* child = ContentChild::GetSingleton();
    child->InitProcessAttributes();

    // Perform other after-fork initializations.
    InitOnContentProcessCreated();

    return NS_OK;
  }
};

static void
DoNuwaFork()
{
  NuwaSpawnPrepare(); // NuwaSpawn will be blocked.

  {
    nsCOMPtr<nsIRunnable> callSpawn(new CallNuwaSpawn());
    NS_DispatchToMainThread(callSpawn);
  }

  // IOThread should be blocked here for waiting NuwaSpawn().
  NuwaSpawnWait(); // Now! NuwaSpawn can go.
  // Here, we can make sure the spawning was finished.
}

/**
 * This function should keep IO thread in a stable state and freeze it
 * until the spawning is finished.
 */
static void
RunNuwaFork()
{
  if (NuwaCheckpointCurrentThread()) {
    DoNuwaFork();
  }
}

static bool sNuwaForking = false;

void
NuwaFork()
{
  if (sNuwaForking) {           // No reentry.
      return;
  }
  sNuwaForking = true;

  MessageLoop* ioloop = XRE_GetIOMessageLoop();
  ioloop->PostTask(FROM_HERE, NewRunnableFunction(RunNuwaFork));
}

} // Anonymous namespace.

#endif

NuwaChild* NuwaChild::sSingleton;

NuwaChild*
NuwaChild::GetSingleton()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sSingleton) {
    PNuwaChild* nuwaChild =
      BackgroundChild::GetForCurrentThread()->SendPNuwaConstructor();
    MOZ_ASSERT(nuwaChild);

    sSingleton = static_cast<NuwaChild*>(nuwaChild);
  }

  return sSingleton;
}


bool
NuwaChild::RecvFork()
{
#ifdef MOZ_NUWA_PROCESS
  if (!IsNuwaProcess()) {
    NS_ERROR(
      nsPrintfCString(
        "Terminating child process %d for unauthorized IPC message: "
          "RecvFork(%d)", getpid()).get());
    return false;
  }

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableFunction(&NuwaFork);
  MOZ_ASSERT(runnable);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable));

  return true;
#else
  NS_ERROR("NuwaChild::RecvFork() not implemented!");
  return false;
#endif
}

} // namespace dom
} // namespace mozilla


extern "C" {

#if defined(MOZ_NUWA_PROCESS)
NS_EXPORT void
GetProtoFdInfos(NuwaProtoFdInfo* aInfoList,
                size_t aInfoListSize,
                size_t* aInfoSize)
{
  size_t i = 0;

  mozilla::dom::ContentChild* content =
    mozilla::dom::ContentChild::GetSingleton();
  aInfoList[i].protoId = content->GetProtocolId();
  aInfoList[i].originFd =
    content->GetTransport()->GetFileDescriptor();
  i++;

  IToplevelProtocol* actors[NUWA_TOPLEVEL_MAX];
  size_t count = content->GetOpenedActorsUnsafe(actors, ArrayLength(actors));
  for (size_t j = 0; j < count; j++) {
    IToplevelProtocol* actor = actors[j];
    if (i >= aInfoListSize) {
      NS_RUNTIMEABORT("Too many top level protocols!");
    }

    aInfoList[i].protoId = actor->GetProtocolId();
    aInfoList[i].originFd =
      actor->GetTransport()->GetFileDescriptor();
    i++;
  }

  if (i > NUWA_TOPLEVEL_MAX) {
    NS_RUNTIMEABORT("Too many top level protocols!");
  }
  *aInfoSize = i;
}

class RunAddNewIPCProcess : public nsRunnable
{
public:
  RunAddNewIPCProcess(pid_t aPid,
                      nsTArray<mozilla::ipc::ProtocolFdMapping>& aMaps)
      : mPid(aPid)
  {
    mMaps.SwapElements(aMaps);
  }

  NS_IMETHOD Run()
  {
    NuwaChild::GetSingleton()->SendAddNewProcess(mPid, mMaps);

    MOZ_ASSERT(sNuwaForking);
    sNuwaForking = false;

    return NS_OK;
  }

private:
  pid_t mPid;
  nsTArray<mozilla::ipc::ProtocolFdMapping> mMaps;
};

/**
 * AddNewIPCProcess() is called by Nuwa process to tell the parent
 * process that a new process is created.
 *
 * In the newly created process, ResetContentChildTransport() is called to
 * reset fd for the IPC Channel and the session.
 */
NS_EXPORT void
AddNewIPCProcess(pid_t aPid, NuwaProtoFdInfo* aInfoList, size_t aInfoListSize)
{
  nsTArray<mozilla::ipc::ProtocolFdMapping> maps;

  for (size_t i = 0; i < aInfoListSize; i++) {
    int _fd = aInfoList[i].newFds[NUWA_NEWFD_PARENT];
    mozilla::ipc::FileDescriptor fd(_fd);
    mozilla::ipc::ProtocolFdMapping map(aInfoList[i].protoId, fd);
    maps.AppendElement(map);
  }

  RefPtr<RunAddNewIPCProcess> runner = new RunAddNewIPCProcess(aPid, maps);
  NS_DispatchToMainThread(runner);
}

NS_EXPORT void
OnNuwaProcessReady()
{
  NuwaChild* nuwaChild = NuwaChild::GetSingleton();
  MOZ_ASSERT(nuwaChild);

  mozilla::Unused << nuwaChild->SendNotifyReady();
}

NS_EXPORT void
AfterNuwaFork()
{
  SetCurrentProcessPrivileges(base::PRIVILEGES_DEFAULT);
#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  mozilla::SandboxEarlyInit(XRE_GetProcessType(), /* isNuwa: */ false);
#endif
}

#endif // MOZ_NUWA_PROCESS

}
