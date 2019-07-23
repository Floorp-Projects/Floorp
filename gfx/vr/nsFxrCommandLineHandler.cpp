/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=4:expandtab:shiftwidth=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFxrCommandLineHandler.h"
#include "nsICommandLine.h"
#include "nsString.h"

NS_IMPL_ISUPPORTS(nsFxrCommandLineHandler, nsICommandLineHandler)

NS_IMETHODIMP
nsFxrCommandLineHandler::Handle(nsICommandLine* aCmdLine) {
  // Bug 1565296 - Implement Command Line Handler for FxR on desktop
  return NS_OK;
}

NS_IMETHODIMP
nsFxrCommandLineHandler::GetHelpInfo(nsACString& aResult) {
  // Bug 1565296 - Implement Command Line Handler for FxR on desktop
  return NS_OK;
}
