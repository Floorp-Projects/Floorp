/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Conrad Carlen <ccarlen@netscape.com>
 */

// Local Includes
#include "CWebBrowserChrome.h"
#include "CBrowserWindow.h"
#include "CBrowserShell.h"

#include "nsIGenericFactory.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIURI.h"
#include "nsIWebProgress.h"
#include "nsIDocShellTreeItem.h"
#include "nsIRequest.h"
#include "nsIChannel.h"
#include "nsIWalletService.h"

#include "UMacUnicode.h"
#include "ApplIDs.h"

#include <LStaticText.h>
#include <LCheckBox.h>
#include <LEditText.h>
#include <UModalDialogs.h>
#include <LPushButton.h>

// Interfaces needed to be included

// Constants
const PRInt32     kGrowIconSize = 15;

// Static Variables
vector<CWebBrowserChrome*> CWebBrowserChrome::mgBrowserList;

class CWebBrowserPrompter : public nsIPrompt
{
public:
  CWebBrowserPrompter(CWebBrowserChrome* aChrome);
  virtual ~CWebBrowserPrompter();
    
  NS_DECL_ISUPPORTS
  NS_FORWARD_NSIPROMPT(mChrome->);
  
protected:
  CWebBrowserChrome *mChrome; 
};

NS_IMPL_ISUPPORTS1(CWebBrowserPrompter, nsIPrompt);

CWebBrowserPrompter::CWebBrowserPrompter(CWebBrowserChrome* aChrome) :
  mChrome(aChrome)
{
  NS_INIT_REFCNT();
}


CWebBrowserPrompter::~CWebBrowserPrompter()
{
}


//*****************************************************************************
//***    CWebBrowserChrome: Object Management
//*****************************************************************************

CWebBrowserChrome::CWebBrowserChrome() :
   mBrowserWindow(nsnull), mBrowserShell(nsnull), mPreviousBalloonState(false)
{
	NS_INIT_REFCNT();
	
	mgBrowserList.push_back(this);
}

CWebBrowserChrome::~CWebBrowserChrome()
{
  vector<CWebBrowserChrome*>::iterator  iter = find(mgBrowserList.begin(), mgBrowserList.end(), this);
  if (iter != mgBrowserList.end())
    mgBrowserList.erase(iter);
}

//*****************************************************************************
// CWebBrowserChrome::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(CWebBrowserChrome)
NS_IMPL_RELEASE(CWebBrowserChrome)

NS_INTERFACE_MAP_BEGIN(CWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserSiteWindow)
   NS_INTERFACE_MAP_ENTRY(nsIPrompt)
   NS_INTERFACE_MAP_ENTRY(nsIContextMenuListener)
   NS_INTERFACE_MAP_ENTRY(nsITooltipListener)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

//*****************************************************************************
// CWebBrowserChrome::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP CWebBrowserChrome::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
   if (aIID.Equals(NS_GET_IID(nsIPrompt)))
   {
      if (!mPrompter)
      {
        nsresult rv;
        
        nsCOMPtr<nsIPrompt> prompt;
        prompt = new CWebBrowserPrompter(this);
        NS_ENSURE_TRUE(prompt, NS_ERROR_OUT_OF_MEMORY);
        
        nsCOMPtr<nsISingleSignOnPrompt> siPrompt = do_CreateInstance(NS_SINGLESIGNONPROMPT_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv))
        {
          siPrompt->SetPromptDialogs(prompt);
          mPrompter = siPrompt;
        }
        else
          mPrompter = prompt;
      }
      NS_ENSURE_TRUE(mPrompter, NS_ERROR_FAILURE);
      return mPrompter->QueryInterface(aIID, aInstancePtr);
   }
   else
      return QueryInterface(aIID, aInstancePtr);
}

//*****************************************************************************
// CWebBrowserChrome::nsIWebBrowserChrome
//*****************************************************************************   

