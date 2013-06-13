/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Local Includes
#include "nsWebBrowser.h"

// Helper Classes
#include "nsGfxCIID.h"
#include "nsWidgetsCID.h"

//Interfaces Needed
#include "nsReadableUtils.h"
#include "nsIComponentManager.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsIDOMElement.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebBrowserChrome.h"
#include "nsPIDOMWindow.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsIWebBrowserFocus.h"
#include "nsIWebBrowserStream.h"
#include "nsIPresShell.h"
#include "nsIURIContentListener.h"
#include "nsGUIEvent.h"
#include "nsISHistoryListener.h"
#include "nsIURI.h"
#include "nsIWebBrowserPersist.h"
#include "nsCWebBrowserPersist.h"
#include "nsIServiceManager.h"
#include "nsAutoPtr.h"
#include "nsFocusManager.h"
#include "Layers.h"
#include "gfxContext.h"

// for painting the background window
#include "mozilla/LookAndFeel.h"

// Printing Includes
#ifdef NS_PRINTING
#include "nsIWebBrowserPrint.h"
#include "nsIContentViewer.h"
#endif

// PSM2 includes
#include "nsISecureBrowserUI.h"
#include "nsXULAppAPI.h"

using namespace mozilla;
using namespace mozilla::layers;

static NS_DEFINE_IID(kWindowCID, NS_WINDOW_CID);
static NS_DEFINE_CID(kChildCID, NS_CHILD_CID);


//*****************************************************************************
//***    nsWebBrowser: Object Management
//*****************************************************************************

nsWebBrowser::nsWebBrowser() : mDocShellTreeOwner(nullptr), 
   mInitInfo(nullptr),
   mContentType(typeContentWrapper),
   mActivating(false),
   mShouldEnableHistory(true),
   mIsActive(true),
   mParentNativeWindow(nullptr),
   mProgressListener(nullptr),
   mBackgroundColor(0),
   mPersistCurrentState(nsIWebBrowserPersist::PERSIST_STATE_READY),
   mPersistResult(NS_OK),
   mPersistFlags(nsIWebBrowserPersist::PERSIST_FLAGS_NONE),
   mStream(nullptr),
   mParentWidget(nullptr),
   mListenerArray(nullptr)
{
    mInitInfo = new nsWebBrowserInitInfo();
    mWWatch = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
    NS_ASSERTION(mWWatch, "failed to get WindowWatcher");
}

nsWebBrowser::~nsWebBrowser()
{
   InternalDestroy();
}

