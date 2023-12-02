/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ForkServiceChild.h"
#include "ForkServer.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "mozilla/Logging.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/ipc/ProtocolMessageUtils.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Services.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "nsIObserverService.h"

#include <unistd.h>
#include <fcntl.h>

namespace mozilla {
namespace ipc {

extern LazyLogModule gForkServiceLog;

mozilla::UniquePtr<ForkServiceChild> ForkServiceChild::sForkServiceChild;

static bool ConfigurePipeFd(int aFd) {
  int flags = fcntl(aFd, F_GETFD, 0);
  return flags != -1 && fcntl(aFd, F_SETFD, flags | FD_CLOEXEC) != -1;
}

void ForkServiceChild::StartForkServer() {
  // Create the socket to use for communication, and mark both ends as
  // FD_CLOEXEC.
  int fds[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
    MOZ_LOG(gForkServiceLog, LogLevel::Error,
            ("failed to create fork server socket"));
    return;
  }
  UniqueFileHandle server(fds[0]);
  UniqueFileHandle client(fds[1]);

  if (!ConfigurePipeFd(server.get()) || !ConfigurePipeFd(client.get())) {
    MOZ_LOG(gForkServiceLog, LogLevel::Error,
            ("failed to configure fork server socket"));
    return;
  }

  GeckoChildProcessHost* subprocess =
      new GeckoChildProcessHost(GeckoProcessType_ForkServer, false);
  subprocess->AddFdToRemap(client.get(), ForkServer::kClientPipeFd);
  if (!subprocess->LaunchAndWaitForProcessHandle(std::vector<std::string>{})) {
    MOZ_LOG(gForkServiceLog, LogLevel::Error, ("failed to launch fork server"));
    return;
  }

  sForkServiceChild =
      mozilla::MakeUnique<ForkServiceChild>(server.release(), subprocess);
}

void ForkServiceChild::StopForkServer() { sForkServiceChild = nullptr; }

ForkServiceChild::ForkServiceChild(int aFd, GeckoChildProcessHost* aProcess)
    : mFailed(false), mProcess(aProcess) {
  mTcver = MakeUnique<MiniTransceiver>(aFd);
}

ForkServiceChild::~ForkServiceChild() {
  mProcess->Destroy();
  close(mTcver->GetFD());
}

Result<Ok, LaunchError> ForkServiceChild::SendForkNewSubprocess(
    const Args& aArgs, pid_t* aPid) {
  mRecvPid = -1;
  IPC::Message msg(MSG_ROUTING_CONTROL, Msg_ForkNewSubprocess__ID);

  IPC::MessageWriter writer(msg);
#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  WriteIPDLParam(&writer, nullptr, aArgs.mForkFlags);
  WriteIPDLParam(&writer, nullptr, aArgs.mChroot);
#endif
  WriteIPDLParam(&writer, nullptr, aArgs.mArgv);
  WriteIPDLParam(&writer, nullptr, aArgs.mEnv);
  WriteIPDLParam(&writer, nullptr, aArgs.mFdsRemap);
  if (!mTcver->Send(msg)) {
    MOZ_LOG(gForkServiceLog, LogLevel::Verbose,
            ("the pipe to the fork server is closed or having errors"));
    OnError();
    return Err(LaunchError("FSC::SFNS::Send"));
  }

  UniquePtr<IPC::Message> reply;
  if (!mTcver->Recv(reply)) {
    MOZ_LOG(gForkServiceLog, LogLevel::Verbose,
            ("the pipe to the fork server is closed or having errors"));
    OnError();
    return Err(LaunchError("FSC::SFNS::Recv"));
  }
  OnMessageReceived(std::move(reply));

  MOZ_ASSERT(mRecvPid != -1);
  *aPid = mRecvPid;
  return Ok();
}

void ForkServiceChild::OnMessageReceived(UniquePtr<IPC::Message> message) {
  if (message->type() != Reply_ForkNewSubprocess__ID) {
    MOZ_LOG(gForkServiceLog, LogLevel::Verbose,
            ("unknown reply type %d", message->type()));
    return;
  }
  IPC::MessageReader reader(*message);

  if (!ReadIPDLParam(&reader, nullptr, &mRecvPid)) {
    MOZ_CRASH("Error deserializing 'pid_t'");
  }
  reader.EndRead();
}

void ForkServiceChild::OnError() {
  mFailed = true;
  ForkServerLauncher::RestartForkServer();
}

NS_IMPL_ISUPPORTS(ForkServerLauncher, nsIObserver)

bool ForkServerLauncher::mHaveStartedClient = false;
StaticRefPtr<ForkServerLauncher> ForkServerLauncher::mSingleton;

ForkServerLauncher::ForkServerLauncher() {}

ForkServerLauncher::~ForkServerLauncher() {}

already_AddRefed<ForkServerLauncher> ForkServerLauncher::Create() {
  if (mSingleton == nullptr) {
    mSingleton = new ForkServerLauncher();
  }
  RefPtr<ForkServerLauncher> launcher = mSingleton;
  return launcher.forget();
}

NS_IMETHODIMP
ForkServerLauncher::Observe(nsISupports* aSubject, const char* aTopic,
                            const char16_t* aData) {
  if (strcmp(aTopic, NS_XPCOM_STARTUP_CATEGORY) == 0) {
    nsCOMPtr<nsIObserverService> obsSvc =
        mozilla::services::GetObserverService();
    MOZ_ASSERT(obsSvc != nullptr);
    // preferences are not available until final-ui-startup
    obsSvc->AddObserver(this, "final-ui-startup", false);
  } else if (!mHaveStartedClient && strcmp(aTopic, "final-ui-startup") == 0) {
    if (StaticPrefs::dom_ipc_forkserver_enable_AtStartup()) {
      mHaveStartedClient = true;
      ForkServiceChild::StartForkServer();

      nsCOMPtr<nsIObserverService> obsSvc =
          mozilla::services::GetObserverService();
      MOZ_ASSERT(obsSvc != nullptr);
      obsSvc->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
    } else {
      mSingleton = nullptr;
    }
  }

  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    if (mHaveStartedClient) {
      mHaveStartedClient = false;
      ForkServiceChild::StopForkServer();
    }

    // To make leak checker happy!
    mSingleton = nullptr;
  }
  return NS_OK;
}

void ForkServerLauncher::RestartForkServer() {
  // Restart fork server
  NS_SUCCEEDED(NS_DispatchToMainThreadQueue(
      NS_NewRunnableFunction("OnForkServerError",
                             [] {
                               if (mSingleton) {
                                 ForkServiceChild::StopForkServer();
                                 ForkServiceChild::StartForkServer();
                               }
                             }),
      EventQueuePriority::Idle));
}

}  // namespace ipc
}  // namespace mozilla
