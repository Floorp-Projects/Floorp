/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 *   Adam Lock <adamlock@netscape.com>
 *   Mike Pinkerton <pinkerton@netscape.com>
 *   Dan Rosen <dr@netscape.com>
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
#include "nsDocShellTreeOwner.h"
#include "nsWebBrowser.h"

// Helper Classes
#include "nsIGenericFactory.h"
#include "nsStyleCoord.h"
#include "nsSize.h"
#include "nsHTMLReflowState.h"
#include "nsIServiceManager.h"
#include "nsComponentManagerUtils.h"
#include "nsXPIDLString.h"
#include "nsIAtom.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsISimpleEnumerator.h"

// Interfaces needed to be included
#include "nsIPresContext.h"
#include "nsIContextMenuListener.h"
#include "nsIContextMenuListener2.h"
#include "nsITooltipListener.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIWebNavigation.h"
#include "nsIDOMHTMLElement.h"
#include "nsIPresShell.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMWindowCollection.h"
#include "nsIFocusController.h"
#include "nsIDOMWindowInternal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIWindowWatcher.h"
#include "nsPIWindowWatcher.h"
#include "nsIPrompt.h"
#include "nsRect.h"
#include "nsIWebBrowserChromeFocus.h"
#include "nsIContent.h"
#include "imgIContainer.h"
#include "nsContextMenuInfo.h"
#include "nsIPresContext.h"

//
// GetEventReceiver
//
// A helper routine that navigates the tricky path from a |nsWebBrowser| to
// a |nsIDOMEventReceiver| via the window root and chrome event handler.
//
static nsresult
GetEventReceiver ( nsWebBrowser* inBrowser, nsIDOMEventReceiver** outEventRcvr )
{
  nsCOMPtr<nsIDOMWindow> domWindow;
  inBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
  NS_ENSURE_TRUE(domWindow, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIDOMWindow> domWindowPrivate = do_QueryInterface(domWindow);
  NS_ENSURE_TRUE(domWindowPrivate, NS_ERROR_FAILURE);
  nsPIDOMWindow *rootWindow = domWindowPrivate->GetPrivateRoot();
  NS_ENSURE_TRUE(rootWindow, NS_ERROR_FAILURE);
  nsIChromeEventHandler *chromeHandler = rootWindow->GetChromeEventHandler();
  NS_ENSURE_TRUE(chromeHandler, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMEventReceiver> rcvr = do_QueryInterface(chromeHandler);
  *outEventRcvr = rcvr;
  NS_IF_ADDREF(*outEventRcvr);
  
  return NS_OK;
}


//*****************************************************************************
//***    nsDocShellTreeOwner: Object Management
//*****************************************************************************

nsDocShellTreeOwner::nsDocShellTreeOwner() :
   mWebBrowser(nsnull), 
   mTreeOwner(nsnull),
   mPrimaryContentShell(nsnull),
   mWebBrowserChrome(nsnull),
   mOwnerWin(nsnull),
   mOwnerRequestor(nsnull),
   mChromeTooltipListener(nsnull),
   mChromeContextMenuListener(nsnull)
{
}

nsDocShellTreeOwner::~nsDocShellTreeOwner()
{
  RemoveChromeListeners();
}

//*****************************************************************************
// nsDocShellTreeOwner::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(nsDocShellTreeOwner)
NS_IMPL_RELEASE(nsDocShellTreeOwner)

NS_INTERFACE_MAP_BEGIN(nsDocShellTreeOwner)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocShellTreeOwner)
    NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeOwner)
    NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
    NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
    NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
    NS_INTERFACE_MAP_ENTRY(nsICDocShellTreeOwner)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsDocShellTreeOwner::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP
nsDocShellTreeOwner::GetInterface(const nsIID& aIID, void** aSink)
{
  NS_ENSURE_ARG_POINTER(aSink);

  if(NS_SUCCEEDED(QueryInterface(aIID, aSink)))
    return NS_OK;

  if (aIID.Equals(NS_GET_IID(nsIWebBrowserChromeFocus)))
    return mOwnerWin->QueryInterface(aIID, aSink);

  if (aIID.Equals(NS_GET_IID(nsIPrompt))) {
    nsIPrompt *prompt;
    EnsurePrompter();
    prompt = mPrompter;
    if (prompt) {
      NS_ADDREF(prompt);
      *aSink = prompt;
      return NS_OK;
    }
    return NS_NOINTERFACE;
  }

  if (aIID.Equals(NS_GET_IID(nsIAuthPrompt))) {
    nsIAuthPrompt *prompt;
    EnsureAuthPrompter();
    prompt = mAuthPrompter;
    if (prompt) {
      NS_ADDREF(prompt);
      *aSink = prompt;
      return NS_OK;
    }
    return NS_NOINTERFACE;
  }

  if(mOwnerRequestor)
    return mOwnerRequestor->GetInterface(aIID, aSink);

  return NS_NOINTERFACE;
}

//*****************************************************************************
// nsDocShellTreeOwner::nsIDocShellTreeOwner
//*****************************************************************************   

NS_IMETHODIMP
nsDocShellTreeOwner::FindItemWithName(const PRUnichar* aName,
                                      nsIDocShellTreeItem* aRequestor,
                                      nsIDocShellTreeItem** aFoundItem)
{
  NS_ENSURE_ARG(aName);
  NS_ENSURE_ARG_POINTER(aFoundItem);
  *aFoundItem = nsnull; // if we don't find one, we return NS_OK and a null result 
  nsresult rv;

  nsAutoString name(aName);

  if (!mWebBrowser)
    return NS_OK; // stymied

  /* special cases */
  if(name.IsEmpty())
    return NS_OK;
  if(name.LowerCaseEqualsLiteral("_blank"))
    return NS_OK;
  // _main is an IE target which should be case-insensitive but isn't
  // see bug 217886 for details
  if(name.LowerCaseEqualsLiteral("_content") || name.EqualsLiteral("_main")) {
    *aFoundItem = mWebBrowser->mDocShellAsItem;
    NS_IF_ADDREF(*aFoundItem);
    return NS_OK;
  }

  // first, is it us?
  {
    nsCOMPtr<nsIDOMWindow> domWindow;
    mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (domWindow) {
      nsAutoString ourName;
      domWindow->GetName(ourName);
      if (name.Equals(ourName, nsCaseInsensitiveStringComparator())) {
        *aFoundItem = mWebBrowser->mDocShellAsItem;
        NS_IF_ADDREF(*aFoundItem);
        return NS_OK;
      }
    }
  }

  // next, check our children
  rv = FindChildWithName(aName, PR_TRUE, aRequestor, aFoundItem);
  if(NS_FAILED(rv) || *aFoundItem)
    return rv;

  // next, if we have a parent and it isn't the requestor, ask it
  nsCOMPtr<nsIDocShellTreeOwner> reqAsTreeOwner(do_QueryInterface(aRequestor));

  if(mTreeOwner) {
    if (mTreeOwner != reqAsTreeOwner)
      return mTreeOwner->FindItemWithName(aName, mWebBrowser->mDocShellAsItem,
                                          aFoundItem);
    return NS_OK;
  }

  // finally, failing everything else, search all windows, if we're not already
  if (mWebBrowser->mDocShellAsItem != aRequestor)
    return FindItemWithNameAcrossWindows(aName, aFoundItem);

  return NS_OK; // failed
}