NS_IMETHODIMP CWebBrowserChrome::SetStatus(PRUint32 statusType, const PRUnichar *status)
{
   NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);

   if (statusType == STATUS_SCRIPT) 
      mBrowserWindow->SetStatus(status);
   else if (statusType == STATUS_LINK)
      mBrowserWindow->SetOverLink(status);
  
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetWebBrowser(nsIWebBrowser** aWebBrowser)
{
   NS_ENSURE_ARG_POINTER(aWebBrowser);
   NS_ENSURE_TRUE(mBrowserShell, NS_ERROR_NOT_INITIALIZED);

   mBrowserShell->GetWebBrowser(aWebBrowser);
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetWebBrowser(nsIWebBrowser* aWebBrowser)
{
   NS_ENSURE_ARG(aWebBrowser);   // Passing nsnull is NOT OK
   NS_ENSURE_TRUE(mBrowserShell, NS_ERROR_NOT_INITIALIZED);

   mBrowserShell->SetWebBrowser(aWebBrowser);
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetChromeFlags(PRUint32* aChromeMask)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP CWebBrowserChrome::SetChromeFlags(PRUint32 aChromeMask)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}


NS_IMETHODIMP CWebBrowserChrome::CreateBrowserWindow(PRUint32 chromeMask, PRInt32 aX, PRInt32 aY, PRInt32 aCX, PRInt32 aCY, nsIWebBrowser **aWebBrowser)
{
   NS_ENSURE_ARG_POINTER(aWebBrowser);
   *aWebBrowser = nsnull;
   
   CBrowserWindow	*theWindow;
   try
   {
      // CreateWindow can throw an we're being called from mozilla, so we need to catch
      theWindow = CBrowserWindow::CreateWindow(chromeMask, aCX, aCY);
   }
   catch (...)
   {
      theWindow = nsnull;
   }
   NS_ENSURE_TRUE(theWindow, NS_ERROR_FAILURE);
   CBrowserShell *aBrowserShell = theWindow->GetBrowserShell();
   NS_ENSURE_TRUE(aBrowserShell, NS_ERROR_FAILURE);
   return aBrowserShell->GetWebBrowser(aWebBrowser);    
}

/* boolean isWindowModal (); */
NS_IMETHODIMP CWebBrowserChrome::IsWindowModal(PRBool *_retval)
{
    *_retval = PR_FALSE;
    return NS_OK;
}


NS_IMETHODIMP CWebBrowserChrome::FindNamedBrowserItem(const PRUnichar* aName,
                                                  	  nsIDocShellTreeItem ** aBrowserItem)
{
   NS_ENSURE_ARG(aName);
   NS_ENSURE_ARG_POINTER(aBrowserItem);
   *aBrowserItem = nsnull;

   vector<CWebBrowserChrome*>::iterator  iter = mgBrowserList.begin();
   while (iter < mgBrowserList.end())
   {
      CWebBrowserChrome* aChrome = *iter++;
      if (aChrome == this)
      	continue;	// Our tree has already been searched???

      NS_ENSURE_TRUE(aChrome->BrowserShell(), NS_ERROR_FAILURE);
      nsCOMPtr<nsIWebBrowser> webBrowser;
      aChrome->BrowserShell()->GetWebBrowser(getter_AddRefs(webBrowser));
      nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(webBrowser));
      NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

      docShellAsItem->FindItemWithName(aName, NS_STATIC_CAST(nsIWebBrowserChrome*, this), aBrowserItem);
 
      if (*aBrowserItem)
         break;
   }

   return NS_OK; // Return OK even if we didn't find it???
}

NS_IMETHODIMP CWebBrowserChrome::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
   mBrowserWindow->ResizeWindowTo(aCX, aCY + kGrowIconSize);
   mBrowserWindow->SetSizeToContent(false);
   return NS_OK;
}


NS_IMETHODIMP CWebBrowserChrome::ShowAsModal(void)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP CWebBrowserChrome::ExitModalEventLoop(nsresult aStatus)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CWebBrowserChrome::SetPersistence(PRBool aPersistX, PRBool aPersistY,
                                  PRBool aPersistCX, PRBool aPersistCY,
                                  PRBool aPersistSizeMode)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CWebBrowserChrome::GetPersistence(PRBool* aPersistX, PRBool* aPersistY,
                                  PRBool* aPersistCX, PRBool* aPersistCY,
                                  PRBool* aPersistSizeMode)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//*****************************************************************************
// CWebBrowserChrome::nsIWebProgressListener
//*****************************************************************************   

NS_IMETHODIMP CWebBrowserChrome::OnProgressChange(nsIWebProgress *progress, nsIRequest *request,
                                                  PRInt32 curSelfProgress, PRInt32 maxSelfProgress,
                                                  PRInt32 curTotalProgress, PRInt32 maxTotalProgress)
{
	NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);
	
   return mBrowserWindow->OnProgressChange(progress, request,
                                           curSelfProgress, maxSelfProgress,
                                           curTotalProgress, maxTotalProgress);
}

