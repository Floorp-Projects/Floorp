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

void ForkServiceChild::StartForkServer() {
  std::vector<std::string> extraArgs;

  GeckoChildProcessHost* subprocess =
      new GeckoChildProcessHost(GeckoProcessType_ForkServer, false);
  subprocess->LaunchAndWaitForProcessHandle(std::move(extraArgs));

  int fd = subprocess->GetChannel()->GetFileDescriptor();
  fd = dup(fd);  // Dup it because the channel will close it.
  int fs_flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, fs_flags & ~O_NONBLOCK);
  int fd_flags = fcntl(fd, F_GETFD, 0);
  fcntl(fd, F_SETFD, fd_flags | FD_CLOEXEC);

  sForkServiceChild = mozilla::MakeUnique<ForkServiceChild>(fd, subprocess);

  // Without doing this, IO thread may intercept messages since the
  // IPC::Channel created by it is still open.
  subprocess->GetChannel()->Close();
}

void ForkServiceChild::StopForkServer() { sForkServiceChild = nullptr; }

ForkServiceChild::ForkServiceChild(int aFd, GeckoChildProcessHost* aProcess)
    : mWaitForHello(true), mFailed(false), mProcess(aProcess) {
  mTcver = MakeUnique<MiniTransceiver>(aFd);
}

ForkServiceChild::~ForkServiceChild() {
  mProcess->Destroy();
  close(mTcver->GetFD());
}

bool ForkServiceChild::SendForkNewSubprocess(
    const nsTArray<nsCString>& aArgv, const nsTArray<EnvVar>& aEnvMap,
    const nsTArray<FdMapping>& aFdsRemap, pid_t* aPid) {
  if (mWaitForHello) {
    // IPC::Channel created by the GeckoChildProcessHost has
    // already send a HELLO.  It is expected to receive a hello
    // message from the fork server too.
    IPC::Message hello;
    mTcver->RecvInfallible(hello, "Fail to receive HELLO message");
    MOZ_ASSERT(hello.type() == ForkServer::kHELLO_MESSAGE_TYPE);
    mWaitForHello = false;
  }

  mRecvPid = -1;
  IPC::Message msg(MSG_ROUTING_CONTROL, Msg_ForkNewSubprocess__ID);

  WriteIPDLParam(&msg, nullptr, aArgv);
  WriteIPDLParam(&msg, nullptr, aEnvMap);
  WriteIPDLParam(&msg, nullptr, aFdsRemap);
  if (!mTcver->Send(msg)) {
    MOZ_LOG(gForkServiceLog, LogLevel::Verbose,
            ("the pipe to the fork server is closed or having errors"));
    OnError();
    return false;
  }

  IPC::Message reply;
  if (!mTcver->Recv(reply)) {
    MOZ_LOG(gForkServiceLog, LogLevel::Verbose,
            ("the pipe to the fork server is closed or having errors"));
    OnError();
    return false;
  }
  OnMessageReceived(std::move(reply));

  MOZ_ASSERT(mRecvPid != -1);
  *aPid = mRecvPid;
  return true;
}

void ForkServiceChild::OnMessageReceived(IPC::Message&& message) {
  if (message.type() != Reply_ForkNewSubprocess__ID) {
    MOZ_LOG(gForkServiceLog, LogLevel::Verbose,
            ("unknown reply type %d", message.type()));
    return;
  }
  PickleIterator iter__(message);

  if (!ReadIPDLParam(&message, &iter__, nullptr, &mRecvPid)) {
    MOZ_CRASH("Error deserializing 'pid_t'");
  }
  message.EndRead(iter__, message.type());
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
