/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "nsWebBrowser.h"

// Helper Classes
#include "nsGfxCIID.h"
#include "nsWidgetsCID.h"

//Interfaces Needed
#include "nsReadableUtils.h"
#include "nsIComponentManager.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMWindow.h"
#include "nsIDOMElement.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebShell.h"
#include "nsPIDOMWindow.h"
#include "nsIFocusController.h"
#include "nsIDOMWindowInternal.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsIWebBrowserFocus.h"
#include "nsIWebBrowserStream.h"
#include "nsIPresShell.h"
#include "nsIGlobalHistory.h"
#include "nsIDocShellHistory.h"
#include "nsIURIContentListener.h"
#include "nsGUIEvent.h"
#include "nsISHistoryListener.h"
#include "nsIURI.h"
#include "nsIWebBrowserPersist.h"
#include "nsCWebBrowserPersist.h"
#include "nsIWebBrowserPrint.h"
#include "nsIServiceManager.h"

// for painting the background window
#include "nsIRenderingContext.h"
#include "nsIRegion.h"
#include "nsILookAndFeel.h"

// Printing Includes
#include "nsIContentViewer.h"

// PSM2 includes
#include "nsISecureBrowserUI.h"

#if (defined(XP_MAC) || defined(XP_MACOSX)) && !defined(MOZ_WIDGET_COCOA)
#include <MacWindows.h>
#include "nsWidgetSupport.h"
#include "nsIEventSink.h"
#endif

static NS_DEFINE_CID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kWindowCID, NS_WINDOW_CID);
static NS_DEFINE_CID(kChildCID, NS_CHILD_CID);
static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);


//*****************************************************************************
//***    nsWebBrowser: Object Management
//*****************************************************************************

nsWebBrowser::nsWebBrowser() : mDocShellTreeOwner(nsnull), 
   mInitInfo(nsnull),
   mContentType(typeContentWrapper),
   mActivating(PR_FALSE),
   mParentNativeWindow(nsnull),
   mProgressListener(nsnull),
   mBackgroundColor(0),
   mPersistCurrentState(nsIWebBrowserPersist::PERSIST_STATE_READY),
   mPersistResult(NS_OK),
   mPersistFlags(nsIWebBrowserPersist::PERSIST_FLAGS_NONE),
   mStream(nsnull),
   mParentWidget(nsnull),
   mParent(nsnull),
   mListenerArray(nsnull)
#if (defined(XP_MAC) || defined(XP_MACOSX)) && !defined(MOZ_WIDGET_COCOA)
   , mTopLevelWidget(nsnull)
#endif
{
    mInitInfo = new nsWebBrowserInitInfo();
    mWWatch = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
    NS_ASSERTION(mWWatch, "failed to get WindowWatcher");
}

nsWebBrowser::~nsWebBrowser()
{
   InternalDestroy();
}

PRBool PR_CALLBACK deleteListener(void *aElement, void *aData) {
    nsWebBrowserListenerState *state = (nsWebBrowserListenerState*)aElement;
    NS_DELETEXPCOM(state);
    return PR_TRUE;
}

NS_IMETHODIMP nsWebBrowser::InternalDestroy()
{

  if (mInternalWidget)
    mInternalWidget->SetClientData(0);

   SetDocShell(nsnull);

   if(mDocShellTreeOwner)
      {
      mDocShellTreeOwner->WebBrowser(nsnull);
      NS_RELEASE(mDocShellTreeOwner);
      }
   if(mInitInfo)
      {
      delete mInitInfo;
      mInitInfo = nsnull;
      }

   if (mListenerArray) {
      (void)mListenerArray->EnumerateForwards(deleteListener, nsnull);
      delete mListenerArray;
      mListenerArray = nsnull;
   }

#if (defined(XP_MAC) || defined(XP_MACOSX)) && !defined(MOZ_WIDGET_COCOA)   
   NS_IF_RELEASE(mTopLevelWidget);
#endif

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
    NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
    NS_INTERFACE_MAP_ENTRY(nsIScrollable)
    NS_INTERFACE_MAP_ENTRY(nsITextScroll)
    NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeItem)
    NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
    NS_INTERFACE_MAP_ENTRY(nsIWebBrowserSetup)
    NS_INTERFACE_MAP_ENTRY(nsIWebBrowserPersist)
    NS_INTERFACE_MAP_ENTRY(nsIWebBrowserFocus)
    NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
    NS_INTERFACE_MAP_ENTRY(nsIWebBrowserStream)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

///*****************************************************************************
// nsWebBrowser::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowser::GetInterface(const nsIID& aIID, void** aSink)
{
   NS_ENSURE_ARG_POINTER(aSink);

   if(NS_SUCCEEDED(QueryInterface(aIID, aSink)))
      return NS_OK;

#if (defined(XP_MAC) || defined(XP_MACOSX)) && !defined(MOZ_WIDGET_COCOA)
   if (aIID.Equals(NS_GET_IID(nsIEventSink)) && mTopLevelWidget)
      return mTopLevelWidget->QueryInterface(NS_GET_IID(nsIEventSink), aSink);
#endif

   if (mDocShell) {
       if (aIID.Equals(NS_GET_IID(nsIWebBrowserPrint))) {
           nsCOMPtr<nsIContentViewer> viewer;
           mDocShell->GetContentViewer(getter_AddRefs(viewer));
           if (viewer) {
               nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint(do_QueryInterface(viewer));
               nsIWebBrowserPrint* print = (nsIWebBrowserPrint*)webBrowserPrint.get();
               NS_ASSERTION(print, "This MUST support this interface!");
               NS_ADDREF(print);
               *aSink = print;
               return NS_OK;
           }
       } else {
           return mDocShellAsReq->GetInterface(aIID, aSink);
       }
   }

   return NS_NOINTERFACE;
}

