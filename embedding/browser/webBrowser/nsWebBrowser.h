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
 *   Travis Bogard <travis@netscape.com>
 */

#ifndef nsWebBrowser_h__
#define nsWebBrowser_h__

#include "nsCOMPtr.h"
#include "nsISupportsArray.h"

#include "nsCWebBrowser.h"

class nsWebBrowserInitInfo
{
public:
   //nsIGenericWindow Stuff
/*   nativeWindow         parentNativeWindow;
   nsCOMPtr<nsIWidget>  parentWidget; */
   PRInt32        x;
   PRInt32        y;
   PRInt32        cx;
   PRInt32        cy;
   PRBool         visible;
};


class nsWebBrowser : public nsIWebBrowser, public nsIWebBrowserNav,
   public nsIProgress, public nsIGenericWindow, public nsIScrollable,
   public nsITextScroll
{
public:
   NS_DECL_ISUPPORTS

   NS_DECL_NSIWEBBROWSER
   NS_DECL_NSIWEBBROWSERNAV
   NS_DECL_NSIPROGRESS
   NS_DECL_NSIGENERICWINDOW
   NS_DECL_NSISCROLLABLE   
   NS_DECL_NSITEXTSCROLL

   static NS_METHOD Create(nsISupports* aOuter, const nsIID& aIID, void** ppv);

protected:
   nsWebBrowser();
   virtual ~nsWebBrowser();

   void UpdateListeners();
   nsresult CreateDocShell(const PRUnichar* contentType);

protected:
   nsCOMPtr<nsISupportsArray> m_ListenerList;
   nsCOMPtr<nsIDocShell>      m_DocShell;
   PRBool                     m_Created;
   nsWebBrowserInitInfo*      m_InitInfo;
   nsCOMPtr<nsIWidget>        m_ParentWidget;
   nativeWindow               m_ParentNativeWindow;
   nsCOMPtr<nsIWidget>        m_InternalWidget;
};

#endif /* nsWebBrowser_h__ */
