/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nswindowsshellserviceinternal_h____
#define nswindowsshellserviceinternal_h____

#include "ErrorList.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsIFile.h"

#include <shlobj.h>

nsresult CreateShellLinkObject(nsIFile* aBinary,
                               const CopyableTArray<nsString>& aArguments,
                               const nsAString& aDescription,
                               nsIFile* aIconFile, uint16_t aIconIndex,
                               const nsAString& aAppUserModelId,
                               IShellLinkW** aLink);

#endif  // nswindowsshellserviceinternal_h____
