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
#include "nsReadableUtils.h"
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
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChromeFocus)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
   NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
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
   CBrowserShell *browserShell = mBrowserWindow->GetBrowserShell();
   NS_ENSURE_TRUE(browserShell, NS_ERROR_NULL_POINTER);
   
   SDimension16 curSize;
   browserShell->GetFrameSize(curSize);
   mBrowserWindow->ResizeWindowBy(aCX - curSize.width, aCY - curSize.height);
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
// CWebBrowserChrome::nsIWebBrowserChromeFocus
//*****************************************************************************

NS_IMETHODIMP CWebBrowserChrome::FocusNextElement()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CWebBrowserChrome::FocusPrevElement()
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
   *aTitle = ToNewUnicode(titleStr);
   
   return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::SetTitle(const PRUnichar * aTitle)
{
    NS_ENSURE_STATE(mBrowserWindow);
    NS_ENSURE_ARG(aTitle);
    
    Str255          pStr;
	
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(aTitle), pStr);
    mBrowserWindow->SetDescriptor(pStr);
    return NS_OK;
}

NS_IMETHODIMP CWebBrowserChrome::GetSiteWindow(void * *aSiteWindow)
{
    NS_ENSURE_ARG(aSiteWindow);
    NS_ENSURE_STATE(mBrowserWindow);

    *aSiteWindow = mBrowserWindow->Compat_GetMacWindow();
    
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
  CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(aTipText), printable);

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
