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

// Interfaces needed to be included

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
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsDocShellTreeOwner::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP nsDocShellTreeOwner::GetInterface(const nsIID& aIID, void** aSink)
{
   NS_ENSURE_ARG_POINTER(aSink);
   NS_ENSURE_STATE(mOwnerRequestor);

   return mOwnerRequestor->GetInterface(aIID, aSink);   
}

//*****************************************************************************
// nsDocShellTreeOwner::nsIDocShellTreeOwner
//*****************************************************************************   

NS_IMETHODIMP nsDocShellTreeOwner::FindItemWithName(const PRUnichar* aName,
   nsIDocShellTreeItem* aRequestor, nsIDocShellTreeItem** aFoundItem)
{
   NS_ENSURE_ARG_POINTER(aFoundItem);

   return NS_ERROR_FAILURE;

//XXXTAB
/*   *aFoundItem = nsnull;

   nsAutoString   name(aName);

   PRBool fIs_Content = PR_FALSE;

   /* Special Cases */ /*
   if(name.Length() == 0)
      return NS_OK;
   if(name.EqualsIgnoreCase("_blank"))
      return NS_OK;
   if(name.EqualsIgnoreCase("_content"))
      {
      fIs_Content = PR_TRUE;
      mXULWindow->GetPrimaryContentShell(aFoundItem);
      if(*aFoundItem)
         return NS_OK;
      }

   nsCOMPtr<nsIWindowMediator> windowMediator(do_GetService(kWindowMediatorCID));
   NS_ENSURE_TRUE(windowMediator, NS_ERROR_FAILURE);

   nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
   NS_ENSURE_SUCCESS(windowMediator->GetXULWindowEnumerator(nsnull, 
      getter_AddRefs(windowEnumerator)), NS_ERROR_FAILURE);
   
   PRBool more;
   
   windowEnumerator->HasMoreElements(&more);
   while(more)
      {
      nsCOMPtr<nsISupports> nextWindow = nsnull;
      windowEnumerator->GetNext(getter_AddRefs(nextWindow));
      nsCOMPtr<nsIXULWindow> xulWindow(do_QueryInterface(nextWindow));
      NS_ENSURE_TRUE(xulWindow, NS_ERROR_FAILURE);

      nsCOMPtr<nsIDocShellTreeItem> shellAsTreeItem;
      xulWindow->GetPrimaryContentShell(getter_AddRefs(shellAsTreeItem));

      if(shellAsTreeItem)
         {
         if(fIs_Content)
            *aFoundItem = shellAsTreeItem;
         else if(aRequestor != shellAsTreeItem.get())
            {
            // Do this so we can pass in the tree owner as the requestor so the child knows not
            // to call back up.
            nsCOMPtr<nsIDocShellTreeOwner> shellOwner;
            shellAsTreeItem->GetTreeOwner(getter_AddRefs(shellOwner));
            nsCOMPtr<nsISupports> shellOwnerSupports(do_QueryInterface(shellOwner));

            shellAsTreeItem->FindItemWithName(aName, shellOwnerSupports, aFoundItem);
            }
         if(*aFoundItem)
            return NS_OK;   
         }
      windowEnumerator->HasMoreElements(&more);
      }
   return NS_OK;*/      
}

NS_IMETHODIMP nsDocShellTreeOwner::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
   PRBool aPrimary, const PRUnichar* aID)
{
   //XXXTAB
   //mXULWindow->ContentShellAdded(aContentShell, aPrimary, aID);
   return NS_OK;
}

NS_IMETHODIMP nsDocShellTreeOwner::GetPrimaryContentShell(nsIDocShellTreeItem** aShell)
{
      //XXXTAB
   //return mXULWindow->GetPrimaryContentShell(aShell);
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::SizeShellTo(nsIDocShellTreeItem* aShellItem,
   PRInt32 aCX, PRInt32 aCY)
{
      //XXXTAB
   //return mXULWindow->SizeShellTo(aShellItem, aCX, aCY);
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::ShowModal()
{
      //XXXTAB
   //return mXULWindow->ShowModal();   
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::GetNewWindow(PRInt32 aChromeFlags,
   nsIDocShellTreeItem** aDocShellTreeItem)
{
      //XXXTAB
   //return mXULWindow->GetNewWindow(aChromeFlags, aDocShellTreeItem);
   return NS_ERROR_FAILURE;
}

//*****************************************************************************
// nsDocShellTreeOwner::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP nsDocShellTreeOwner::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)   
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::Create()
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::Destroy()
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::SetPosition(PRInt32 aX, PRInt32 aY)
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::GetPosition(PRInt32* aX, PRInt32* aY)
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::SetSize(PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::GetSize(PRInt32* aCX, PRInt32* aCY)
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::SetPositionAndSize(PRInt32 aX, PRInt32 aY,
   PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::GetPositionAndSize(PRInt32* aX, PRInt32* aY,
   PRInt32* aCX, PRInt32* aCY)
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::Repaint(PRBool aForce)
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::GetParentWidget(nsIWidget** aParentWidget)
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::SetParentWidget(nsIWidget* aParentWidget)
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::GetVisibility(PRBool* aVisibility)
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::SetVisibility(PRBool aVisibility)
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::GetMainWidget(nsIWidget** aMainWidget)
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::SetFocus()
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::FocusAvailable(nsIBaseWindow* aCurrentFocus, 
   PRBool* aTookFocus)
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::GetTitle(PRUnichar** aTitle)
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShellTreeOwner::SetTitle(const PRUnichar* aTitle)
{
   //XXX
   NS_ERROR("NOT YET IMPLEMENTED");
   return NS_ERROR_FAILURE;
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
      mOwnerRequestor = nsnull;
      }
   else
      {
      nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(aWebBrowserChrome));
      nsCOMPtr<nsIInterfaceRequestor> requestor(do_QueryInterface(aWebBrowserChrome));

      NS_ENSURE_TRUE(baseWin && requestor, NS_ERROR_INVALID_ARG);

      mWebBrowserChrome = aWebBrowserChrome;
      mOwnerWin = baseWin;
      mOwnerRequestor = requestor;
      }
   return NS_OK;
}
