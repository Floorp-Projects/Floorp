/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* XPConnect JavaScript interactive shell. */

#include <stdio.h>

#include "mozilla/WindowsDllBlocklist.h"
#include "mozilla/Bootstrap.h"

#include "nsXULAppAPI.h"
#ifdef XP_MACOSX
#include "xpcshellMacUtils.h"
#endif
#ifdef XP_WIN
#include <windows.h>
#include <shlobj.h>

// we want a wmain entry point
#define XRE_DONT_PROTECT_DLL_LOAD
#define XRE_WANT_ENVIRON
#include "nsWindowsWMain.cpp"
#ifdef MOZ_SANDBOX
#include "mozilla/sandboxing/SandboxInitialization.h"
#endif
#endif

#ifdef MOZ_WIDGET_GTK
#include <gtk/gtk.h>
#endif

int
main(int argc, char** argv, char** envp)
{
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
    setbuf(stdout, 0);

#ifdef HAS_DLL_BLOCKLIST
    DllBlocklist_Initialize();
#endif

    XREShellData shellData;
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
    shellData.sandboxBrokerServices =
      mozilla::sandboxing::GetInitializedBrokerServices();
#endif

    mozilla::Bootstrap::UniquePtr bootstrap;
    XRE_GetBootstrap(bootstrap);
    if (!bootstrap) {
        return 2;
    }

    int result = bootstrap->XRE_XPCShellMain(argc, argv, envp, &shellData);

#ifdef XP_MACOSX
    FinishAutoreleasePool();
#endif

    return result;
}
