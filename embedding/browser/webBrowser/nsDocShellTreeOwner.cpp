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
 */

// Local Includes
#include "nsDocShellTreeOwner.h"
#include "nsWebBrowser.h"

// Helper Classes
#include "nsIGenericFactory.h"
#include "nsStyleCoord.h"
#include "nsHTMLReflowState.h"

// Interfaces needed to be included
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIPresShell.h"

// CIDs

//*****************************************************************************
//***    nsDocShellTreeOwner: Object Management
//*****************************************************************************

nsDocShellTreeOwner::nsDocShellTreeOwner() : mWebBrowser(nsnull), 
   mTreeOwner(nsnull)
{
	NS_INIT_REFCNT();
}

nsDocShellTreeOwner::~nsDocShellTreeOwner()
{
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
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsDocShellTreeOwner::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP nsDocShellTreeOwner::GetInterface(const nsIID& aIID, void** aSink)
{
   NS_ENSURE_ARG_POINTER(aSink);

   if(NS_SUCCEEDED(QueryInterface(aIID, aSink)))
      return NS_OK;

   if(mOwnerRequestor)
      return mOwnerRequestor->GetInterface(aIID, aSink);

   return NS_NOINTERFACE;
}

//*****************************************************************************
// nsDocShellTreeOwner::nsIDocShellTreeOwner
//*****************************************************************************   

NS_IMETHODIMP nsDocShellTreeOwner::FindItemWithName(const PRUnichar* aName,
   nsIDocShellTreeItem* aRequestor, nsIDocShellTreeItem** aFoundItem)
{
   NS_ENSURE_ARG_POINTER(aFoundItem);
   *aFoundItem = nsnull;

   nsAutoString name(aName);

   /* Special Cases */
   if(name.Length() == 0)
      return NS_OK;
   if(name.EqualsIgnoreCase("_blank"))
      return NS_OK;
   if(name.EqualsIgnoreCase("_content"))
      {
      *aFoundItem = mWebBrowser->mDocShellAsItem;
      NS_IF_ADDREF(*aFoundItem);
      return NS_OK;
      }

   if(mTreeOwner)
      return mTreeOwner->FindItemWithName(aName, aRequestor, aFoundItem);

   NS_ENSURE_TRUE(mWebBrowserChrome, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(mWebBrowserChrome->FindNamedBrowserItem(aName, aFoundItem),
      NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShellTreeOwner::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
   PRBool aPrimary, const PRUnichar* aID)
{
   if(mTreeOwner)
      return mTreeOwner->ContentShellAdded(aContentShell, aPrimary, aID);

   return NS_OK;
}

NS_IMETHODIMP nsDocShellTreeOwner::GetPrimaryContentShell(nsIDocShellTreeItem** aShell)
{
   NS_ENSURE_ARG_POINTER(aShell);

   *aShell = mWebBrowser->mDocShellAsItem;
   NS_IF_ADDREF(*aShell);

   return NS_OK;
}

NS_IMETHODIMP nsDocShellTreeOwner::SizeShellTo(nsIDocShellTreeItem* aShellItem,
   PRInt32 aCX, PRInt32 aCY)
{
   NS_ENSURE_STATE(mTreeOwner || mWebBrowserChrome);

   if(mTreeOwner)
      return mTreeOwner->SizeShellTo(aShellItem, aCX, aCY);

   if(aShellItem == mWebBrowser->mDocShellAsItem.get())
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

   nsCOMPtr<nsIPresShell> presShell;
   presContext->GetShell(getter_AddRefs(presShell));
   NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(presShell->ResizeReflow(NS_UNCONSTRAINEDSIZE,
      NS_UNCONSTRAINEDSIZE), NS_ERROR_FAILURE);
   
   nsRect shellArea;

   presContext->GetVisibleArea(shellArea);
   float pixelScale;
   presContext->GetTwipsToPixels(&pixelScale);
   PRInt32 browserCX = PRInt32((float)shellArea.width*pixelScale);
   PRInt32 browserCY = PRInt32((float)shellArea.height*pixelScale);

   return mWebBrowserChrome->SizeBrowserTo(browserCX, browserCY);
}

NS_IMETHODIMP nsDocShellTreeOwner::ShowModal()
{
   if(mTreeOwner)
      return mTreeOwner->ShowModal();

   return mWebBrowserChrome->ShowAsModal();
}

NS_IMETHODIMP nsDocShellTreeOwner::GetNewWindow(PRInt32 aChromeFlags,
   nsIDocShellTreeItem** aDocShellTreeItem)
{
   if(mTreeOwner)
      return mTreeOwner->GetNewWindow(aChromeFlags, aDocShellTreeItem);

   *aDocShellTreeItem = nsnull;

   nsCOMPtr<nsIWebBrowser> webBrowser;
   mWebBrowserChrome->GetNewBrowser(aChromeFlags, getter_AddRefs(webBrowser));
   NS_ENSURE_TRUE(webBrowser, NS_ERROR_FAILURE);

   nsCOMPtr<nsIDocShell> docShell;
   webBrowser->GetDocShell(getter_AddRefs(docShell));
   NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(CallQueryInterface(docShell, aDocShellTreeItem),
      NS_ERROR_FAILURE);

   return NS_OK;
}

//*****************************************************************************
// nsDocShellTreeOwner::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP nsDocShellTreeOwner::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* aParentWidget, PRInt32 aX, PRInt32 aY, PRInt32 aCX, PRInt32 aCY)   
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->InitWindow(aParentNativeWindow, aParentWidget, aX, aY,
      aCX, aCY); 
}

NS_IMETHODIMP nsDocShellTreeOwner::Create()
{
   NS_ENSURE_STATE(mOwnerWin);
   
   return mOwnerWin->Create();
}

NS_IMETHODIMP nsDocShellTreeOwner::Destroy()
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->Destroy();
}

NS_IMETHODIMP nsDocShellTreeOwner::SetPosition(PRInt32 aX, PRInt32 aY)
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->SetPosition(aX, aY);
}

