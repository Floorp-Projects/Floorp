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

//*****************************************************************************
//***    WebBrowser: Object Management
//*****************************************************************************

WebBrowser::WebBrowser()
{
	NS_INIT_REFCNT();
}

WebBrowser::~WebBrowser()
{
}

nsresult 
WebBrowser::Init(nsNativeWidget widget)
{
    nsresult rv;

    mWebBrowser = do_CreateInstance(NS_WEBBROWSER_PROGID, &rv);
    
	if (NS_FAILED(rv))
        return rv;

    mBaseWindow = do_QueryInterface(mWebBrowser);
        
    rv = mBaseWindow->InitWindow( widget,
                                    nsnull, 
                                    0, 
                                    32, 
                                    450, 
                                    450);


    mWebBrowser->SetTopLevelWindow(this);
    
    mBaseWindow->Create();

    nsCOMPtr <nsIDocShell> rootDocShell;
    mWebBrowser->GetDocShell(getter_AddRefs(rootDocShell));
    rootDocShell->SetAllowPlugins(PR_TRUE);

    mBaseWindow->SetVisibility(PR_TRUE);

   return rv;
}

nsresult 
WebBrowser::GetIWebBrowser(nsIWebBrowser **outBrowser)
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
WebBrowser::Print(void)
{
    nsCOMPtr <nsIDocShell> rootDocShell;
    mWebBrowser->GetDocShell(getter_AddRefs(rootDocShell));


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

















// an empty implementation.


//*****************************************************************************
// WebBrowser::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(WebBrowser)
NS_IMPL_RELEASE(WebBrowser)

NS_INTERFACE_MAP_BEGIN(WebBrowser)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
   NS_INTERFACE_MAP_ENTRY(nsIPrompt)
NS_INTERFACE_MAP_END

//*****************************************************************************
// WebBrowser::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP WebBrowser::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
   return QueryInterface(aIID, aInstancePtr);
}

//*****************************************************************************
// WebBrowser::nsIWebBrowserChrome
//*****************************************************************************   

