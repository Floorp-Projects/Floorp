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
#include "nsIDOMWindow.h"

#include "UMacUnicode.h"
#include "ApplIDs.h"

#include <LStaticText.h>
#include <LCheckBox.h>
#include <LEditText.h>
#include <UModalDialogs.h>
#include <LPushButton.h>

// Interfaces needed to be included

// Defines
#define USE_BALLOONS_FOR_TOOL_TIPS 0 // Using balloons for this is really obnoxious

// Constants
const PRInt32     kGrowIconSize = 15;


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
   mBrowserWindow(nsnull), mBrowserShell(nsnull),
   mPreviousBalloonState(false), mInModalLoop(false)
{
	NS_INIT_REFCNT();
}

CWebBrowserChrome::~CWebBrowserChrome()
{
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
   NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
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
   if (aIID.Equals(NS_GET_IID(nsIDOMWindow)))
   {
      nsCOMPtr<nsIWebBrowser> browser;
      GetWebBrowser(getter_AddRefs(browser));
      if (browser)
         return browser->GetContentDOMWindow((nsIDOMWindow **) aInstancePtr);
      return NS_ERROR_NOT_INITIALIZED;
   }

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

NS_IMETHODIMP CWebBrowserChrome::DestroyBrowserWindow()
{
    mInModalLoop = false;
    delete mBrowserWindow;
    return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::IsWindowModal(PRBool *_retval)
{
    *_retval = PR_FALSE;
    return NS_OK;
}


NS_IMETHODIMP CWebBrowserChrome::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
   mBrowserWindow->ResizeWindowTo(aCX, aCY + kGrowIconSize);
   mBrowserWindow->SetSizeToContent(false);
   return NS_OK;
}


NS_IMETHODIMP CWebBrowserChrome::ShowAsModal(void)
{
    // We need this override because StDialogHandler deletes
    // its window in its destructor. We don't want that here. 
    class CChromeDialogHandler : public StDialogHandler
    {
        public:
						CChromeDialogHandler(LWindow*		inWindow,
								             LCommander*	inSuper) :
					        StDialogHandler(inWindow, inSuper)
					    { }
					        
	    virtual         ~CChromeDialogHandler()
	                    { mDialog = nil; }
    };
    
    CChromeDialogHandler	 theHandler(mBrowserWindow, mBrowserWindow->GetSuperCommander());
	
	// Set to false by ExitModalEventLoop or DestroyBrowserWindow
	mInModalLoop = true;
	while (mInModalLoop) 
	    theHandler.DoDialog();

    return NS_OK;

}

NS_IMETHODIMP CWebBrowserChrome::ExitModalEventLoop(nsresult aStatus)
{
   mInModalLoop = false;
   return NS_OK;
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
// CWebBrowserChrome::nsIEmbeddingSiteWindow
//*****************************************************************************   

NS_IMETHODIMP CWebBrowserChrome::SetDimensions(PRUint32 flags, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)
{
    NS_ENSURE_STATE(mBrowserWindow);

    nsresult rv = NS_OK;
    CBrowserShell *browserShell;
        
    if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION)    // setting position
    {
        if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER)
        {
            // Don't allow setting the position of the embedded area
            rv = NS_ERROR_UNEXPECTED;
        }
        else // (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER)
        {
            mBrowserWindow->MoveWindowTo(x, y);
        }
    }
    else                                                        // setting size
    {
        mBrowserWindow->SetSizeToContent(false);
        if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER)
        {
            browserShell = mBrowserWindow->GetBrowserShell();
            NS_ENSURE_TRUE(browserShell, NS_ERROR_NULL_POINTER);
            SDimension16 curSize;
            browserShell->GetFrameSize(curSize);
            mBrowserWindow->ResizeWindowBy(cx - curSize.width, cy - curSize.height);
        }
        else // (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER)
        {
            if (mBrowserWindow->HasAttribute(windAttr_Resizable /*windAttr_SizeBox*/))
                cy += 15;
            mBrowserWindow->ResizeWindowTo(cx, cy);
        }
    }
    return rv;
}

NS_IMETHODIMP CWebBrowserChrome::GetDimensions(PRUint32 flags, PRInt32 *x, PRInt32 *y, PRInt32 *cx, PRInt32 *cy)
{
    NS_ENSURE_STATE(mBrowserWindow);

    nsresult rv = NS_OK;
    CBrowserShell *browserShell;
    Rect bounds;
        
    if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION)    // getting position
    {
        if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER)
        {
            browserShell = mBrowserWindow->GetBrowserShell();
            NS_ENSURE_TRUE(browserShell, NS_ERROR_NULL_POINTER);
            SPoint32 curPos;
            browserShell->GetFrameLocation(curPos);
            if (x)
                *x = curPos.h;
            if (y)
                *y = curPos.v;
        }
        else // (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER)
        {
            mBrowserWindow->GetGlobalBounds(bounds);
            if (x)
                *x = bounds.left;
            if (y)
                *y = bounds.top;
        }
    }
    else                                                        // getting size
    {
        if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER)
        {
            browserShell = mBrowserWindow->GetBrowserShell();
            NS_ENSURE_TRUE(browserShell, NS_ERROR_NULL_POINTER);
            SDimension16 curSize;
            browserShell->GetFrameSize(curSize);
            if (cx)
                *cx = curSize.width;
            if (cy)
                *cy = curSize.height;
        }
        else // (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER)
        {
            mBrowserWindow->GetGlobalBounds(bounds);
            if (cx)
                *cx = bounds.right - bounds.left;
            if (cy)
            {
                *cy = bounds.bottom - bounds.top;
                if (mBrowserWindow->HasAttribute(windAttr_Resizable /*windAttr_SizeBox*/))
                    *cy -= 15;
            }
        }
    }
    return rv;
}