NS_IMETHODIMP nsWebBrowser::InternalDestroy()
{

   if (mInternalWidget) {
     mInternalWidget->SetWidgetListener(nullptr);
     mInternalWidget->Destroy();
     mInternalWidget = nullptr; // Force release here.
   }

   SetDocShell(nullptr);

   if(mDocShellTreeOwner)
      {
      mDocShellTreeOwner->WebBrowser(nullptr);
      NS_RELEASE(mDocShellTreeOwner);
      }
   if(mInitInfo)
      {
      delete mInitInfo;
      mInitInfo = nullptr;
      }

   if (mListenerArray) {
      for (uint32_t i = 0, end = mListenerArray->Length(); i < end; i++) {
         nsWebBrowserListenerState *state = mListenerArray->ElementAt(i);
         delete state;
      }
      delete mListenerArray;
      mListenerArray = nullptr;
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
    NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeNode)
    NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
    NS_INTERFACE_MAP_ENTRY(nsIWebBrowserSetup)
    NS_INTERFACE_MAP_ENTRY(nsIWebBrowserPersist)
    NS_INTERFACE_MAP_ENTRY(nsICancelable)
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

   if (mDocShell) {
#ifdef NS_PRINTING
       if (aIID.Equals(NS_GET_IID(nsIWebBrowserPrint))) {
           nsCOMPtr<nsIContentViewer> viewer;
           mDocShell->GetContentViewer(getter_AddRefs(viewer));
           if (!viewer)
               return NS_NOINTERFACE;

           nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint(do_QueryInterface(viewer));
           nsIWebBrowserPrint* print = (nsIWebBrowserPrint*)webBrowserPrint.get();
           NS_ASSERTION(print, "This MUST support this interface!");
           NS_ADDREF(print);
           *aSink = print;
           return NS_OK;
       }
#endif
       return mDocShellAsReq->GetInterface(aIID, aSink);
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
    NS_ENSURE_ARG_POINTER(aListener);

    nsresult rv = NS_OK;
    if (!mWebProgress) {
        // The window hasn't been created yet, so queue up the listener. They'll be
        // registered when the window gets created.
        nsAutoPtr<nsWebBrowserListenerState> state;
        state = new nsWebBrowserListenerState();
        if (!state) return NS_ERROR_OUT_OF_MEMORY;

        state->mWeakPtr = aListener;
        state->mID = aIID;

        if (!mListenerArray) {
            mListenerArray = new nsTArray<nsWebBrowserListenerState*>();
            if (!mListenerArray) {
                return NS_ERROR_OUT_OF_MEMORY;
            }
        }

        if (!mListenerArray->AppendElement(state)) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        // We're all set now; don't delete |state| after this point
        state.forget();
    } else {
        nsCOMPtr<nsISupports> supports(do_QueryReferent(aListener));
        if (!supports) return NS_ERROR_INVALID_ARG;
        rv = BindListener(supports, aIID);
    }
    
    return rv;
}

NS_IMETHODIMP nsWebBrowser::BindListener(nsISupports *aListener, const nsIID& aIID) {
    NS_ENSURE_ARG_POINTER(aListener);
    NS_ASSERTION(mWebProgress, "this should only be called after we've retrieved a progress iface");
    nsresult rv = NS_OK;

    // register this listener for the specified interface id
    if (aIID.Equals(NS_GET_IID(nsIWebProgressListener))) {
        nsCOMPtr<nsIWebProgressListener> listener = do_QueryInterface(aListener, &rv);
        if (NS_FAILED(rv)) return rv;
        NS_ENSURE_STATE(mWebProgress);
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
    NS_ENSURE_ARG_POINTER(aListener);

    nsresult rv = NS_OK;
    if (!mWebProgress) {
        // if there's no-one to register the listener w/, and we don't have a queue going,
        // the the called is calling Remove before an Add which doesn't make sense.
        if (!mListenerArray) return NS_ERROR_FAILURE;

        // iterate the array and remove the queued listener
        int32_t count = mListenerArray->Length();
        while (count > 0) {
            nsWebBrowserListenerState *state = mListenerArray->ElementAt(count);
            NS_ASSERTION(state, "list construction problem");

            if (state->Equals(aListener, aIID)) {
                // this is the one, pull it out.
                mListenerArray->RemoveElementAt(count);
                break;
            }
            count--; 
        }

        // if we've emptied the array, get rid of it.
        if (0 >= mListenerArray->Length()) {
            for (uint32_t i = 0, end = mListenerArray->Length(); i < end; i++) {
               nsWebBrowserListenerState *state = mListenerArray->ElementAt(i);
               delete state;
            }
            delete mListenerArray;
            mListenerArray = nullptr;
        }

    } else {
        nsCOMPtr<nsISupports> supports(do_QueryReferent(aListener));
        if (!supports) return NS_ERROR_INVALID_ARG;
        rv = UnBindListener(supports, aIID);
    }
    
    return rv;
}

NS_IMETHODIMP nsWebBrowser::UnBindListener(nsISupports *aListener, const nsIID& aIID) {
    NS_ENSURE_ARG_POINTER(aListener);
    NS_ASSERTION(mWebProgress, "this should only be called after we've retrieved a progress iface");
    nsresult rv = NS_OK;

    // remove the listener for the specified interface id
    if (aIID.Equals(NS_GET_IID(nsIWebProgressListener))) {
        nsCOMPtr<nsIWebProgressListener> listener = do_QueryInterface(aListener, &rv);
        if (NS_FAILED(rv)) return rv;
        NS_ENSURE_STATE(mWebProgress);
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

NS_IMETHODIMP nsWebBrowser::EnableGlobalHistory(bool aEnable)
{
    nsresult rv;
    
    NS_ENSURE_STATE(mDocShell);
    
    return mDocShell->SetUseGlobalHistory(aEnable);
}

NS_IMETHODIMP nsWebBrowser::GetContainerWindow(nsIWebBrowserChrome** aTopWindow)
{
   NS_ENSURE_ARG_POINTER(aTopWindow);

   if(mDocShellTreeOwner) {
      *aTopWindow = mDocShellTreeOwner->GetWebBrowserChrome().get();
   } else {
      *aTopWindow = nullptr;
   }

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
   *aParentContentListener = nullptr;

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

NS_IMETHODIMP nsWebBrowser::GetIsActive(bool *rv)
{
  *rv = mIsActive;
  return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetIsActive(bool aIsActive)
{
  // Set our copy of the value
  mIsActive = aIsActive;

  // If we have a docshell, pass on the request
  if (mDocShell)
    return mDocShell->SetIsActive(aIsActive);
  return NS_OK;
}

//*****************************************************************************
// nsWebBrowser::nsIDocShellTreeItem
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowser::GetName(nsAString& aName)
{
   if(mDocShell)  
      mDocShell->GetName(aName);
   else
      aName = mInitInfo->name;

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetName(const nsAString& aName)
{
   if(mDocShell)
      {
      return mDocShell->SetName(aName);
      }
   else
      mInitInfo->name = aName;

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::NameEquals(const PRUnichar *aName, bool *_retval)
{
    NS_ENSURE_ARG_POINTER(aName);
    NS_ENSURE_ARG_POINTER(_retval);
    if(mDocShell)
    {
        return mDocShell->NameEquals(aName, _retval);
    }
    else
        *_retval = mInitInfo->name.Equals(aName);

    return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetItemType(int32_t* aItemType)
{
   NS_ENSURE_ARG_POINTER(aItemType);

   *aItemType = mContentType;
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetItemType(int32_t aItemType)
{
    NS_ENSURE_TRUE((aItemType == typeContentWrapper || aItemType == typeChromeWrapper), NS_ERROR_FAILURE);
    mContentType = aItemType;
    if (mDocShell)
        mDocShell->SetItemType(mContentType == typeChromeWrapper
                                   ? static_cast<int32_t>(typeChrome)
                                   : static_cast<int32_t>(typeContent));
    return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetParent(nsIDocShellTreeItem** aParent)
{
   *aParent = nullptr;
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetSameTypeParent(nsIDocShellTreeItem** aParent)
{
   *aParent = nullptr;

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetRootTreeItem(nsIDocShellTreeItem** aRootTreeItem)
{
   NS_ENSURE_ARG_POINTER(aRootTreeItem);
   *aRootTreeItem = static_cast<nsIDocShellTreeItem*>(this);

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
   *aRootTreeItem = static_cast<nsIDocShellTreeItem*>(this);

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
   nsISupports* aRequestor, nsIDocShellTreeItem* aOriginalRequestor,
   nsIDocShellTreeItem **_retval)
{
   NS_ENSURE_STATE(mDocShell);
   NS_ASSERTION(mDocShellTreeOwner, "This should always be set when in this situation");

   return mDocShell->FindItemWithName(aName, 
      static_cast<nsIDocShellTreeOwner*>(mDocShellTreeOwner),
      aOriginalRequestor, _retval);
}

NS_IMETHODIMP nsWebBrowser::GetTreeOwner(nsIDocShellTreeOwner** aTreeOwner)
{  
    NS_ENSURE_ARG_POINTER(aTreeOwner);
    *aTreeOwner = nullptr;
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

//*****************************************************************************
// nsWebBrowser::nsIDocShellTreeItem
//*****************************************************************************

NS_IMETHODIMP nsWebBrowser::GetChildCount(int32_t * aChildCount)
{
    NS_ENSURE_ARG_POINTER(aChildCount);
    *aChildCount = 0;
    return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::AddChild(nsIDocShellTreeItem * aChild)
{
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsWebBrowser::RemoveChild(nsIDocShellTreeItem * aChild)
{
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsWebBrowser::GetChildAt(int32_t aIndex,
                                       nsIDocShellTreeItem ** aChild)
{
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsWebBrowser::FindChildWithName(
                                       const PRUnichar * aName,
                                       bool aRecurse, bool aSameType,
                                       nsIDocShellTreeItem * aRequestor,
                                       nsIDocShellTreeItem * aOriginalRequestor,
                                       nsIDocShellTreeItem ** _retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    *_retval = nullptr;
    return NS_OK;
}

//*****************************************************************************
// nsWebBrowser::nsIWebNavigation
//*****************************************************************************

NS_IMETHODIMP nsWebBrowser::GetCanGoBack(bool* aCanGoBack)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->GetCanGoBack(aCanGoBack);
}

NS_IMETHODIMP nsWebBrowser::GetCanGoForward(bool* aCanGoForward)
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
                                    uint32_t aLoadFlags,
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

NS_IMETHODIMP nsWebBrowser::Reload(uint32_t aReloadFlags)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->Reload(aReloadFlags);
}

NS_IMETHODIMP nsWebBrowser::GotoIndex(int32_t aIndex)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsNav->GotoIndex(aIndex);
}

NS_IMETHODIMP nsWebBrowser::Stop(uint32_t aStopFlags)
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
NS_IMETHODIMP nsWebBrowser::SetProperty(uint32_t aId, uint32_t aValue)
{
    nsresult rv = NS_OK;
    
    switch (aId)
    {
    case nsIWebBrowserSetup::SETUP_ALLOW_PLUGINS:
        {
           NS_ENSURE_STATE(mDocShell);
           NS_ENSURE_TRUE((aValue == true || aValue == false), NS_ERROR_INVALID_ARG);
           mDocShell->SetAllowPlugins(!!aValue);
        }
        break;
    case nsIWebBrowserSetup::SETUP_ALLOW_JAVASCRIPT:
        {
           NS_ENSURE_STATE(mDocShell);
           NS_ENSURE_TRUE((aValue == true || aValue == false), NS_ERROR_INVALID_ARG);
           mDocShell->SetAllowJavascript(!!aValue);
        }
        break;
    case nsIWebBrowserSetup::SETUP_ALLOW_META_REDIRECTS:
        {
           NS_ENSURE_STATE(mDocShell);
           NS_ENSURE_TRUE((aValue == true || aValue == false), NS_ERROR_INVALID_ARG);
           mDocShell->SetAllowMetaRedirects(!!aValue);
        }
        break;
    case nsIWebBrowserSetup::SETUP_ALLOW_SUBFRAMES:
        {
           NS_ENSURE_STATE(mDocShell);
           NS_ENSURE_TRUE((aValue == true || aValue == false), NS_ERROR_INVALID_ARG);
           mDocShell->SetAllowSubframes(!!aValue);
        }
        break;
    case nsIWebBrowserSetup::SETUP_ALLOW_IMAGES:
        {
           NS_ENSURE_STATE(mDocShell);
           NS_ENSURE_TRUE((aValue == true || aValue == false), NS_ERROR_INVALID_ARG);
           mDocShell->SetAllowImages(!!aValue);
        }
        break;
    case nsIWebBrowserSetup::SETUP_ALLOW_DNS_PREFETCH:
        {
            NS_ENSURE_STATE(mDocShell);
            NS_ENSURE_TRUE((aValue == true || aValue == false), NS_ERROR_INVALID_ARG);
            mDocShell->SetAllowDNSPrefetch(!!aValue);
        }
        break;
    case nsIWebBrowserSetup::SETUP_USE_GLOBAL_HISTORY:
        {
           NS_ENSURE_STATE(mDocShell);
           NS_ENSURE_TRUE((aValue == true || aValue == false), NS_ERROR_INVALID_ARG);
           rv = EnableGlobalHistory(!!aValue);
           mShouldEnableHistory = aValue;
        }
        break;
    case nsIWebBrowserSetup::SETUP_FOCUS_DOC_BEFORE_CONTENT:
        {
            // obsolete
        }
        break;
    case nsIWebBrowserSetup::SETUP_IS_CHROME_WRAPPER:
        {
           NS_ENSURE_TRUE((aValue == true || aValue == false), NS_ERROR_INVALID_ARG);
           SetItemType(aValue ? static_cast<int32_t>(typeChromeWrapper)
                              : static_cast<int32_t>(typeContentWrapper));
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
NS_IMETHODIMP nsWebBrowser::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, uint32_t aStateFlags, nsresult aStatus)
{
    if (mPersist)
    {
        mPersist->GetCurrentState(&mPersistCurrentState);
    }
    if (aStateFlags & STATE_IS_NETWORK && aStateFlags & STATE_STOP)
    {
        mPersist = nullptr;
    }
    if (mProgressListener)
    {
        return mProgressListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);
    }
    return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP nsWebBrowser::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, int32_t aCurSelfProgress, int32_t aMaxSelfProgress, int32_t aCurTotalProgress, int32_t aMaxTotalProgress)
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

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location, in unsigned long aFlags); */
NS_IMETHODIMP nsWebBrowser::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location, uint32_t aFlags)
{
    if (mProgressListener)
    {
        return mProgressListener->OnLocationChange(aWebProgress, aRequest, location, aFlags);
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
NS_IMETHODIMP nsWebBrowser::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, uint32_t state)
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
NS_IMETHODIMP nsWebBrowser::GetPersistFlags(uint32_t *aPersistFlags)
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
NS_IMETHODIMP nsWebBrowser::SetPersistFlags(uint32_t aPersistFlags)
{
    nsresult rv = NS_OK;
    mPersistFlags = aPersistFlags;
    if (mPersist)
    {
        rv = mPersist->SetPersistFlags(mPersistFlags);
        mPersist->GetPersistFlags(&mPersistFlags);
    }
    return rv;
}


/* readonly attribute unsigned long currentState; */
NS_IMETHODIMP nsWebBrowser::GetCurrentState(uint32_t *aCurrentState)
{
    NS_ENSURE_ARG_POINTER(aCurrentState);
    if (mPersist)
    {
        mPersist->GetCurrentState(&mPersistCurrentState);
    }
    *aCurrentState = mPersistCurrentState;
    return NS_OK;
}

/* readonly attribute nsresult result; */
NS_IMETHODIMP nsWebBrowser::GetResult(nsresult *aResult)
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
   in nsISupports aFile, in nsILoadContext aPrivacyContext); */
NS_IMETHODIMP nsWebBrowser::SaveURI(
    nsIURI *aURI, nsISupports *aCacheKey, nsIURI *aReferrer, nsIInputStream *aPostData,
    const char *aExtraHeaders, nsISupports *aFile, nsILoadContext* aPrivacyContext)
{
    return SavePrivacyAwareURI(aURI, aCacheKey, aReferrer, aPostData, aExtraHeaders,
                               aFile, aPrivacyContext && aPrivacyContext->UsePrivateBrowsing());
}

NS_IMETHODIMP nsWebBrowser::SavePrivacyAwareURI(
    nsIURI *aURI, nsISupports *aCacheKey, nsIURI *aReferrer, nsIInputStream *aPostData,
    const char *aExtraHeaders, nsISupports *aFile, bool aIsPrivate)
{
    if (mPersist)
    {
        uint32_t currentState;
        mPersist->GetCurrentState(&currentState);
        if (currentState == PERSIST_STATE_FINISHED)
        {
            mPersist = nullptr;
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
    NS_ENSURE_SUCCESS(rv, rv);
    mPersist->SetProgressListener(this);
    mPersist->SetPersistFlags(mPersistFlags);
    mPersist->GetCurrentState(&mPersistCurrentState);
    rv = mPersist->SavePrivacyAwareURI(uri, aCacheKey, aReferrer, aPostData,
                                       aExtraHeaders, aFile, aIsPrivate);
    if (NS_FAILED(rv))
    {
        mPersist = nullptr;
    }
    return rv;
}

/* void saveChannel (in nsIChannel aChannel, in nsISupports aFile); */
NS_IMETHODIMP nsWebBrowser::SaveChannel(
    nsIChannel* aChannel, nsISupports *aFile)
{
    if (mPersist)
    {
        uint32_t currentState;
        mPersist->GetCurrentState(&currentState);
        if (currentState == PERSIST_STATE_FINISHED)
        {
            mPersist = nullptr;
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
    NS_ENSURE_SUCCESS(rv, rv);
    mPersist->SetProgressListener(this);
    mPersist->SetPersistFlags(mPersistFlags);
    mPersist->GetCurrentState(&mPersistCurrentState);
    rv = mPersist->SaveChannel(aChannel, aFile);
    if (NS_FAILED(rv))
    {
        mPersist = nullptr;
    }
    return rv;
}

/* void saveDocument (in nsIDOMDocument document, in nsISupports aFile, in nsISupports aDataPath); */
NS_IMETHODIMP nsWebBrowser::SaveDocument(
    nsIDOMDocument *aDocument, nsISupports *aFile, nsISupports *aDataPath,
    const char *aOutputContentType, uint32_t aEncodingFlags, uint32_t aWrapColumn)
{
    if (mPersist)
    {
        uint32_t currentState;
        mPersist->GetCurrentState(&currentState);
        if (currentState == PERSIST_STATE_FINISHED)
        {
            mPersist = nullptr;
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
    NS_ENSURE_SUCCESS(rv, rv);
    mPersist->SetProgressListener(this);
    mPersist->SetPersistFlags(mPersistFlags);
    mPersist->GetCurrentState(&mPersistCurrentState);
    rv = mPersist->SaveDocument(doc, aFile, aDataPath, aOutputContentType, aEncodingFlags, aWrapColumn);
    if (NS_FAILED(rv))
    {
        mPersist = nullptr;
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

/* void cancel(nsresult aReason); */
NS_IMETHODIMP nsWebBrowser::Cancel(nsresult aReason)
{
    if (mPersist)
    {
        return mPersist->Cancel(aReason);
    }
    return NS_OK;
}




//*****************************************************************************
// nsWebBrowser::nsIBaseWindow
//*****************************************************************************

NS_IMETHODIMP nsWebBrowser::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* aParentWidget, int32_t aX, int32_t aY, int32_t aCX, int32_t aCY)   
{
   NS_ENSURE_ARG(aParentNativeWindow || aParentWidget);
   NS_ENSURE_STATE(!mDocShell || mInitInfo);

   if(aParentWidget)
      NS_ENSURE_SUCCESS(SetParentWidget(aParentWidget), NS_ERROR_FAILURE);
   else
      NS_ENSURE_SUCCESS(SetParentNativeWindow(aParentNativeWindow),
         NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(SetPositionAndSize(aX, aY, aCX, aCY, false),
      NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::Create()
{
   NS_ENSURE_STATE(!mDocShell && (mParentNativeWindow || mParentWidget));

    nsresult rv = EnsureDocShellTreeOwner();
    NS_ENSURE_SUCCESS(rv, rv);

   nsCOMPtr<nsIWidget> docShellParentWidget(mParentWidget);
   if(!mParentWidget) // We need to create a widget
      {
      // Create the widget
        mInternalWidget = do_CreateInstance(kChildCID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

      docShellParentWidget = mInternalWidget;
      nsWidgetInitData  widgetInit;

      widgetInit.clipChildren = true;

      widgetInit.mWindowType = eWindowType_child;
      nsIntRect bounds(mInitInfo->x, mInitInfo->y, mInitInfo->cx, mInitInfo->cy);
      
      mInternalWidget->SetWidgetListener(this);
      mInternalWidget->Create(nullptr, mParentNativeWindow, bounds, nullptr, &widgetInit);
      }

    nsCOMPtr<nsIDocShell> docShell(do_CreateInstance("@mozilla.org/docshell;1", &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = SetDocShell(docShell);
    NS_ENSURE_SUCCESS(rv, rv);

    // get the system default window background colour
    LookAndFeel::GetColor(LookAndFeel::eColorID_WindowBackground,
                          &mBackgroundColor);

   // the docshell has been set so we now have our listener registrars.
   if (mListenerArray) {
      // we had queued up some listeners, let's register them now.
      uint32_t count = mListenerArray->Length();
      uint32_t i = 0;
      NS_ASSERTION(count > 0, "array construction problem");
      while (i < count) {
          nsWebBrowserListenerState *state = mListenerArray->ElementAt(i);
          NS_ASSERTION(state, "array construction problem");
          nsCOMPtr<nsISupports> listener = do_QueryReferent(state->mWeakPtr);
          NS_ASSERTION(listener, "bad listener");
          (void)BindListener(listener, state->mID);
          i++;
      }
      for (uint32_t i = 0, end = mListenerArray->Length(); i < end; i++) {
         nsWebBrowserListenerState *state = mListenerArray->ElementAt(i);
         delete state;
      }
      delete mListenerArray;
      mListenerArray = nullptr;
   }

   // HACK ALERT - this registration registers the nsDocShellTreeOwner as a 
   // nsIWebBrowserListener so it can setup its MouseListener in one of the 
   // progress callbacks. If we can register the MouseListener another way, this 
   // registration can go away, and nsDocShellTreeOwner can stop implementing
   // nsIWebProgressListener.
   nsCOMPtr<nsISupports> supports = nullptr;
   (void)mDocShellTreeOwner->QueryInterface(NS_GET_IID(nsIWebProgressListener),
                             static_cast<void**>(getter_AddRefs(supports)));
   (void)BindListener(supports, NS_GET_IID(nsIWebProgressListener));

   NS_ENSURE_SUCCESS(mDocShellAsWin->InitWindow(nullptr,
      docShellParentWidget, mInitInfo->x, mInitInfo->y, mInitInfo->cx,
      mInitInfo->cy), NS_ERROR_FAILURE);

   mDocShell->SetName(mInitInfo->name);
   if (mContentType == typeChromeWrapper)
   {
       mDocShell->SetItemType(nsIDocShellTreeItem::typeChrome);
   }
   else
   {
       mDocShell->SetItemType(nsIDocShellTreeItem::typeContent);
   }
   mDocShell->SetTreeOwner(mDocShellTreeOwner);
   
   // If the webbrowser is a content docshell item then we won't hear any
   // events from subframes. To solve that we install our own chrome event handler
   // that always gets called (even for subframes) for any bubbling event.

    if (!mInitInfo->sessionHistory) {
        mInitInfo->sessionHistory = do_CreateInstance(NS_SHISTORY_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
    }
   mDocShellAsNav->SetSessionHistory(mInitInfo->sessionHistory);

   if (XRE_GetProcessType() == GeckoProcessType_Default) {
       // Hook up global history. Do not fail if we can't - just warn.
       rv = EnableGlobalHistory(mShouldEnableHistory);
       NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "EnableGlobalHistory() failed");
   }

   NS_ENSURE_SUCCESS(mDocShellAsWin->Create(), NS_ERROR_FAILURE);

    // Hook into the OnSecurityChange() notification for lock/unlock icon
   // updates
   nsCOMPtr<nsIDOMWindow> domWindow;
   rv = GetContentDOMWindow(getter_AddRefs(domWindow));
   if (NS_SUCCEEDED(rv))
   {
       // this works because the implementation of nsISecureBrowserUI
       // (nsSecureBrowserUIImpl) gets a docShell from the domWindow,
       // and calls docShell->SetSecurityUI(this);
       nsCOMPtr<nsISecureBrowserUI> securityUI =
           do_CreateInstance(NS_SECURE_BROWSER_UI_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv))
            securityUI->Init(domWindow);
   }

   mDocShellTreeOwner->AddToWatcher(); // evil twin of Remove in SetDocShell(0)
   mDocShellTreeOwner->AddChromeListeners();

   delete mInitInfo;
   mInitInfo = nullptr;

   return NS_OK; 
}

NS_IMETHODIMP nsWebBrowser::Destroy()
{
   InternalDestroy();

   if(!mInitInfo)
      mInitInfo = new nsWebBrowserInitInfo();

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::GetUnscaledDevicePixelsPerCSSPixel(double *aScale)
{
  *aScale = mParentWidget ? mParentWidget->GetDefaultScale() : 1.0;
  return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetPosition(int32_t aX, int32_t aY)
{
   int32_t cx = 0;
   int32_t cy = 0;

   GetSize(&cx, &cy);

   return SetPositionAndSize(aX, aY, cx, cy, false);
}

NS_IMETHODIMP nsWebBrowser::GetPosition(int32_t* aX, int32_t* aY)
{
   return GetPositionAndSize(aX, aY, nullptr, nullptr);
}

NS_IMETHODIMP nsWebBrowser::SetSize(int32_t aCX, int32_t aCY, bool aRepaint)
{
   int32_t x = 0;
   int32_t y = 0;

   GetPosition(&x, &y);

   return SetPositionAndSize(x, y, aCX, aCY, aRepaint);
}

NS_IMETHODIMP nsWebBrowser::GetSize(int32_t* aCX, int32_t* aCY)
{
   return GetPositionAndSize(nullptr, nullptr, aCX, aCY);
}

NS_IMETHODIMP nsWebBrowser::SetPositionAndSize(int32_t aX, int32_t aY,
   int32_t aCX, int32_t aCY, bool aRepaint)
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
      int32_t doc_x = aX;
      int32_t doc_y = aY;

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

NS_IMETHODIMP nsWebBrowser::GetPositionAndSize(int32_t* aX, int32_t* aY, 
   int32_t* aCX, int32_t* aCY)
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
         nsIntRect bounds;
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

NS_IMETHODIMP nsWebBrowser::Repaint(bool aForce)
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
      mParentNativeWindow = nullptr;

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

NS_IMETHODIMP nsWebBrowser::GetNativeHandle(nsAString& aNativeHandle)
{
   // the nativeHandle should be accessed from nsIXULWindow
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsWebBrowser::GetVisibility(bool* visibility)
{
   NS_ENSURE_ARG_POINTER(visibility);

   if(!mDocShell)
      *visibility = mInitInfo->visible;
   else
      NS_ENSURE_SUCCESS(mDocShellAsWin->GetVisibility(visibility), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetVisibility(bool aVisibility)
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

NS_IMETHODIMP nsWebBrowser::GetEnabled(bool* aEnabled)
{
  if (mInternalWidget) {
    *aEnabled = mInternalWidget->IsEnabled();
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowser::SetEnabled(bool aEnabled)
{
  if (mInternalWidget)
    return mInternalWidget->Enable(aEnabled);
  return NS_ERROR_FAILURE;
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
  nsCOMPtr<nsIDOMWindow> window = do_GetInterface(mDocShell);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  return fm ? fm->SetFocusedWindow(window) : NS_OK;
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

NS_IMETHODIMP nsWebBrowser::GetCurScrollPos(int32_t aScrollOrientation, 
   int32_t* aCurPos)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->GetCurScrollPos(aScrollOrientation, aCurPos);
}

NS_IMETHODIMP nsWebBrowser::SetCurScrollPos(int32_t aScrollOrientation, 
   int32_t aCurPos)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->SetCurScrollPos(aScrollOrientation, aCurPos);
}

NS_IMETHODIMP nsWebBrowser::SetCurScrollPosEx(int32_t aCurHorizontalPos, 
   int32_t aCurVerticalPos)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->SetCurScrollPosEx(aCurHorizontalPos, 
      aCurVerticalPos);
}

NS_IMETHODIMP nsWebBrowser::GetScrollRange(int32_t aScrollOrientation,
   int32_t* aMinPos, int32_t* aMaxPos)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->GetScrollRange(aScrollOrientation, aMinPos,
      aMaxPos);
}

NS_IMETHODIMP nsWebBrowser::SetScrollRange(int32_t aScrollOrientation,
   int32_t aMinPos, int32_t aMaxPos)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->SetScrollRange(aScrollOrientation, aMinPos,
      aMaxPos);
}

NS_IMETHODIMP nsWebBrowser::SetScrollRangeEx(int32_t aMinHorizontalPos,
   int32_t aMaxHorizontalPos, int32_t aMinVerticalPos, int32_t aMaxVerticalPos)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->SetScrollRangeEx(aMinHorizontalPos,
      aMaxHorizontalPos, aMinVerticalPos, aMaxVerticalPos);
}

NS_IMETHODIMP nsWebBrowser::GetDefaultScrollbarPreferences(int32_t aScrollOrientation,
   int32_t* aScrollbarPref)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->GetDefaultScrollbarPreferences(aScrollOrientation,
      aScrollbarPref);
}

NS_IMETHODIMP nsWebBrowser::SetDefaultScrollbarPreferences(int32_t aScrollOrientation,
   int32_t aScrollbarPref)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->SetDefaultScrollbarPreferences(aScrollOrientation,
      aScrollbarPref);
}

NS_IMETHODIMP nsWebBrowser::GetScrollbarVisibility(bool* aVerticalVisible,
   bool* aHorizontalVisible)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsScrollable->GetScrollbarVisibility(aVerticalVisible,
      aHorizontalVisible);
}

//*****************************************************************************
// nsWebBrowser::nsITextScroll
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowser::ScrollByLines(int32_t aNumLines)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsTextScroll->ScrollByLines(aNumLines);
}

NS_IMETHODIMP nsWebBrowser::ScrollByPages(int32_t aNumPages)
{
   NS_ENSURE_STATE(mDocShell);

   return mDocShellAsTextScroll->ScrollByPages(aNumPages);
}


//*****************************************************************************
// nsWebBrowser: Listener Helpers
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowser::SetDocShell(nsIDocShell* aDocShell)
{
     nsCOMPtr<nsIDocShell> kungFuDeathGrip(mDocShell);
     if(aDocShell)
     {
         NS_ENSURE_TRUE(!mDocShell, NS_ERROR_FAILURE);
 
         nsCOMPtr<nsIInterfaceRequestor> req(do_QueryInterface(aDocShell));
         nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(aDocShell));
         nsCOMPtr<nsIWebNavigation> nav(do_QueryInterface(aDocShell));
         nsCOMPtr<nsIScrollable> scrollable(do_QueryInterface(aDocShell));
         nsCOMPtr<nsITextScroll> textScroll(do_QueryInterface(aDocShell));
         nsCOMPtr<nsIWebProgress> progress(do_GetInterface(aDocShell));
         NS_ENSURE_TRUE(req && baseWin && nav && scrollable && textScroll && progress,
             NS_ERROR_FAILURE);
 
         mDocShell = aDocShell;
         mDocShellAsReq = req;
         mDocShellAsWin = baseWin;
         mDocShellAsNav = nav;
         mDocShellAsScrollable = scrollable;
         mDocShellAsTextScroll = textScroll;
         mWebProgress = progress;

         // By default, do not allow DNS prefetch, so we don't break our frozen
         // API.  Embeddors who decide to enable it should do so manually.
         mDocShell->SetAllowDNSPrefetch(false);

         // It's possible to call setIsActive() on us before we have a docshell.
         // If we're getting a docshell now, pass along our desired value. The
         // default here (true) matches the default of the docshell, so this is
         // a no-op unless setIsActive(false) has been called on us.
         mDocShell->SetIsActive(mIsActive);
     }
     else
     {
         if (mDocShellTreeOwner)
           mDocShellTreeOwner->RemoveFromWatcher(); // evil twin of Add in Create()
         if (mDocShellAsWin)
           mDocShellAsWin->Destroy();

         mDocShell = nullptr;
         mDocShellAsReq = nullptr;
         mDocShellAsWin = nullptr;
         mDocShellAsNav = nullptr;
         mDocShellAsScrollable = nullptr;
         mDocShellAsTextScroll = nullptr;
         mWebProgress = nullptr;
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

static void DrawThebesLayer(ThebesLayer* aLayer,
                            gfxContext* aContext,
                            const nsIntRegion& aRegionToDraw,
                            const nsIntRegion& aRegionToInvalidate,
                            void* aCallbackData)
{
  nscolor* color = static_cast<nscolor*>(aCallbackData);
  aContext->NewPath();
  aContext->SetColor(gfxRGBA(*color));
  nsIntRect dirtyRect = aRegionToDraw.GetBounds();
  aContext->Rectangle(gfxRect(dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height));
  aContext->Fill();  
}

void nsWebBrowser::WindowRaised(nsIWidget* aWidget)
{
#if defined(DEBUG_smaug)
  nsCOMPtr<nsIDOMDocument> domDocument = do_GetInterface(mDocShell);
  nsAutoString documentURI;
  domDocument->GetDocumentURI(documentURI);
  printf("nsWebBrowser::NS_ACTIVATE %p %s\n", (void*)this,
         NS_ConvertUTF16toUTF8(documentURI).get());
#endif
  Activate();
}

void nsWebBrowser::WindowLowered(nsIWidget* aWidget)
{
#if defined(DEBUG_smaug)
  nsCOMPtr<nsIDOMDocument> domDocument = do_GetInterface(mDocShell);
  nsAutoString documentURI;
  domDocument->GetDocumentURI(documentURI);
  printf("nsWebBrowser::NS_DEACTIVATE %p %s\n", (void*)this,
         NS_ConvertUTF16toUTF8(documentURI).get());
#endif
  Deactivate();
}

bool nsWebBrowser::PaintWindow(nsIWidget* aWidget, nsIntRegion aRegion)
{
  LayerManager* layerManager = aWidget->GetLayerManager();
  NS_ASSERTION(layerManager, "Must be in paint event");

  layerManager->BeginTransaction();
  nsRefPtr<ThebesLayer> root = layerManager->CreateThebesLayer();
  if (root) {
    nsIntRect dirtyRect = aRegion.GetBounds();
    root->SetVisibleRegion(dirtyRect);
    layerManager->SetRoot(root);
  }

  layerManager->EndTransaction(DrawThebesLayer, &mBackgroundColor);
  return true;
}

NS_IMETHODIMP nsWebBrowser::GetPrimaryContentWindow(nsIDOMWindow** aDOMWindow)
{
  *aDOMWindow = 0;

  nsCOMPtr<nsIDocShellTreeItem> item;
  NS_ENSURE_TRUE(mDocShellTreeOwner, NS_ERROR_FAILURE);
  mDocShellTreeOwner->GetPrimaryContentShell(getter_AddRefs(item));
  NS_ENSURE_TRUE(item, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShell> docShell;
  docShell = do_QueryInterface(item);
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIDOMWindow> domWindow = do_GetInterface(docShell);
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
  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  nsCOMPtr<nsIDOMWindow> window = do_GetInterface(mDocShell);
  if (fm && window)
    return fm->WindowRaised(window);
  return NS_OK;
}

/* void deactivate (); */
NS_IMETHODIMP nsWebBrowser::Deactivate(void)
{
  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  nsCOMPtr<nsIDOMWindow> window = do_GetInterface(mDocShell);
  if (fm && window)
    return fm->WindowLowered(window);
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
  *aFocusedWindow = nullptr;

  nsCOMPtr<nsIDOMWindow> window = do_GetInterface(mDocShell);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMElement> focusedElement;
  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  return fm ? fm->GetFocusedElementForWindow(window, true, aFocusedWindow,
                                             getter_AddRefs(focusedElement)) : NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetFocusedWindow(nsIDOMWindow * aFocusedWindow)
{
  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  return fm ? fm->SetFocusedWindow(aFocusedWindow) : NS_OK;
}

/* attribute nsIDOMElement focusedElement; */
NS_IMETHODIMP nsWebBrowser::GetFocusedElement(nsIDOMElement * *aFocusedElement)
{
  NS_ENSURE_ARG_POINTER(aFocusedElement);

  nsCOMPtr<nsIDOMWindow> window = do_GetInterface(mDocShell);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  return fm ? fm->GetFocusedElementForWindow(window, true, nullptr, aFocusedElement) : NS_OK;
}

NS_IMETHODIMP nsWebBrowser::SetFocusedElement(nsIDOMElement * aFocusedElement)
{
  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  return fm ? fm->SetFocus(aFocusedElement, 0) : NS_OK;
}

//*****************************************************************************
// nsWebBrowser::nsIWebBrowserStream
//*****************************************************************************   

/* void openStream(in nsIURI aBaseURI, in ACString aContentType); */
NS_IMETHODIMP nsWebBrowser::OpenStream(nsIURI *aBaseURI, const nsACString& aContentType)
{
  nsresult rv;

  if (!mStream) {
    mStream = new nsEmbedStream();
        if (!mStream)
             return NS_ERROR_OUT_OF_MEMORY;

    mStreamGuard = do_QueryInterface(mStream);
    mStream->InitOwner(this);
    rv = mStream->Init();
    if (NS_FAILED(rv))
      return rv;
  }

  return mStream->OpenStream(aBaseURI, aContentType);
}

/* void appendToStream([const, array, size_is(aLen)] in octet aData,
 * in unsigned long aLen); */
NS_IMETHODIMP nsWebBrowser::AppendToStream(const uint8_t *aData, uint32_t aLen)
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
