/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/ForkServer.h"

#include "chrome/common/chrome_switches.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "mozilla/BlockingResourceBase.h"
#include "mozilla/Logging.h"
#include "mozilla/Omnijar.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "mozilla/ipc/ProtocolMessageUtils.h"
#include "mozilla/ipc/SetProcessTitle.h"
#include "nsTraceRefcnt.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
#  include "mozilla/SandboxLaunch.h"
#endif

#include <algorithm>

namespace mozilla {
namespace ipc {

LazyLogModule gForkServiceLog("ForkService");

ForkServer::ForkServer() {}

/**
 * Prepare an environment for running a fork server.
 */
void ForkServer::InitProcess(int* aArgc, char*** aArgv) {
  base::InitForkServerProcess();

  mTcver = MakeUnique<MiniTransceiver>(kClientPipeFd,
                                       DataBufferClear::AfterReceiving);
}

/**
 * Preload any resources that the forked child processes might need,
 * and which might change incompatibly or become unavailable by the
 * time they're started.  For example: the omnijar files, or certain
 * shared libraries.
 */
static void ForkServerPreload(int& aArgc, char** aArgv) {
  Omnijar::ChildProcessInit(aArgc, aArgv);
}

/**
 * Start providing the service at the IPC channel.
 */
bool ForkServer::HandleMessages() {
  while (true) {
    UniquePtr<IPC::Message> msg;
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
    nsCString& var = std::get<0>(elt);
    nsCString& val = std::get<1>(elt);
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
    int fd = std::get<0>(elt).ClonePlatformHandle().release();
    std::pair<int, int> fdmap(fd, std::get<1>(elt));
    aOptions->fds_to_remap.push_back(fdmap);
    MOZ_LOG(gForkServiceLog, LogLevel::Verbose,
            ("\t%d => %d", fdmap.first, fdmap.second));
  }
}

template <class P>
static void ReadParamInfallible(IPC::MessageReader* aReader, P* aResult,
                                const char* aCrashMessage) {
  if (!IPC::ReadParam(aReader, aResult)) {
    MOZ_CRASH_UNSAFE(aCrashMessage);
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

  IPC::MessageReader reader(aMsg);
  nsTArray<nsCString> argv_array;
  nsTArray<EnvVar> env_map;
  nsTArray<FdMapping> fds_remap;

  ReadParamInfallible(&reader, &argv_array,
                      "Error deserializing 'nsCString[]'");
  ReadParamInfallible(&reader, &env_map, "Error deserializing 'EnvVar[]'");
  ReadParamInfallible(&reader, &fds_remap, "Error deserializing 'FdMapping[]'");
  reader.EndRead();

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
void ForkServer::OnMessageReceived(UniquePtr<IPC::Message> message) {
  std::vector<std::string> argv;
  base::LaunchOptions options;
  if (!ParseForkNewSubprocess(*message, argv, &options)) {
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
  IPC::MessageWriter writer(reply);
  WriteIPDLParam(&writer, nullptr, child_pid);
  mTcver->SendInfallible(reply, "failed to send a reply message");

  // Without this, the content processes that is forked later are
  // able to read the content of buffers even the buffers have been
  // released.
  SanitizeBuffers(*message, argv, options);
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

  SetProcessTitleInit(*aArgv);

  // Do this before NS_LogInit() to avoid log files taking lower
  // FDs.
  ForkServer forkserver;
  forkserver.InitProcess(aArgc, aArgv);

  XRE_SetProcessType("forkserver");
  NS_LogInit();
  mozilla::LogModule::Init(0, nullptr);
  ForkServerPreload(*aArgc, *aArgv);
  MOZ_LOG(gForkServiceLog, LogLevel::Verbose, ("Start a fork server"));
  {
    DebugOnly<base::ProcessHandle> forkserver_pid = base::GetCurrentProcId();
    if (forkserver.HandleMessages()) {
      // In the fork server process
      // The server has stopped.
      MOZ_LOG(gForkServiceLog, LogLevel::Verbose,
              ("Terminate the fork server"));
      Omnijar::CleanUp();
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

  // Open log files again with right names and the new PID.
  nsTraceRefcnt::ResetLogFiles((*aArgv)[*aArgc - 1]);

  return false;
}

}  // namespace ipc
}  // namespace mozilla
