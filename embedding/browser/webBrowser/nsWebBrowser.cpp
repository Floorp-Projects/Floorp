/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "nsWebBrowserPersist.h"

// Helper Classes
#include "nsGfxCIID.h"
#include "nsWidgetsCID.h"

//Interfaces Needed
#include "nsReadableUtils.h"
#include "nsIComponentManager.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
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
#include "nsIPresShell.h"
#include "nsIGlobalHistory.h"
#include "nsIDocShellHistory.h"
#include "nsIURIContentListener.h"
#include "nsGUIEvent.h"
#include "nsISHistoryListener.h"

// for painting the background window
#include "nsIRenderingContext.h"
#include "nsILookAndFeel.h"

// Printing Includes
#include "nsIContentViewer.h"
#include "nsIContentViewerFile.h"
// Print Options
#include "nsIPrintOptions.h"
#include "nsGfxCIID.h"
#include "nsIServiceManager.h"

// PSM2 includes
#include "nsISecureBrowserUI.h"

static NS_DEFINE_CID(kPrintOptionsCID, NS_PRINTOPTIONS_CID);
static NS_DEFINE_CID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_CID(kChildCID, NS_CHILD_CID);
static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

//*****************************************************************************
//***    nsWebBrowser: Object Management
//*****************************************************************************

nsWebBrowser::nsWebBrowser() : mDocShellTreeOwner(nsnull), 
   mInitInfo(nsnull), mContentType(typeContentWrapper),
   mParentNativeWindow(nsnull), mParentWidget(nsnull), mParent(nsnull),
   mProgressListener(nsnull), mListenerArray(nsnull),
   mBackgroundColor(0),
   mPersistCurrentState(nsIWebBrowserPersist::PERSIST_STATE_READY),
   mPersistFlags(nsIWebBrowserPersist::PERSIST_FLAGS_NONE),
   mPersistResult(NS_OK)
{
    NS_INIT_REFCNT();
    mInitInfo = new nsWebBrowserInitInfo();
    mWWatch = do_GetService("@mozilla.org/embedcomp/window-watcher;1");
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
    NS_INTERFACE_MAP_ENTRY(nsIWebBrowserPrint)
    NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
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

   if(mDocShell)
      return mDocShellAsReq->GetInterface(aIID, aSink);

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
        rv = mWebProgress->AddProgressListener(listener);
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
        rv = UnBindListener(aListener, aIID);
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
       nsCOMPtr<nsIGlobalHistory> history = 
                do_GetService(NS_GLOBALHISTORY_CONTRACTID, &rv);
       if (NS_FAILED(rv)) return rv;  
       rv = dsHistory->SetGlobalHistory(history);
    }
    else
       rv = dsHistory->SetGlobalHistory(nsnull);
       
    return rv;
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

NS_IMETHODIMP nsWebBrowser::LoadURI(const PRUnichar* aURI, PRUint32 aLoadFlags)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->LoadURI(aURI, aLoadFlags);
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
    default:
        rv = NS_ERROR_INVALID_ARG;
  
    }
    return rv;
}


//*****************************************************************************
// nsWebBrowser::nsIWebProgressListener
//*****************************************************************************

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aStateFlags, in unsigned long aStatus); */
NS_IMETHODIMP nsWebBrowser::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aStateFlags, PRUint32 aStatus)
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

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long state); */
NS_IMETHODIMP nsWebBrowser::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 state)
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

/* void saveURI (in nsIURI aURI, in nsILocalFile aFile); */
NS_IMETHODIMP nsWebBrowser::SaveURI(nsIURI *aURI, nsIInputStream *aPostData, nsILocalFile *aFile)
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
    nsWebBrowserPersist *persist = new nsWebBrowserPersist();
    mPersist = do_QueryInterface(NS_STATIC_CAST(nsIWebBrowserPersist *, persist));
    mPersist->SetProgressListener(this);
    mPersist->SetPersistFlags(mPersistFlags);
    mPersist->GetCurrentState(&mPersistCurrentState);
    nsresult rv = mPersist->SaveURI(uri, aPostData, aFile);
    if (NS_FAILED(rv))
    {
        mPersist = nsnull;
    }
    return rv;
}

