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

#include "WebBrowser.h"

#include "nsCWebBrowser.h"
#include "nsWidgetsCID.h"
#include "nsIGenericFactory.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIURI.h"
#include "nsIWebProgress.h"
#include "nsIWebNavigation.h"
#include "nsIDocShell.h"
#include "nsIContentViewer.h"
#include "nsIContentViewerFile.h"
#include "nsIDocShell.h"
#include "nsIWebNavigation.h"
#include "nsIEditorShell.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIInterfaceRequestor.h"

#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"

nsresult
ConvertDocShellToDOMWindow(nsIDocShell* aDocShell, nsIDOMWindow** aDOMWindow)
{
  if (!aDOMWindow)
    return NS_ERROR_FAILURE;

  *aDOMWindow = nsnull;

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject(do_GetInterface(aDocShell));

  nsCOMPtr<nsIDOMWindow> domWindow(do_QueryInterface(scriptGlobalObject));
  if (!domWindow)
    return NS_ERROR_FAILURE;

  *aDOMWindow = domWindow.get();
  NS_ADDREF(*aDOMWindow);

  return NS_OK;
}



WebBrowser::WebBrowser(){}
WebBrowser::~WebBrowser()
{
    PRBool duh;
    if (mEditor)  // not good place for it!
        mEditor->SaveDocument(PR_FALSE, PR_FALSE, &duh);
}

nsresult 
WebBrowser::Init(nsNativeWidget widget, nsIWebBrowserChrome* aTopWindow)
{
    nsresult rv;

    mWebBrowser = do_CreateInstance(NS_WEBBROWSER_PROGID, &rv);
    
	if (!mWebBrowser)
        return NS_ERROR_FAILURE;

    mBaseWindow = do_QueryInterface(mWebBrowser);
    
    mTopWindow = aTopWindow;
    mWebBrowser->SetContainerWindow(aTopWindow);
    
    nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(mWebBrowser);
    dsti->SetItemType(nsIDocShellTreeItem::typeChromeWrapper);
     
    rv = mBaseWindow->InitWindow( widget,
                                  nsnull, 
                                  0, 
                                  0, 
                                  100, 
                                  100);
    
    mBaseWindow->Create();
    mBaseWindow->SetVisibility(PR_TRUE);


    return rv;
}

nsresult 
WebBrowser::GetWebBrowser(nsIWebBrowser **outBrowser)
{
    *outBrowser = mWebBrowser;
    NS_IF_ADDREF(*outBrowser);
    return NS_OK;
}

nsresult 
WebBrowser::GoTo(char* url)
{
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mWebBrowser));
    return webNav->LoadURI(NS_ConvertASCIItoUCS2(url).GetUnicode());
}

nsresult 
WebBrowser::Edit(char* url)
{
    nsresult rv;
    mEditor = do_CreateInstance("component://netscape/editor/editorshell", &rv);
    
	if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr <nsIDocShell> rootDocShell = do_GetInterface(mWebBrowser);
    
    nsCOMPtr<nsIDOMWindow> domWindow;
    ConvertDocShellToDOMWindow(rootDocShell, getter_AddRefs(domWindow));
   
    mEditor->Init();
    mEditor->SetEditorType(NS_ConvertASCIItoUCS2("html").GetUnicode());
    mEditor->SetWebShellWindow(domWindow);
    mEditor->SetContentWindow(domWindow);
    return mEditor->LoadUrl(NS_ConvertASCIItoUCS2(url).GetUnicode());
}

nsresult 
WebBrowser::Print(void)
{
    nsCOMPtr <nsIDocShell> rootDocShell = do_GetInterface(mWebBrowser);


    nsIContentViewer *pContentViewer = nsnull;
    nsresult res = rootDocShell->GetContentViewer(&pContentViewer);

    if (NS_SUCCEEDED(res))
    {
        nsCOMPtr<nsIContentViewerFile> spContentViewerFile = do_QueryInterface(pContentViewer); 
        spContentViewerFile->Print(PR_TRUE, nsnull);
        NS_RELEASE(pContentViewer);
    }
  return NS_OK;
}


nsresult 
WebBrowser::SetPositionAndSize(int x, int y, int cx, int cy)
{
    return mBaseWindow->SetPositionAndSize(x, y, cx, cy, PR_TRUE);
}

