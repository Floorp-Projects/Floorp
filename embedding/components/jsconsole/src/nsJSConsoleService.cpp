/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsJSConsoleService.h"
#include "jsapi.h"

#include "nsIDOMWindow.h"
#include "nsIWindowWatcher.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

static const char console_chrome_url[] = "chrome://global/content/console.xul";
static const char console_window_options[] = "dialog=no,close,chrome,menubar,toolbar,resizable";


NS_IMPL_ISUPPORTS1(nsJSConsoleService, nsIJSConsoleService)


nsJSConsoleService::nsJSConsoleService()  
{
  NS_INIT_REFCNT();
}

nsJSConsoleService::~nsJSConsoleService()
{
}


NS_IMETHODIMP
nsJSConsoleService :: Open ( nsIDOMWindow *inParent )               
{
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
  if (!wwatch)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindow> consoleWindow;
  return wwatch->OpenWindow(inParent, console_chrome_url, "_blank", console_window_options,
                              nsnull, getter_AddRefs(consoleWindow));
}

