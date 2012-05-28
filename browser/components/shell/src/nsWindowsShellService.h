/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nswindowsshellservice_h____
#define nswindowsshellservice_h____

#include "nscore.h"
#include "nsStringAPI.h"
#include "nsIWindowsShellService.h"
#include "nsITimer.h"

#include <windows.h>
#include <ole2.h>

class nsWindowsShellService : public nsIWindowsShellService
{
public:
  nsWindowsShellService();
  virtual ~nsWindowsShellService();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISHELLSERVICE
  NS_DECL_NSIWINDOWSSHELLSERVICE

protected:
  bool      IsDefaultBrowserVista(bool* aIsDefaultBrowser);

private:
#if defined(MOZ_MAINTENANCE_SERVICE)
  static void LaunchPrefetchClearCommand(nsITimer *aTimer, void*);
  nsCOMPtr<nsITimer> mTimer;
#endif
  bool      mCheckedThisSession;
};

#endif // nswindowsshellservice_h____
