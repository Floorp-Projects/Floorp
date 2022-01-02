/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=4:expandtab:shiftwidth=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XP_WIN
#  error "nsFxrCommandLineHandler currently only supported on Windows"
#endif

#include "nsFxrCommandLineHandler.h"
#include "FxRWindowManager.h"

#include "nsICommandLine.h"
#include "nsIWindowWatcher.h"
#include "mozIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "mozilla/WidgetUtils.h"
#include "nsIWidget.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsArray.h"
#include "nsCOMPtr.h"
#include "mozilla/StaticPrefs_extensions.h"

#include <windows.h>
#include "WinUtils.h"

#include "VRShMem.h"

NS_IMPL_ISUPPORTS(nsFxrCommandLineHandler, nsICommandLineHandler)

// nsFxrCommandLineHandler acts in the middle of bootstrapping Firefox
// Reality with desktop Firefox. Details of the processes involved are
// described below:
//
//      Host
// (vrhost!CreateVRWindow)      Fx Main                 Fx GPU
//       |                         +                       +
//  VRShMem creates shared         +                       +
//  memory in OS                   +                       +
//       |                         +                       +
//  Launch firefox.exe             +                       +
//  with --fxr                     +                       +
//       |                         |                       +
//  Wait for Signal...       nsFxrCLH handles param        +
//       |                   joins VRShMem                 +
//       |                   creates new window            |
//       |                   sets .hwndFx in VRShMem       |
//       |                         |                       |
//       |                         |                  After compositor and
//       |                         |                  swapchain created,
//       |                         |                  share texture handle to
//       |                         |                  VRShMem and set signal
//  CreateVRWindow returns         |                       |
//  to host with relevant          |                       |
//  return data from VRShMem       |                       |
//       |                   Fx continues to run           |
//       |                         |                  Fx continues to render
//       |                         |                       |
//      ...                       ...                     ...

NS_IMETHODIMP
nsFxrCommandLineHandler::Handle(nsICommandLine* aCmdLine) {
  bool handleFlagRetVal = false;
  nsresult result = aCmdLine->HandleFlag(u"fxr"_ns, false, &handleFlagRetVal);
  if (result == NS_OK && handleFlagRetVal) {
    if (XRE_IsParentProcess() && !XRE_IsE10sParentProcess()) {
      MOZ_CRASH("--fxr not supported without e10s");
    }

    MOZ_ASSERT(mozilla::StaticPrefs::extensions_webextensions_remote(),
               "Remote extensions are the only supported configuration on "
               "desktop platforms");

    aCmdLine->SetPreventDefault(true);

    nsCOMPtr<nsIWindowWatcher> wwatch =
        do_GetService(NS_WINDOWWATCHER_CONTRACTID);
    NS_ENSURE_TRUE(wwatch, NS_ERROR_FAILURE);

    nsCOMPtr<mozIDOMWindowProxy> newWindow;
    result = wwatch->OpenWindow(
        nullptr,                                        // aParent
        "chrome://fxr/content/fxrui.html"_ns,           // aUrl
        "_blank"_ns,                                    // aName
        "chrome,dialog=no,all,private,alwaysontop"_ns,  // aFeatures
        nullptr,                                        // aArguments
        getter_AddRefs(newWindow));

    MOZ_ASSERT(result == NS_OK);

    nsPIDOMWindowOuter* newWindowOuter = nsPIDOMWindowOuter::From(newWindow);
    FxRWindowManager::GetInstance()->AddWindow(newWindowOuter);

    // Set ForceFullScreenInWidget so that full-screen (in an FxR window)
    // fills only the window and thus the same texture that will already be
    // shared with the host. Also, this is set here per-window because
    // changing the related pref would impact all browser window instances.
    newWindowOuter->ForceFullScreenInWidget();

    // Send the window's HWND to vrhost through VRShMem
    mozilla::gfx::VRShMem shmem(nullptr, true /*aRequiresMutex*/);
    if (shmem.JoinShMem()) {
      mozilla::gfx::VRWindowState windowState = {0};
      shmem.PullWindowState(windowState);

      nsCOMPtr<nsIWidget> newWidget =
          mozilla::widget::WidgetUtils::DOMWindowToWidget(newWindowOuter);
      HWND hwndWidget = (HWND)newWidget->GetNativeData(NS_NATIVE_WINDOW);

      // The CLH should populate these members first
      MOZ_ASSERT(windowState.hwndFx == 0);
      MOZ_ASSERT(windowState.textureFx == nullptr);
      windowState.hwndFx = (uint64_t)hwndWidget;

      shmem.PushWindowState(windowState);
      shmem.LeaveShMem();

      // The GPU process will notify the host that window creation is complete
      // after output data is set in VRShMem
      newWidget->RequestFxrOutput();
    } else {
#ifndef NIGHTLY_BUILD
      MOZ_CRASH("failed to start with --fxr");
#endif
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFxrCommandLineHandler::GetHelpInfo(nsACString& aResult) {
  aResult.AssignLiteral(
      "  --fxr Creates a new window for Firefox Reality on Desktop when "
      "available\n");
  return NS_OK;
}