NS_IMETHODIMP nsDocShellTreeOwner::GetPosition(PRInt32* aX, PRInt32* aY)
{
   NS_ENSURE_STATE(mOwnerWin);
   
   return mOwnerWin->GetPosition(aX, aY);
}

NS_IMETHODIMP nsDocShellTreeOwner::SetSize(PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->SetSize(aCX, aCY, aRepaint);
}

NS_IMETHODIMP nsDocShellTreeOwner::GetSize(PRInt32* aCX, PRInt32* aCY)
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->GetSize(aCX, aCY);
}

NS_IMETHODIMP nsDocShellTreeOwner::SetPositionAndSize(PRInt32 aX, PRInt32 aY,
   PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->SetPositionAndSize(aX, aY, aCX, aCY, aRepaint);
}

NS_IMETHODIMP nsDocShellTreeOwner::GetPositionAndSize(PRInt32* aX, PRInt32* aY,
   PRInt32* aCX, PRInt32* aCY)
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->GetPositionAndSize(aX, aY, aCX, aCY);
}

NS_IMETHODIMP nsDocShellTreeOwner::Repaint(PRBool aForce)
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->Repaint(aForce);
}

NS_IMETHODIMP nsDocShellTreeOwner::GetParentWidget(nsIWidget** aParentWidget)
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->GetParentWidget(aParentWidget);
}

NS_IMETHODIMP nsDocShellTreeOwner::SetParentWidget(nsIWidget* aParentWidget)
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->SetParentWidget(aParentWidget);
}

NS_IMETHODIMP nsDocShellTreeOwner::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->GetParentNativeWindow(aParentNativeWindow);
}

NS_IMETHODIMP nsDocShellTreeOwner::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->SetParentNativeWindow(aParentNativeWindow);
}

NS_IMETHODIMP nsDocShellTreeOwner::GetVisibility(PRBool* aVisibility)
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->GetVisibility(aVisibility);
}

NS_IMETHODIMP nsDocShellTreeOwner::SetVisibility(PRBool aVisibility)
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->SetVisibility(aVisibility);
}

NS_IMETHODIMP nsDocShellTreeOwner::GetMainWidget(nsIWidget** aMainWidget)
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->GetMainWidget(aMainWidget);
}

NS_IMETHODIMP nsDocShellTreeOwner::SetFocus()
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->SetFocus();
}

NS_IMETHODIMP nsDocShellTreeOwner::FocusAvailable(nsIBaseWindow* aCurrentFocus, 
   PRBool* aTookFocus)
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->FocusAvailable(aCurrentFocus, aTookFocus);
}

NS_IMETHODIMP nsDocShellTreeOwner::GetTitle(PRUnichar** aTitle)
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->GetTitle(aTitle);
}