//*****************************************************************************
// nsWebBrowser::nsIWebBrowser
//*****************************************************************************   

// listeners that currently support registration through AddWebBrowserListener:
//  - nsIWebProgressListener
NS_IMETHODIMP nsWebBrowser::AddWebBrowserListener(nsIWeakReference *aListener, const nsIID& aIID)
{           
    nsresult rv = NS_ERROR_INVALID_ARG;
    NS_ENSURE_ARG_POINTER(aListener);

    if (!mWebProgress) {
        // The window hasn't been created yet, so queue up the listener. They'll be
        // registered when the window gets created.
        nsWebBrowserListenerState *state = nsnull;
        NS_NEWXPCOM(state, nsWebBrowserListenerState);
        if (!state) return NS_ERROR_OUT_OF_MEMORY;

        state->mWeakPtr = aListener;
        state->mID = aIID;

        if (!mListenerArray) {
            NS_NEWXPCOM(mListenerArray, nsVoidArray);
            if (!mListenerArray) return NS_ERROR_OUT_OF_MEMORY;
        }

        if (!mListenerArray->AppendElement(state)) return NS_ERROR_OUT_OF_MEMORY;
    } else {
        nsCOMPtr<nsISupports> supports(do_QueryReferent(aListener));
        if (!supports) return NS_ERROR_INVALID_ARG;
        rv = BindListener(supports, aIID);
    }
    
    return rv;
}

NS_IMETHODIMP nsWebBrowser::BindListener(nsISupports *aListener, const nsIID& aIID) {
    NS_ASSERTION(aListener, "invalid args");
    NS_ASSERTION(mWebProgress, "this should only be called after we've retrieved a progress iface");
    nsresult rv = NS_OK;

    // register this listener for the specified interface id
    if (aIID.Equals(NS_GET_IID(nsIWebProgressListener))) {
        nsCOMPtr<nsIWebProgressListener> listener = do_QueryInterface(aListener, &rv);
        if (NS_FAILED(rv)) return rv;
        rv = mWebProgress->AddProgressListener(listener, nsIWebProgress::NOTIFY_ALL);
    }
    else if (aIID.Equals(NS_GET_IID(nsISHistoryListener))) {      
      nsCOMPtr<nsISHistory> shistory(do_GetInterface(mDocShell, &rv));
      if (NS_FAILED(rv)) return rv;
      nsCOMPtr<nsISHistoryListener> listener(do_QueryInterface(aListener, &rv));
      if (NS_FAILED(rv)) return rv;
      rv = shistory->AddSHistoryListener(listener);
    }
    return rv;
}

NS_IMETHODIMP nsWebBrowser::RemoveWebBrowserListener(nsIWeakReference *aListener, const nsIID& aIID)
{
    nsresult rv = NS_ERROR_INVALID_ARG;
    NS_ENSURE_ARG_POINTER(aListener);

    if (!mWebProgress) {
        // if there's no-one to register the listener w/, and we don't have a queue going,
        // the the called is calling Remove before an Add which doesn't make sense.
        if (!mListenerArray) return NS_ERROR_FAILURE;

        // iterate the array and remove the queued listener
        PRInt32 count = mListenerArray->Count();
        while (count > 0) {
            nsWebBrowserListenerState *state = (nsWebBrowserListenerState*)mListenerArray->ElementAt(count);
            NS_ASSERTION(state, "list construction problem");

            if (state->Equals(aListener, aIID)) {
                // this is the one, pull it out.
                mListenerArray->RemoveElementAt(count);
                break;
            }
            count--; 
        }

        // if we've emptied the array, get rid of it.
        if (0 >= mListenerArray->Count()) {
            (void)mListenerArray->EnumerateForwards(deleteListener, nsnull);
            NS_DELETEXPCOM(mListenerArray);
            mListenerArray = nsnull;
        }

    } else {
        nsCOMPtr<nsISupports> supports(do_QueryReferent(aListener));
        if (!supports) return NS_ERROR_INVALID_ARG;
        rv = UnBindListener(supports, aIID);
    }
    
    return rv;
}

NS_IMETHODIMP nsWebBrowser::UnBindListener(nsISupports *aListener, const nsIID& aIID) {
    NS_ASSERTION(aListener, "invalid args");
    NS_ASSERTION(mWebProgress, "this should only be called after we've retrieved a progress iface");
    nsresult rv = NS_OK;

    // remove the listener for the specified interface id
    if (aIID.Equals(NS_GET_IID(nsIWebProgressListener))) {
        nsCOMPtr<nsIWebProgressListener> listener = do_QueryInterface(aListener, &rv);
        if (NS_FAILED(rv)) return rv;
        rv = mWebProgress->RemoveProgressListener(listener);
    }
    else if (aIID.Equals(NS_GET_IID(nsISHistoryListener))) {
      nsCOMPtr<nsISHistory> shistory(do_GetInterface(mDocShell, &rv));
      if (NS_FAILED(rv)) return rv;
      nsCOMPtr<nsISHistoryListener> listener(do_QueryInterface(aListener, &rv));
      if (NS_FAILED(rv)) return rv;
      rv = shistory->RemoveSHistoryListener(listener);
    }
    return rv;
}