NS_IMETHODIMP CWebBrowserChrome::OnStateChange(nsIWebProgress *progress, nsIRequest *request,
                                               PRInt32 progressStateFlags, PRUint32 status)
{
	NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);
	
    if (progressStateFlags & STATE_IS_NETWORK) {
      if (progressStateFlags & STATE_START)
         mBrowserWindow->OnStatusNetStart(progress, request, progressStateFlags, status);
      else if (progressStateFlags & STATE_STOP)
	      mBrowserWindow->OnStatusNetStop(progress, request, progressStateFlags, status);
    }

   return NS_OK;
}


NS_IMETHODIMP CWebBrowserChrome::OnLocationChange(nsIWebProgress* aWebProgress,
                                                  nsIRequest* aRequest,
                                                  nsIURI *location)
{
	NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);

	char *buf = nsnull;
 
	if (location)
		location->GetSpec(&buf);

	nsAutoString tmp; tmp.AssignWithConversion(buf);
	mBrowserWindow->SetLocation(tmp);

	if (buf)	
	    Recycle(buf);

	return NS_OK;
}

NS_IMETHODIMP 
CWebBrowserChrome::OnStatusChange(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest,
                                  nsresult aStatus,
                                  const PRUnichar* aMessage)
{
    return NS_OK;
}



NS_IMETHODIMP 
CWebBrowserChrome::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                    nsIRequest *aRequest, 
                                    PRInt32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


//*****************************************************************************
// CWebBrowserChrome::nsIWebBrowserSiteWindow
//*****************************************************************************   

