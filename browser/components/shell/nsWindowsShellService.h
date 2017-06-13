/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nswindowsshellservice_h____
#define nswindowsshellservice_h____

#include "nscore.h"
#include "nsString.h"
#include "nsIShellService.h"

#include <windows.h>
#include <ole2.h>

class nsWindowsShellService : public nsIShellService
{
  virtual ~nsWindowsShellService();

public:
  nsWindowsShellService();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISHELLSERVICE

protected:
  nsresult LaunchControlPanelDefaultsSelectionUI();
  nsresult LaunchControlPanelDefaultPrograms();
  nsresult LaunchModernSettingsDialogDefaultApps();
  nsresult InvokeHTTPOpenAsVerb();
  nsresult LaunchHTTPHandlerPane();
};

#endif // nswindowsshellservice_h____
