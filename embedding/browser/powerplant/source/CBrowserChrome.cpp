/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Conrad Carlen <ccarlen@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Local Includes
#include "CBrowserChrome.h"
#include "CBrowserShell.h"
#include "CBrowserMsgDefs.h"

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
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIWindowWatcher.h"
#include "nsIServiceManagerUtils.h"

#include "UMacUnicode.h"
#include "ApplIDs.h"

#include <LStaticText.h>
#include <LCheckBox.h>
#include <LEditText.h>
#include <UModalDialogs.h>
#include <LPushButton.h>

// Interfaces needed to be included

// Defines

// Constants
const PRInt32     kGrowIconSize = 15;

//*****************************************************************************
//***    CBrowserChrome:
//*****************************************************************************

CBrowserChrome::CBrowserChrome(CBrowserShell *aShell,
                               UInt32 aChromeFlags,
                               Boolean aIsMainContent) :
    mBrowserShell(aShell), mBrowserWindow(nsnull),
    mChromeFlags(aChromeFlags), mIsMainContent(aIsMainContent),
    mSizeToContent(false),    
    mInModalLoop(false), mWindowVisible(false),
    mInitialLoadComplete(false)
{
	ThrowIfNil_(mBrowserShell);
	mBrowserWindow = LWindow::FetchWindowObject(mBrowserShell->GetMacWindow());
	StartListening();
}

CBrowserChrome::~CBrowserChrome()
{
}

void CBrowserChrome::SetBrowserShell(CBrowserShell *aShell)
{
    mBrowserShell = aShell;
    if (mBrowserShell)
	    mBrowserWindow = LWindow::FetchWindowObject(mBrowserShell->GetMacWindow());
    else
        mBrowserWindow = nsnull;    
}

//*****************************************************************************
// CBrowserChrome::nsISupports
//*****************************************************************************   

NS_IMPL_ISUPPORTS8(CBrowserChrome,
                   nsIWebBrowserChrome,
                   nsIInterfaceRequestor,
                   nsIWebBrowserChromeFocus,
                   nsIEmbeddingSiteWindow,
                   nsIEmbeddingSiteWindow2,
                   nsIContextMenuListener2,
                   nsITooltipListener,
                   nsISupportsWeakReference);
                   
//*****************************************************************************
// CBrowserChrome::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP CBrowserChrome::GetInterface(const nsIID &aIID, void** aInstancePtr)
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
// CBrowserChrome::nsIWebBrowserChrome
//*****************************************************************************   

NS_IMETHODIMP CBrowserChrome::SetStatus(PRUint32 statusType, const PRUnichar *status)
{
     NS_ENSURE_TRUE(mBrowserShell, NS_ERROR_NOT_INITIALIZED);

     MsgChromeStatusChangeInfo info(mBrowserShell, statusType, status);
     mBrowserShell->BroadcastMessage(msg_OnChromeStatusChange, &info);
  
     return NS_OK;
}

NS_IMETHODIMP CBrowserChrome::GetWebBrowser(nsIWebBrowser** aWebBrowser)
{
    NS_ENSURE_ARG_POINTER(aWebBrowser);
    NS_ENSURE_TRUE(mBrowserShell, NS_ERROR_NOT_INITIALIZED);

    mBrowserShell->GetWebBrowser(aWebBrowser);
    return NS_OK;
}

NS_IMETHODIMP CBrowserChrome::SetWebBrowser(nsIWebBrowser* aWebBrowser)
{
    NS_ENSURE_ARG(aWebBrowser);   // Passing nsnull is NOT OK
    NS_ENSURE_TRUE(mBrowserShell, NS_ERROR_NOT_INITIALIZED);

    mBrowserShell->SetWebBrowser(aWebBrowser);
    return NS_OK;
}

NS_IMETHODIMP CBrowserChrome::GetChromeFlags(PRUint32* aChromeMask)
{
    NS_ENSURE_ARG_POINTER(aChromeMask);
   
    *aChromeMask = mChromeFlags;
    return NS_OK;
}

