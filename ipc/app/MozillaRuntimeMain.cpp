/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../contentproc/plugin-container.cpp"

#include "mozilla/Bootstrap.h"
#include "mozilla/RuntimeExceptionModule.h"
#include "mozilla/ScopeExit.h"
#if defined(XP_WIN)
#  include "mozilla/WindowsDllBlocklist.h"
#  include "mozilla/GeckoArgs.h"
#endif  // defined(XP_WIN)

using namespace mozilla;

static bool UseForkServer(int argc, char* argv[]) {
#if defined(MOZ_ENABLE_FORKSERVER)
  return strcmp(argv[argc - 1], "forkserver") == 0;
#else
  return false;
#endif
}

static int RunForkServer(Bootstrap::UniquePtr&& bootstrap, int argc,
                         char* argv[]) {
#if defined(MOZ_ENABLE_FORKSERVER)
  int ret = 0;

  bootstrap->NS_LogInit();

  // Run a fork server in this process, single thread.  When it
  // returns, it means the fork server have been stopped or a new
  // content process is created.
  //
  // For the later case, XRE_ForkServer() will return false, running
  // in a content process just forked from the fork server process.
  // argc & argv will be updated with the values passing from the
  // chrome process.  With the new values, this function
  // continues the reset of the code acting as a content process.
  if (bootstrap->XRE_ForkServer(&argc, &argv)) {
    // Return from the fork server in the fork server process.
    // Stop the fork server.
  } else {
    // In a content process forked from the fork server.
    // Start acting as a content process.
    ret = content_process_main(bootstrap.get(), argc, argv);
  }

  bootstrap->NS_LogTerm();
  return ret;
#else
  return 0;
#endif
}

int main(int argc, char* argv[]) {
  auto bootstrapResult = GetBootstrap();
  if (bootstrapResult.isErr()) {
    return 2;
  }

  Bootstrap::UniquePtr bootstrap = bootstrapResult.unwrap();

  int ret;
  if (UseForkServer(argc, argv)) {
    ret = RunForkServer(std::move(bootstrap), argc, argv);
  } else {
    // Set the process type. We don't remove the arg here as that will be done
    // later in common code.
    SetGeckoProcessType(argv[argc - 1]);

    // Register an external module to report on otherwise uncatchable
    // exceptions. Note that in child processes this must be called after Gecko
    // process type has been set.
    CrashReporter::RegisterRuntimeExceptionModule();

    // Make sure we unregister the runtime exception module before returning.
    auto unregisterRuntimeExceptionModule = MakeScopeExit(
        [] { CrashReporter::UnregisterRuntimeExceptionModule(); });

#ifdef HAS_DLL_BLOCKLIST
    uint32_t initFlags = eDllBlocklistInitFlagIsChildProcess;
#  if defined(MOZ_SANDBOX)
    Maybe<uint64_t> sandboxingKind =
        geckoargs::sSandboxingKind.Get(argc, argv, CheckArgFlag::None);
    if (sandboxingKind.isSome()) {
      initFlags |= eDllBlocklistInitFlagIsUtilityProcess;
    }
#  endif  // defined(MOZ_SANDBOX)
    DllBlocklist_Initialize(initFlags);
#endif

    ret = content_process_main(bootstrap.get(), argc, argv);

#if defined(DEBUG) && defined(HAS_DLL_BLOCKLIST)
    DllBlocklist_Shutdown();
#endif
  }

  return ret;
}