/* void saveDocument (in nsIDOMDocument document, in nsILocalFile aFile, in nsILocalFile aDataPath); */
NS_IMETHODIMP nsWebBrowser::SaveDocument(nsIDOMDocument *aDocument, nsILocalFile *aFile, nsILocalFile *aDataPath)
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
    nsWebBrowserPersist *persist = new nsWebBrowserPersist();
    mPersist = do_QueryInterface(NS_STATIC_CAST(nsIWebBrowserPersist *, persist));
    mPersist->SetProgressListener(this);
    mPersist->SetPersistFlags(mPersistFlags);
    mPersist->GetCurrentState(&mPersistCurrentState);
    nsresult rv = mPersist->SaveDocument(doc, aFile, aDataPath);
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
      widgetInit.mWindowType = eWindowType_child;
      nsRect bounds(mInitInfo->x, mInitInfo->y, mInitInfo->cx, mInitInfo->cy);
      
      mInternalWidget->SetClientData(NS_STATIC_CAST(nsWebBrowser *, this));
      mInternalWidget->Create(mParentNativeWindow, bounds, nsWebBrowser::HandleEvent,
                              nsnull, nsnull, nsnull, &widgetInit);  
      }

   // get the system default window background colour
   {
      nsCOMPtr<nsILookAndFeel> laf = do_GetService(kLookAndFeelCID);
      laf->GetColor(nsILookAndFeel::eColor_WindowBackground, mBackgroundColor);
   }

   nsCOMPtr<nsIDocShell> docShell(do_CreateInstance(kWebShellCID));
   NS_ENSURE_SUCCESS(SetDocShell(docShell), NS_ERROR_FAILURE);

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
       if (NS_SUCCEEDED(rv))mSecurityUI->Init(domWindow, nsnull);
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
      rc->FillRect(*paintEvent->rect);
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
  // try to set focus on the last focused window as stored in the
  // focus controller object.
  nsCOMPtr<nsIDOMWindow> domWindowExternal;
  GetContentDOMWindow(getter_AddRefs(domWindowExternal));
  nsCOMPtr<nsIDOMWindowInternal> domWindow;
  domWindow = do_QueryInterface(domWindowExternal);
  nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(domWindow));
  nsCOMPtr<nsIFocusController> focusController;
  piWin->GetRootFocusController(getter_AddRefs(focusController));
  PRBool needToFocus = PR_TRUE;
  if (focusController) {
    // Go ahead and mark the focus controller as being active.  We have
    // to do this even before the activate message comes in.
    focusController->SetActive(PR_TRUE);

    nsCOMPtr<nsIDOMWindowInternal> focusedWindow;
    focusController->GetFocusedWindow(getter_AddRefs(focusedWindow));
    if (focusedWindow) {
      needToFocus = PR_FALSE;
      focusController->SetSuppressFocus(PR_TRUE, "Activation Suppression");
      domWindow->Focus(); // This sets focus, but we'll ignore it.  
                         // A subsequent activate will cause us to stop suppressing.
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
    else if (domWindow)
      domWindow->Focus();
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
        if(privateDOMWindow)
          privateDOMWindow->Deactivate();
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
    
    nsCOMPtr<nsIFocusController> focusController;
    piWin->GetRootFocusController(getter_AddRefs(focusController));
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

  nsCOMPtr<nsIFocusController> focusController;
  piWin->GetRootFocusController(getter_AddRefs(focusController));
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
// nsWebBrowser::nsIWebBrowserPrint
//*****************************************************************************   

/* void Print (in nsIDOMWindow aDOMWindow, in nsIPrintOptions aThePrintOptions); */
NS_IMETHODIMP nsWebBrowser::Print(nsIDOMWindow *aDOMWindow, 
                                  nsIPrintOptions *aThePrintOptions,
                                  nsIPrintListener *aPrintListener)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMWindow> thisDOMWin;
  PRBool silent = PR_FALSE;
  if( aThePrintOptions != nsnull){
    aThePrintOptions->GetPrintSilent (&silent);
  }
  

  // XXX this next line may need to be changed
  // it is unclear what the correct way is to get the document.
  GetContentDOMWindow(getter_AddRefs(thisDOMWin));
  if (aDOMWindow == thisDOMWin.get()) {
     nsCOMPtr<nsIContentViewer> contentViewer;
     mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
     if (contentViewer) {
       nsCOMPtr<nsIContentViewerFile> contentViewerFile(do_QueryInterface(contentViewer));
       if (contentViewerFile) {
         rv = contentViewerFile->Print(silent, nsnull, aPrintListener);
       }
     }
  }
  return rv;
}

  /* void Cancel (); */
NS_IMETHODIMP nsWebBrowser::Cancel(void)
{
  nsresult rv;
  nsCOMPtr<nsIPrintOptions> printService = 
           do_GetService(kPrintOptionsCID, &rv);
  if (NS_SUCCEEDED(rv) && printService) {
    return printService->SetIsCancelled(PR_TRUE);
  }
  return NS_OK;
}