NS_IMETHODIMP nsWebBrowser::EnableGlobalHistory(PRBool aEnable)
{
    nsresult rv;
    
    NS_ENSURE_STATE(mDocShell);
    nsCOMPtr<nsIDocShellHistory> dsHistory(do_QueryInterface(mDocShell, &rv));
    if (NS_FAILED(rv)) return rv;
    
    if (aEnable) {
        rv = dsHistory->SetUseGlobalHistory(PR_TRUE);
    }
    else {
        rv = dsHistory->SetUseGlobalHistory(PR_FALSE);
    }
       
    return rv;
}

#if (defined(XP_MAC) || defined(XP_MACOSX)) && !defined(MOZ_WIDGET_COCOA)
NS_IMETHODIMP nsWebBrowser::EnsureTopLevelWidget(nativeWindow aWindow)
{
    WindowPtr macWindow = NS_STATIC_CAST(WindowPtr, aWindow);
    nsIWidget *widget = nsnull;
    nsCOMPtr<nsIWidget> newWidget;
    nsresult rv = NS_ERROR_FAILURE;

    ::GetWindowProperty(macWindow, kTopLevelWidgetPropertyCreator,
        kTopLevelWidgetRefPropertyTag, sizeof(nsIWidget*), nsnull, (void*)&widget);
    if (!widget) {
      newWidget = do_CreateInstance(kWindowCID, &rv);
      if (NS_SUCCEEDED(rv)) {
        
        // Create it with huge bounds. The actual bounds that matters is that of the
        // nsIBaseWindow. The bounds of the top level widget clips its children so
        // we just have to make sure it is big enough to always contain the children.

        nsRect r(0, 0, 32000, 32000);
        rv = newWidget->Create(macWindow, r, nsnull, nsnull, nsnull, nsnull, nsnull);
        if (NS_SUCCEEDED(rv))
          widget = newWidget;
      }
    }
    NS_ASSERTION(widget, "Failed to get or create a toplevel widget!!");
    if (widget) {
      mTopLevelWidget = widget;
      NS_ADDREF(mTopLevelWidget); // Allows multiple nsWebBrowsers to be in 1 window.
      rv = NS_OK;
    }
    return rv;
}
#endif

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
   *aParentContentListener = nsnull;

   // get the interface from the docshell
   nsCOMPtr<nsIURIContentListener> listener(do_GetInterface(mDocShell));

   if (listener)
       return listener->GetParentContentListener(aParentContentListener);
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetParentURIContentListener(nsIURIContentListener*
   aParentContentListener)
{
   // get the interface from the docshell
   nsCOMPtr<nsIURIContentListener> listener(do_GetInterface(mDocShell));

   if (listener)
       return listener->SetParentContentListener(aParentContentListener);
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::GetContentDOMWindow(nsIDOMWindow **_retval)
{
    NS_ENSURE_STATE(mDocShell);
    nsresult rv = NS_OK;
    nsCOMPtr<nsIDOMWindow> retval = do_GetInterface(mDocShell, &rv);
    if (NS_FAILED(rv)) return rv;

    *_retval = retval;
    NS_ADDREF(*_retval);
    return rv;
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
      *aName = ToNewUnicode(mInitInfo->name);

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

NS_IMETHODIMP nsWebBrowser::NameEquals(const PRUnichar *aName, PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(aName);
    NS_ENSURE_ARG_POINTER(_retval);
    if(mDocShell)
    {
        nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
        NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);
        return docShellAsItem->NameEquals(aName, _retval);
    }
    else
        *_retval = mInitInfo->name.Equals(aName);

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
    if (mDocShellAsItem)
        mDocShellAsItem->SetItemType(mContentType == typeChromeWrapper ? typeChrome : typeContent);
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

NS_IMETHODIMP nsWebBrowser::LoadURI(const PRUnichar* aURI,
                                    PRUint32 aLoadFlags,
                                    nsIURI* aReferringURI,
                                    nsIInputStream* aPostDataStream,
                                    nsIInputStream* aExtraHeaderStream)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->LoadURI(aURI,
                                  aLoadFlags,
                                  aReferringURI,
                                  aPostDataStream,
                                  aExtraHeaderStream);
}

NS_IMETHODIMP nsWebBrowser::Reload(PRUint32 aReloadFlags)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->Reload(aReloadFlags);
}

NS_IMETHODIMP nsWebBrowser::GotoIndex(PRInt32 aIndex)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->GotoIndex(aIndex);
}

NS_IMETHODIMP nsWebBrowser::Stop(PRUint32 aStopFlags)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->Stop(aStopFlags);
}

NS_IMETHODIMP nsWebBrowser::GetCurrentURI(nsIURI** aURI)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->GetCurrentURI(aURI);
}