nsresult
nsDocShellTreeOwner::FindChildWithName(const PRUnichar *aName, PRBool aRecurse,
                                       nsIDocShellTreeItem* aRequestor, 
                                       nsIDocShellTreeItem **aFoundItem)
{
  if (!mWebBrowser)
    return NS_OK;

  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMWindow> domWindow;
  mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
  if (!domWindow)
    return NS_OK;

  nsCOMPtr<nsIDOMWindowCollection> frames;
  domWindow->GetFrames(getter_AddRefs(frames));
  if (!frames)
    return NS_OK;

  PRUint32 ctr, count;
  frames->GetLength(&count);
  for (ctr = 0; ctr < count; ctr++) {
    nsCOMPtr<nsIDOMWindow> frame;
    frames->Item(ctr, getter_AddRefs(frame));
    if (frame) {
      nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(frame));
      if (sgo) {
        nsCOMPtr<nsIDocShellTreeItem> item =
          do_QueryInterface(sgo->GetDocShell());
        if (item && item != aRequestor) {
          rv = item->FindItemWithName(aName, mWebBrowser->mDocShellAsItem, aFoundItem);
          if (NS_FAILED(rv) || *aFoundItem)
            break;
        }
      }
    }
  }
  return rv;
}

nsresult
nsDocShellTreeOwner::FindItemWithNameAcrossWindows(const PRUnichar* aName,
                                                   nsIDocShellTreeItem** aFoundItem)
{
  // search for the item across the list of top-level windows
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  if (!wwatch)
    return NS_OK;

  PRBool   more;
  nsresult rv;

  nsCOMPtr<nsISimpleEnumerator> windows;
  wwatch->GetWindowEnumerator(getter_AddRefs(windows));

  rv = NS_OK;
  do {
    windows->HasMoreElements(&more);
    if (!more)
      break;
    nsCOMPtr<nsISupports> nextSupWindow;
    windows->GetNext(getter_AddRefs(nextSupWindow));
    if (nextSupWindow) {
      // it's a DOM Window. cut straight to the ScriptGlobalObject.
      nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(nextSupWindow));
      if (sgo) {
        nsCOMPtr<nsIDocShellTreeItem> item =
          do_QueryInterface(sgo->GetDocShell());
        if (item) {
          rv = item->FindItemWithName(aName, item, aFoundItem);
          if (NS_FAILED(rv) || *aFoundItem)
            break;
        }
      }
    }
  } while(1);

  return rv;
}

void
nsDocShellTreeOwner::EnsurePrompter()
{
  if (mPrompter)
    return;

  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  if (wwatch && mWebBrowser) {
    nsCOMPtr<nsIDOMWindow> domWindow;
    mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (domWindow)
      wwatch->GetNewPrompter(domWindow, getter_AddRefs(mPrompter));
  }
}

void
nsDocShellTreeOwner::EnsureAuthPrompter()
{
  if (mAuthPrompter)
    return;

  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  if (wwatch && mWebBrowser) {
    nsCOMPtr<nsIDOMWindow> domWindow;
    mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (domWindow)
      wwatch->GetNewAuthPrompter(domWindow, getter_AddRefs(mAuthPrompter));
  }
}

void
nsDocShellTreeOwner::AddToWatcher()
{
  if (mWebBrowser) {
    nsCOMPtr<nsIDOMWindow> domWindow;
    mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (domWindow) {
      nsCOMPtr<nsPIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
      if (wwatch)
        wwatch->AddWindow(domWindow, mWebBrowserChrome);
    }
  }
}

void
nsDocShellTreeOwner::RemoveFromWatcher()
{
  if (mWebBrowser) {
    nsCOMPtr<nsIDOMWindow> domWindow;
    mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (domWindow) {
      nsCOMPtr<nsPIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
      if (wwatch)
        wwatch->RemoveWindow(domWindow);
    }
  }
}


