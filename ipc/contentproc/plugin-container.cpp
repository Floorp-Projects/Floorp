/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXPCOM.h"
#include "nsXULAppAPI.h"
#include "nsAutoPtr.h"
#include "mozilla/Bootstrap.h"

#ifdef XP_WIN
#include <windows.h>
// we want a wmain entry point
// but we don't want its DLL load protection, because we'll handle it here
#define XRE_DONT_PROTECT_DLL_LOAD
#include "nsWindowsWMain.cpp"
#include "nsSetDllDirectory.h"
#else
// FIXME/cjones testing
#include <unistd.h>
#endif

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
#include "mozilla/sandboxing/SandboxInitialization.h"
#include "mozilla/sandboxing/sandboxLogging.h"
#endif

int
content_process_main(mozilla::Bootstrap* bootstrap, int argc, char* argv[])
{
    // Check for the absolute minimum number of args we need to move
    // forward here. We expect the last arg to be the child process type.
    if (argc < 1) {
      return 3;
    }

    XREChildData childData;

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
    if (IsSandboxedProcess()) {
        childData.sandboxTargetServices =
            mozilla::sandboxing::GetInitializedTargetServices();
        if (!childData.sandboxTargetServices) {
            return 1;
        }

        childData.ProvideLogFunction = mozilla::sandboxing::ProvideLogFunction;
    }
#endif

    bootstrap->XRE_SetProcessType(argv[--argc]);

#ifdef XP_WIN
    // For plugins, this is done in PluginProcessChild::Init, as we need to
    // avoid it for unsupported plugins.  See PluginProcessChild::Init for
    // the details.
    if (bootstrap->XRE_GetProcessType() != GeckoProcessType_Plugin) {
        mozilla::SanitizeEnvironmentVariables();
        SetDllDirectoryW(L"");
    }
#endif

    nsresult rv = bootstrap->XRE_InitChildProcess(argc, argv, &childData);
    return NS_FAILED(rv);
}