NS_IMETHODIMP CWebBrowserChrome::Destroy()
{
   delete mBrowserWindow;
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetPosition(PRInt32 x, PRInt32 y)
{
	NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);

   mBrowserWindow->MoveWindowTo(x, y);
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetPosition(PRInt32* x, PRInt32* y)
{
   NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);

   Rect  bounds;
   mBrowserWindow->GetGlobalBounds(bounds);
   if (x)
      *x = bounds.left;
   if (y)
      *y = bounds.top;
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
	NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);

   mBrowserWindow->ResizeWindowTo(cx, cy + kGrowIconSize);
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetSize(PRInt32* cx, PRInt32* cy)
{
   NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);

   Rect  bounds;
   mBrowserWindow->GetGlobalBounds(bounds);
   if (cx)
       *cx = bounds.right - bounds.left;
   if (cy)
       *cy = bounds.bottom - bounds.top - kGrowIconSize;
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
	NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);

   Rect  bounds;
   bounds.top = y;
   bounds.left = x;
   bounds.bottom = y + cy + kGrowIconSize;
   bounds.right = x + cx;

   mBrowserWindow->DoSetBounds(bounds);
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetPositionAndSize(PRInt32* x, PRInt32* y, PRInt32* cx, PRInt32* cy)
{
   NS_ENSURE_ARG_POINTER(x && y && cx && cy);
	NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);

   Rect  bounds;
   mBrowserWindow->GetGlobalBounds(bounds);
   *x = bounds.left;
   *y = bounds.top;
   *cx = bounds.right - bounds.left;
   *cy = bounds.bottom - bounds.top - kGrowIconSize;

   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetSiteWindow(void ** aParentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(aParentNativeWindow);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetFocus()
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetTitle(PRUnichar** aTitle)
{
   NS_ENSURE_ARG_POINTER(aTitle);
   NS_ENSURE_STATE(mBrowserWindow);

   Str255         pStr;
   nsAutoString   titleStr;
   
   mBrowserWindow->GetDescriptor(pStr);
   CPlatformUCSConversion::GetInstance()->PlatformToUCS(pStr, titleStr);
   *aTitle = titleStr.ToNewUnicode();
   
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetTitle(const PRUnichar* aTitle)
{
    NS_ENSURE_STATE(mBrowserWindow);

    Str255          pStr;
	
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(aTitle), pStr);
    mBrowserWindow->SetDescriptor(pStr);
   
    return NS_OK;
}

//*****************************************************************************
// CWebBrowserChrome::nsIPrompt
//*****************************************************************************   

NS_IMETHODIMP CWebBrowserChrome::Alert(const PRUnichar *dialogTitle, const PRUnichar *text)
{    
    StDialogHandler	 theHandler(dlog_Alert, mBrowserWindow);
    LWindow			 *theDialog = theHandler.GetDialog();
    nsCAutoString    cStr;
    Str255           pStr;

    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(dialogTitle), pStr);
    theDialog->SetDescriptor(pStr);

    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(text), cStr);
    cStr.ReplaceChar('\n', '\r');   			
    msgText->SetText(const_cast<char *>(cStr.GetBuffer()), cStr.Length());

    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
   		break;
	}

    return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::AlertCheck(const PRUnichar *dialogTitle, 
                                            const PRUnichar *text, 
                                            const PRUnichar *checkMsg, 
                                            PRBool *checkValue)
{
    NS_ENSURE_ARG_POINTER(checkValue);

    StDialogHandler	theHandler(dlog_ConfirmCheck, mBrowserWindow);
    LWindow			 *theDialog = theHandler.GetDialog();
    nsCAutoString    cStr;
    Str255           pStr;

    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(dialogTitle), pStr);
    theDialog->SetDescriptor(pStr);

    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(text), cStr);
    cStr.ReplaceChar('\n', '\r');   			
    msgText->SetText(const_cast<char *>(cStr.GetBuffer()), cStr.Length());
    
    LCheckBox *checkBox = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID('Chck'));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(checkMsg), pStr);
    checkBox->SetDescriptor(pStr);
    checkBox->SetValue(*checkValue ? 1 : 0);

    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
		{
		    *checkValue = checkBox->GetValue();    
   		    break;
   		}
	}

    return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::Confirm(const PRUnichar *dialogTitle, const PRUnichar *text, PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    
    StDialogHandler	theHandler(dlog_Confirm, mBrowserWindow);
    LWindow			 *theDialog = theHandler.GetDialog();
    nsCAutoString    cStr;
    Str255           pStr;
    
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(dialogTitle), pStr);
    theDialog->SetDescriptor(pStr);
   			
    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(text), cStr);
    cStr.ReplaceChar('\n', '\r');   			
    msgText->SetText(const_cast<char *>(cStr.GetBuffer()), cStr.Length());

    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
		{
		    *_retval = PR_TRUE;    
   		    break;
   		}
   		else if (hitMessage == msg_Cancel)
   		{
   		    *_retval = PR_FALSE;
   		    break;
   		}
	}

    return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::ConfirmCheck(const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *checkMsg, PRBool *checkValue, PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(checkValue);
    NS_ENSURE_ARG_POINTER(_retval);

    StDialogHandler	theHandler(dlog_ConfirmCheck, mBrowserWindow);
    LWindow			 *theDialog = theHandler.GetDialog();
    nsCAutoString    cStr;
    Str255           pStr;

    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(dialogTitle), pStr);
    theDialog->SetDescriptor(pStr);

    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(text), cStr);
    cStr.ReplaceChar('\n', '\r');   			
    msgText->SetText(const_cast<char *>(cStr.GetBuffer()), cStr.Length());
    
    LCheckBox *checkBox = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID('Chck'));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(checkMsg), pStr);
    checkBox->SetDescriptor(pStr);
    checkBox->SetValue(*checkValue ? 1 : 0);

    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
		{
		    *_retval = PR_TRUE;
		    *checkValue = checkBox->GetValue();    
   		    break;
   		}
   		else if (hitMessage == msg_Cancel)
   		{
   		    *_retval = PR_FALSE;
   		    break;
   		}
	}

    return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::Prompt(const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *passwordRealm, PRUint32 savePassword, const PRUnichar *defaultText, PRUnichar **result, PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(result);
    NS_ENSURE_ARG_POINTER(_retval);

    nsresult resultErr = NS_OK;

    StDialogHandler	theHandler(dlog_Prompt, mBrowserWindow);
    LWindow			 *theDialog = theHandler.GetDialog();
    nsCAutoString   cStr;
    Str255          pStr;

    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(dialogTitle), pStr);
    theDialog->SetDescriptor(pStr);

    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(text), cStr);
    cStr.ReplaceChar('\n', '\r');
    msgText->SetText(const_cast<char *>(cStr.GetBuffer()), cStr.Length());
    
    LEditText *responseText = dynamic_cast<LEditText*>(theDialog->FindPaneByID('Rslt'));
    theDialog->SetLatentSub(responseText);
    
    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
		{
		    nsAutoString ucStr;

		    *_retval = PR_TRUE;
		    responseText->GetDescriptor(pStr);
		    CPlatformUCSConversion::GetInstance()->PlatformToUCS(pStr, ucStr);
		    *result = ucStr.ToNewUnicode();    
   		    if (!result)
   		        resultErr = NS_ERROR_OUT_OF_MEMORY;
   		    break;
   		}
   		else if (hitMessage == msg_Cancel)
   		{
   		    *_retval = PR_FALSE;
   		    break;
   		}
	}

    return resultErr;
}

