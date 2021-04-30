/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* XPConnect JavaScript interactive shell. */

#include <stdio.h>

#include "mozilla/Bootstrap.h"
#include "XREShellData.h"

#include "nsXULAppAPI.h"
#ifdef XP_MACOSX
#  include "xpcshellMacUtils.h"
#endif
#ifdef XP_WIN
#  include "mozilla/WindowsDllBlocklist.h"

#  include <windows.h>
#  include <shlobj.h>

// we want a wmain entry point
#  define XRE_WANT_ENVIRON
#  include "nsWindowsWMain.cpp"
#  ifdef MOZ_SANDBOX
#    include "mozilla/sandboxing/SandboxInitialization.h"
#  endif
#endif

#ifdef MOZ_WIDGET_GTK
#  include <gtk/gtk.h>
#endif

#ifdef MOZ_GECKO_PROFILER
#  include "BaseProfiler.h"
#endif

#ifdef LIBFUZZER
#  include "FuzzerDefs.h"
#endif

int main(int argc, char** argv, char** envp) {
#ifdef MOZ_WIDGET_GTK
  // A default display may or may not be required for xpcshell tests, and so
  // is not created here. Instead we set the command line args, which is a
  // fairly cheap operation.
  gtk_parse_args(&argc, &argv);
#endif

#ifdef XP_MACOSX
  InitAutoreleasePool();
#endif

  // unbuffer stdout so that output is in the correct order; note that stderr
  // is unbuffered by default
  setbuf(stdout, nullptr);

#ifdef HAS_DLL_BLOCKLIST
  DllBlocklist_Initialize();
#endif

#ifdef MOZ_GECKO_PROFILER
  char aLocal;
  mozilla::baseprofiler::profiler_init(&aLocal);
#endif

  XREShellData shellData;
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  shellData.sandboxBrokerServices =
      mozilla::sandboxing::GetInitializedBrokerServices();
#endif

  auto bootstrapResult = mozilla::GetBootstrap();
  if (bootstrapResult.isErr()) {
    return 2;
  }

  mozilla::Bootstrap::UniquePtr bootstrap = bootstrapResult.unwrap();

#ifdef LIBFUZZER
  shellData.fuzzerDriver = fuzzer::FuzzerDriver;
#endif

  int result = bootstrap->XRE_XPCShellMain(argc, argv, envp, &shellData);

#ifdef MOZ_GECKO_PROFILER
  mozilla::baseprofiler::profiler_shutdown();
#endif

#if defined(DEBUG) && defined(HAS_DLL_BLOCKLIST)
  DllBlocklist_Shutdown();
#endif

#ifdef XP_MACOSX
  FinishAutoreleasePool();
#endif

  return result;
}
