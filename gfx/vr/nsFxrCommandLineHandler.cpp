/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=4:expandtab:shiftwidth=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XP_WIN
#  error "nsFxrCommandLineHandler currently only supported on Windows"
#endif

#include "nsFxrCommandLineHandler.h"

#include "nsICommandLine.h"
#include "nsIWindowWatcher.h"
#include "mozIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsArray.h"
#include "nsCOMPtr.h"

#include "windows.h"

NS_IMPL_ISUPPORTS(nsFxrCommandLineHandler, nsICommandLineHandler)

NS_IMETHODIMP
nsFxrCommandLineHandler::Handle(nsICommandLine* aCmdLine) {
  bool handleFlagRetVal = false;
  nsresult result =
      aCmdLine->HandleFlag(NS_LITERAL_STRING("fxr"), false, &handleFlagRetVal);
  if (result == NS_OK && handleFlagRetVal) {
    aCmdLine->SetPreventDefault(true);

    nsCOMPtr<nsIWindowWatcher> wwatch =
        do_GetService(NS_WINDOWWATCHER_CONTRACTID);
    NS_ENSURE_TRUE(wwatch, NS_ERROR_FAILURE);

    nsCOMPtr<mozIDOMWindowProxy> newWindow;
    result = wwatch->OpenWindow(nullptr,                            // aParent
                                "chrome://fxr/content/fxrui.html",  // aUrl
                                "_blank",                           // aName
                                "chrome,dialog=no,all",             // aFeatures
                                nullptr,  // aArguments
                                getter_AddRefs(newWindow));

    MOZ_ASSERT(result == NS_OK);
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