NS_IMETHODIMP CWebBrowserChrome::PromptUsernameAndPassword(const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *passwordRealm, PRUint32 savePassword, PRUnichar **user, PRUnichar **pwd, PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(user);
    NS_ENSURE_ARG_POINTER(pwd);
    NS_ENSURE_ARG_POINTER(_retval);

    nsresult resultErr = NS_OK;

    StDialogHandler	theHandler(dlog_PromptNameAndPass, mBrowserWindow);
    LWindow			 *theDialog = theHandler.GetDialog();
    nsCAutoString   cStr;
    Str255          pStr;

    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(dialogTitle), pStr);
    theDialog->SetDescriptor(pStr);

    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(text), cStr);
    cStr.ReplaceChar('\n', '\r');
    msgText->SetText(const_cast<char *>(cStr.GetBuffer()), cStr.Length());
    
    LEditText *userText = dynamic_cast<LEditText*>(theDialog->FindPaneByID('Name'));
    LEditText *pwdText = dynamic_cast<LEditText*>(theDialog->FindPaneByID('Pass'));
 
    theDialog->SetLatentSub(userText);   
    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
		{
		    nsAutoString    ucStr;
		    
		    userText->GetDescriptor(pStr);
		    CPlatformUCSConversion::GetInstance()->PlatformToUCS(pStr, ucStr);
		    *user = ucStr.ToNewUnicode();
		    if (*user == nsnull)
		        resultErr = NS_ERROR_OUT_OF_MEMORY;
		    
		    pwdText->GetDescriptor(pStr);
		    CPlatformUCSConversion::GetInstance()->PlatformToUCS(pStr, ucStr);
		    *pwd = ucStr.ToNewUnicode();
		    if (*pwd == nsnull)
		        resultErr = NS_ERROR_OUT_OF_MEMORY;
		    
		    *_retval = PR_TRUE;        
   		    break;
   		}
   		else if (hitMessage == msg_Cancel)
   		{
   		    *_retval = PR_FALSE;
   		    break;
   		}
	}

    return resultErr;
}

NS_IMETHODIMP CWebBrowserChrome::PromptPassword(const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *passwordRealm, PRUint32 savePassword, PRUnichar **pwd, PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(pwd);
    NS_ENSURE_ARG_POINTER(_retval);
    
    nsresult resultErr = NS_OK;

    StDialogHandler	 theHandler(dlog_PromptPassword, mBrowserWindow);
    LWindow			 *theDialog = theHandler.GetDialog();
    nsCAutoString    cStr;
    Str255           pStr;

    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(dialogTitle), pStr);
    theDialog->SetDescriptor(pStr);

    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(text), cStr);
    cStr.ReplaceChar('\n', '\r');
    msgText->SetText(const_cast<char *>(cStr.GetBuffer()), cStr.Length());
    
    LEditText *pwdText = dynamic_cast<LEditText*>(theDialog->FindPaneByID('Pass'));
 
    theDialog->SetLatentSub(pwdText);   
    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
		{
		    nsAutoString    ucStr;
		    		    
		    pwdText->GetDescriptor(pStr);
		    CPlatformUCSConversion::GetInstance()->PlatformToUCS(pStr, ucStr);
		    *pwd = ucStr.ToNewUnicode();
		    if (*pwd == nsnull)
		        resultErr = NS_ERROR_OUT_OF_MEMORY;
		    *_retval = PR_TRUE;        
   		    break;
   		}
   		else if (hitMessage == msg_Cancel)
   		{
   		    *_retval = PR_FALSE;
   		    break;
   		}
	}

    return resultErr;
}