NS_IMETHODIMP WebBrowser::SetJSStatus(const PRUnichar* aStatus)
{
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::SetJSDefaultStatus(const PRUnichar* aStatus)
{
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::SetOverLink(const PRUnichar* aLink)
{
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::GetWebBrowser(nsIWebBrowser** aWebBrowser)
{
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::SetWebBrowser(nsIWebBrowser* aWebBrowser)
{
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::GetChromeMask(PRUint32* aChromeMask)
{
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::SetChromeMask(PRUint32 aChromeMask)
{
   return NS_OK;
}

extern nsresult CreateNativeWindowWidget(nsIWebBrowser **outBrowser);

NS_IMETHODIMP WebBrowser::GetNewBrowser(PRUint32 chromeMask, nsIWebBrowser **outBrowser)
{    
    CreateNativeWindowWidget(outBrowser);
    return NS_OK;
}


NS_IMETHODIMP WebBrowser::FindNamedBrowserItem(const PRUnichar* aName,
                                                  	  nsIDocShellTreeItem ** aWebBrowser)
{
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP WebBrowser::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
   return NS_OK;
}


NS_IMETHODIMP WebBrowser::ShowAsModal(void)
{
   return NS_OK;
}


//*****************************************************************************
// WebBrowser::nsIWebProgressListener
//*****************************************************************************   

NS_IMETHODIMP WebBrowser::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aStateFlags, PRUint32 aStatus)
{
	return NS_OK;
}
NS_IMETHODIMP WebBrowser::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
	return NS_OK;
}

NS_IMETHODIMP WebBrowser::OnLocationChange(nsIURI *location)
{
	return NS_OK;
}


//*****************************************************************************
// WebBrowser::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP WebBrowser::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)   
{
    return NS_OK;
}

NS_IMETHODIMP WebBrowser::Create()
{
   return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP WebBrowser::Destroy()
{
   return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP WebBrowser::SetPosition(PRInt32 x, PRInt32 y)
{
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::GetPosition(PRInt32* x, PRInt32* y)
{
   NS_ENSURE_ARG_POINTER(x && y);

   return NS_OK;
}

NS_IMETHODIMP WebBrowser::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::GetSize(PRInt32* cx, PRInt32* cy)
{
   NS_ENSURE_ARG_POINTER(cx && cy);
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
   PRInt32 cy, PRBool fRepaint)
{
    return mBaseWindow->SetPositionAndSize(x, y,cx, cy,fRepaint);
}

NS_IMETHODIMP WebBrowser::GetPositionAndSize(PRInt32* x, PRInt32* y, PRInt32* cx,
   PRInt32* cy)
{
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::Repaint(PRBool aForce)
{
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::GetParentWidget(nsIWidget** aParentWidget)
{
   NS_ENSURE_ARG_POINTER(aParentWidget);
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::SetParentWidget(nsIWidget* aParentWidget)
{
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(aParentNativeWindow);
    return NS_OK;
}

NS_IMETHODIMP WebBrowser::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::GetVisibility(PRBool* aVisibility)
{
   NS_ENSURE_ARG_POINTER(aVisibility);
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::SetVisibility(PRBool aVisibility)
{
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::GetMainWidget(nsIWidget** aMainWidget)
{
   NS_ENSURE_ARG_POINTER(aMainWidget);

   return NS_OK;
}

NS_IMETHODIMP WebBrowser::SetFocus()
{
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::FocusAvailable(nsIBaseWindow* aCurrentFocus, 
   PRBool* aTookFocus)
{
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::GetTitle(PRUnichar** aTitle)
{
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::SetTitle(const PRUnichar* aTitle)
{
   return NS_OK;
}

//*****************************************************************************
// WebBrowser::nsIPrompt
//*****************************************************************************   

NS_IMETHODIMP WebBrowser::Alert(const PRUnichar *dialogTitle, const PRUnichar *text)
{
    return NS_OK;
}

NS_IMETHODIMP WebBrowser::Confirm(const PRUnichar *dialogTitle, const PRUnichar *text, PRBool *_retval)
{
    return NS_OK;
}

NS_IMETHODIMP WebBrowser::ConfirmCheck(const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *checkMsg, PRBool *checkValue, PRBool *_retval)
{
    return NS_OK;
}

NS_IMETHODIMP WebBrowser::Prompt(const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *passwordRealm, PRUint32 savePassword, const PRUnichar *defaultText, PRUnichar **result, PRBool *_retval)
{
    return NS_OK;
}

NS_IMETHODIMP WebBrowser::PromptUsernameAndPassword(const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *passwordRealm, PRUint32 savePassword, PRUnichar **user, PRUnichar **pwd, PRBool *_retval)
{
    return NS_OK;
}


NS_IMETHODIMP WebBrowser::PromptPassword(const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *passwordRealm, PRUint32 savePassword, PRUnichar **user, PRBool *_retval)
{
    return NS_OK;
}

NS_IMETHODIMP WebBrowser::Select(const PRUnichar *dialogTitle, const PRUnichar *text, PRUint32 count, const PRUnichar **selectList, PRInt32 *outSelection, PRBool *_retval)
{
   return NS_OK;
}

NS_IMETHODIMP WebBrowser::UniversalDialog(const PRUnichar *inTitleMessage, const PRUnichar *inDialogTitle, const PRUnichar *inMsg, const PRUnichar *inCheckboxMsg, const PRUnichar *inButton0Text, const PRUnichar *inButton1Text, const PRUnichar *inButton2Text, const PRUnichar *inButton3Text, const PRUnichar *inEditfield1Msg, const PRUnichar *inEditfield2Msg, PRUnichar **inoutEditfield1Value, PRUnichar **inoutEditfield2Value, const PRUnichar *inIConURL, PRBool *inoutCheckboxState, PRInt32 inNumberButtons, PRInt32 inNumberEditfields, PRInt32 inEditField1Password, PRInt32 *outButtonPressed)
{
   return NS_OK;
}