NS_IMETHODIMP nsDocShellTreeOwner::SetTitle(const PRUnichar* aTitle)
{
   NS_ENSURE_STATE(mOwnerWin);

   return mOwnerWin->SetTitle(aTitle);
}

//*****************************************************************************
// nsDocShellTreeOwner::nsIWebProgressListener
//*****************************************************************************   

NS_IMETHODIMP nsDocShellTreeOwner::OnProgressChange(nsIChannel* aChannel,
   PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, 
   PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
   if(!mOwnerProgressListener)
      return NS_OK;
      
   return mOwnerProgressListener->OnProgressChange(aChannel, 
         aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress,
         aMaxTotalProgress);
}

NS_IMETHODIMP nsDocShellTreeOwner::OnChildProgressChange(nsIChannel* aChannel,
   PRInt32 aCurChildProgress, PRInt32 aMaxChildProgress)
{
   if(!mOwnerProgressListener)
      return NS_OK;

   return mOwnerProgressListener->OnChildProgressChange(aChannel, 
         aCurChildProgress, aMaxChildProgress);
} 

NS_IMETHODIMP nsDocShellTreeOwner::OnStatusChange(nsIChannel* aChannel,
   PRInt32 aProgressStatusFlags)
{
   if(!mOwnerProgressListener)
      return NS_OK;
   
   return mOwnerProgressListener->OnStatusChange(aChannel, 
         aProgressStatusFlags);
}

NS_IMETHODIMP nsDocShellTreeOwner::OnChildStatusChange(nsIChannel* aChannel,
   PRInt32 aProgressStatusFlags)
{
   if(!mOwnerProgressListener)
      return NS_OK;

   return mOwnerProgressListener->OnChildStatusChange(aChannel, 
      aProgressStatusFlags);
}

NS_IMETHODIMP nsDocShellTreeOwner::OnLocationChange(nsIURI* aURI)
{
   if(!mOwnerProgressListener)
      return NS_OK;

   return mOwnerProgressListener->OnLocationChange(aURI);
}

//*****************************************************************************
// nsDocShellTreeOwner: Helpers
//*****************************************************************************   

//*****************************************************************************
// nsDocShellTreeOwner: Accessors
//*****************************************************************************   

void nsDocShellTreeOwner::WebBrowser(nsWebBrowser* aWebBrowser)
{
   mWebBrowser = aWebBrowser;
}

nsWebBrowser* nsDocShellTreeOwner::WebBrowser()
{
   return mWebBrowser;
}

NS_IMETHODIMP nsDocShellTreeOwner::SetTreeOwner(nsIDocShellTreeOwner* aTreeOwner)
{ 
   if(aTreeOwner)
      {
      nsCOMPtr<nsIWebBrowserChrome> webBrowserChrome(do_GetInterface(aTreeOwner));
      NS_ENSURE_TRUE(webBrowserChrome, NS_ERROR_INVALID_ARG);
      NS_ENSURE_SUCCESS(SetWebBrowserChrome(webBrowserChrome), NS_ERROR_INVALID_ARG);
      mTreeOwner = aTreeOwner;
      }
   else if(mWebBrowserChrome)
      mTreeOwner = nsnull;
   else
      {
      mTreeOwner = nsnull;
      NS_ENSURE_SUCCESS(SetWebBrowserChrome(nsnull), NS_ERROR_FAILURE);
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShellTreeOwner::SetWebBrowserChrome(nsIWebBrowserChrome* aWebBrowserChrome)
{
   if(!aWebBrowserChrome)
      {
      mWebBrowserChrome = nsnull;
      mOwnerWin = nsnull;
      mOwnerProgressListener = nsnull;
      mOwnerRequestor = nsnull;
      }
   else
      {
      nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(aWebBrowserChrome));
      nsCOMPtr<nsIInterfaceRequestor> requestor(do_QueryInterface(aWebBrowserChrome));
      nsCOMPtr<nsIWebProgressListener> progressListener(do_QueryInterface(aWebBrowserChrome));

      NS_ENSURE_TRUE(baseWin, NS_ERROR_INVALID_ARG);

      mWebBrowserChrome = aWebBrowserChrome;
      mOwnerWin = baseWin;
      mOwnerProgressListener = progressListener;
      mOwnerRequestor = requestor;
      }
   return NS_OK;
}