NS_IMETHODIMP CWebBrowserChrome::Select(const PRUnichar *inDialogTitle, const PRUnichar *inMsg, PRUint32 inCount, const PRUnichar **inList, PRInt32 *outSelection, PRBool *_retval)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::UniversalDialog(const PRUnichar *inTitleMessage,
                                                 const PRUnichar *inDialogTitle,
                                                 const PRUnichar *inMsg,
                                                 const PRUnichar *inCheckboxMsg,
                                                 const PRUnichar *inButton0Text,
                                                 const PRUnichar *inButton1Text,
                                                 const PRUnichar *inButton2Text,
                                                 const PRUnichar *inButton3Text,
                                                 const PRUnichar *inEditfield1Msg,
                                                 const PRUnichar *inEditfield2Msg,
                                                 PRUnichar **inoutEditfield1Value,
                                                 PRUnichar **inoutEditfield2Value,
                                                 const PRUnichar *inIConURL,
                                                 PRBool *inoutCheckboxState,
                                                 PRInt32 inNumberButtons,
                                                 PRInt32 inNumberEditfields,
                                                 PRInt32 inEditField1Password,
                                                 PRInt32 *outButtonPressed)
{
    NS_ENSURE_ARG_POINTER(outButtonPressed);

    // NOTE: inEditField1Password is not used. PowerPlant's LEditText
    // does not allow being switched from being a password field to
    // being clear text. An override needs to be made which allows this
      
    nsresult resultErr = NS_OK;

    StDialogHandler	theHandler(dlog_Universal, mBrowserWindow);
    LWindow			    *theDialog = theHandler.GetDialog();
    nsCAutoString   cStr;
    Str255          pStr;
    LCheckBox       *checkbox = nsnull;

    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(inDialogTitle), pStr);
    theDialog->SetDescriptor(pStr);

    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(inMsg), cStr);
    cStr.ReplaceChar('\n', '\r');
    msgText->SetText(const_cast<char *>(cStr.GetBuffer()), cStr.Length());

    checkbox = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID('Chck'));    
    if (inCheckboxMsg && inoutCheckboxState)
    {
      CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(inCheckboxMsg), pStr);
      checkbox->SetDescriptor(pStr);
      checkbox->SetValue(*inoutCheckboxState);
    }
    else
    {
      checkbox->Hide();
      checkbox->Disable();
    }
    
    LEditText *edit1Value = nsnull;
    LEditText *edit2Value = nsnull;
       
    if (inNumberEditfields > 0)
    {  
        LStaticText *edit1Msg = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('EdM1'));
        if (inEditfield1Msg)
        {
          CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(inEditfield1Msg), cStr);
          edit1Msg->SetText(const_cast<char *>(cStr.GetBuffer()), cStr.Length());
        }    
        edit1Value = dynamic_cast<LEditText*>(theDialog->FindPaneByID('EdV1'));
        if (inoutEditfield1Value)
        {
          CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(*inoutEditfield1Value), cStr);
          edit1Value->SetText(const_cast<char *>(cStr.GetBuffer()), cStr.Length());
        }
        
        if (inNumberEditfields > 1)
        {
            LStaticText *edit2Msg = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('EdM2'));
            if (inEditfield2Msg)
            {
              CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(inEditfield2Msg), cStr);
              edit2Msg->SetText(const_cast<char *>(cStr.GetBuffer()), cStr.Length());
            }
            edit2Value = dynamic_cast<LEditText*>(theDialog->FindPaneByID('EdV2'));
            if (inoutEditfield2Value)
            {
              CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(*inoutEditfield2Value), cStr);
              edit2Value->SetText(const_cast<char *>(cStr.GetBuffer()), cStr.Length());
            }
        }
        else
        {
            // One edit field. If inEditField1Password is TRUE, we need to turn the first field,
            // which by default is not a password field, into a password field.
        }
    }
    
    // If 1 or more edit fields are not shown, shrink the view enclosing the edit fields
    // which will hide them. Also, resize the dialog box. All the items below the edit fields
    // are sticky to the bottom and will be shifted up when we resize the window.
    
    SInt32 vShrink = (2 - inNumberEditfields) * 32;
    if (vShrink)
    {
        LView *editFieldsEncl = dynamic_cast<LView*>(theDialog->FindPaneByID('Encl'));
        editFieldsEncl->ResizeFrameBy(0, vShrink, false);
        theDialog->ResizeWindowBy(0, vShrink);
    }
    
    const PRUnichar* buttonTitles[4] = { inButton0Text, inButton1Text, inButton2Text, inButton3Text };
    
    for (PaneIDT buttonID = 1; buttonID <= 4; buttonID++)
    {
      LPushButton *aButton = dynamic_cast<LPushButton*>(theDialog->FindPaneByID(buttonID));
      if (buttonID <= inNumberButtons)
      {
        if (buttonTitles[buttonID - 1])
        {
          CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(inEditfield1Msg), pStr);
          aButton->SetDescriptor(pStr);
        }
      }
      else
      {
        aButton->Hide();
        aButton->Disable();
      }
    }
 
    if (edit1Value)
        theDialog->SetLatentSub(edit1Value);   
    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
		{
		    *outButtonPressed = 0;        
   		    break;
   		}
   		else if (hitMessage == msg_Cancel)
   		{
   		    *outButtonPressed = 1;
   		    break;
   		}
   		else if (hitMessage == 3)
   		{
   		    *outButtonPressed = 1003;
   		    break;
   		}
   		else if (hitMessage == 4)
   		{
   		    *outButtonPressed = 1004;
   		    break;
   		}
	}

  nsAutoString    ucStr;

  if (inoutEditfield1Value && edit1Value)
  {
    nsMemory::Free(*inoutEditfield1Value);
    *inoutEditfield1Value = nsnull;
    edit1Value->GetDescriptor(pStr);
    CPlatformUCSConversion::GetInstance()->PlatformToUCS(pStr, ucStr);
    *inoutEditfield1Value = ucStr.ToNewUnicode();
    if (*inoutEditfield1Value == nsnull)
      resultErr = NS_ERROR_OUT_OF_MEMORY;
  }

  if (inoutEditfield2Value && edit2Value)
  {
    nsMemory::Free(*inoutEditfield2Value);
    *inoutEditfield2Value = nsnull;
    edit2Value->GetDescriptor(pStr);
    CPlatformUCSConversion::GetInstance()->PlatformToUCS(pStr, ucStr);
    *inoutEditfield2Value = ucStr.ToNewUnicode();
    if (*inoutEditfield2Value == nsnull)
      resultErr = NS_ERROR_OUT_OF_MEMORY;
  }

  if (inoutCheckboxState)
    *inoutCheckboxState = checkbox->GetValue();

  return resultErr;
}