NS_IMETHODIMP CBrowserChrome::SetChromeFlags(PRUint32 aChromeMask)
{
    // Yuck - our window would have to be rebuilt to do this.
    NS_ERROR("Haven't implemented this yet!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CBrowserChrome::DestroyBrowserWindow()
{
    NS_ENSURE_TRUE(mBrowserShell, NS_ERROR_NOT_INITIALIZED);

    mInModalLoop = false;
    delete mBrowserWindow;
    return NS_OK;
}

NS_IMETHODIMP CBrowserChrome::IsWindowModal(PRBool *_retval)
{
    *_retval = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP CBrowserChrome::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
   SDimension16 curSize;
   mBrowserShell->GetFrameSize(curSize);
   mBrowserWindow->ResizeWindowBy((aCX - curSize.width), (aCY - curSize.height));
   return NS_OK;
}


NS_IMETHODIMP CBrowserChrome::ShowAsModal(void)
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

NS_IMETHODIMP CBrowserChrome::ExitModalEventLoop(nsresult aStatus)
{
   mInModalLoop = false;
   return NS_OK;
}


//*****************************************************************************
// CBrowserChrome::nsIWebBrowserChromeFocus
//*****************************************************************************

NS_IMETHODIMP CBrowserChrome::FocusNextElement()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CBrowserChrome::FocusPrevElement()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


//*****************************************************************************
// CBrowserChrome::nsIEmbeddingSiteWindow
//*****************************************************************************   

NS_IMETHODIMP CBrowserChrome::SetDimensions(PRUint32 flags, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)
{
    NS_ENSURE_STATE(mBrowserWindow);
    NS_ENSURE_STATE(mBrowserShell);
    
    if ((flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER) &&
        (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER))
        return NS_ERROR_INVALID_ARG;
    
    if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION)
    {
        mBrowserWindow->MoveWindowTo(x, y);
    }        
    
    if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER)
    {
        // Don't resize the inner view independently from the window. Keep them in
        // proportion by resizing the window and letting that affect the inner view.
        SDimension16 curSize;
        mBrowserShell->GetFrameSize(curSize);
        mBrowserWindow->ResizeWindowBy(cx - curSize.width, cy - curSize.height);
    }
    else if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER)
    {
        if (mBrowserWindow->HasAttribute(windAttr_Resizable /*windAttr_SizeBox*/))
            cy += 15;
        mBrowserWindow->ResizeWindowTo(cx, cy);
    }
    
    return NS_OK;
}