NS_IMETHODIMP
nsDocShellTreeOwner::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
                                       PRBool aPrimary, const PRUnichar* aID)
{
   if(mTreeOwner)
      return mTreeOwner->ContentShellAdded(aContentShell, aPrimary, aID);

   if (aPrimary)
      mPrimaryContentShell = aContentShell;
   return NS_OK;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetPrimaryContentShell(nsIDocShellTreeItem** aShell)
{
   NS_ENSURE_ARG_POINTER(aShell);

   if(mTreeOwner)
       return mTreeOwner->GetPrimaryContentShell(aShell);

   *aShell = (mPrimaryContentShell ? mPrimaryContentShell : mWebBrowser->mDocShellAsItem.get());
   NS_IF_ADDREF(*aShell);

   return NS_OK;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SizeShellTo(nsIDocShellTreeItem* aShellItem,
                                 PRInt32 aCX, PRInt32 aCY)
{
   NS_ENSURE_STATE(mTreeOwner || mWebBrowserChrome);

   if(mTreeOwner)
      return mTreeOwner->SizeShellTo(aShellItem, aCX, aCY);

   if(aShellItem == mWebBrowser->mDocShellAsItem)
      return mWebBrowserChrome->SizeBrowserTo(aCX, aCY);

   nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(aShellItem));
   NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

   nsCOMPtr<nsIDOMDocument> domDocument;
   webNav->GetDocument(getter_AddRefs(domDocument));
   NS_ENSURE_TRUE(domDocument, NS_ERROR_FAILURE);

   nsCOMPtr<nsIDOMElement> domElement;
   domDocument->GetDocumentElement(getter_AddRefs(domElement));
   NS_ENSURE_TRUE(domElement, NS_ERROR_FAILURE);

   // Set the preferred Size
   //XXX
   NS_ERROR("Implement this");
   /*
   Set the preferred size on the aShellItem.
   */

   nsCOMPtr<nsIPresContext> presContext;
   mWebBrowser->mDocShell->GetPresContext(getter_AddRefs(presContext));
   NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

   nsIPresShell *presShell = presContext->GetPresShell();
   NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(presShell->ResizeReflow(NS_UNCONSTRAINEDSIZE,
      NS_UNCONSTRAINEDSIZE), NS_ERROR_FAILURE);
   
   nsRect shellArea = presContext->GetVisibleArea();

   float pixelScale;
   pixelScale = presContext->TwipsToPixels();
   PRInt32 browserCX = PRInt32((float)shellArea.width*pixelScale);
   PRInt32 browserCY = PRInt32((float)shellArea.height*pixelScale);

   return mWebBrowserChrome->SizeBrowserTo(browserCX, browserCY);
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetPersistence(PRBool aPersistPosition,
                                    PRBool aPersistSize,
                                    PRBool aPersistSizeMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetPersistence(PRBool* aPersistPosition,
                                    PRBool* aPersistSize,
                                    PRBool* aPersistSizeMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//*****************************************************************************
// nsDocShellTreeOwner::nsIBaseWindow
//*****************************************************************************   


NS_IMETHODIMP
nsDocShellTreeOwner::InitWindow(nativeWindow aParentNativeWindow,
                                nsIWidget* aParentWidget, PRInt32 aX,
                                PRInt32 aY, PRInt32 aCX, PRInt32 aCY)   
{
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::Create()
{
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::Destroy()
{
  if (mOwnerWin)
  {
    return mWebBrowserChrome->DestroyBrowserWindow();
  }

  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetPosition(PRInt32 aX, PRInt32 aY)
{
  if (mOwnerWin)
  {
    return mOwnerWin->SetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION,
                                    aX, aY, 0, 0);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetPosition(PRInt32* aX, PRInt32* aY)
{
  if (mOwnerWin)
  {
    return mOwnerWin->GetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION,
                                    aX, aY, nsnull, nsnull);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetSize(PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
  if (mOwnerWin)
  {
    return mOwnerWin->SetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER,
                                    0, 0, aCX, aCY);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetSize(PRInt32* aCX, PRInt32* aCY)
{
  if (mOwnerWin)
  {
    return mOwnerWin->GetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER,
                                    nsnull, nsnull, aCX, aCY);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetPositionAndSize(PRInt32 aX, PRInt32 aY, PRInt32 aCX,
                                        PRInt32 aCY, PRBool aRepaint)
{
  if (mOwnerWin)
  {
    return mOwnerWin->SetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER |
                                    nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION,
                                    aX, aY, aCX, aCY);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetPositionAndSize(PRInt32* aX, PRInt32* aY, PRInt32* aCX,
                                        PRInt32* aCY)
{
  if (mOwnerWin)
  {
    return mOwnerWin->GetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER |
                                    nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION,
                                    aX, aY, aCX, aCY);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::Repaint(PRBool aForce)
{
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetParentWidget(nsIWidget** aParentWidget)
{
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetParentWidget(nsIWidget* aParentWidget)
{
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
  if (mOwnerWin)
  {
    return mOwnerWin->GetSiteWindow(aParentNativeWindow);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetVisibility(PRBool* aVisibility)
{
  if (mOwnerWin)
  {
    return mOwnerWin->GetVisibility(aVisibility);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetVisibility(PRBool aVisibility)
{
  if (mOwnerWin)
  {
    return mOwnerWin->SetVisibility(aVisibility);
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetEnabled(PRBool *aEnabled)
{
  NS_ENSURE_ARG_POINTER(aEnabled);
  *aEnabled = PR_TRUE;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetEnabled(PRBool aEnabled)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetBlurSuppression(PRBool *aBlurSuppression)
{
  NS_ENSURE_ARG_POINTER(aBlurSuppression);
  *aBlurSuppression = PR_FALSE;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetBlurSuppression(PRBool aBlurSuppression)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetMainWidget(nsIWidget** aMainWidget)
{
    return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetFocus()
{
    if (mOwnerWin)
    {
        return mOwnerWin->SetFocus();
    }
    return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::GetTitle(PRUnichar** aTitle)
{
    if (mOwnerWin)
    {
        return mOwnerWin->GetTitle(aTitle);
    }
    return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetTitle(const PRUnichar* aTitle)
{
    if (mOwnerWin)
    {
        return mOwnerWin->SetTitle(aTitle);
    }
    return NS_ERROR_NULL_POINTER;
}


//*****************************************************************************
// nsDocShellTreeOwner::nsIWebProgressListener
//*****************************************************************************   


NS_IMETHODIMP
nsDocShellTreeOwner::OnProgressChange(nsIWebProgress* aProgress,
                                      nsIRequest* aRequest,
                                      PRInt32 aCurSelfProgress,
                                      PRInt32 aMaxSelfProgress, 
                                      PRInt32 aCurTotalProgress,
                                      PRInt32 aMaxTotalProgress)
{
    // In the absence of DOM document creation event, this method is the
    // most convenient place to install the mouse listener on the
    // DOM document.
    return AddChromeListeners();
}

NS_IMETHODIMP
nsDocShellTreeOwner::OnStateChange(nsIWebProgress* aProgress,
                                   nsIRequest* aRequest,
                                   PRUint32 aProgressStateFlags,
                                   nsresult aStatus)
{
    return NS_OK;
}

NS_IMETHODIMP
nsDocShellTreeOwner::OnLocationChange(nsIWebProgress* aWebProgress,
                                      nsIRequest* aRequest,
                                      nsIURI* aURI)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsDocShellTreeOwner::OnStatusChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    nsresult aStatus,
                                    const PRUnichar* aMessage)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsDocShellTreeOwner::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                      nsIRequest *aRequest, 
                                      PRUint32 state)
{
    return NS_OK;
}


//*****************************************************************************
// nsDocShellTreeOwner: Helpers
//*****************************************************************************   

//*****************************************************************************
// nsDocShellTreeOwner: Accessors
//*****************************************************************************   

void
nsDocShellTreeOwner::WebBrowser(nsWebBrowser* aWebBrowser)
{
  if ( !aWebBrowser )
    RemoveChromeListeners();
  if (aWebBrowser != mWebBrowser) {
    mPrompter = 0;
    mAuthPrompter = 0;
  }

  mWebBrowser = aWebBrowser;
}

nsWebBrowser *
nsDocShellTreeOwner::WebBrowser()
{
   return mWebBrowser;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetTreeOwner(nsIDocShellTreeOwner* aTreeOwner)
{ 
  if(aTreeOwner) {
    nsCOMPtr<nsIWebBrowserChrome> webBrowserChrome(do_GetInterface(aTreeOwner));
    NS_ENSURE_TRUE(webBrowserChrome, NS_ERROR_INVALID_ARG);
    NS_ENSURE_SUCCESS(SetWebBrowserChrome(webBrowserChrome), NS_ERROR_INVALID_ARG);
    mTreeOwner = aTreeOwner;
  }
  else if(mWebBrowserChrome)
    mTreeOwner = nsnull;
  else {
    mTreeOwner = nsnull;
    NS_ENSURE_SUCCESS(SetWebBrowserChrome(nsnull), NS_ERROR_FAILURE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShellTreeOwner::SetWebBrowserChrome(nsIWebBrowserChrome* aWebBrowserChrome)
{
  if(!aWebBrowserChrome) {
    mWebBrowserChrome = nsnull;
    mOwnerWin = nsnull;
    mOwnerRequestor = nsnull;
  } else {
    nsCOMPtr<nsIEmbeddingSiteWindow> ownerWin(do_QueryInterface(aWebBrowserChrome));
    nsCOMPtr<nsIInterfaceRequestor> requestor(do_QueryInterface(aWebBrowserChrome));

    // it's ok for ownerWin or requestor to be null.
    mWebBrowserChrome = aWebBrowserChrome;
    mOwnerWin = ownerWin;
    mOwnerRequestor = requestor;
  }
  return NS_OK;
}


//
// AddChromeListeners
//
// Hook up things to the chrome like context menus and tooltips, if the chrome
// has implemented the right interfaces.
//
NS_IMETHODIMP
nsDocShellTreeOwner::AddChromeListeners()
{
  nsresult rv = NS_OK;
  
  // install tooltips
  if ( !mChromeTooltipListener ) { 
    nsCOMPtr<nsITooltipListener> tooltipListener ( do_QueryInterface(mWebBrowserChrome) );
    if ( tooltipListener ) {
      mChromeTooltipListener = new ChromeTooltipListener ( mWebBrowser, mWebBrowserChrome );
      if ( mChromeTooltipListener ) {
        NS_ADDREF(mChromeTooltipListener);
        rv = mChromeTooltipListener->AddChromeListeners();
      }
      else
        rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  
  // install context menus
  if ( !mChromeContextMenuListener ) {
    nsCOMPtr<nsIContextMenuListener2> contextListener2 ( do_QueryInterface(mWebBrowserChrome) );
    nsCOMPtr<nsIContextMenuListener> contextListener ( do_QueryInterface(mWebBrowserChrome) );
    if ( contextListener2 || contextListener ) {
      mChromeContextMenuListener = new ChromeContextMenuListener ( mWebBrowser, mWebBrowserChrome );
      if ( mChromeContextMenuListener ) {
        NS_ADDREF(mChromeContextMenuListener);
        rv = mChromeContextMenuListener->AddChromeListeners();
      }
      else
        rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
   
  // install the external dragDrop handler
  if ( !mChromeDragHandler ) {
    mChromeDragHandler = do_CreateInstance("@mozilla.org:/content/content-area-dragdrop;1", &rv);
    NS_ASSERTION(mChromeDragHandler, "Couldn't create the chrome drag handler");
    if ( mChromeDragHandler ) {
      nsCOMPtr<nsIDOMEventReceiver> rcvr;
      GetEventReceiver(mWebBrowser, getter_AddRefs(rcvr));
      nsCOMPtr<nsIDOMEventTarget> rcvrTarget(do_QueryInterface(rcvr));
      mChromeDragHandler->HookupTo(rcvrTarget, NS_STATIC_CAST(nsIWebNavigation*, mWebBrowser));
    }
  }

  return rv;
  
} // AddChromeListeners


NS_IMETHODIMP
nsDocShellTreeOwner::RemoveChromeListeners()
{
  if ( mChromeTooltipListener ) {
    mChromeTooltipListener->RemoveChromeListeners();
    NS_RELEASE(mChromeTooltipListener);
  }
  if ( mChromeContextMenuListener ) {
    mChromeContextMenuListener->RemoveChromeListeners();
    NS_RELEASE(mChromeContextMenuListener);
  }
  if ( mChromeDragHandler )
    mChromeDragHandler->Detach();

  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif


///////////////////////////////////////////////////////////////////////////////
// DefaultTooltipTextProvider

class DefaultTooltipTextProvider : public nsITooltipTextProvider
{
public:
    DefaultTooltipTextProvider();

    NS_DECL_ISUPPORTS
    NS_DECL_NSITOOLTIPTEXTPROVIDER
    
protected:
    nsCOMPtr<nsIAtom>   mTag_dialog;
    nsCOMPtr<nsIAtom>   mTag_dialogheader;
    nsCOMPtr<nsIAtom>   mTag_window;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(DefaultTooltipTextProvider, nsITooltipTextProvider)

DefaultTooltipTextProvider::DefaultTooltipTextProvider()
{
    // There are certain element types which we don't want to use
    // as tool tip text. 
    mTag_dialog       = do_GetAtom("dialog");
    mTag_dialogheader = do_GetAtom("dialogheader");
    mTag_window       = do_GetAtom("window");   
}

/* void getNodeText (in nsIDOMNode aNode, out wstring aText); */
NS_IMETHODIMP
DefaultTooltipTextProvider::GetNodeText(nsIDOMNode *aNode, PRUnichar **aText,
                                        PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(aNode);
  NS_ENSURE_ARG_POINTER(aText);
    
  nsString outText;

  PRBool found = PR_FALSE;
  nsCOMPtr<nsIDOMNode> current ( aNode );
  while ( !found && current ) {
    nsCOMPtr<nsIDOMElement> currElement ( do_QueryInterface(current) );
    if ( currElement ) {
      nsCOMPtr<nsIContent> content(do_QueryInterface(currElement));
      if (content) {
        nsIAtom *tagAtom = content->Tag();
        if (tagAtom != mTag_dialog &&
            tagAtom != mTag_dialogheader &&
            tagAtom != mTag_window) {
          // first try the normal title attribute...
          currElement->GetAttribute(NS_LITERAL_STRING("title"), outText);
          if ( outText.Length() )
            found = PR_TRUE;
          else {
            // ...ok, that didn't work, try it in the XLink namespace
            currElement->GetAttributeNS(NS_LITERAL_STRING("http://www.w3.org/1999/xlink"), NS_LITERAL_STRING("title"), outText);
            if ( outText.Length() )
              found = PR_TRUE;
          }
        }
      }
    }
    
    // not found here, walk up to the parent and keep trying
    if ( !found ) {
      nsCOMPtr<nsIDOMNode> temp ( current );
      temp->GetParentNode(getter_AddRefs(current));
    }
  } // while not found

  *_retval = found;
  *aText = (found) ? ToNewUnicode(outText) : nsnull;

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////

NS_IMPL_ADDREF(ChromeTooltipListener)
NS_IMPL_RELEASE(ChromeTooltipListener)

NS_INTERFACE_MAP_BEGIN(ChromeTooltipListener)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMouseListener)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMMouseListener)
    NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
    NS_INTERFACE_MAP_ENTRY(nsIDOMMouseMotionListener)
    NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
NS_INTERFACE_MAP_END


//
// ChromeTooltipListener ctor
//

ChromeTooltipListener::ChromeTooltipListener(nsWebBrowser* inBrowser,
                                             nsIWebBrowserChrome* inChrome) 
  : mWebBrowser(inBrowser), mWebBrowserChrome(inChrome),
     mTooltipListenerInstalled(PR_FALSE),
     mMouseClientX(0), mMouseClientY(0),
     mShowingTooltip(PR_FALSE)
{
  mTooltipTextProvider = do_GetService(NS_TOOLTIPTEXTPROVIDER_CONTRACTID);
  if (!mTooltipTextProvider) {
    nsISupports *pProvider = (nsISupports *) new DefaultTooltipTextProvider;
    mTooltipTextProvider = do_QueryInterface(pProvider);
  }
} // ctor


//
// ChromeTooltipListener dtor
//
ChromeTooltipListener::~ChromeTooltipListener()
{

} // dtor


//
// AddChromeListeners
//
// Hook up things to the chrome like context menus and tooltips, if the chrome
// has implemented the right interfaces.
//
NS_IMETHODIMP
ChromeTooltipListener::AddChromeListeners()
{  
  if ( !mEventReceiver )
    GetEventReceiver(mWebBrowser, getter_AddRefs(mEventReceiver));
  
  // Register the appropriate events for tooltips, but only if
  // the embedding chrome cares.
  nsresult rv = NS_OK;
  nsCOMPtr<nsITooltipListener> tooltipListener ( do_QueryInterface(mWebBrowserChrome) );
  if ( tooltipListener && !mTooltipListenerInstalled ) {
    rv = AddTooltipListener();
    if ( NS_FAILED(rv) )
      return rv;
  }
  
  return rv;
  
} // AddChromeListeners


//
// AddTooltipListener
//
// Subscribe to the events that will allow us to track tooltips. We need "mouse" for mouseExit,
// "mouse motion" for mouseMove, and "key" for keyDown. As we add the listeners, keep track
// of how many succeed so we can clean up correctly in Release().
//
NS_IMETHODIMP
ChromeTooltipListener::AddTooltipListener()
{
  if (mEventReceiver) {
    nsIDOMMouseListener *pListener = NS_STATIC_CAST(nsIDOMMouseListener *, this);
    nsresult rv = mEventReceiver->AddEventListenerByIID(pListener, NS_GET_IID(nsIDOMMouseListener));
    nsresult rv2 = mEventReceiver->AddEventListenerByIID(pListener, NS_GET_IID(nsIDOMMouseMotionListener));
    nsresult rv3 = mEventReceiver->AddEventListenerByIID(pListener, NS_GET_IID(nsIDOMKeyListener));
    
    // if all 3 succeed, we're a go!
    if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(rv2) && NS_SUCCEEDED(rv3)) 
      mTooltipListenerInstalled = PR_TRUE;
  }

  return NS_OK;
}


//
// RemoveChromeListeners
//
// Unsubscribe from the various things we've hooked up to the window root.
//
NS_IMETHODIMP
ChromeTooltipListener::RemoveChromeListeners ( )
{
  HideTooltip();

  if ( mTooltipListenerInstalled )
    RemoveTooltipListener();
  
  mEventReceiver = nsnull;
  
  // it really doesn't matter if these fail...
  return NS_OK;
  
} // RemoveChromeTooltipListeners



//
// RemoveTooltipListener
//
// Unsubscribe from all the various tooltip events that we were listening to
//
NS_IMETHODIMP 
ChromeTooltipListener::RemoveTooltipListener()
{
  if (mEventReceiver) {
    nsIDOMMouseListener *pListener = NS_STATIC_CAST(nsIDOMMouseListener *, this);
    nsresult rv = mEventReceiver->RemoveEventListenerByIID(pListener, NS_GET_IID(nsIDOMMouseListener));
    nsresult rv2 = mEventReceiver->RemoveEventListenerByIID(pListener, NS_GET_IID(nsIDOMMouseMotionListener));
    nsresult rv3 = mEventReceiver->RemoveEventListenerByIID(pListener, NS_GET_IID(nsIDOMKeyListener));
    if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(rv2) && NS_SUCCEEDED(rv3))
      mTooltipListenerInstalled = PR_FALSE;
  }

  return NS_OK;
}


//
// KeyDown
//
// When the user starts typing, they generaly don't want to see any messy wax
// builup. Hide the tooltip.
//
nsresult
ChromeTooltipListener::KeyDown(nsIDOMEvent* aMouseEvent)
{
  return HideTooltip();
} // KeyDown


//
// KeyUp
// KeyPress
//
// We can ignore these as they are already handled by KeyDown
//
nsresult
ChromeTooltipListener::KeyUp(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
    
} // KeyUp

nsresult
ChromeTooltipListener::KeyPress(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
    
} // KeyPress


//
// MouseDown
//
// On a click, hide the tooltip
//
nsresult 
ChromeTooltipListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
  return HideTooltip();

} // MouseDown


nsresult 
ChromeTooltipListener::MouseUp(nsIDOMEvent* aMouseEvent)
{
    return NS_OK; 
}

nsresult 
ChromeTooltipListener::MouseClick(nsIDOMEvent* aMouseEvent)
{
    return NS_OK; 
}

nsresult 
ChromeTooltipListener::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
    return NS_OK; 
}

nsresult 
ChromeTooltipListener::MouseOver(nsIDOMEvent* aMouseEvent)
{
    return NS_OK; 
}


//
// MouseOut
//
// If we're responding to tooltips, hide the tip whenever the mouse leaves
// the area it was in.
nsresult 
ChromeTooltipListener::MouseOut(nsIDOMEvent* aMouseEvent)
{
  return HideTooltip();
}


//
// MouseMove
//
// If we're a tooltip, fire off a timer to see if a tooltip should be shown. If the
// timer fires, we cache the node in |mPossibleTooltipNode|.
//
nsresult
ChromeTooltipListener::MouseMove(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
  if (!mouseEvent)
    return NS_OK;

  // stash the coordinates of the event so that we can still get back to it from within the 
  // timer callback. On win32, we'll get a MouseMove event even when a popup goes away --
  // even when the mouse doesn't change position! To get around this, we make sure the
  // mouse has really moved before proceeding.
  PRInt32 newMouseX, newMouseY;
  mouseEvent->GetClientX(&newMouseX);
  mouseEvent->GetClientY(&newMouseY);
  if ( mMouseClientX == newMouseX && mMouseClientY == newMouseY )
    return NS_OK;
  mMouseClientX = newMouseX; mMouseClientY = newMouseY;

  // We want to close the tip if it is being displayed and the mouse moves. Recall 
  // that |mShowingTooltip| is set when the popup is showing. Furthermore, as the mouse
  // moves, we want to make sure we reset the timer to show it, so that the delay
  // is from when the mouse stops moving, not when it enters the element.
  if ( mShowingTooltip )
    return HideTooltip();
  if ( mTooltipTimer )
    mTooltipTimer->Cancel();

  mTooltipTimer = do_CreateInstance("@mozilla.org/timer;1");
  if ( mTooltipTimer ) {
    nsCOMPtr<nsIDOMEventTarget> eventTarget;
    aMouseEvent->GetTarget(getter_AddRefs(eventTarget));
    if ( eventTarget )
      mPossibleTooltipNode = do_QueryInterface(eventTarget);
    if ( mPossibleTooltipNode ) {
      nsresult rv = mTooltipTimer->InitWithFuncCallback(sTooltipCallback, this, kTooltipShowTime, 
                                                        nsITimer::TYPE_ONE_SHOT);
      if (NS_FAILED(rv))
        mPossibleTooltipNode = nsnull;
    }
  }
  else
    NS_WARNING ( "Could not create a timer for tooltip tracking" );
    
  return NS_OK;
  
} // MouseMove


//
// ShowTooltip
//
// Tell the registered chrome that they should show the tooltip
//
NS_IMETHODIMP
ChromeTooltipListener::ShowTooltip(PRInt32 inXCoords, PRInt32 inYCoords,
                                   const nsAString & inTipText)
{
  nsresult rv = NS_OK;
  
  // do the work to call the client
  nsCOMPtr<nsITooltipListener> tooltipListener ( do_QueryInterface(mWebBrowserChrome) );
  if ( tooltipListener ) {
    rv = tooltipListener->OnShowTooltip ( inXCoords, inYCoords, PromiseFlatString(inTipText).get() ); 
    if ( NS_SUCCEEDED(rv) )
      mShowingTooltip = PR_TRUE;
  }

  return rv;
  
} // ShowTooltip


//
// HideTooltip
//
// Tell the registered chrome that they should rollup the tooltip
// NOTE: This routine is safe to call even if the popup is already closed.
//
NS_IMETHODIMP
ChromeTooltipListener::HideTooltip()
{
  nsresult rv = NS_OK;
  
  // shut down the relevant timers
  if ( mTooltipTimer ) {
    mTooltipTimer->Cancel();
    mTooltipTimer = nsnull;
    // release tooltip target
    mPossibleTooltipNode = nsnull;
  }
  if ( mAutoHideTimer ) {
    mAutoHideTimer->Cancel();
    mAutoHideTimer = nsnull;
  }

  // if we're showing the tip, tell the chrome to hide it
  if ( mShowingTooltip ) {
    nsCOMPtr<nsITooltipListener> tooltipListener ( do_QueryInterface(mWebBrowserChrome) );
    if ( tooltipListener ) {
      rv = tooltipListener->OnHideTooltip ( );
      if ( NS_SUCCEEDED(rv) )
        mShowingTooltip = PR_FALSE;
    }
  }

  return rv;
  
} // HideTooltip


//
// sTooltipCallback
//
// A timer callback, fired when the mouse has hovered inside of a frame for the 
// appropriate amount of time. Getting to this point means that we should show the
// tooltip, but only after we determine there is an appropriate TITLE element.
//
// This relies on certain things being cached into the |aChromeTooltipListener| object passed to
// us by the timer:
//   -- the x/y coordinates of the mouse      (mMouseClientY, mMouseClientX)
//   -- the dom node the user hovered over    (mPossibleTooltipNode)
//
void
ChromeTooltipListener::sTooltipCallback(nsITimer *aTimer,
                                        void *aChromeTooltipListener)
{
  ChromeTooltipListener* self = NS_STATIC_CAST(ChromeTooltipListener*, aChromeTooltipListener);
  if ( self && self->mPossibleTooltipNode ) {

    // if there is text associated with the node, show the tip and fire
    // off a timer to auto-hide it.

    nsXPIDLString tooltipText;
    if (self->mTooltipTextProvider) {
      PRBool textFound = PR_FALSE;

      self->mTooltipTextProvider->GetNodeText(
          self->mPossibleTooltipNode, getter_Copies(tooltipText), &textFound);
      
      if (textFound) {
        nsString tipText(tooltipText);
        self->CreateAutoHideTimer ( );  
        self->ShowTooltip (self->mMouseClientX, self->mMouseClientY, tipText);
      }
    }
    
    // release tooltip target if there is one, NO MATTER WHAT
    self->mPossibleTooltipNode = nsnull;
  } // if "self" data valid
  
} // sTooltipCallback


//
// CreateAutoHideTimer
//
// Create a new timer to see if we should auto-hide. It's ok if this fails.
//
void
ChromeTooltipListener::CreateAutoHideTimer()
{
  // just to be anal (er, safe)
  if ( mAutoHideTimer ) {
    mAutoHideTimer->Cancel();
    mAutoHideTimer = nsnull;
  }
  
  mAutoHideTimer = do_CreateInstance("@mozilla.org/timer;1");
  if ( mAutoHideTimer )
    mAutoHideTimer->InitWithFuncCallback(sAutoHideCallback, this, kTooltipAutoHideTime, 
                                         nsITimer::TYPE_ONE_SHOT);

} // CreateAutoHideTimer


//
// sAutoHideCallback
//
// This fires after a tooltip has been open for a certain length of time. Just tell
// the listener to close the popup. We don't have to worry, because HideTooltip() can
// be called multiple times, even if the tip has already been closed.
//
void
ChromeTooltipListener::sAutoHideCallback(nsITimer *aTimer, void* aListener)
{
  ChromeTooltipListener* self = NS_STATIC_CAST(ChromeTooltipListener*, aListener);
  if ( self )
    self->HideTooltip();

  // NOTE: |aTimer| and |self->mAutoHideTimer| are invalid after calling ClosePopup();
  
} // sAutoHideCallback



#ifdef XP_MAC
#pragma mark -
#endif


NS_IMPL_ADDREF(ChromeContextMenuListener)
NS_IMPL_RELEASE(ChromeContextMenuListener)

NS_INTERFACE_MAP_BEGIN(ChromeContextMenuListener)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMContextMenuListener)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMContextMenuListener)
    NS_INTERFACE_MAP_ENTRY(nsIDOMContextMenuListener)
NS_INTERFACE_MAP_END


//
// ChromeTooltipListener ctor
//
ChromeContextMenuListener::ChromeContextMenuListener(nsWebBrowser* inBrowser, nsIWebBrowserChrome* inChrome ) 
  : mContextMenuListenerInstalled(PR_FALSE),
    mWebBrowser(inBrowser),
    mWebBrowserChrome(inChrome)
{
} // ctor


//
// ChromeTooltipListener dtor
//
ChromeContextMenuListener::~ChromeContextMenuListener()
{
} // dtor


//
// AddContextMenuListener
//
// Subscribe to the events that will allow us to track context menus. Bascially, this
// is just the context-menu DOM event.
//
NS_IMETHODIMP
ChromeContextMenuListener::AddContextMenuListener()
{
  if (mEventReceiver) {
    nsIDOMContextMenuListener *pListener = NS_STATIC_CAST(nsIDOMContextMenuListener *, this);
    nsresult rv = mEventReceiver->AddEventListenerByIID(pListener, NS_GET_IID(nsIDOMContextMenuListener));
    if (NS_SUCCEEDED(rv))
      mContextMenuListenerInstalled = PR_TRUE;
  }

  return NS_OK;
}


//
// RemoveContextMenuListener
//
// Unsubscribe from all the various context menu events that we were listening to. 
//
NS_IMETHODIMP 
ChromeContextMenuListener::RemoveContextMenuListener()
{
  if (mEventReceiver) {
    nsIDOMContextMenuListener *pListener = NS_STATIC_CAST(nsIDOMContextMenuListener *, this);
    nsresult rv = mEventReceiver->RemoveEventListenerByIID(pListener, NS_GET_IID(nsIDOMContextMenuListener));
    if (NS_SUCCEEDED(rv))
      mContextMenuListenerInstalled = PR_FALSE;
  }

  return NS_OK;
}


//
// AddChromeListeners
//
// Hook up things to the chrome like context menus and tooltips, if the chrome
// has implemented the right interfaces.
//
NS_IMETHODIMP
ChromeContextMenuListener::AddChromeListeners()
{  
  if ( !mEventReceiver )
    GetEventReceiver(mWebBrowser, getter_AddRefs(mEventReceiver));
  
  // Register the appropriate events for context menus, but only if
  // the embedding chrome cares.
  nsresult rv = NS_OK;

  nsCOMPtr<nsIContextMenuListener2> contextListener2 ( do_QueryInterface(mWebBrowserChrome) );
  nsCOMPtr<nsIContextMenuListener> contextListener ( do_QueryInterface(mWebBrowserChrome) );
  if ( (contextListener || contextListener2) && !mContextMenuListenerInstalled )
    rv = AddContextMenuListener();

  return rv;
  
} // AddChromeListeners


//
// RemoveChromeListeners
//
// Unsubscribe from the various things we've hooked up to the window root.
//
NS_IMETHODIMP
ChromeContextMenuListener::RemoveChromeListeners()
{
  if ( mContextMenuListenerInstalled )
    RemoveContextMenuListener();
  
  mEventReceiver = nsnull;
  
  // it really doesn't matter if these fail...
  return NS_OK;
  
} // RemoveChromeTooltipListeners



//
// ContextMenu
//
// We're on call to show the context menu. Dig around in the DOM to
// find the type of object we're dealing with and notify the front
// end chrome.
//
NS_IMETHODIMP
ChromeContextMenuListener::ContextMenu(nsIDOMEvent* aMouseEvent)
{     
  nsCOMPtr<nsIDOMEventTarget> targetNode;
  nsresult res = aMouseEvent->GetTarget(getter_AddRefs(targetNode));
  if (NS_FAILED(res))
    return res;
  if (!targetNode)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> targetDOMnode;
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(targetNode);
  if (!node)
    return NS_OK;

  // Stop the context menu event going to other windows (bug 78396)
  aMouseEvent->PreventDefault();
  
  // If the listener is a nsIContextMenuListener2, create the info object
  nsCOMPtr<nsIContextMenuListener2> menuListener2(do_QueryInterface(mWebBrowserChrome));
  nsContextMenuInfo *menuInfoImpl = nsnull;
  nsCOMPtr<nsIContextMenuInfo> menuInfo;
  if (menuListener2) {
    menuInfoImpl = new nsContextMenuInfo;
    if (!menuInfoImpl)
      return NS_ERROR_OUT_OF_MEMORY;
    menuInfo = menuInfoImpl; 
  }

  // Find the first node to be an element starting with this node and
  // working up through its parents.

  PRUint32 flags = nsIContextMenuListener::CONTEXT_NONE;
  PRUint32 flags2 = nsIContextMenuListener2::CONTEXT_NONE;
  nsCOMPtr<nsIContent> content;
  do {
    // XXX test for selected text
    content = do_QueryInterface(node);
    if (content && content->IsContentOfType(nsIContent::eHTML)) {
      const char *tagStr;
      content->Tag()->GetUTF8String(&tagStr);

      // Test what kind of element we're dealing with here
      if (strcmp(tagStr, "img") == 0)
      {
        flags |= nsIContextMenuListener::CONTEXT_IMAGE;
        flags2 |= nsIContextMenuListener2::CONTEXT_IMAGE;
        targetDOMnode = node;
        // if we see an image, keep searching for a possible anchor
      }
      else if (strcmp(tagStr, "input") == 0)
      {
        // INPUT element - button, combo, checkbox, text etc.
        flags |= nsIContextMenuListener::CONTEXT_INPUT;
        flags2 |= nsIContextMenuListener2::CONTEXT_INPUT;
        targetDOMnode = node;
        
        // See if the input type is an image
        if (menuListener2) {
          nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(node));
          if (inputElement) {
            nsAutoString inputElemType;
            inputElement->GetType(inputElemType);
            if (inputElemType.LowerCaseEqualsLiteral("image"))
              flags2 |= nsIContextMenuListener2::CONTEXT_IMAGE;
          }
        }
        break; // exit do-while
      }
      else if (strcmp(tagStr, "textarea") == 0)
      {
        // text area
        flags |= nsIContextMenuListener::CONTEXT_TEXT;
        flags2 |= nsIContextMenuListener2::CONTEXT_TEXT;
        targetDOMnode = node;
        break; // exit do-while
      }
      else if (strcmp(tagStr, "html") == 0)
      {
        if (!flags && !flags2) { 
        // only care about this if no other context was found.
            flags |= nsIContextMenuListener::CONTEXT_DOCUMENT;
            flags2 |= nsIContextMenuListener2::CONTEXT_DOCUMENT;
            targetDOMnode = node;
        }
        if (!(flags & nsIContextMenuListener::CONTEXT_IMAGE)) {
          // first check if this is a background image that the user was trying to click on
          // and if the listener is ready for that (only nsIContextMenuListener2 and up)
          if (menuInfoImpl && menuInfoImpl->HasBackgroundImage(node)) {
            flags2 |= nsIContextMenuListener2::CONTEXT_BACKGROUND_IMAGE;
          }
        }
        break; // exit do-while
      }
      else if (strcmp(tagStr, "object") == 0 ||  /* XXX what about images and documents? */
               strcmp(tagStr, "embed") == 0 ||
               strcmp(tagStr, "applet") == 0)
      {                // always consume events for plugins and Java 
        return NS_OK;  // who may throw their own context menus
      }

      // Test if the element has an associated link
      nsCOMPtr<nsIDOMElement> element(do_QueryInterface(node));

      PRBool hasAttr = PR_FALSE;
      res = element->HasAttribute(NS_LITERAL_STRING("href"), &hasAttr);

      if (NS_SUCCEEDED(res) && hasAttr)
      {
        flags |= nsIContextMenuListener::CONTEXT_LINK;
        flags2 |= nsIContextMenuListener2::CONTEXT_LINK;
        if (!targetDOMnode)
          targetDOMnode = node;
        if (menuInfoImpl)
          menuInfoImpl->SetAssociatedLink(node);
        break; // exit do-while
      }
    }

    // walk-up-the-tree
    nsCOMPtr<nsIDOMNode> parentNode;
    node->GetParentNode(getter_AddRefs(parentNode));
    node = parentNode;
  } while (node);

  // we need to cache the event target into the focus controller's popupNode
  // so we can get at it later from command code, etc.:

  // get the dom window
  nsCOMPtr<nsIDOMWindow> win;
  res = mWebBrowser->GetContentDOMWindow(getter_AddRefs(win));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(win, NS_ERROR_FAILURE);
  // get the private dom window
  nsCOMPtr<nsPIDOMWindow> privateWin(do_QueryInterface(win, &res));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(privateWin, NS_ERROR_FAILURE);
  // get the focus controller
  nsIFocusController *focusController = privateWin->GetRootFocusController();
  NS_ENSURE_TRUE(focusController, NS_ERROR_FAILURE);
  // set the focus controller's popup node to the event target
  res = focusController->SetPopupNode(targetDOMnode);
  NS_ENSURE_SUCCESS(res, res);

  // Tell the listener all about the event
  if ( menuListener2 ) {
    menuInfoImpl->SetMouseEvent(aMouseEvent);
    menuInfoImpl->SetDOMNode(targetDOMnode);
    menuListener2->OnShowContextMenu(flags2, menuInfo);
  }
  else {
    nsCOMPtr<nsIContextMenuListener> menuListener(do_QueryInterface(mWebBrowserChrome));
    if ( menuListener )
      menuListener->OnShowContextMenu(flags, aMouseEvent, targetDOMnode);
  }

  return NS_OK;

} // MouseDown