//*****************************************************************************
// CWebBrowserChrome::nsIContextMenuListener
//*****************************************************************************   

NS_IMETHODIMP CWebBrowserChrome::OnShowContextMenu(PRUint32 aContextFlags, nsIDOMEvent *aEvent, nsIDOMNode *aNode)
{
    nsresult rv;
	NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);
    
    try
    {
        rv = mBrowserWindow->OnShowContextMenu(aContextFlags, aEvent, aNode);
    }
    catch (...)
    {
        rv = NS_ERROR_FAILURE;
    }
    return rv;
}


//*****************************************************************************
// CWebBrowserChrome: Helpers
//*****************************************************************************   

//*****************************************************************************
// CWebBrowserChrome: Accessors
//*****************************************************************************   

CBrowserWindow*& CWebBrowserChrome::BrowserWindow()
{
   return mBrowserWindow;
}

CBrowserShell*& CWebBrowserChrome::BrowserShell()
{
   return mBrowserShell;
}

NS_IMETHODIMP
CWebBrowserChrome::OnShowTooltip(PRInt32 aXCoords, PRInt32 aYCoords, const PRUnichar *aTipText)
{
  nsAutoString tipText ( aTipText );
  const char* printable = tipText.ToNewCString();
  printf("--------- SHOW TOOLTIP AT %ld %ld, |%s|\n", aXCoords, aYCoords, printable );
  
  Point where;
  ::GetMouse ( &where );
  ::LocalToGlobal ( &where );
  
  HMMessageRecord helpRec;
  helpRec.hmmHelpType = khmmString;
  helpRec.u.hmmString[0] = strlen(printable);
  memcpy ( &helpRec.u.hmmString[1], printable, strlen(printable) );
  
  mPreviousBalloonState = ::HMGetBalloons();
  ::HMSetBalloons ( true );
  OSErr err = ::HMShowBalloon ( &helpRec, where, NULL, NULL, 0, 0, 0 );
  
  nsMemory::Free( (void*)printable );
  
  return NS_OK;
}

NS_IMETHODIMP
CWebBrowserChrome::OnHideTooltip()
{
  printf("--------- HIDE TOOLTIP\n");
  ::HMRemoveBalloon();
  ::HMSetBalloons ( mPreviousBalloonState );
    return NS_OK;
}