NS_IMETHODIMP CBrowserChrome::GetDimensions(PRUint32 flags, PRInt32 *x, PRInt32 *y, PRInt32 *cx, PRInt32 *cy)
{
    NS_ENSURE_STATE(mBrowserWindow);
    NS_ENSURE_STATE(mBrowserShell);
     
    if ((flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER) &&
        (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER))
        return NS_ERROR_INVALID_ARG;

    Rect outerBounds;
    if ((flags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION) ||
        (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER))    
        mBrowserWindow->GetGlobalBounds(outerBounds);
    
    if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION)
    {
        if (x)
            *x = outerBounds.left;
        if (y)
            *y = outerBounds.top;
    }        
    
    if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER)
    {
        SDimension16 curSize;
        mBrowserShell->GetFrameSize(curSize);
        if (cx)
            *cx = curSize.width;
        if (cy)
            *cy = curSize.height;
    }
    else if (flags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER)
    {
        if (cx)
            *cx = outerBounds.right - outerBounds.left;
        if (cy)
        {
            *cy = outerBounds.bottom - outerBounds.top;
            if (mBrowserWindow->HasAttribute(windAttr_Resizable /*windAttr_SizeBox*/))
                *cy -= 15;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP CBrowserChrome::SetFocus()
{
    mBrowserWindow->Select();
    return NS_OK;
}

NS_IMETHODIMP CBrowserChrome::GetVisibility(PRBool *aVisibility)
{
    NS_ENSURE_STATE(mBrowserWindow);
    NS_ENSURE_ARG_POINTER(aVisibility);
    
    *aVisibility = mWindowVisible;
    
    return NS_OK;
}

NS_IMETHODIMP CBrowserChrome::SetVisibility(PRBool aVisibility)
{
    NS_ENSURE_STATE(mBrowserWindow);
    
    if (aVisibility == mWindowVisible)
        return NS_OK;
        
    mWindowVisible = aVisibility;
    
    // If we are being told to become visible but we need to wait for
    // content to load so that we can size ourselves to it,
    // don't actually show it now. That will be done after the
    // load completes.
    PRBool sizingToContent = mIsMainContent &&
                             (mChromeFlags & CHROME_OPENAS_CHROME) &&
                             (mChromeFlags & CHROME_OPENAS_DIALOG);
    if (sizingToContent && mWindowVisible && !mInitialLoadComplete)
        return NS_OK;    
    aVisibility ? mBrowserWindow->Show() : mBrowserWindow->Hide();
    
    return NS_OK;
}

NS_IMETHODIMP CBrowserChrome::GetTitle(PRUnichar * *aTitle)
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

NS_IMETHODIMP CBrowserChrome::SetTitle(const PRUnichar * aTitle)
{
    NS_ENSURE_STATE(mBrowserWindow);
    NS_ENSURE_ARG(aTitle);
    
    Str255          pStr;
	
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(aTitle), pStr);
    mBrowserWindow->SetDescriptor(pStr);
    
    return NS_OK;
}

NS_IMETHODIMP CBrowserChrome::GetSiteWindow(void * *aSiteWindow)
{
    NS_ENSURE_ARG(aSiteWindow);
    NS_ENSURE_STATE(mBrowserWindow);

    *aSiteWindow = mBrowserWindow->GetMacWindow();
    
    return NS_OK;
}


//*****************************************************************************
// CBrowserChrome::nsIEmbeddingSiteWindow2
//*****************************************************************************   

NS_IMETHODIMP CBrowserChrome::Blur(void)
{    
    WindowPtr   currWindow = ::GetWindowList();
    WindowPtr   nextWindow;

    // Find the rearmost window and put ourselves behind it
    while (currWindow && ((nextWindow = ::MacGetNextWindow(currWindow)) != nsnull))
        currWindow = nextWindow;

    WindowPtr ourWindow = mBrowserWindow->GetMacWindow();
    if (ourWindow != currWindow)
        ::SendBehind(ourWindow, currWindow);
    
    return NS_OK;
}

//*****************************************************************************
// CBrowserChrome::nsIContextMenuListener2
//*****************************************************************************   

NS_IMETHODIMP CBrowserChrome::OnShowContextMenu(PRUint32 aContextFlags, nsIContextMenuInfo *aInfo)
{
    nsresult rv;
    
    try
    {
        rv = mBrowserShell->OnShowContextMenu(aContextFlags, aInfo);
    }
    catch (...)
    {
        rv = NS_ERROR_FAILURE;
    }
    return rv;
}

//*****************************************************************************
// CBrowserChrome::nsITooltipListener
//*****************************************************************************

NS_IMETHODIMP CBrowserChrome::OnShowTooltip(PRInt32 aXCoords, PRInt32 aYCoords, const PRUnichar *aTipText)
{
    nsresult rv;
    
    try
    {
        rv = mBrowserShell->OnShowTooltip(aXCoords, aYCoords, aTipText);
    }
    catch (...)
    {
        rv = NS_ERROR_FAILURE;
    }
    return rv;
}

NS_IMETHODIMP CBrowserChrome::OnHideTooltip()
{
    nsresult rv;
    
    try
    {
        rv = mBrowserShell->OnHideTooltip();
    }
    catch (...)
    {
        rv = NS_ERROR_FAILURE;
    }
    return rv;
}

//*****************************************************************************
// CBrowserChrome::LListener
//*****************************************************************************

void CBrowserChrome::ListenToMessage(MessageT inMessage, void* ioParam)
{
    switch (inMessage)
    {
        case msg_OnNetStopChange:
            {
                mInitialLoadComplete = true;

                // See if we need to size it and show it
                if (mIsMainContent &&
                    (mChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME) &&
                    (mChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_DIALOG))
                {
                    nsCOMPtr<nsIDOMWindow> domWindow;
                    (void)GetInterface(NS_GET_IID(nsIDOMWindow), getter_AddRefs(domWindow));
                    if (domWindow)
                        domWindow->SizeToContent();
                    if (mWindowVisible != mBrowserWindow->IsVisible())
                        mBrowserWindow->Show();
                }
                
                // If we are chrome, get the window title from the DOM
                if (mChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME)
                {
                    nsresult rv;
                    nsCOMPtr<nsIDOMWindow> domWindow;
                    rv = GetInterface(NS_GET_IID(nsIDOMWindow), getter_AddRefs(domWindow));
                    if (NS_FAILED(rv)) return;
                    nsCOMPtr<nsIDOMDocument> domDoc;
                    rv = domWindow->GetDocument(getter_AddRefs(domDoc));
                    if (NS_FAILED(rv)) return;
                    nsCOMPtr<nsIDOMElement> domDocElem;
                    rv = domDoc->GetDocumentElement(getter_AddRefs(domDocElem));
                    if (NS_FAILED(rv)) return;

                    nsAutoString windowTitle;
                    domDocElem->GetAttribute(NS_LITERAL_STRING("title"), windowTitle);
                    if (!windowTitle.IsEmpty()) {
                        Str255 pStr;
                        CPlatformUCSConversion::GetInstance()->UCSToPlatform(windowTitle, pStr);
                        mBrowserWindow->SetDescriptor(pStr);
                    }
                }
            }
            break;
    }
}

//*****************************************************************************
// Static Utility Method
//*****************************************************************************

LWindow* CBrowserChrome::GetLWindowForDOMWindow(nsIDOMWindow* aDOMWindow)
{
    if (!aDOMWindow)
        return nsnull;
        
    nsCOMPtr<nsIWindowWatcher> windowWatcher(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
    if (!windowWatcher)
        return nsnull;
    nsCOMPtr<nsIWebBrowserChrome> windowChrome;
    windowWatcher->GetChromeForWindow(aDOMWindow, getter_AddRefs(windowChrome));
    if (!windowChrome)
        return nsnull;        
    nsCOMPtr<nsIEmbeddingSiteWindow> siteWindow(do_QueryInterface(windowChrome));
    if (!siteWindow)
        return nsnull;
    WindowPtr macWindow = nsnull;
    siteWindow->GetSiteWindow((void **)&macWindow);
    if (!macWindow)
        return nsnull;
    
    return LWindow::FetchWindowObject(macWindow);
}

