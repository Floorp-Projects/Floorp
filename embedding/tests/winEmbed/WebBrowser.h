/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  Doug Turner <dougt@netscape.com> 
 */

#ifndef __WebBrowser__
#define __WebBrowser__

#include "nsCOMPtr.h"
#include "nsIWidget.h"
#include "nsIBaseWindow.h"
#include "nsIWebBrowser.h"
#include "nsIEditorShell.h"
#include "nsIWebBrowserChrome.h"

class WebBrowser
{
public:
   nsresult Init(nsNativeWidget widget, nsIWebBrowserChrome* aTopWindow);
   nsresult SetPositionAndSize(int x, int y, int cx, int cy);
   nsresult GoTo(char* url);
   nsresult Edit(char* url);
   nsresult Print(void);

   nsresult GetWebBrowser(nsIWebBrowser **outBrowser);

   WebBrowser();
   virtual ~WebBrowser();

protected:
    nsCOMPtr<nsIWebBrowser> mWebBrowser;
    nsCOMPtr<nsIBaseWindow> mBaseWindow;
    nsCOMPtr<nsIWebBrowserChrome> mTopWindow;
    //for editing
    nsCOMPtr<nsIEditorShell> mEditor;
};

#endif /* __WebBrowser__ */
