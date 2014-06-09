/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsgnomeshellservice_h____
#define nsgnomeshellservice_h____

#include "nsIShellService.h"
#include "nsStringAPI.h"
#include "mozilla/Attributes.h"

class nsGNOMEShellService MOZ_FINAL : public nsIShellService
{
public:
  nsGNOMEShellService() : mCheckedThisSession(false), mAppIsInPath(false) { }

  NS_DECL_ISUPPORTS
  NS_DECL_NSISHELLSERVICE

  nsresult Init();

private:
  ~nsGNOMEShellService() {}

  bool KeyMatchesAppName(const char *aKeyValue) const;
  bool CheckHandlerMatchesAppName(const nsACString& handler) const;

  bool GetAppPathFromLauncher();
  bool mCheckedThisSession;
  bool mUseLocaleFilenames;
  nsCString    mAppPath;
  bool mAppIsInPath;
};

#endif // nsgnomeshellservice_h____
