/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Conrad Carlen <conrad@ingress.com> based on work by:
 *   Travis Bogard <travis@netscape.com>
 */

#ifndef __CWebBrowserChrome__
#define __CWebBrowserChrome__

// Helper Classes
#include "nsCOMPtr.h"

// Interfaces Needed
#include "nsIWebBrowserChrome.h"
#include "nsIBaseWindow.h"
#include "nsIWebProgressListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIPrompt.h"
#include "nsIContextMenuListener.h"
#include "nsITooltipListener.h"
#include "nsWeakReference.h"

// Other
#include "nsIWebBrowser.h"

#include <vector>
using namespace std;

class CBrowserWindow;
class CBrowserShell;

class CWebBrowserChrome : public nsIWebBrowserChrome,
                           public nsIWebProgressListener,
                           public nsIBaseWindow,
                           public nsIPrompt,
                           public nsIInterfaceRequestor,
                           public nsIContextMenuListener,
                           public nsITooltipListener,
                           public nsSupportsWeakReference
{
friend class CBrowserWindow;

public:
   NS_DECL_ISUPPORTS
   NS_DECL_NSIWEBBROWSERCHROME
   NS_DECL_NSIWEBPROGRESSLISTENER
   NS_DECL_NSIBASEWINDOW
   NS_DECL_NSIPROMPT
   NS_DECL_NSIINTERFACEREQUESTOR
   NS_DECL_NSICONTEXTMENULISTENER
   NS_DECL_NSITOOLTIPLISTENER
  
protected:
   CWebBrowserChrome();
   virtual ~CWebBrowserChrome();

   CBrowserWindow*& BrowserWindow();
   CBrowserShell*& BrowserShell();

protected:
   CBrowserWindow*  mBrowserWindow;
   CBrowserShell*   mBrowserShell;
   
   Boolean mPreviousBalloonState;     // are balloons on or off?
   
   nsCOMPtr<nsIPrompt> mPrompter;
   
   static vector<CWebBrowserChrome*> mgBrowserList;
};

#endif /* __CWebBrowserChrome__ */