NS_IMETHODIMP nsWebBrowser::GetReferringURI(nsIURI** aURI)
{
    NS_ENSURE_STATE(mDocShell);

    return mDocShellAsNav->GetReferringURI(aURI);
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
    nsresult rv = NS_OK;
    
    switch (aId)
    {
    case nsIWebBrowserSetup::SETUP_ALLOW_PLUGINS:
        {
           NS_ENSURE_STATE(mDocShell);
           NS_ENSURE_TRUE((aValue == PR_TRUE || aValue == PR_FALSE), NS_ERROR_INVALID_ARG);
           mDocShell->SetAllowPlugins(aValue);
        }
        break;
    case nsIWebBrowserSetup::SETUP_ALLOW_JAVASCRIPT:
        {
           NS_ENSURE_STATE(mDocShell);
           NS_ENSURE_TRUE((aValue == PR_TRUE || aValue == PR_FALSE), NS_ERROR_INVALID_ARG);
           mDocShell->SetAllowJavascript(aValue);
        }
        break;
    case nsIWebBrowserSetup::SETUP_ALLOW_META_REDIRECTS:
        {
           NS_ENSURE_STATE(mDocShell);
           NS_ENSURE_TRUE((aValue == PR_TRUE || aValue == PR_FALSE), NS_ERROR_INVALID_ARG);
           mDocShell->SetAllowMetaRedirects(aValue);
        }
        break;
    case nsIWebBrowserSetup::SETUP_ALLOW_SUBFRAMES:
        {
           NS_ENSURE_STATE(mDocShell);
           NS_ENSURE_TRUE((aValue == PR_TRUE || aValue == PR_FALSE), NS_ERROR_INVALID_ARG);
           mDocShell->SetAllowSubframes(aValue);
        }
        break;
    case nsIWebBrowserSetup::SETUP_ALLOW_IMAGES:
        {
           NS_ENSURE_STATE(mDocShell);
           NS_ENSURE_TRUE((aValue == PR_TRUE || aValue == PR_FALSE), NS_ERROR_INVALID_ARG);
           mDocShell->SetAllowImages(aValue);
        }
        break;
    case nsIWebBrowserSetup::SETUP_USE_GLOBAL_HISTORY:
        {
           NS_ENSURE_STATE(mDocShell);
           NS_ENSURE_TRUE((aValue == PR_TRUE || aValue == PR_FALSE), NS_ERROR_INVALID_ARG);
           rv = EnableGlobalHistory(aValue);
        }
        break;
    case nsIWebBrowserSetup::SETUP_FOCUS_DOC_BEFORE_CONTENT:
        {
            // obsolete
        }
        break;
    case nsIWebBrowserSetup::SETUP_IS_CHROME_WRAPPER:
        {
           NS_ENSURE_TRUE((aValue == PR_TRUE || aValue == PR_FALSE), NS_ERROR_INVALID_ARG);
           SetItemType(aValue ? typeChromeWrapper : typeContentWrapper);
        }
        break;
    default:
        rv = NS_ERROR_INVALID_ARG;
  
    }
    return rv;
}


//*****************************************************************************
// nsWebBrowser::nsIWebProgressListener
//*****************************************************************************

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long aStateFlags, in nsresult aStatus); */
NS_IMETHODIMP nsWebBrowser::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
    if (mPersist)
    {
        mPersist->GetCurrentState(&mPersistCurrentState);
    }
    if (aStateFlags & STATE_IS_NETWORK && aStateFlags & STATE_STOP)
    {
        mPersist = nsnull;
    }
    if (mProgressListener)
    {
        return mProgressListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);
    }
    return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP nsWebBrowser::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
    if (mPersist)
    {
        mPersist->GetCurrentState(&mPersistCurrentState);
    }
    if (mProgressListener)
    {
        return mProgressListener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress);
    }
    return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP nsWebBrowser::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    if (mProgressListener)
    {
        return mProgressListener->OnLocationChange(aWebProgress, aRequest, location);
    }
    return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP nsWebBrowser::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
    if (mProgressListener)
    {
        return mProgressListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);
    }
    return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP nsWebBrowser::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
    if (mProgressListener)
    {
        return mProgressListener->OnSecurityChange(aWebProgress, aRequest, state);
    }
    return NS_OK;
}

//*****************************************************************************
// nsWebBrowser::nsIWebBrowserPersist
//*****************************************************************************

/* attribute unsigned long persistFlags; */
NS_IMETHODIMP nsWebBrowser::GetPersistFlags(PRUint32 *aPersistFlags)
{
    NS_ENSURE_ARG_POINTER(aPersistFlags);
    nsresult rv = NS_OK;
    if (mPersist)
    {
        rv = mPersist->GetPersistFlags(&mPersistFlags);
    }
    *aPersistFlags = mPersistFlags;
    return rv;
}
NS_IMETHODIMP nsWebBrowser::SetPersistFlags(PRUint32 aPersistFlags)
{
    nsresult rv = NS_OK;
    mPersistFlags = aPersistFlags;
    if (mPersist)
    {
        rv = SetPersistFlags(mPersistFlags);
        mPersist->GetPersistFlags(&mPersistFlags);
    }
    return rv;
}


/* readonly attribute unsigned long currentState; */
NS_IMETHODIMP nsWebBrowser::GetCurrentState(PRUint32 *aCurrentState)
{
    NS_ENSURE_ARG_POINTER(aCurrentState);
    if (mPersist)
    {
        mPersist->GetCurrentState(&mPersistCurrentState);
    }
    *aCurrentState = mPersistCurrentState;
    return NS_OK;
}

/* readonly attribute unsigned long result; */
NS_IMETHODIMP nsWebBrowser::GetResult(PRUint32 *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    if (mPersist)
    {
        mPersist->GetResult(&mPersistResult);
    }
    *aResult = mPersistResult;
    return NS_OK;
}

/* attribute nsIWebBrowserPersistProgress progressListener; */
NS_IMETHODIMP nsWebBrowser::GetProgressListener(nsIWebProgressListener * *aProgressListener)
{
    NS_ENSURE_ARG_POINTER(aProgressListener);
    *aProgressListener = mProgressListener;
    NS_IF_ADDREF(*aProgressListener);
    return NS_OK;
}
  
NS_IMETHODIMP nsWebBrowser::SetProgressListener(nsIWebProgressListener * aProgressListener)
{
    mProgressListener = aProgressListener;
    return NS_OK;
}

/* void saveURI (in nsIURI aURI, in nsIURI aReferrer,
   in nsISupports aCacheKey, in nsIInputStream aPostData, in wstring aExtraHeaders,
   in nsISupports aFile); */
