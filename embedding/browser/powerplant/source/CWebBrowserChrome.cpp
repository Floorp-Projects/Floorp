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
 *   Conrad Carlen <conrad@ingress.com>
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

#include "UMacUnicode.h"
#include "ApplIDs.h"

#include <LStaticText.h>
#include <LIconControl.h>
#include <LCheckBox.h>
#include <LEditText.h>
#include <URegistrar.h>
#include <UModalDialogs.h>

// Interfaces needed to be included

// Constants
const PRInt32     kGrowIconSize = 15;

// Static Variables
vector<CWebBrowserChrome*> CWebBrowserChrome::mgBrowserList;


//*****************************************************************************
//***    CWebBrowserChrome: Object Management
//*****************************************************************************

CWebBrowserChrome::CWebBrowserChrome() :
   mBrowserWindow(nsnull), mBrowserShell(nsnull)
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
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
   NS_INTERFACE_MAP_ENTRY(nsIPrompt)
NS_INTERFACE_MAP_END

//*****************************************************************************
// CWebBrowserChrome::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP CWebBrowserChrome::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
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


NS_IMETHODIMP CWebBrowserChrome::CreateBrowserWindow(PRUint32 chromeMask, nsIWebBrowser **aWebBrowser)
{
   NS_ENSURE_ARG_POINTER(aWebBrowser);
   *aWebBrowser = nsnull;

   // Note: For now, until we can create a window with specific chrome flags, this will
   // put up a plain window without navigation controls or location text. This is
   // most likely being used to pop up an add.
   
   CBrowserWindow	*theWindow;
   try
   {
      // CreateWindow can throw an we're being called from mozilla, so we need to catch
      theWindow = dynamic_cast<CBrowserWindow*>(LWindow::CreateWindow(wind_PlainBrowserWindow, LCommander::GetTopCommander()));
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
   mBrowserWindow->ResizeFrameTo(aCX, aCY + kGrowIconSize, true);
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

//*****************************************************************************
// CWebBrowserChrome::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP CWebBrowserChrome::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)   
{
   // Ignore wigdet parents for now.  Don't think those are a vaild thing to call.
   NS_ENSURE_SUCCESS(SetPositionAndSize(x, y, cx, cy, PR_FALSE), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::Create()
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP CWebBrowserChrome::Destroy()
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_UNEXPECTED;
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
   NS_ENSURE_ARG_POINTER(x && y);

   Rect  bounds;
   mBrowserWindow->GetGlobalBounds(bounds);
   *x = bounds.left;
   *y = bounds.top;
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
	NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);

   mBrowserWindow->ResizeFrameTo(cx, cy + kGrowIconSize, fRepaint);
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetSize(PRInt32* cx, PRInt32* cy)
{
   NS_ENSURE_ARG_POINTER(cx && cy);
	NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);

   Rect  bounds;
   mBrowserWindow->GetGlobalBounds(bounds);
   *cx = bounds.right - bounds.left;
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

NS_IMETHODIMP CWebBrowserChrome::Repaint(PRBool aForce)
{
	NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);

   mBrowserWindow->Refresh();
   if (aForce)
      mBrowserWindow->UpdatePort();
      
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetParentWidget(nsIWidget** aParentWidget)
{
   NS_ENSURE_ARG_POINTER(aParentWidget);
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetParentWidget(nsIWidget* aParentWidget)
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CWebBrowserChrome::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(aParentNativeWindow);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CWebBrowserChrome::GetVisibility(PRBool* aVisibility)
{
   NS_ENSURE_ARG_POINTER(aVisibility);
	NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);

   *aVisibility = mBrowserWindow->IsVisible();
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetVisibility(PRBool aVisibility)
{
	NS_ENSURE_TRUE(mBrowserWindow, NS_ERROR_NOT_INITIALIZED);

   if (aVisibility)
   {
      mBrowserWindow->Show();
      mBrowserWindow->Select();
   }
   else
      mBrowserWindow->Hide();
      
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetMainWidget(nsIWidget** aMainWidget)
{
   NS_ENSURE_ARG_POINTER(aMainWidget);

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

NS_IMETHODIMP CWebBrowserChrome::FocusAvailable(nsIBaseWindow* aCurrentFocus, 
   PRBool* aTookFocus)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetTitle(PRUnichar** aTitle)
{
   NS_ENSURE_ARG_POINTER(aTitle);
   NS_ENSURE_STATE(mBrowserWindow);

   Str255         aStr;
   nsAutoString   titleStr;
   
   mBrowserWindow->GetDescriptor(aStr);
   UMacUnicode::Str255ToString(aStr, titleStr);
   *aTitle = titleStr.ToNewUnicode();
   
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetTitle(const PRUnichar* aTitle)
{
   NS_ENSURE_STATE(mBrowserWindow);

	nsAutoString   titleStr(aTitle);
	Str255         aStr;
	
	UMacUnicode::StringToStr255(titleStr, aStr);
   mBrowserWindow->SetDescriptor(aStr);
   
   return NS_OK;
}

//*****************************************************************************
// CWebBrowserChrome::nsIPrompt
//*****************************************************************************   

NS_IMETHODIMP CWebBrowserChrome::Alert(const PRUnichar *dialogTitle, const PRUnichar *text)
{
    RegisterClass_(LIconControl);
    
    StDialogHandler	theHandler(dlog_Alert, mBrowserWindow);
    LWindow			 *theDialog = theHandler.GetDialog();
    Str255            aStr;

    UMacUnicode::StringToStr255(text, aStr);
   			
    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    msgText->SetDescriptor(aStr);

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

NS_IMETHODIMP CWebBrowserChrome::Confirm(const PRUnichar *dialogTitle, const PRUnichar *text, PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    
    StDialogHandler	theHandler(dlog_Confirm, mBrowserWindow);
    LWindow			 *theDialog = theHandler.GetDialog();
    Str255            aStr;

    UMacUnicode::StringToStr255(text, aStr);
   			
    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    msgText->SetDescriptor(aStr);

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
    Str255          msgStr, checkBoxStr;

    UMacUnicode::StringToStr255(text, msgStr);	
    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    msgText->SetDescriptor(msgStr);
    UMacUnicode::StringToStr255(checkMsg, checkBoxStr);
    LCheckBox *checkBox = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID('Chck'));
    checkBox->SetDescriptor(checkBoxStr);
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
    Str255          aStr;

    UMacUnicode::StringToStr255(text, aStr);	
    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    msgText->SetDescriptor(aStr);
    LEditText *responseText = dynamic_cast<LEditText*>(theDialog->FindPaneByID('Rslt'));
    theDialog->SetLatentSub(responseText);
    
    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
		{
		    nsAutoString    responseStr;
		    
		    *_retval = PR_TRUE;
		    responseText->GetDescriptor(aStr);
		    UMacUnicode::Str255ToString(aStr, responseStr);
		    *result = responseStr.ToNewUnicode();    
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
    Str255          aStr;

    UMacUnicode::StringToStr255(text, aStr);	
    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    msgText->SetDescriptor(aStr);	
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
		    nsAutoString    responseStr;
		    
		    userText->GetDescriptor(aStr);
		    UMacUnicode::Str255ToString(aStr, responseStr);
		    *user = responseStr.ToNewUnicode();
		    if (*user == nsnull)
		        resultErr = NS_ERROR_OUT_OF_MEMORY;
		    
		    pwdText->GetDescriptor(aStr);
		    UMacUnicode::Str255ToString(aStr, responseStr);
		    *pwd = responseStr.ToNewUnicode();
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

    StDialogHandler	theHandler(dlog_PromptPassword, mBrowserWindow);
    LWindow			 *theDialog = theHandler.GetDialog();
    Str255          aStr;

    UMacUnicode::StringToStr255(text, aStr);	
    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    msgText->SetDescriptor(aStr);	
    LEditText *pwdText = dynamic_cast<LEditText*>(theDialog->FindPaneByID('Pass'));
 
    theDialog->SetLatentSub(pwdText);   
    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
		{
		    nsAutoString    responseStr;
		    		    
		    pwdText->GetDescriptor(aStr);
		    UMacUnicode::Str255ToString(aStr, responseStr);
		    *pwd = responseStr.ToNewUnicode();
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

NS_IMETHODIMP CWebBrowserChrome::UniversalDialog(const PRUnichar *inTitleMessage, const PRUnichar *inDialogTitle, const PRUnichar *inMsg, const PRUnichar *inCheckboxMsg, const PRUnichar *inButton0Text, const PRUnichar *inButton1Text, const PRUnichar *inButton2Text, const PRUnichar *inButton3Text, const PRUnichar *inEditfield1Msg, const PRUnichar *inEditfield2Msg, PRUnichar **inoutEditfield1Value, PRUnichar **inoutEditfield2Value, const PRUnichar *inIConURL, PRBool *inoutCheckboxState, PRInt32 inNumberButtons, PRInt32 inNumberEditfields, PRInt32 inEditField1Password, PRInt32 *outButtonPressed)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
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

