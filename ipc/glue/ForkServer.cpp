/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/ipc/ForkServer.h"
#include "mozilla/Logging.h"
#include "chrome/common/chrome_switches.h"
#include "mozilla/BlockingResourceBase.h"
#include "mozilla/ipc/ProtocolMessageUtils.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "nsTraceRefcnt.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
#  include "mozilla/SandboxLaunch.h"
#endif

#include <algorithm>

namespace mozilla {
namespace ipc {

static const int sClientFd = 3;

LazyLogModule gForkServiceLog("ForkService");

ForkServer::ForkServer() {}

/**
 * Prepare an environment for running a fork server.
 */
void ForkServer::InitProcess(int* aArgc, char*** aArgv) {
  base::InitForkServerProcess();

  int fd = sClientFd;
  int fd_flags = fcntl(sClientFd, F_GETFL, 0);
  fcntl(fd, F_SETFL, fd_flags & ~O_NONBLOCK);
  mTcver = MakeUnique<MiniTransceiver>(fd, DataBufferClear::AfterReceiving);
}

/**
 * Start providing the service at the IPC channel.
 */
bool ForkServer::HandleMessages() {
  // |sClientFd| is created by an instance of |IPC::Channel|.
  // It sends a HELLO automatically.
  IPC::Message hello;
  mTcver->RecvInfallible(
      hello, "Expect to receive a HELLO message from the parent process!");
  MOZ_ASSERT(hello.type() == kHELLO_MESSAGE_TYPE);

  // Send it back
  mTcver->SendInfallible(hello, "Fail to ack the received HELLO!");

  while (true) {
    IPC::Message msg;
    if (!mTcver->Recv(msg)) {
      break;
    }

    OnMessageReceived(std::move(msg));

    if (mAppProcBuilder) {
      // New process - child
      return false;
    }
  }
  // Stop the server
  return true;
}

inline void CleanCString(nsCString& str) {
  char* data;
  int sz = str.GetMutableData(&data);

  memset(data, ' ', sz);
}

inline void CleanString(std::string& str) {
  const char deadbeef[] =
      "\xde\xad\xbe\xef\xde\xad\xbe\xef\xde\xad\xbe\xef\xde\xad\xbe\xef"
      "\xde\xad\xbe\xef\xde\xad\xbe\xef\xde\xad\xbe\xef\xde\xad\xbe\xef";
  int pos = 0;
  size_t sz = str.size();
  while (sz > 0) {
    int toclean = std::min(sz, sizeof(deadbeef) - 1);
    str.replace(pos, toclean, deadbeef);
    sz -= toclean;
    pos += toclean;
  }
}

inline void PrepareArguments(std::vector<std::string>& aArgv,
                             nsTArray<nsCString>& aArgvArray) {
  for (auto& elt : aArgvArray) {
    aArgv.push_back(elt.get());
    CleanCString(elt);
  }
}

// Prepare aOptions->env_map
inline void PrepareEnv(base::LaunchOptions* aOptions,
                       nsTArray<EnvVar>& aEnvMap) {
  for (auto& elt : aEnvMap) {
    nsCString& var = Get<0>(elt);
    nsCString& val = Get<1>(elt);
    aOptions->env_map[var.get()] = val.get();
    CleanCString(var);
    CleanCString(val);
  }
}

// Prepare aOptions->fds_to_remap
inline void PrepareFdsRemap(base::LaunchOptions* aOptions,
                            nsTArray<FdMapping>& aFdsRemap) {
  MOZ_LOG(gForkServiceLog, LogLevel::Verbose, ("fds mapping:"));
  for (auto& elt : aFdsRemap) {
    // FDs are duplicated here.
    int fd = Get<0>(elt).ClonePlatformHandle().release();
    std::pair<int, int> fdmap(fd, Get<1>(elt));
    aOptions->fds_to_remap.push_back(fdmap);
    MOZ_LOG(gForkServiceLog, LogLevel::Verbose,
            ("\t%d => %d", fdmap.first, fdmap.second));
  }
}

/**
 * Parse a Message to get a list of arguments and fill a LaunchOptions.
 */
inline bool ParseForkNewSubprocess(IPC::Message& aMsg,
                                   std::vector<std::string>& aArgv,
                                   base::LaunchOptions* aOptions) {
  if (aMsg.type() != Msg_ForkNewSubprocess__ID) {
    MOZ_LOG(gForkServiceLog, LogLevel::Verbose,
            ("unknown message type %d\n", aMsg.type()));
    return false;
  }

  PickleIterator iter(aMsg);
  nsTArray<nsCString> argv_array;
  nsTArray<EnvVar> env_map;
  nsTArray<FdMapping> fds_remap;

  ReadIPDLParamInfallible(&aMsg, &iter, nullptr, &argv_array,
                          "Error deserializing 'nsCString[]'");
  ReadIPDLParamInfallible(&aMsg, &iter, nullptr, &env_map,
                          "Error deserializing 'EnvVar[]'");
  ReadIPDLParamInfallible(&aMsg, &iter, nullptr, &fds_remap,
                          "Error deserializing 'FdMapping[]'");
  aMsg.EndRead(iter, aMsg.type());

  PrepareArguments(aArgv, argv_array);
  PrepareEnv(aOptions, env_map);
  PrepareFdsRemap(aOptions, fds_remap);

  return true;
}

inline void SanitizeBuffers(IPC::Message& aMsg, std::vector<std::string>& aArgv,
                            base::LaunchOptions& aOptions) {
  // Clean all buffers in the message to make sure content processes
  // not peeking others.
  auto& blist = aMsg.Buffers();
  for (auto itr = blist.Iter(); !itr.Done();
       itr.Advance(blist, itr.RemainingInSegment())) {
    memset(itr.Data(), 0, itr.RemainingInSegment());
  }

  // clean all data string made from the message.
  for (auto& var : aOptions.env_map) {
    // Do it anyway since it is not going to be used anymore.
    CleanString(*const_cast<std::string*>(&var.first));
    CleanString(var.second);
  }
  for (auto& arg : aArgv) {
    CleanString(arg);
  }
}

/**
 * Extract parameters from the |Message| to create a
 * |base::AppProcessBuilder| as |mAppProcBuilder|.
 *
 * It will return in both the fork server process and the new content
 * process.  |mAppProcBuilder| is null for the fork server.
 */
void ForkServer::OnMessageReceived(IPC::Message&& message) {
  IPC::Message msg(std::move(message));

  std::vector<std::string> argv;
  base::LaunchOptions options;
  if (!ParseForkNewSubprocess(msg, argv, &options)) {
    return;
  }

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  mozilla::SandboxLaunchForkServerPrepare(argv, options);
#endif

  base::ProcessHandle child_pid = -1;
  mAppProcBuilder = MakeUnique<base::AppProcessBuilder>();
  if (!mAppProcBuilder->ForkProcess(argv, options, &child_pid)) {
    MOZ_CRASH("fail to fork");
  }
  MOZ_ASSERT(child_pid >= 0);

  if (child_pid == 0) {
    // Content process
    return;
  }

  // Fork server process

  mAppProcBuilder = nullptr;

  IPC::Message reply(MSG_ROUTING_CONTROL, Reply_ForkNewSubprocess__ID);
  WriteIPDLParam(&reply, nullptr, child_pid);
  mTcver->SendInfallible(reply, "failed to send a reply message");

  // Without this, the content processes that is forked later are
  // able to read the content of buffers even the buffers have been
  // released.
  SanitizeBuffers(msg, argv, options);
}

/**
 * Setup and run a fork server at the main thread.
 *
 * This function returns for two reasons:
 *  - the fork server is stopped normally, or
 *  - a new process is forked from the fork server and this function
 *    returned in the child, the new process.
 *
 * For the later case, aArgc and aArgv are modified to pass the
 * arguments from the chrome process.
 */
bool ForkServer::RunForkServer(int* aArgc, char*** aArgv) {
#ifdef DEBUG
  if (getenv("MOZ_FORKSERVER_WAIT_GDB")) {
    printf(
        "Waiting for 30 seconds."
        "  Attach the fork server with gdb %s %d\n",
        (*aArgv)[0], base::GetCurrentProcId());
    sleep(30);
  }
  bool sleep_newproc = !!getenv("MOZ_FORKSERVER_WAIT_GDB_NEWPROC");
#endif

  // Do this before NS_LogInit() to avoid log files taking lower
  // FDs.
  ForkServer forkserver;
  forkserver.InitProcess(aArgc, aArgv);

  XRE_SetProcessType("forkserver");
  NS_LogInit();
  mozilla::LogModule::Init(0, nullptr);
  MOZ_LOG(gForkServiceLog, LogLevel::Verbose, ("Start a fork server"));
  {
    DebugOnly<base::ProcessHandle> forkserver_pid = base::GetCurrentProcId();
    if (forkserver.HandleMessages()) {
      // In the fork server process
      // The server has stopped.
      MOZ_LOG(gForkServiceLog, LogLevel::Verbose,
              ("Terminate the fork server"));
      NS_LogTerm();
      return true;
    }
    // Now, we are running in a content process just forked from
    // the fork server process.
    MOZ_ASSERT(base::GetCurrentProcId() != forkserver_pid);
    MOZ_LOG(gForkServiceLog, LogLevel::Verbose, ("Fork a new content process"));
  }
#ifdef DEBUG
  if (sleep_newproc) {
    printf(
        "Waiting for 30 seconds."
        "  Attach the new process with gdb %s %d\n",
        (*aArgv)[0], base::GetCurrentProcId());
    sleep(30);
  }
#endif
  NS_LogTerm();

  MOZ_ASSERT(forkserver.mAppProcBuilder);
  // |messageloop| has been destroyed.  So, we can intialized the
  // process safely.  Message loops may allocates some file
  // descriptors.  If it is destroyed later, it may mess up this
  // content process by closing wrong file descriptors.
  forkserver.mAppProcBuilder->InitAppProcess(aArgc, aArgv);
  forkserver.mAppProcBuilder.reset();

  MOZ_ASSERT("tab"_ns == (*aArgv)[*aArgc - 1], "Only |tab| is allowed!");

  // Open log files again with right names and the new PID.
  nsTraceRefcnt::ResetLogFiles((*aArgv)[*aArgc - 1]);

  return false;
}

}  // namespace ipc
}  // namespace mozilla