NS_IMETHODIMP CWebBrowserChrome::SetFocus()
{
    NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CWebBrowserChrome::GetVisibility(PRBool *aVisibility)
{
    NS_ENSURE_STATE(mBrowserWindow);
    NS_ENSURE_ARG_POINTER(aVisibility);
    
    mBrowserWindow->GetVisibility(aVisibility);
    return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetVisibility(PRBool aVisibility)
{
    NS_ENSURE_STATE(mBrowserWindow);
    
    mBrowserWindow->SetVisibility(aVisibility);
    return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetTitle(PRUnichar * *aTitle)
{
   NS_ENSURE_STATE(mBrowserWindow);
   NS_ENSURE_ARG_POINTER(aTitle);

   Str255         pStr;
   nsAutoString   titleStr;
   
   mBrowserWindow->GetDescriptor(pStr);
   CPlatformUCSConversion::GetInstance()->PlatformToUCS(pStr, titleStr);
   *aTitle = titleStr.ToNewUnicode();
   
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetTitle(const PRUnichar * aTitle)
{
    NS_ENSURE_STATE(mBrowserWindow);
    NS_ENSURE_ARG(aTitle);
    
    Str255          pStr;
	
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(aTitle), pStr);
    mBrowserWindow->SetDescriptor(pStr);
    return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetSiteWindow(void * *aSiteWindow)
{
    return NS_ERROR_NOT_IMPLEMENTED;
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
    msgText->SetText(const_cast<char *>(cStr.get()), cStr.Length());

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
    msgText->SetText(const_cast<char *>(cStr.get()), cStr.Length());
    
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
    msgText->SetText(const_cast<char *>(cStr.get()), cStr.Length());

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
    msgText->SetText(const_cast<char *>(cStr.get()), cStr.Length());
    
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

NS_IMETHODIMP CWebBrowserChrome::ConfirmEx(const PRUnichar *dialogTitle,
                                           const PRUnichar *text,
                                           PRUint32 button0And1Flags,
                                           const PRUnichar *button2Title,
                                           const PRUnichar *checkMsg,
                                           PRBool *checkValue,
                                           PRInt32 *buttonPressed)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CWebBrowserChrome::Prompt(const PRUnichar *dialogTitle,
                                        const PRUnichar *text,
                                        PRUnichar **answer,
                                        const PRUnichar *checkMsg,
                                        PRBool *checkValue,
                                        PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    nsresult resultErr = NS_OK;

    StDialogHandler	theHandler(dlog_Prompt, mBrowserWindow);
    LWindow			 *theDialog = theHandler.GetDialog();
    LCheckBox        *checkbox = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID('Chck'));
    nsCAutoString   cStr;
    Str255          pStr;

    if (dialogTitle) {
        CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(dialogTitle), pStr);
        theDialog->SetDescriptor(pStr);
    }

    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(text), cStr);
    cStr.ReplaceChar('\n', '\r');
    msgText->SetText(const_cast<char *>(cStr.get()), cStr.Length());
    
    LEditText *responseText = dynamic_cast<LEditText*>(theDialog->FindPaneByID('Rslt'));
    if (answer && *answer) {
        CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(*answer), pStr);
        responseText->SetDescriptor(pStr);
    }
    
    if (checkValue) {
        checkbox->SetValue(*checkValue);
        if (checkMsg) {
            CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(checkMsg), pStr);
            checkbox->SetDescriptor(pStr);
        }
    }
    else
        checkbox->Hide();

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
		    if (answer && *answer) {
		        nsMemory::Free(*answer);
		        *answer = nsnull;
		    }
		    responseText->GetDescriptor(pStr);
		    CPlatformUCSConversion::GetInstance()->PlatformToUCS(pStr, ucStr);
		    *answer = ucStr.ToNewUnicode();    
   		    if (*answer == nsnull)
   		        resultErr = NS_ERROR_OUT_OF_MEMORY;
   		        
   		    if (checkValue)
   		        *checkValue = checkbox->GetValue();
   		        
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

NS_IMETHODIMP CWebBrowserChrome::PromptUsernameAndPassword(const PRUnichar *dialogTitle,
                                                           const PRUnichar *text,
                                                           PRUnichar **username,
                                                           PRUnichar **password,
                                                           const PRUnichar *checkMsg,
                                                           PRBool *checkValue,
                                                           PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    nsresult resultErr = NS_OK;

    StDialogHandler	theHandler(dlog_PromptNameAndPass, mBrowserWindow);
    LWindow			 *theDialog = theHandler.GetDialog();
    LCheckBox        *checkbox = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID('Chck'));
    nsCAutoString   cStr;
    Str255          pStr;

    if (dialogTitle) {
        CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(dialogTitle), pStr);
        theDialog->SetDescriptor(pStr);
    }

    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(text), cStr);
    cStr.ReplaceChar('\n', '\r');
    msgText->SetText(const_cast<char *>(cStr.get()), cStr.Length());
    
    LEditText *userText = dynamic_cast<LEditText*>(theDialog->FindPaneByID('Name'));
    if (username && *username) {
        CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(*username), pStr);
        userText->SetDescriptor(pStr);
    }
    LEditText *pwdText = dynamic_cast<LEditText*>(theDialog->FindPaneByID('Pass'));
    if (password && *password) {
        CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(*password), pStr);
        pwdText->SetDescriptor(pStr);
    }

    if (checkValue) {
        checkbox->SetValue(*checkValue);
        if (checkMsg) {
            CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(checkMsg), pStr);
            checkbox->SetDescriptor(pStr);
        }
    }
    else
        checkbox->Hide();
 
    theDialog->SetLatentSub(userText);   
    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
		{
		    nsAutoString    ucStr;
		    
		    if (username && *username) {
		        nsMemory::Free(*username);
		        *username = nsnull;
		    }
		    userText->GetDescriptor(pStr);
		    CPlatformUCSConversion::GetInstance()->PlatformToUCS(pStr, ucStr);
		    *username = ucStr.ToNewUnicode();
		    if (*username == nsnull)
		        resultErr = NS_ERROR_OUT_OF_MEMORY;
		    
		    if (password && *password) {
		        nsMemory::Free(*password);
		        *password = nsnull;
		    }
		    pwdText->GetDescriptor(pStr);
		    CPlatformUCSConversion::GetInstance()->PlatformToUCS(pStr, ucStr);
		    *password = ucStr.ToNewUnicode();
		    if (*password == nsnull)
		        resultErr = NS_ERROR_OUT_OF_MEMORY;

   		    if (checkValue)
   		        *checkValue = checkbox->GetValue();
		    
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

NS_IMETHODIMP CWebBrowserChrome::PromptPassword(const PRUnichar *dialogTitle,
                                                const PRUnichar *text,
                                                PRUnichar **password,
                                                const PRUnichar *checkMsg,
                                                PRBool *checkValue,
                                                PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    
    nsresult resultErr = NS_OK;

    StDialogHandler	 theHandler(dlog_PromptPassword, mBrowserWindow);
    LWindow			 *theDialog = theHandler.GetDialog();
    LCheckBox        *checkbox = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID('Chck'));
    nsCAutoString    cStr;
    Str255           pStr;

    if (dialogTitle) {
        CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(dialogTitle), pStr);
        theDialog->SetDescriptor(pStr);
    }

    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(text), cStr);
    cStr.ReplaceChar('\n', '\r');
    msgText->SetText(const_cast<char *>(cStr.get()), cStr.Length());
    
    LEditText *pwdText = dynamic_cast<LEditText*>(theDialog->FindPaneByID('Pass'));
    if (password && *password) {
        CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(*password), pStr);
        pwdText->SetDescriptor(pStr);
    }
    
    if (checkValue) {
        checkbox->SetValue(*checkValue);
        if (checkMsg) {
            CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(checkMsg), pStr);
            checkbox->SetDescriptor(pStr);
        }
    }
    else
        checkbox->Hide();
 
    theDialog->SetLatentSub(pwdText);   
    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
		{
		    nsAutoString    ucStr;
		    		    
		    if (password && *password) {
		        nsMemory::Free(*password);
		        *password = nsnull;
		    }
		    pwdText->GetDescriptor(pStr);
		    CPlatformUCSConversion::GetInstance()->PlatformToUCS(pStr, ucStr);
		    *password = ucStr.ToNewUnicode();
		    if (*password == nsnull)
		        resultErr = NS_ERROR_OUT_OF_MEMORY;

   		    if (checkValue)
   		        *checkValue = checkbox->GetValue();

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
  nsCAutoString printable;
  CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsLiteralString(aTipText), printable);

#ifdef DEBUG
  printf("--------- SHOW TOOLTIP AT %ld %ld, |%s|\n", aXCoords, aYCoords, (const char *)printable );
#endif

#if USE_BALLOONS_FOR_TOOL_TIPS  
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
#endif
    
  return NS_OK;
}

NS_IMETHODIMP
CWebBrowserChrome::OnHideTooltip()
{
#ifdef DEBUG
  printf("--------- HIDE TOOLTIP\n");
#endif

#if USE_BALLOONS_FOR_TOOL_TIPS
  ::HMRemoveBalloon();
  ::HMSetBalloons ( mPreviousBalloonState );
#endif

  return NS_OK;
}