NS_IMETHODIMP nsWebBrowser::SaveURI(
    nsIURI *aURI, nsISupports *aCacheKey, nsIURI *aReferrer, nsIInputStream *aPostData,
    const char *aExtraHeaders, nsISupports *aFile)
{
    if (mPersist)
    {
        PRUint32 currentState;
        mPersist->GetCurrentState(&currentState);
        if (currentState == PERSIST_STATE_FINISHED)
        {
            mPersist = nsnull;
        }
        else
        {
            // You can't save again until the last save has completed
            return NS_ERROR_FAILURE;
        }
    }

    nsCOMPtr<nsIURI> uri;
    if (aURI)
    {
        uri = aURI;
    }
    else
    {
        nsresult rv = GetCurrentURI(getter_AddRefs(uri));
        if (NS_FAILED(rv))
        {
            return NS_ERROR_FAILURE;
        }
    }

    // Create a throwaway persistence object to do the work
    nsresult rv;
    mPersist = do_CreateInstance(NS_WEBBROWSERPERSIST_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    mPersist->SetProgressListener(this);
    mPersist->SetPersistFlags(mPersistFlags);
    mPersist->GetCurrentState(&mPersistCurrentState);
    rv = mPersist->SaveURI(uri, aCacheKey, aReferrer, aPostData, aExtraHeaders, aFile);
    if (NS_FAILED(rv))
    {
        mPersist = nsnull;
    }
    return rv;
}

/* void saveChannel (in nsIChannel aChannel, in nsISupports aFile); */
NS_IMETHODIMP nsWebBrowser::SaveChannel(
    nsIChannel* aChannel, nsISupports *aFile)
{
    if (mPersist)
    {
        PRUint32 currentState;
        mPersist->GetCurrentState(&currentState);
        if (currentState == PERSIST_STATE_FINISHED)
        {
            mPersist = nsnull;
        }
        else
        {
            // You can't save again until the last save has completed
            return NS_ERROR_FAILURE;
        }
    }

    // Create a throwaway persistence object to do the work
    nsresult rv;
    mPersist = do_CreateInstance(NS_WEBBROWSERPERSIST_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    mPersist->SetProgressListener(this);
    mPersist->SetPersistFlags(mPersistFlags);
    mPersist->GetCurrentState(&mPersistCurrentState);
    rv = mPersist->SaveChannel(aChannel, aFile);
    if (NS_FAILED(rv))
    {
        mPersist = nsnull;
    }
    return rv;
}

/* void saveDocument (in nsIDOMDocument document, in nsISupports aFile, in nsISupports aDataPath); */
NS_IMETHODIMP nsWebBrowser::SaveDocument(
    nsIDOMDocument *aDocument, nsISupports *aFile, nsISupports *aDataPath,
    const char *aOutputContentType, PRUint32 aEncodingFlags, PRUint32 aWrapColumn)
{
    if (mPersist)
    {
        PRUint32 currentState;
        mPersist->GetCurrentState(&currentState);
        if (currentState == PERSIST_STATE_FINISHED)
        {
            mPersist = nsnull;
        }
        else
        {
            // You can't save again until the last save has completed
            return NS_ERROR_FAILURE;
        }
    }

    // Use the specified DOM document, or if none is specified, the one
    // attached to the web browser.

    nsCOMPtr<nsIDOMDocument> doc;
    if (aDocument)
    {
        doc = do_QueryInterface(aDocument);
    }
    else
    {
        GetDocument(getter_AddRefs(doc));
    }
    if (!doc)
    {
        return NS_ERROR_FAILURE;
    }

    // Create a throwaway persistence object to do the work
    nsresult rv;
    mPersist = do_CreateInstance(NS_WEBBROWSERPERSIST_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    mPersist->SetProgressListener(this);
    mPersist->SetPersistFlags(mPersistFlags);
    mPersist->GetCurrentState(&mPersistCurrentState);
    rv = mPersist->SaveDocument(doc, aFile, aDataPath, aOutputContentType, aEncodingFlags, aWrapColumn);
    if (NS_FAILED(rv))
    {
        mPersist = nsnull;
    }
    return rv;
}

/* void cancelSave(); */
NS_IMETHODIMP nsWebBrowser::CancelSave()
{
    if (mPersist)
    {
        return mPersist->CancelSave();
    }
    return NS_OK;
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

   nsCOMPtr<nsIWidget> docShellParentWidget(mParentWidget);
   if(!mParentWidget) // We need to create a widget
      {
      // Create the widget
      NS_ENSURE_TRUE(mInternalWidget = do_CreateInstance(kChildCID), NS_ERROR_FAILURE);

      docShellParentWidget = mInternalWidget;
      nsWidgetInitData  widgetInit;

      widgetInit.clipChildren = PR_TRUE;
      widgetInit.mContentType = (mContentType == typeChrome || 
        mContentType == typeChromeWrapper)? eContentTypeUI: eContentTypeContent;

      widgetInit.mWindowType = eWindowType_child;
      nsRect bounds(mInitInfo->x, mInitInfo->y, mInitInfo->cx, mInitInfo->cy);
      
      mInternalWidget->SetClientData(NS_STATIC_CAST(nsWebBrowser *, this));
      mInternalWidget->Create(mParentNativeWindow, bounds, nsWebBrowser::HandleEvent,
                              nsnull, nsnull, nsnull, &widgetInit);  
      }

   nsCOMPtr<nsIDocShell> docShell(do_CreateInstance(kWebShellCID));
   NS_ENSURE_SUCCESS(SetDocShell(docShell), NS_ERROR_FAILURE);

   // get the system default window background colour
   {
      nsCOMPtr<nsILookAndFeel> laf = do_GetService(kLookAndFeelCID);
      laf->GetColor(nsILookAndFeel::eColor_WindowBackground, mBackgroundColor);
   }

   // the docshell has been set so we now have our listener registrars.
   if (mListenerArray) {
      // we had queued up some listeners, let's register them now.
      PRInt32 count = mListenerArray->Count();
      PRInt32 i = 0;
      NS_ASSERTION(count > 0, "array construction problem");
      while (i < count) {
          nsWebBrowserListenerState *state = (nsWebBrowserListenerState*)mListenerArray->ElementAt(i);
          NS_ASSERTION(state, "array construction problem");
          nsCOMPtr<nsISupports> listener = do_QueryReferent(state->mWeakPtr);
          NS_ASSERTION(listener, "bad listener");
          (void)BindListener(listener, state->mID);
          i++;
      }
      (void)mListenerArray->EnumerateForwards(deleteListener, nsnull);
      NS_DELETEXPCOM(mListenerArray);
      mListenerArray = nsnull;
   }

   // HACK ALERT - this registration registers the nsDocShellTreeOwner as a 
   // nsIWebBrowserListener so it can setup it's MouseListener in one of the 
   // progress callbacks. If we can register the MouseListener another way, this 
   // registration can go away, and nsDocShellTreeOwner can stop implementing
   // nsIWebProgressListener.
   nsCOMPtr<nsISupports> supports = nsnull;
   (void)mDocShellTreeOwner->QueryInterface(NS_GET_IID(nsIWebProgressListener),
                             NS_STATIC_CAST(void**, getter_AddRefs(supports)));
   (void)BindListener(supports, NS_GET_IID(nsIWebProgressListener));

   NS_ENSURE_SUCCESS(mDocShellAsWin->InitWindow(nsnull,
      docShellParentWidget, mInitInfo->x, mInitInfo->y, mInitInfo->cx,
      mInitInfo->cy), NS_ERROR_FAILURE);

   mDocShellAsItem->SetName(mInitInfo->name.get());
   if (mContentType == typeChromeWrapper)
   {
       mDocShellAsItem->SetItemType(nsIDocShellTreeItem::typeChrome);
   }
   else
   {
       mDocShellAsItem->SetItemType(nsIDocShellTreeItem::typeContent);
   }
   mDocShellAsItem->SetTreeOwner(mDocShellTreeOwner);
   
   // If the webbrowser is a content docshell item then we won't hear any
   // events from subframes. To solve that we install our own chrome event handler
   // that always gets called (even for subframes) for any bubbling event.

   if(!mInitInfo->sessionHistory)
      mInitInfo->sessionHistory = do_CreateInstance(NS_SHISTORY_CONTRACTID);
   NS_ENSURE_TRUE(mInitInfo->sessionHistory, NS_ERROR_FAILURE);
   mDocShellAsNav->SetSessionHistory(mInitInfo->sessionHistory);
   
   // Hook up global history. Do not fail if we can't - just assert.
   nsresult rv = EnableGlobalHistory(PR_TRUE);
   NS_ASSERTION(NS_SUCCEEDED(rv), "EnableGlobalHistory() failed");

   NS_ENSURE_SUCCESS(mDocShellAsWin->Create(), NS_ERROR_FAILURE);

   // Hook into the OnSecurirtyChange() notification for lock/unlock icon
   // updates
   nsCOMPtr<nsIDOMWindow> domWindow;
   rv = GetContentDOMWindow(getter_AddRefs(domWindow));
   if (NS_SUCCEEDED(rv))
   {
       mSecurityUI = do_CreateInstance(NS_SECURE_BROWSER_UI_CONTRACTID, &rv);
       if (NS_SUCCEEDED(rv))mSecurityUI->Init(domWindow);
   }

   mDocShellTreeOwner->AddToWatcher(); // evil twin of Remove in SetDocShell(0)
   mDocShellTreeOwner->AddChromeListeners();

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
   
#if (defined(XP_MAC) || defined(XP_MACOSX)) && !defined(MOZ_WIDGET_COCOA)
   nsresult rv = EnsureTopLevelWidget(aParentNativeWindow);
   NS_ENSURE_SUCCESS(rv, rv);
   mParentNativeWindow = mTopLevelWidget->GetNativeData(NS_NATIVE_WINDOW);
#else
   mParentNativeWindow = aParentNativeWindow;
#endif

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

NS_IMETHODIMP nsWebBrowser::GetEnabled(PRBool *aEnabled)
{
  if (mInternalWidget)
    return mInternalWidget->IsEnabled(aEnabled);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::SetEnabled(PRBool aEnabled)
{
  if (mInternalWidget)
    return mInternalWidget->Enable(aEnabled);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWebBrowser::GetBlurSuppression(PRBool *aBlurSuppression)
{
  NS_ENSURE_ARG_POINTER(aBlurSuppression);
  *aBlurSuppression = PR_FALSE;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebBrowser::SetBlurSuppression(PRBool aBlurSuppression)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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

   if (NS_FAILED(mDocShellAsWin->SetFocus()))
     return NS_ERROR_FAILURE;

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

NS_IMETHODIMP nsWebBrowser::GetDefaultScrollbarPreferences(PRInt32 aScrollOrientation,
   PRInt32* aScrollbarPref)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->GetDefaultScrollbarPreferences(aScrollOrientation,
      aScrollbarPref);
}

NS_IMETHODIMP nsWebBrowser::SetDefaultScrollbarPreferences(PRInt32 aScrollOrientation,
   PRInt32 aScrollbarPref)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->SetDefaultScrollbarPreferences(aScrollOrientation,
      aScrollbarPref);
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
         nsCOMPtr<nsIScrollable> scrollable(do_QueryInterface(aDocShell));
         nsCOMPtr<nsITextScroll> textScroll(do_QueryInterface(aDocShell));
         nsCOMPtr<nsIWebProgress> progress(do_GetInterface(aDocShell));
         NS_ENSURE_TRUE(req && baseWin && item && nav && scrollable && textScroll && progress,
             NS_ERROR_FAILURE);
 
         mDocShell = aDocShell;
         mDocShellAsReq = req;
         mDocShellAsWin = baseWin;
         mDocShellAsItem = item;
         mDocShellAsNav = nav;
         mDocShellAsScrollable = scrollable;
         mDocShellAsTextScroll = textScroll;
         mWebProgress = progress;
     }
     else
     {
         if (mDocShellTreeOwner)
           mDocShellTreeOwner->RemoveFromWatcher(); // evil twin of Add in Create()
         if (mDocShellAsWin)
           mDocShellAsWin->Destroy();

         mDocShell = nsnull;
         mDocShellAsReq = nsnull;
         mDocShellAsWin = nsnull;
         mDocShellAsItem = nsnull;
         mDocShellAsNav = nsnull;
         mDocShellAsScrollable = nsnull;
         mDocShellAsTextScroll = nsnull;
         mWebProgress = nsnull;
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

/* static */
nsEventStatus PR_CALLBACK nsWebBrowser::HandleEvent(nsGUIEvent *aEvent)
{
  nsEventStatus  result = nsEventStatus_eIgnore;
  nsWebBrowser  *browser = nsnull;
  void          *data = nsnull;

  if (!aEvent->widget)
    return result;

  aEvent->widget->GetClientData(data);
  if (!data)
    return result;

  browser = NS_STATIC_CAST(nsWebBrowser *, data);

  switch(aEvent->message) {

  case NS_PAINT: {
      nsPaintEvent *paintEvent = NS_STATIC_CAST(nsPaintEvent *, aEvent);
      nsIRenderingContext *rc = paintEvent->renderingContext;
      nscolor oldColor;
      rc->GetColor(oldColor);
      rc->SetColor(browser->mBackgroundColor);

      nsIRegion *region = paintEvent->region;
      if (region) {
          nsRegionRectSet *rects = nsnull;
          region->GetRects(&rects);
          if (rects) {
              for (PRUint32 i = 0; i < rects->mNumRects; ++i) {
                  nsRect r(rects->mRects[i].x, rects->mRects[i].y,
                           rects->mRects[i].width, rects->mRects[i].height);
                  rc->FillRect(r);
              }

              region->FreeRects(rects);
          }
      } else if (paintEvent->rect) {
          rc->FillRect(*paintEvent->rect);
      }
      rc->SetColor(oldColor);
      break;
    }

  default:
    break;
  }

  return result;
    
  
}

NS_IMETHODIMP nsWebBrowser::GetPrimaryContentWindow(nsIDOMWindowInternal **aDOMWindow)
{
  *aDOMWindow = 0;

  nsCOMPtr<nsIDocShellTreeItem> item;
  NS_ENSURE_TRUE(mDocShellTreeOwner, NS_ERROR_FAILURE);
  mDocShellTreeOwner->GetPrimaryContentShell(getter_AddRefs(item));
  NS_ENSURE_TRUE(item, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShell> docShell;
  docShell = do_QueryInterface(item);
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIDOMWindowInternal> domWindow;
  domWindow = do_GetInterface(docShell);
  NS_ENSURE_TRUE(domWindow, NS_ERROR_FAILURE);

  *aDOMWindow = domWindow;
  NS_ADDREF(*aDOMWindow);
  return NS_OK;
  
}
//*****************************************************************************
// nsWebBrowser::nsIWebBrowserFocus
//*****************************************************************************   

/* void activate (); */
NS_IMETHODIMP nsWebBrowser::Activate(void)
{
  // stop infinite recursion from windows with onfocus handlers that
  // reactivate the window
  if (mActivating)
    return NS_OK;

  mActivating = PR_TRUE;

  // try to set focus on the last focused window as stored in the
  // focus controller object.
  nsCOMPtr<nsIDOMWindow> domWindowExternal;
  GetContentDOMWindow(getter_AddRefs(domWindowExternal));
  nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(domWindowExternal));
  PRBool needToFocus = PR_TRUE;
  if (piWin) {
    nsIFocusController *focusController = piWin->GetRootFocusController();
    if (focusController) {
      // Go ahead and mark the focus controller as being active.  We have
      // to do this even before the activate message comes in.
      focusController->SetActive(PR_TRUE);

      nsCOMPtr<nsIDOMWindowInternal> focusedWindow;
      focusController->GetFocusedWindow(getter_AddRefs(focusedWindow));
      if (focusedWindow) {
        needToFocus = PR_FALSE;
        focusController->SetSuppressFocus(PR_TRUE, "Activation Suppression");
        piWin->Focus(); // This sets focus, but we'll ignore it.  
                        // A subsequent activate will cause us to stop suppressing.
      }
    }
  }

  // If there wasn't a focus controller and focused window just set
  // focus on the primary content shell.  If that wasn't focused,
  // try and just set it on the toplevel DOM window.
  if (needToFocus) {
    nsCOMPtr<nsIDOMWindowInternal> contentDomWindow;
    GetPrimaryContentWindow(getter_AddRefs(contentDomWindow));
    if (contentDomWindow)
      contentDomWindow->Focus();
    else if (piWin)
      piWin->Focus();
  }

  nsCOMPtr<nsIDOMWindow> win;
  GetContentDOMWindow(getter_AddRefs(win));
  if (win) {
    // tell windowwatcher about the new active window
    if (mWWatch)
      mWWatch->SetActiveWindow(win);

    /* Activate the window itself. Do this only if the PresShell has
       been created, since DOMWindow->Activate asserts otherwise.
       (This method can be called during window creation before
       the PresShell exists. For ex, Windows apps responding to
       WM_ACTIVATE). */
    NS_ENSURE_STATE(mDocShell);
    nsCOMPtr<nsIPresShell> presShell;
    mDocShell->GetPresShell(getter_AddRefs(presShell));
    if(presShell) {
      nsCOMPtr<nsPIDOMWindow> privateDOMWindow = do_QueryInterface(win);
      if(privateDOMWindow)
        privateDOMWindow->Activate();
    }
  }

  mActivating = PR_FALSE;
  return NS_OK;
}

/* void deactivate (); */
NS_IMETHODIMP nsWebBrowser::Deactivate(void)
{
  /* At this time we don't clear mWWatch's ActiveWindow; we just allow
     the presumed other newly active window to set it when it comes in.
     This seems harmless and maybe safer, but we have no real evidence
     either way just yet. */

  NS_ENSURE_STATE(mDocShell);
  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));
  if(!presShell)
    return NS_OK;

  nsCOMPtr<nsIDOMWindow> domWindow;
  GetContentDOMWindow(getter_AddRefs(domWindow));
  if (domWindow) {
    nsCOMPtr<nsPIDOMWindow> privateDOMWindow = do_QueryInterface(domWindow);
    if(privateDOMWindow) {
      nsIFocusController *focusController =
          privateDOMWindow->GetRootFocusController();
      if (focusController)
        focusController->SetActive(PR_FALSE);
      privateDOMWindow->Deactivate();
    }
  }

  return NS_OK;
}

/* void setFocusAtFirstElement (); */
NS_IMETHODIMP nsWebBrowser::SetFocusAtFirstElement(void)
{
  return NS_OK;
}

/* void setFocusAtLastElement (); */
NS_IMETHODIMP nsWebBrowser::SetFocusAtLastElement(void)
{
  return NS_OK;
}

/* attribute nsIDOMWindow focusedWindow; */
NS_IMETHODIMP nsWebBrowser::GetFocusedWindow(nsIDOMWindow * *aFocusedWindow)
{
    NS_ENSURE_ARG_POINTER(aFocusedWindow);
    *aFocusedWindow = nsnull;

    nsresult rv;
    nsCOMPtr<nsIDOMWindowInternal> focusedWindow;

    nsCOMPtr<nsIDOMWindow> domWindowExternal;
    rv = GetContentDOMWindow(getter_AddRefs(domWindowExternal));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(domWindowExternal /*domWindow*/, &rv));
    if (NS_FAILED(rv)) return rv;
    
    nsIFocusController *focusController = piWin->GetRootFocusController();
    if (focusController)
      rv = focusController->GetFocusedWindow(getter_AddRefs(focusedWindow));
    
    *aFocusedWindow = focusedWindow;
    NS_IF_ADDREF(*aFocusedWindow);
    
    return *aFocusedWindow ? NS_OK : NS_ERROR_FAILURE;
}
NS_IMETHODIMP nsWebBrowser::SetFocusedWindow(nsIDOMWindow * aFocusedWindow)
{
  return NS_OK;
}

/* attribute nsIDOMElement focusedElement; */
NS_IMETHODIMP nsWebBrowser::GetFocusedElement(nsIDOMElement * *aFocusedElement)
{
  NS_ENSURE_ARG_POINTER(aFocusedElement);
  *aFocusedElement = nsnull;
  
  nsresult rv;
  nsCOMPtr<nsIDOMElement> focusedElement;

  nsCOMPtr<nsIDOMWindow> domWindowExternal;
  rv = GetContentDOMWindow(getter_AddRefs(domWindowExternal));
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(domWindowExternal, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIFocusController *focusController = piWin->GetRootFocusController();
  if (focusController)
  rv = focusController->GetFocusedElement(getter_AddRefs(focusedElement));

  *aFocusedElement = focusedElement;
  NS_IF_ADDREF(*aFocusedElement);
  return *aFocusedElement ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::SetFocusedElement(nsIDOMElement * aFocusedElement)
{
  return NS_OK;
}

//*****************************************************************************
// nsWebBrowser::nsIWebBrowserStream
//*****************************************************************************   

/* void openStream (in string aBaseURI, in string aContentType); */
NS_IMETHODIMP nsWebBrowser::OpenStream(const char *aBaseURI, const char *aContentType)
{
  nsresult rv;

  if (!mStream) {
    mStream = new nsEmbedStream();
    mStreamGuard = do_QueryInterface(mStream);
    mStream->InitOwner(this);
    rv = mStream->Init();
    if (NS_FAILED(rv))
      return rv;
  }

  return mStream->OpenStream(aBaseURI, aContentType);
}

/* void appendStream (in string aData, in long aLen); */
NS_IMETHODIMP nsWebBrowser::AppendToStream(const char *aData, PRInt32 aLen)
{
  if (!mStream)
    return NS_ERROR_FAILURE;

  return mStream->AppendToStream(aData, aLen);
}

/* void closeStream (); */
NS_IMETHODIMP nsWebBrowser::CloseStream()
{
  nsresult rv;

  if (!mStream)
    return NS_ERROR_FAILURE;
  rv = mStream->CloseStream();

  // release
  mStream = 0;
  mStreamGuard = 0;

  return rv;
}
