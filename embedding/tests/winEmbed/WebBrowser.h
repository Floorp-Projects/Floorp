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

// Helper Classes
#include "nsCOMPtr.h"

// Interfaces Needed
#include "nsIWidget.h"

#include "nsIWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "nsIBaseWindow.h"
#include "nsIWebProgressListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIPrompt.h"

class WebBrowser : public nsIWebBrowserChrome,
                           public nsIWebProgressListener,
                           public nsIBaseWindow,
                           public nsIPrompt,
                           public nsIInterfaceRequestor
{

public:
 
   NS_DECL_ISUPPORTS
   NS_DECL_NSIWEBBROWSERCHROME
   NS_DECL_NSIWEBPROGRESSLISTENER
   NS_DECL_NSIBASEWINDOW
   NS_DECL_NSIPROMPT
   NS_DECL_NSIINTERFACEREQUESTOR
    
   nsresult Init(nsNativeWidget widget);
   nsresult GoTo(char* url);
   nsresult Print(void);

   WebBrowser();
   virtual ~WebBrowser();

protected:
    
    nsCOMPtr<nsIWidget>     mWindow;
    nsCOMPtr<nsIWebBrowser> mWebBrowser;
};

#endif /* __WebBrowser__ */
