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
#include "nsWebBrowser.h"

// Helper Classes
#include "nsGfxCIID.h"
#include "nsWidgetsCID.h"

//Interfaces Needed
#include "nsIComponentManager.h"
#include "nsIDeviceContext.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIInterfaceRequestor.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebShell.h"

static NS_DEFINE_CID(kWebShellCID,         NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kChildCID,               NS_CHILD_CID);
static NS_DEFINE_IID(kDeviceContextCID,       NS_DEVICE_CONTEXT_CID);

//*****************************************************************************
//***    nsWebBrowser: Object Management
//*****************************************************************************

nsWebBrowser::nsWebBrowser() : mDocShellTreeOwner(nsnull), 
   mContentListener(nsnull), mInitInfo(nsnull), mParentNativeWindow(nsnull),
   mParentWidget(nsnull), mParent(nsnull), mContentType(typeContentWrapper)
{
    NS_INIT_REFCNT();
   mInitInfo = new nsWebBrowserInitInfo();
}

nsWebBrowser::~nsWebBrowser()
{
   InternalDestroy();
}

NS_IMETHODIMP nsWebBrowser::InternalDestroy()
{
   SetDocShell(nsnull);

   if(mDocShellTreeOwner)
      {
      mDocShellTreeOwner->WebBrowser(nsnull);
      NS_RELEASE(mDocShellTreeOwner);
      }
   if(mContentListener)
      {
      mContentListener->WebBrowser(nsnull);
      NS_RELEASE(mContentListener);
      }
   if(mInitInfo)
      {
      delete mInitInfo;
      mInitInfo = nsnull;
      }

   return NS_OK;
}


//*****************************************************************************
// nsWebBrowser::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(nsWebBrowser)
NS_IMPL_RELEASE(nsWebBrowser)

NS_INTERFACE_MAP_BEGIN(nsWebBrowser)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowser)
    NS_INTERFACE_MAP_ENTRY(nsIWebBrowser)
    NS_INTERFACE_MAP_ENTRY(nsIWebNavigation)
    NS_INTERFACE_MAP_ENTRY(nsIWebProgress)
    NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
    NS_INTERFACE_MAP_ENTRY(nsIScrollable)
    NS_INTERFACE_MAP_ENTRY(nsITextScroll)
    NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeItem)
    NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
    NS_INTERFACE_MAP_ENTRY(nsIWebBrowserSetup)
NS_INTERFACE_MAP_END

///*****************************************************************************
// nsWebBrowser::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowser::GetInterface(const nsIID& aIID, void** aSink)
{
   NS_ENSURE_ARG_POINTER(aSink);

   if(NS_SUCCEEDED(QueryInterface(aIID, aSink)))
      return NS_OK;

   if(mDocShell)
      return mDocShellAsReq->GetInterface(aIID, aSink);

   return NS_NOINTERFACE;
}

//*****************************************************************************
// nsWebBrowser::nsIWebBrowser
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowser::AddWebBrowserListener(nsIInterfaceRequestor* aListener)
{                   
   if(!mListenerList)
      NS_ENSURE_SUCCESS(NS_NewISupportsArray(getter_AddRefs(mListenerList)), 
         NS_ERROR_FAILURE);

   // Make sure it isn't already in the list...  This is bad!
   NS_ENSURE_TRUE(mListenerList->IndexOf(aListener) == -1, NS_ERROR_INVALID_ARG);

   NS_ENSURE_SUCCESS(mListenerList->AppendElement(aListener), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::RemoveWebBrowserListener(nsIInterfaceRequestor* aListener)
{
   NS_ENSURE_STATE(mListenerList);
   NS_ENSURE_ARG(aListener);

   NS_ENSURE_TRUE(mListenerList->RemoveElement(aListener), NS_ERROR_INVALID_ARG);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetContainerWindow(nsIWebBrowserChrome** aTopWindow)
{
   NS_ENSURE_ARG_POINTER(aTopWindow);

   if(mDocShellTreeOwner)
      *aTopWindow = mDocShellTreeOwner->mWebBrowserChrome;
   else
      *aTopWindow = nsnull;
   NS_IF_ADDREF(*aTopWindow);

   return NS_OK;   
}

NS_IMETHODIMP nsWebBrowser::SetContainerWindow(nsIWebBrowserChrome* aTopWindow)
{
   NS_ENSURE_SUCCESS(EnsureDocShellTreeOwner(), NS_ERROR_FAILURE);
   return mDocShellTreeOwner->SetWebBrowserChrome(aTopWindow);
}

NS_IMETHODIMP nsWebBrowser::GetParentURIContentListener(nsIURIContentListener**
   aParentContentListener)
{
   NS_ENSURE_ARG_POINTER(aParentContentListener);
   NS_ENSURE_SUCCESS(EnsureContentListener(), NS_ERROR_FAILURE);

   return mContentListener->GetParentContentListener(aParentContentListener);
}

NS_IMETHODIMP nsWebBrowser::SetParentURIContentListener(nsIURIContentListener*
   aParentContentListener)
{
   NS_ENSURE_SUCCESS(EnsureContentListener(), NS_ERROR_FAILURE);

   return mContentListener->SetParentContentListener(aParentContentListener);
}

//*****************************************************************************
// nsWebBrowser::nsIDocShellTreeItem
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowser::GetName(PRUnichar** aName)
{
   NS_ENSURE_ARG_POINTER(aName);

   if(mDocShell)  
      mDocShellAsItem->GetName(aName);
   else
      *aName = mInitInfo->name.ToNewUnicode();

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetName(const PRUnichar* aName)
{
   if(mDocShell)
      {
      nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
      NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

      return docShellAsItem->SetName(aName);
      }
   else
      mInitInfo->name = aName;

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetItemType(PRInt32* aItemType)
{
   NS_ENSURE_ARG_POINTER(aItemType);

   *aItemType = mContentType;
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetItemType(PRInt32 aItemType)
{
    NS_ENSURE_TRUE((aItemType == typeContentWrapper || aItemType == typeChromeWrapper), NS_ERROR_FAILURE);
    mContentType = aItemType;
    return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetParent(nsIDocShellTreeItem** aParent)
{
   NS_ENSURE_ARG_POINTER(aParent);

   *aParent = mParent;
   NS_IF_ADDREF(*aParent);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetParent(nsIDocShellTreeItem* aParent)
{
  // null aParent is ok
   
  /* Note this doesn't do an addref on purpose.  This is because the parent
   is an implied lifetime.  We don't want to create a cycle by refcounting
   the parent.*/
  mParent = aParent;
  return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetSameTypeParent(nsIDocShellTreeItem** aParent)
{
   NS_ENSURE_ARG_POINTER(aParent);
   *aParent = nsnull;

   if(!mParent)
      return NS_OK;
      
   PRInt32  parentType;
   NS_ENSURE_SUCCESS(mParent->GetItemType(&parentType), NS_ERROR_FAILURE);

   if(typeContentWrapper == parentType)
      {
      *aParent = mParent;
      NS_ADDREF(*aParent);
      }                   
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetRootTreeItem(nsIDocShellTreeItem** aRootTreeItem)
{
   NS_ENSURE_ARG_POINTER(aRootTreeItem);
   *aRootTreeItem = NS_STATIC_CAST(nsIDocShellTreeItem*, this);

   nsCOMPtr<nsIDocShellTreeItem> parent;
   NS_ENSURE_SUCCESS(GetParent(getter_AddRefs(parent)), NS_ERROR_FAILURE);
   while(parent)
      {
      *aRootTreeItem = parent;
      NS_ENSURE_SUCCESS((*aRootTreeItem)->GetParent(getter_AddRefs(parent)), NS_ERROR_FAILURE);
      }
   NS_ADDREF(*aRootTreeItem);
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetSameTypeRootTreeItem(nsIDocShellTreeItem** aRootTreeItem)
{
   NS_ENSURE_ARG_POINTER(aRootTreeItem);
   *aRootTreeItem = NS_STATIC_CAST(nsIDocShellTreeItem*, this);

   nsCOMPtr<nsIDocShellTreeItem> parent;
   NS_ENSURE_SUCCESS(GetSameTypeParent(getter_AddRefs(parent)), NS_ERROR_FAILURE);
   while(parent)
      {
      *aRootTreeItem = parent;
      NS_ENSURE_SUCCESS((*aRootTreeItem)->GetSameTypeParent(getter_AddRefs(parent)), 
         NS_ERROR_FAILURE);
      }
   NS_ADDREF(*aRootTreeItem);
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::FindItemWithName(const PRUnichar *aName, 
   nsISupports* aRequestor, nsIDocShellTreeItem **_retval)
{
   NS_ENSURE_STATE(mDocShell);
   NS_ASSERTION(mDocShellTreeOwner, "This should always be set when in this situation");

   return mDocShellAsItem->FindItemWithName(aName, 
      NS_STATIC_CAST(nsIDocShellTreeOwner*, mDocShellTreeOwner), _retval);
}

NS_IMETHODIMP nsWebBrowser::GetTreeOwner(nsIDocShellTreeOwner** aTreeOwner)
{  
    NS_ENSURE_ARG_POINTER(aTreeOwner);
    *aTreeOwner = nsnull;
    if(mDocShellTreeOwner)
    {
        if (mDocShellTreeOwner->mTreeOwner)
        {
            *aTreeOwner = mDocShellTreeOwner->mTreeOwner;
        }
        else
        {
            *aTreeOwner = mDocShellTreeOwner;
        }
    }
    NS_IF_ADDREF(*aTreeOwner);
    return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetTreeOwner(nsIDocShellTreeOwner* aTreeOwner)
{
   NS_ENSURE_SUCCESS(EnsureDocShellTreeOwner(), NS_ERROR_FAILURE);
   return mDocShellTreeOwner->SetTreeOwner(aTreeOwner);
}

NS_IMETHODIMP nsWebBrowser::SetChildOffset(PRInt32 aChildOffset)
{
  // Not implemented
  return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetChildOffset(PRInt32 *aChildOffset)
{
  // Not implemented
  return NS_OK;
}

//*****************************************************************************
// nsWebBrowser::nsIWebNavigation
//*****************************************************************************

NS_IMETHODIMP nsWebBrowser::GetCanGoBack(PRBool* aCanGoBack)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->GetCanGoBack(aCanGoBack);
}

NS_IMETHODIMP nsWebBrowser::GetCanGoForward(PRBool* aCanGoForward)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->GetCanGoForward(aCanGoForward);
}

NS_IMETHODIMP nsWebBrowser::GoBack()
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->GoBack();
}

NS_IMETHODIMP nsWebBrowser::GoForward()
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->GoForward();
}

NS_IMETHODIMP nsWebBrowser::LoadURI(const PRUnichar* aURI)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->LoadURI(aURI);
}

NS_IMETHODIMP nsWebBrowser::Reload(PRInt32 aReloadType)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->Reload(aReloadType);
}

NS_IMETHODIMP nsWebBrowser::GotoIndex(PRInt32 aIndex)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->GotoIndex(aIndex);
}

NS_IMETHODIMP nsWebBrowser::Stop()
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->Stop();
}

NS_IMETHODIMP nsWebBrowser::GetCurrentURI(nsIURI** aURI)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->GetCurrentURI(aURI);
}

NS_IMETHODIMP nsWebBrowser::SetSessionHistory(nsISHistory* aSessionHistory)
{
   if(mDocShell)
      return mDocShellAsNav->SetSessionHistory(aSessionHistory);
   else
      mInitInfo->sessionHistory = aSessionHistory;

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetSessionHistory(nsISHistory** aSessionHistory)
{
   NS_ENSURE_ARG_POINTER(aSessionHistory);
   if(mDocShell)
      return mDocShellAsNav->GetSessionHistory(aSessionHistory);
   else
      *aSessionHistory = mInitInfo->sessionHistory;

   NS_IF_ADDREF(*aSessionHistory);

   return NS_OK;
}


NS_IMETHODIMP nsWebBrowser::GetDocument(nsIDOMDocument** aDocument)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->GetDocument(aDocument);
}


//*****************************************************************************
// nsWebBrowser::nsIWebBrowserSetup
//*****************************************************************************

/* void setProperty (in unsigned long aId, in unsigned long aValue); */
NS_IMETHODIMP nsWebBrowser::SetProperty(PRUint32 aId, PRUint32 aValue)
{
    switch (aId)
    {
    case nsIWebBrowserSetup::SETUP_ALLOW_PLUGINS:
        {
           NS_ENSURE_STATE(mDocShell);
           NS_ENSURE_TRUE((aValue == PR_TRUE || aValue == PR_FALSE), NS_ERROR_INVALID_ARG);
           mDocShell->SetAllowPlugins(aValue);
        }
    default:
        return NS_ERROR_INVALID_ARG;
  
    }
    return NS_OK;
}


//*****************************************************************************
// nsWebBrowser::nsIWebProgress
//*****************************************************************************

NS_IMETHODIMP nsWebBrowser::AddProgressListener(nsIWebProgressListener* aListener)
{
   NS_ENSURE_STATE(mDocShell);
   
   return mDocShellAsProgress->AddProgressListener(aListener);
}

NS_IMETHODIMP nsWebBrowser::RemoveProgressListener(nsIWebProgressListener* aListener)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsProgress->RemoveProgressListener(aListener);
}

//*****************************************************************************
// nsWebBrowser::nsIBaseWindow
//*****************************************************************************

NS_IMETHODIMP nsWebBrowser::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* aParentWidget, PRInt32 aX, PRInt32 aY, PRInt32 aCX, PRInt32 aCY)   
{
   NS_ENSURE_ARG(aParentNativeWindow || aParentWidget);
   NS_ENSURE_STATE(!mDocShell || mInitInfo);

   if(aParentWidget)
      NS_ENSURE_SUCCESS(SetParentWidget(aParentWidget), NS_ERROR_FAILURE);
   else
      NS_ENSURE_SUCCESS(SetParentNativeWindow(aParentNativeWindow),
         NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(SetPositionAndSize(aX, aY, aCX, aCY, PR_FALSE),
      NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::Create()
{
   NS_ENSURE_STATE(!mDocShell && (mParentNativeWindow || mParentWidget));

   NS_ENSURE_SUCCESS(EnsureDocShellTreeOwner(), NS_ERROR_FAILURE);
   NS_ENSURE_SUCCESS(EnsureContentListener(), NS_ERROR_FAILURE);

   nsCOMPtr<nsIWidget> docShellParentWidget(mParentWidget);
   if(!mParentWidget) // We need to create a widget
      {
      nsCOMPtr<nsIDeviceContext> deviceContext = 
                                          do_CreateInstance(kDeviceContextCID);
      NS_ENSURE_TRUE(deviceContext, NS_ERROR_FAILURE);

      deviceContext->Init(mParentNativeWindow);
      float dev2twip;
      deviceContext->GetDevUnitsToTwips(dev2twip);
      deviceContext->SetDevUnitsToAppUnits(dev2twip);
      float twip2dev;
      deviceContext->GetTwipsToDevUnits(twip2dev);
      deviceContext->SetAppUnitsToDevUnits(twip2dev);
      deviceContext->SetGamma(1.0f);

      // Create the widget
      NS_ENSURE_TRUE(mInternalWidget = do_CreateInstance(kChildCID), NS_ERROR_FAILURE);

      docShellParentWidget = mInternalWidget;
      nsWidgetInitData  widgetInit;

      widgetInit.clipChildren = PR_FALSE;
      widgetInit.mWindowType = eWindowType_child;
      nsRect bounds(mInitInfo->x, mInitInfo->y, mInitInfo->cx, mInitInfo->cy);
      
      mInternalWidget->Create(mParentNativeWindow, bounds, nsnull /* was nsWebShell::HandleEvent*/,
         deviceContext, nsnull, nsnull, &widgetInit);  
      }

   nsCOMPtr<nsIDocShell> docShell(do_CreateInstance(kWebShellCID));
   NS_ENSURE_SUCCESS(SetDocShell(docShell), NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(mDocShellAsWin->InitWindow(nsnull,
      docShellParentWidget, mInitInfo->x, mInitInfo->y, mInitInfo->cx,
      mInitInfo->cy), NS_ERROR_FAILURE);

   mDocShellAsItem->SetName(mInitInfo->name.GetUnicode());
   if (mContentType == typeChromeWrapper)
   {
       mDocShellAsItem->SetItemType(nsIDocShellTreeItem::typeChrome);
   }
   else
   {
       mDocShellAsItem->SetItemType(nsIDocShellTreeItem::typeContent);
   }
   mDocShellAsItem->SetTreeOwner(mDocShellTreeOwner);
   mDocShell->SetParentURIContentListener(mContentListener);

   if(!mInitInfo->sessionHistory)
      mInitInfo->sessionHistory = do_CreateInstance(NS_SHISTORY_PROGID);
   NS_ENSURE_TRUE(mInitInfo->sessionHistory, NS_ERROR_FAILURE);
   mDocShellAsNav->SetSessionHistory(mInitInfo->sessionHistory);
      
   NS_ENSURE_SUCCESS(mDocShellAsWin->Create(), NS_ERROR_FAILURE);

   delete mInitInfo;
   mInitInfo = nsnull;

   return NS_OK; 
}

NS_IMETHODIMP nsWebBrowser::Destroy()
{
   InternalDestroy();

   if(!mInitInfo)
      mInitInfo = new nsWebBrowserInitInfo();

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetPosition(PRInt32 aX, PRInt32 aY)
{
   PRInt32 cx = 0;
   PRInt32 cy = 0;

   GetSize(&cx, &cy);

   return SetPositionAndSize(aX, aY, cx, cy, PR_FALSE);
}

NS_IMETHODIMP nsWebBrowser::GetPosition(PRInt32* aX, PRInt32* aY)
{
   return GetPositionAndSize(aX, aY, nsnull, nsnull);
}

NS_IMETHODIMP nsWebBrowser::SetSize(PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
   PRInt32 x = 0;
   PRInt32 y = 0;

   GetPosition(&x, &y);

   return SetPositionAndSize(x, y, aCX, aCY, aRepaint);
}

NS_IMETHODIMP nsWebBrowser::GetSize(PRInt32* aCX, PRInt32* aCY)
{
   return GetPositionAndSize(nsnull, nsnull, aCX, aCY);
}

NS_IMETHODIMP nsWebBrowser::SetPositionAndSize(PRInt32 aX, PRInt32 aY,
   PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
   if(!mDocShell)
      {
      mInitInfo->x = aX;
      mInitInfo->y = aY;
      mInitInfo->cx = aCX;
      mInitInfo->cy = aCY;
      }
   else
      {
      PRInt32 doc_x = aX;
      PRInt32 doc_y = aY;

      // If there is an internal widget we need to make the docShell coordinates
      // relative to the internal widget rather than the calling app's parent.
      // We also need to resize our widget then.
      if(mInternalWidget)
         {
         doc_x = doc_y = 0;
         NS_ENSURE_SUCCESS(mInternalWidget->Resize(aX, aY, aCX, aCY, aRepaint),
            NS_ERROR_FAILURE);
         }
      // Now reposition/ resize the doc
      NS_ENSURE_SUCCESS(mDocShellAsWin->SetPositionAndSize(doc_x, doc_y, aCX, aCY, 
         aRepaint), NS_ERROR_FAILURE);
      }

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetPositionAndSize(PRInt32* aX, PRInt32* aY, 
   PRInt32* aCX, PRInt32* aCY)
{
   if(!mDocShell)
      {
      if(aX)
         *aX = mInitInfo->x;
      if(aY)
         *aY = mInitInfo->y;
      if(aCX)
         *aCX = mInitInfo->cx;
      if(aCY)
         *aCY = mInitInfo->cy;
      }
   else
      {
      if(mInternalWidget)
         {
         nsRect bounds;
         NS_ENSURE_SUCCESS(mInternalWidget->GetBounds(bounds), NS_ERROR_FAILURE);

         if(aX)
            *aX = bounds.x;
         if(aY)
            *aY = bounds.y;
         if(aCX)
            *aCX = bounds.width;
         if(aCY)
            *aCY = bounds.height;
         return NS_OK;
         }
      else
         return mDocShellAsWin->GetPositionAndSize(aX, aY, aCX, aCY); // Can directly return this as it is the
      }
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::Repaint(PRBool aForce)
{
   NS_ENSURE_STATE(mDocShell);
   return mDocShellAsWin->Repaint(aForce); // Can directly return this as it is the
}                                     // same interface, thus same returns.

NS_IMETHODIMP nsWebBrowser::GetParentWidget(nsIWidget** aParentWidget)
{
   NS_ENSURE_ARG_POINTER(aParentWidget);

   *aParentWidget = mParentWidget;

   NS_IF_ADDREF(*aParentWidget);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetParentWidget(nsIWidget* aParentWidget)
{
   NS_ENSURE_STATE(!mDocShell);

   mParentWidget = aParentWidget;
   if(mParentWidget)
      mParentNativeWindow = mParentWidget->GetNativeData(NS_NATIVE_WIDGET);
   else
      mParentNativeWindow = nsnull;

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(aParentNativeWindow);
   
   *aParentNativeWindow = mParentNativeWindow;

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
   NS_ENSURE_STATE(!mDocShell);

   mParentNativeWindow = aParentNativeWindow;

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetVisibility(PRBool* visibility)
{
   NS_ENSURE_ARG_POINTER(visibility);

   if(!mDocShell)
      *visibility = mInitInfo->visible;
   else
      NS_ENSURE_SUCCESS(mDocShellAsWin->GetVisibility(visibility), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetVisibility(PRBool aVisibility)
{
   if(!mDocShell)
      mInitInfo->visible = aVisibility;
   else
      {
      NS_ENSURE_SUCCESS(mDocShellAsWin->SetVisibility(aVisibility), NS_ERROR_FAILURE);
      if(mInternalWidget)
         mInternalWidget->Show(aVisibility);
      }

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetMainWidget(nsIWidget** mainWidget)
{
   NS_ENSURE_ARG_POINTER(mainWidget);

   if(mInternalWidget)
      *mainWidget = mInternalWidget;
   else
      *mainWidget = mParentWidget;

   NS_IF_ADDREF(*mainWidget);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetFocus()
{
   NS_ENSURE_STATE(mDocShell);

   NS_ENSURE_SUCCESS(mDocShellAsWin->SetFocus(), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::FocusAvailable(nsIBaseWindow* aCurrentFocus,
   PRBool* aTookFocus)
{
   NS_ENSURE_ARG_POINTER(aTookFocus);

   // Next person we should call is first the parent otherwise the 
   // docshell tree owner.
   nsCOMPtr<nsIBaseWindow> nextCallWin(do_QueryInterface(mParent));
   if(!nextCallWin)
      nextCallWin = do_QueryInterface(nsnull /*mTreeOwner*/);

   //If the current focus is us, offer it to the next owner.
   if(aCurrentFocus == NS_STATIC_CAST(nsIBaseWindow*, this))
      {
      if(nextCallWin)
         return nextCallWin->FocusAvailable(aCurrentFocus, aTookFocus);
      return NS_OK;
      }

   //Otherwise, check the chilren and offer it to the next sibling.
   if((mDocShellAsWin.get() != aCurrentFocus) &&
      NS_SUCCEEDED(mDocShellAsWin->SetFocus()))
      {
      *aTookFocus = PR_TRUE;
      return NS_OK;
      }

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetTitle(PRUnichar** aTitle)
{
   NS_ENSURE_ARG_POINTER(aTitle);
   NS_ENSURE_STATE(mDocShell);

   NS_ENSURE_SUCCESS(mDocShellAsWin->GetTitle(aTitle), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetTitle(const PRUnichar* aTitle)
{
   NS_ENSURE_STATE(mDocShell);

   NS_ENSURE_SUCCESS(mDocShellAsWin->SetTitle(aTitle), NS_ERROR_FAILURE);

   return NS_OK;
}

//*****************************************************************************
// nsWebBrowser::nsIScrollable
//*****************************************************************************

NS_IMETHODIMP nsWebBrowser::GetCurScrollPos(PRInt32 aScrollOrientation, 
   PRInt32* aCurPos)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->GetCurScrollPos(aScrollOrientation, aCurPos);
}

NS_IMETHODIMP nsWebBrowser::SetCurScrollPos(PRInt32 aScrollOrientation, 
   PRInt32 aCurPos)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->SetCurScrollPos(aScrollOrientation, aCurPos);
}

NS_IMETHODIMP nsWebBrowser::SetCurScrollPosEx(PRInt32 aCurHorizontalPos, 
   PRInt32 aCurVerticalPos)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->SetCurScrollPosEx(aCurHorizontalPos, 
      aCurVerticalPos);
}

NS_IMETHODIMP nsWebBrowser::GetScrollRange(PRInt32 aScrollOrientation,
   PRInt32* aMinPos, PRInt32* aMaxPos)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->GetScrollRange(aScrollOrientation, aMinPos,
      aMaxPos);
}

NS_IMETHODIMP nsWebBrowser::SetScrollRange(PRInt32 aScrollOrientation,
   PRInt32 aMinPos, PRInt32 aMaxPos)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->SetScrollRange(aScrollOrientation, aMinPos,
      aMaxPos);
}

NS_IMETHODIMP nsWebBrowser::SetScrollRangeEx(PRInt32 aMinHorizontalPos,
   PRInt32 aMaxHorizontalPos, PRInt32 aMinVerticalPos, PRInt32 aMaxVerticalPos)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->SetScrollRangeEx(aMinHorizontalPos,
      aMaxHorizontalPos, aMinVerticalPos, aMaxVerticalPos);
}

NS_IMETHODIMP nsWebBrowser::GetCurrentScrollbarPreferences(PRInt32 aScrollOrientation,
   PRInt32* aScrollbarPref)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->GetCurrentScrollbarPreferences(aScrollOrientation,
      aScrollbarPref);
}

NS_IMETHODIMP nsWebBrowser::GetDefaultScrollbarPreferences(PRInt32 aScrollOrientation,
   PRInt32* aScrollbarPref)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->GetDefaultScrollbarPreferences(aScrollOrientation,
      aScrollbarPref);
}

NS_IMETHODIMP nsWebBrowser::SetCurrentScrollbarPreferences(PRInt32 aScrollOrientation,
   PRInt32 aScrollbarPref)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->SetCurrentScrollbarPreferences(aScrollOrientation,
      aScrollbarPref);

}

NS_IMETHODIMP nsWebBrowser::SetDefaultScrollbarPreferences(PRInt32 aScrollOrientation,
   PRInt32 aScrollbarPref)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->SetDefaultScrollbarPreferences(aScrollOrientation,
      aScrollbarPref);
}

NS_IMETHODIMP nsWebBrowser::ResetScrollbarPreferences()
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->ResetScrollbarPreferences();
}

NS_IMETHODIMP nsWebBrowser::GetScrollbarVisibility(PRBool* aVerticalVisible,
   PRBool* aHorizontalVisible)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->GetScrollbarVisibility(aVerticalVisible,
      aHorizontalVisible);
}

//*****************************************************************************
// nsWebBrowser::nsITextScroll
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowser::ScrollByLines(PRInt32 aNumLines)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsTextScroll->ScrollByLines(aNumLines);
}

NS_IMETHODIMP nsWebBrowser::ScrollByPages(PRInt32 aNumPages)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsTextScroll->ScrollByPages(aNumPages);
}


//*****************************************************************************
// nsWebBrowser: Listener Helpers
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowser::SetDocShell(nsIDocShell* aDocShell)
{
     if(aDocShell)
     {
         NS_ENSURE_TRUE(!mDocShell, NS_ERROR_FAILURE);
 
         nsCOMPtr<nsIInterfaceRequestor> req(do_QueryInterface(aDocShell));
         nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(aDocShell));
         nsCOMPtr<nsIDocShellTreeItem> item(do_QueryInterface(aDocShell));
         nsCOMPtr<nsIWebNavigation> nav(do_QueryInterface(aDocShell));
         nsCOMPtr<nsIWebProgress> progress(do_GetInterface(aDocShell));
         nsCOMPtr<nsIScrollable> scrollable(do_QueryInterface(aDocShell));
         nsCOMPtr<nsITextScroll> textScroll(do_QueryInterface(aDocShell));
         NS_ENSURE_TRUE(req && baseWin && item && nav && scrollable && textScroll &&
           progress, NS_ERROR_FAILURE);
 
         mDocShell = aDocShell;
         mDocShellAsReq = req;
         mDocShellAsWin = baseWin;
         mDocShellAsItem = item;
         mDocShellAsNav = nav;
         mDocShellAsProgress = progress;
         mDocShellAsScrollable = scrollable;
         mDocShellAsTextScroll = textScroll;
 
         AddProgressListener(NS_STATIC_CAST(nsIWebProgressListener *, mDocShellTreeOwner));
     }
     else
     {
         if (mDocShell)
         {
             RemoveProgressListener(NS_STATIC_CAST(nsIWebProgressListener *, mDocShellTreeOwner));
             mDocShellAsWin->Destroy();
         }
         mDocShell = nsnull;
         mDocShellAsReq = nsnull;
         mDocShellAsWin = nsnull;
         mDocShellAsItem = nsnull;
         mDocShellAsNav = nsnull;
         mDocShellAsProgress = nsnull;
         mDocShellAsScrollable = nsnull;
         mDocShellAsTextScroll = nsnull;
     }

     return NS_OK; 
}

NS_IMETHODIMP nsWebBrowser::EnsureDocShellTreeOwner()
{
   if(mDocShellTreeOwner)
      return NS_OK;

   mDocShellTreeOwner = new nsDocShellTreeOwner();
   NS_ENSURE_TRUE(mDocShellTreeOwner, NS_ERROR_OUT_OF_MEMORY);

   NS_ADDREF(mDocShellTreeOwner);
   mDocShellTreeOwner->WebBrowser(this);
   
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::EnsureContentListener()
{
   if(mContentListener)
      return NS_OK;

   mContentListener = new nsWBURIContentListener();
   NS_ENSURE_TRUE(mContentListener, NS_ERROR_OUT_OF_MEMORY);

   NS_ADDREF(mContentListener);
   mContentListener->WebBrowser(this);
   
   return NS_OK;
}



