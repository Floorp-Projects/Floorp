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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsIComponentManager.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDocumentViewer.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIPluginHost.h"
#include "nsCURILoader.h"
#include "nsLayoutCID.h"
#include "nsNetUtil.h"
#include "nsRect.h"
#include "prprf.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsXPIDLString.h"
#include "nsIChromeEventHandler.h"
#include "nsIDOMWindow.h"
#include "nsIWebBrowserChrome.h"
#include "nsPoint.h"
#include "nsGfxCIID.h"
#include "nsIPrompt.h"
#include "nsTextFormatter.h"
#include "nsIHTTPEventSink.h"

// Local Includes
#include "nsDocShell.h"
#include "nsDocShellLoadInfo.h"

// Helper Classes
#include "nsDOMError.h"
#include "nsEscape.h"
#include "nsHTTPEnums.h"

// Interfaces Needed
#include "nsICharsetConverterManager.h"
#include "nsIHTTPChannel.h"
#include "nsIDataChannel.h"
#include "nsIProgressEventSink.h"
#include "nsIWebProgress.h"
#include "nsILayoutHistoryState.h"
#include "nsILocaleService.h"
#include "nsIPlatformCharset.h"
#include "nsITimer.h"
#include "nsIFileStream.h"

#include "nsIPrincipal.h"

// For reporting errors with the console service.
// These can go away if error reporting is propagated up past nsDocShell.
#include "nsIConsoleService.h"
#include "nsIScriptError.h"

// used to dispatch urls to default protocol handlers
#include "nsCExternalHandlerService.h"
#include "nsIExternalProtocolService.h"

static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_CID(kPlatformCharsetCID, NS_PLATFORMCHARSET_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kSimpleURICID,            NS_SIMPLEURI_CID);
static NS_DEFINE_CID(kDocumentCharsetInfoCID, NS_DOCUMENTCHARSETINFO_CID);
static NS_DEFINE_CID(kPluginManagerCID, NS_PLUGINMANAGER_CID);

//*****************************************************************************
//***    nsDocShell: Object Management
//*****************************************************************************

nsDocShell::nsDocShell() : 
  mContentListener(nsnull),
  mInitInfo(nsnull), 
  mMarginWidth(0), 
  mMarginHeight(0),
  mItemType(typeContent),
  mCurrentScrollbarPref(-1,-1),
  mDefaultScrollbarPref(-1,-1),
  mInitialPageLoad(PR_TRUE),
  mAllowPlugins(PR_TRUE),
  mViewMode(viewNormal),
  mEODForCurrentDocument (PR_FALSE),
  mUseExternalProtocolHandler (PR_FALSE),
  mParent(nsnull),
  mTreeOwner(nsnull),
  mChromeEventHandler(nsnull)
{
  NS_INIT_REFCNT();
}

nsDocShell::~nsDocShell()
{
   Destroy();
}

NS_IMETHODIMP nsDocShell::DestroyChildren()
{
   PRInt32 i, n = mChildren.Count();
   nsCOMPtr<nsIDocShellTreeItem>   shell;
   for (i = 0; i < n; i++) 
      {
      shell = dont_AddRef((nsIDocShellTreeItem*)mChildren.ElementAt(i));
      if(!NS_WARN_IF_FALSE(shell, "docshell has null child"))
         shell->SetParent(nsnull);
      nsCOMPtr<nsIBaseWindow> shellWin(do_QueryInterface(shell));
      if(shellWin)
         shellWin->Destroy();
      }
   mChildren.Clear();
   return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsISupports
//*****************************************************************************   

NS_IMPL_THREADSAFE_ADDREF(nsDocShell)
NS_IMPL_THREADSAFE_RELEASE(nsDocShell)

NS_INTERFACE_MAP_BEGIN(nsDocShell)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocShell)
   NS_INTERFACE_MAP_ENTRY(nsIDocShell)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeItem)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeNode)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellHistory)
   NS_INTERFACE_MAP_ENTRY(nsIWebNavigation)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
   NS_INTERFACE_MAP_ENTRY(nsIScrollable)
   NS_INTERFACE_MAP_ENTRY(nsITextScroll)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObjectOwner)
   NS_INTERFACE_MAP_ENTRY(nsIRefreshURI)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_THREADSAFE


///*****************************************************************************
// nsDocShell::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::GetInterface(const nsIID& aIID, void** aSink)
{
   NS_ENSURE_ARG_POINTER(aSink);

   if(aIID.Equals(NS_GET_IID(nsIURIContentListener)) &&
      NS_SUCCEEDED(EnsureContentListener()))
      *aSink = mContentListener;
   else if(aIID.Equals(NS_GET_IID(nsIScriptGlobalObject)) &&
      NS_SUCCEEDED(EnsureScriptEnvironment()))
      *aSink = mScriptGlobal;
   else if(aIID.Equals(NS_GET_IID(nsIDOMWindow)) &&
      NS_SUCCEEDED(EnsureScriptEnvironment()))
      {
      NS_ENSURE_SUCCESS(mScriptGlobal->QueryInterface(NS_GET_IID(nsIDOMWindow),
         aSink), NS_ERROR_FAILURE);
      return NS_OK;
      }
   else if(aIID.Equals(NS_GET_IID(nsIPrompt)))
      {
        nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mTreeOwner));
        if (prompter)
        {
            *aSink = prompter;
            NS_ADDREF((nsISupports*)*aSink);
            return NS_OK;
        }
        else
            return NS_NOINTERFACE;
      }
   else if (aIID.Equals(NS_GET_IID(nsIProgressEventSink)) || aIID.Equals(NS_GET_IID(nsIHTTPEventSink)) ||
            aIID.Equals(NS_GET_IID(nsIWebProgress)))
   {
     nsCOMPtr<nsIURILoader> uriLoader(do_GetService(NS_URI_LOADER_PROGID));
     NS_ENSURE_TRUE(uriLoader, NS_ERROR_FAILURE);
     nsCOMPtr<nsIDocumentLoader> docLoader;
     NS_ENSURE_SUCCESS(uriLoader->GetDocumentLoaderForContext(NS_STATIC_CAST(nsIDocShell*, this),
      getter_AddRefs(docLoader)), NS_ERROR_FAILURE);  
     if (docLoader) {
       nsCOMPtr<nsIInterfaceRequestor> requestor(do_QueryInterface(docLoader));
       return requestor->GetInterface(aIID, aSink);
     }
     else
       return NS_ERROR_FAILURE;
   }
   else
      return QueryInterface(aIID, aSink);

   NS_IF_ADDREF(((nsISupports*)*aSink));
   return NS_OK;   
}

//*****************************************************************************
// nsDocShell::nsIDocShell
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::LoadURI(nsIURI* aURI, nsIDocShellLoadInfo* aLoadInfo)
{
  nsresult rv;
  nsCOMPtr<nsIURI> referrer;
  nsCOMPtr<nsISupports> owner;
  nsCOMPtr<nsISHEntry> shEntry;
  nsDocShellInfoLoadType loadType = nsIDocShellLoadInfo::loadNormal;

  NS_ENSURE_ARG(aURI);

  // Extract the info from the DocShellLoadInfo struct...
  if(aLoadInfo) {
    aLoadInfo->GetReferrer(getter_AddRefs(referrer));
    aLoadInfo->GetLoadType(&loadType);
    aLoadInfo->GetOwner(getter_AddRefs(owner));
    aLoadInfo->GetSHEntry(getter_AddRefs(shEntry));
  }

  if (!shEntry && loadType != nsIDocShellLoadInfo::loadNormalReplace) {
    /* Check if we are in the middle of loading a subframe whose parent
     * was originally loaded thro' Session History. ie., you were in a frameset
     * page, went somewhere else and clicked 'back'. The loading of the root page
     * is done and we are currently loading one of its children or sub-children.
     */
    nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
    GetSameTypeParent(getter_AddRefs(parentAsItem));

    // Try to get your SHEntry from your parent
    if (parentAsItem) {
      nsCOMPtr<nsIDocShellHistory> parent(do_QueryInterface(parentAsItem));

      // XXX: Should we care if this QI fails?
      if (parent) {
        parent->GetChildSHEntry(mChildOffset, getter_AddRefs(shEntry));
        if (shEntry) {
          loadType = nsIDocShellLoadInfo::loadHistory;
        }
      }
    }
  }

  if (shEntry) {
    rv = LoadHistoryEntry(shEntry, loadType);
  } else {
    rv = InternalLoad(aURI, referrer, owner, nsnull, nsnull, loadType, nsnull);
  }

  return rv;
}

NS_IMETHODIMP nsDocShell::LoadStream(nsIInputStream *aStream, nsIURI *aURI, 
                                     const char *aContentType, PRInt32 aContentLen,
                                     nsIDocShellLoadInfo* aLoadInfo)
{
   NS_ENSURE_ARG(aStream);
   NS_ENSURE_ARG(aContentType);
   NS_ENSURE_ARG(aContentLen);

   // if the caller doesn't pass in a URI we need to create a dummy URI. necko
   // currently requires a URI in various places during the load. Some consumers
   // do as well.
   nsCOMPtr<nsIURI> uri = aURI;
   if (!uri) {
      // HACK ALERT
      nsresult rv = NS_OK;
      uri = do_CreateInstance(kSimpleURICID, &rv);
      if (NS_FAILED(rv)) return rv;
      rv = uri->SetSpec("stream");
      if (NS_FAILED(rv)) return rv;
   }

   nsDocShellInfoLoadType loadType = nsIDocShellLoadInfo::loadNormal;

   if(aLoadInfo)
      (void)aLoadInfo->GetLoadType(&loadType);

   NS_ENSURE_SUCCESS(StopLoad(), NS_ERROR_FAILURE);
   // Cancel any timers that were set for this loader.
   (void)CancelRefreshURITimers();

   mLoadType = loadType;

   // build up a channel for this stream.
   nsCOMPtr<nsIChannel> channel;
   NS_ENSURE_SUCCESS(NS_NewInputStreamChannel(getter_AddRefs(channel), aURI, aStream, 
                                              aContentType, aContentLen), NS_ERROR_FAILURE);
   
   nsCOMPtr<nsIURILoader> uriLoader(do_GetService(NS_URI_LOADER_PROGID));
   NS_ENSURE_TRUE(uriLoader, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(DoChannelLoad(channel, nsIURILoader::viewNormal, nsnull, uriLoader), NS_ERROR_FAILURE);
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::CreateLoadInfo(nsIDocShellLoadInfo** aLoadInfo)
{
   nsDocShellLoadInfo* loadInfo = new nsDocShellLoadInfo();
   NS_ENSURE_TRUE(loadInfo, NS_ERROR_OUT_OF_MEMORY);
   nsCOMPtr<nsIDocShellLoadInfo> localRef(loadInfo);

   *aLoadInfo = localRef;
   NS_ADDREF(*aLoadInfo);
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::StopLoad()
{
   // Cancel any timers that were set for this loader.
   CancelRefreshURITimers();

   if(mLoadCookie)
      {
      nsCOMPtr<nsIURILoader> uriLoader = do_GetService(NS_URI_LOADER_PROGID);
      if(uriLoader)
         uriLoader->Stop(mLoadCookie);
      }

   PRInt32 n;
   PRInt32 count = mChildren.Count();
   for(n = 0; n < count; n++)
      {
      nsIDocShellTreeItem* shellItem = (nsIDocShellTreeItem*)mChildren.ElementAt(n);
      nsCOMPtr<nsIDocShell> shell(do_QueryInterface(shellItem));
      if(shell)
         shell->StopLoad();
      }

   return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetDocument(nsIDOMDocument *aDOMDoc, nsIDOMElement *aRootNode)
{
  /* XXX: This method is obsolete and will be removed. */
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::GetCurrentURI(nsIURI** aURI)
{
   NS_ENSURE_ARG_POINTER(aURI);
   
   *aURI = mCurrentURI;
   NS_IF_ADDREF(*aURI);

   return NS_OK;
}

NS_IMETHODIMP 
nsDocShell::GetDocLoaderObserver(nsIDocumentLoaderObserver * *aDocLoaderObserver)
{
  NS_ENSURE_ARG_POINTER(aDocLoaderObserver);

  *aDocLoaderObserver = mDocLoaderObserver;
  NS_IF_ADDREF(*aDocLoaderObserver);
  return NS_OK;
}

NS_IMETHODIMP 
nsDocShell::SetDocLoaderObserver(nsIDocumentLoaderObserver * aDocLoaderObserver)
{
  // it's legal for aDocLoaderObserver to be null.  
  mDocLoaderObserver = aDocLoaderObserver;
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetPresContext(nsIPresContext** aPresContext)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG_POINTER(aPresContext);
  *aPresContext = nsnull;

  if (mContentViewer) {
    nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(mContentViewer));

    if (docv) {
      rv = docv->GetPresContext(*aPresContext);
    }
  }

  // Fail silently, if no PresContext is available...
  return rv;
}

NS_IMETHODIMP nsDocShell::GetPresShell(nsIPresShell** aPresShell)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG_POINTER(aPresShell);
  *aPresShell = nsnull;
   
  nsCOMPtr<nsIPresContext> presContext;
  (void) GetPresContext(getter_AddRefs(presContext));

  if(presContext) {
    rv = presContext->GetShell(aPresShell);
  }

  return rv;
}

NS_IMETHODIMP nsDocShell::GetContentViewer(nsIContentViewer** aContentViewer)
{
   NS_ENSURE_ARG_POINTER(aContentViewer);

   *aContentViewer = mContentViewer;
   NS_IF_ADDREF(*aContentViewer);
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetChromeEventHandler(nsIChromeEventHandler* aChromeEventHandler)
{
   // Weak reference. Don't addref.
   mChromeEventHandler = aChromeEventHandler;
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetChromeEventHandler(nsIChromeEventHandler** aChromeEventHandler)
{
   NS_ENSURE_ARG_POINTER(aChromeEventHandler);

   *aChromeEventHandler = mChromeEventHandler;
   NS_IF_ADDREF(*aChromeEventHandler);
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetParentURIContentListener(nsIURIContentListener**
   aParent)
{
   NS_ENSURE_ARG_POINTER(aParent);
   NS_ENSURE_SUCCESS(EnsureContentListener(), NS_ERROR_FAILURE);

   return mContentListener->GetParentContentListener(aParent);
}

NS_IMETHODIMP nsDocShell::SetParentURIContentListener(nsIURIContentListener*
   aParent)
{
   NS_ENSURE_SUCCESS(EnsureContentListener(), NS_ERROR_FAILURE);

   return mContentListener->SetParentContentListener(aParent);
}

NS_IMETHODIMP nsDocShell::GetDocumentCharsetInfo(nsIDocumentCharsetInfo** 
   aDocumentCharsetInfo)
{
   NS_ENSURE_ARG_POINTER(aDocumentCharsetInfo);

  // if the mDocumentCharsetInfo does not exist already, we create it now
  if (!mDocumentCharsetInfo) {
    nsresult res = nsComponentManager::CreateInstance(kDocumentCharsetInfoCID, 
      NULL, NS_GET_IID(nsIDocumentCharsetInfo), 
      getter_AddRefs(mDocumentCharsetInfo));
    if (NS_FAILED(res)) return NS_ERROR_FAILURE;
  }

   *aDocumentCharsetInfo = mDocumentCharsetInfo;
   NS_IF_ADDREF(*aDocumentCharsetInfo);
   return NS_OK;
} 

NS_IMETHODIMP nsDocShell::SetDocumentCharsetInfo(nsIDocumentCharsetInfo* 
   aDocumentCharsetInfo)
{
   mDocumentCharsetInfo = aDocumentCharsetInfo;
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetAllowPlugins(PRBool* aAllowPlugins)
{
   NS_ENSURE_ARG_POINTER(aAllowPlugins);

   *aAllowPlugins = mAllowPlugins;
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetAllowPlugins(PRBool aAllowPlugins)
{
   mAllowPlugins = aAllowPlugins;
   //XXX should enable or disable a plugin host
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetViewMode(PRInt32* aViewMode)
{
   NS_ENSURE_ARG_POINTER(aViewMode);

   *aViewMode = mViewMode;
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetViewMode(PRInt32 aViewMode)
{
   NS_ENSURE_ARG((viewNormal == aViewMode) || (viewSource == aViewMode));

   PRBool reload = PR_FALSE;

   if((mViewMode != aViewMode) && mCurrentURI)
      reload = PR_TRUE;

   mViewMode = aViewMode;

   if(reload)
	   Reload(nsIDocShellLoadInfo::loadReloadNormal);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetZoom(float* zoom)
{
   NS_ENSURE_ARG_POINTER(zoom);
   NS_ENSURE_SUCCESS(EnsureDeviceContext(), NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(mDeviceContext->GetZoom(*zoom), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetZoom(float zoom)
{
   NS_ENSURE_SUCCESS(EnsureDeviceContext(), NS_ERROR_FAILURE);
   mDeviceContext->SetZoom(zoom);

   // get the pres shell
   nsCOMPtr<nsIPresShell> presShell;
   NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

   // get the view manager
   nsCOMPtr<nsIViewManager> vm;
   NS_ENSURE_SUCCESS(presShell->GetViewManager(getter_AddRefs(vm)), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

   // get the root scrollable view
   nsIScrollableView* scrollableView = nsnull;
   vm->GetRootScrollableView(&scrollableView);
   if(scrollableView)
      scrollableView->ComputeScrollOffsets();

   // get the root view
   nsIView *rootView=nsnull; // views are not ref counted
   vm->GetRootView(rootView);
   if(rootView)
      vm->UpdateView(rootView, 0);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetMarginWidth(PRInt32* aWidth)
{
  NS_ENSURE_ARG_POINTER(aWidth);

  *aWidth = mMarginWidth;
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetMarginWidth(PRInt32 aWidth)
{
  mMarginWidth = aWidth;
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetMarginHeight(PRInt32* aHeight)
{
  NS_ENSURE_ARG_POINTER(aHeight);

  *aHeight = mMarginHeight;
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetMarginHeight(PRInt32 aHeight)
{
  mMarginHeight = aHeight;
  return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIDocShellTreeItem
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::GetName(PRUnichar** aName)
{
   NS_ENSURE_ARG_POINTER(aName);
   *aName = mName.ToNewUnicode();
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetName(const PRUnichar* aName)
{
   mName = aName;  // this does a copy of aName
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetItemType(PRInt32* aItemType)
{
   NS_ENSURE_ARG_POINTER(aItemType);

   *aItemType = mItemType;
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetItemType(PRInt32 aItemType)
{
   NS_ENSURE_ARG((aItemType == typeChrome) || (typeContent == aItemType));
   NS_ENSURE_STATE(!mParent);

   mItemType = aItemType;

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetParent(nsIDocShellTreeItem** aParent)
{
   NS_ENSURE_ARG_POINTER(aParent);

   *aParent = mParent;
   NS_IF_ADDREF(*aParent);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetParent(nsIDocShellTreeItem* aParent)
{
  // null aParent is ok
   /*
   Note this doesn't do an addref on purpose.  This is because the parent
   is an implied lifetime.  We don't want to create a cycle by refcounting
   the parent.
   */
   mParent = aParent;

   nsCOMPtr<nsIURIContentListener> parentURIListener(do_GetInterface(aParent));
   if(parentURIListener)
      SetParentURIContentListener(parentURIListener);
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetSameTypeParent(nsIDocShellTreeItem** aParent)
{
   NS_ENSURE_ARG_POINTER(aParent);
   *aParent = nsnull;

   if(!mParent)
      return NS_OK;
      
   PRInt32  parentType;
   NS_ENSURE_SUCCESS(mParent->GetItemType(&parentType), NS_ERROR_FAILURE);

   if(parentType == mItemType)
      {
      *aParent = mParent;
      NS_ADDREF(*aParent);
      }
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetRootTreeItem(nsIDocShellTreeItem** aRootTreeItem)
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

NS_IMETHODIMP nsDocShell::GetSameTypeRootTreeItem(nsIDocShellTreeItem** aRootTreeItem)
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

NS_IMETHODIMP nsDocShell::FindItemWithName(const PRUnichar *aName, 
   nsISupports* aRequestor, nsIDocShellTreeItem **_retval)
{
   NS_ENSURE_ARG(aName);
   NS_ENSURE_ARG_POINTER(_retval);
  
   *_retval = nsnull;  // if we don't find one, we return NS_OK and a null result 
   
   // This QI may fail, but the places where we want to compare, comparing
   // against nsnull serves the same purpose.
   nsCOMPtr<nsIDocShellTreeItem> reqAsTreeItem(do_QueryInterface(aRequestor));

   // First we check our name.
   if(mName.EqualsWithConversion(aName))
      {
      *_retval = NS_STATIC_CAST(nsIDocShellTreeItem*, this);
      NS_ADDREF(*_retval);
      return NS_OK;
      }

   // Second we check our children making sure not to ask a child if it
   // is the aRequestor.
   NS_ENSURE_SUCCESS(FindChildWithName(aName, PR_TRUE, PR_TRUE, reqAsTreeItem,
      _retval), 
      NS_ERROR_FAILURE);
   if(*_retval)
      return NS_OK;

   // Third if we have a parent and it isn't the requestor then we should ask
   // it to do the search.  If it is the requestor we should just stop here
   // and let the parent do the rest.
   // If we don't have a parent, then we should ask the docShellTreeOwner to do
   // the search.
   if(mParent)
      {
      if(mParent == reqAsTreeItem.get())
         return NS_OK;

      PRInt32 parentType;
      mParent->GetItemType(&parentType);
      if(parentType == mItemType)
         {
         NS_ENSURE_SUCCESS(mParent->FindItemWithName(aName,
            NS_STATIC_CAST(nsIDocShellTreeItem*, this), _retval), 
            NS_ERROR_FAILURE);
         return NS_OK;
         }
      // If the parent isn't of the same type fall through and ask tree owner.
      }

   // This QI may fail, but comparing against null serves the same purpose
   nsCOMPtr<nsIDocShellTreeOwner> reqAsTreeOwner(do_QueryInterface(aRequestor));

   if(mTreeOwner && (mTreeOwner != reqAsTreeOwner.get()))
      {
      NS_ENSURE_SUCCESS(mTreeOwner->FindItemWithName(aName, 
         NS_STATIC_CAST(nsIDocShellTreeItem*, this), _retval),
         NS_ERROR_FAILURE);
      }

  return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetTreeOwner(nsIDocShellTreeOwner** aTreeOwner)
{
   NS_ENSURE_ARG_POINTER(aTreeOwner);

   *aTreeOwner = mTreeOwner;
   NS_IF_ADDREF(*aTreeOwner);
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetTreeOwner(nsIDocShellTreeOwner* aTreeOwner)
{
   // Don't automatically set the progress based on the tree owner for frames
  if (!IsFrame()) {
    nsCOMPtr<nsIWebProgress> webProgress(do_QueryInterface(mLoadCookie));
    
    if (webProgress) {
      nsCOMPtr<nsIWebProgressListener> oldListener(do_QueryInterface(mTreeOwner));
      nsCOMPtr<nsIWebProgressListener> newListener(do_QueryInterface(aTreeOwner));

      if (oldListener) {
        webProgress->RemoveProgressListener(oldListener);
      }

      if (newListener) {
        webProgress->AddProgressListener(newListener);
      }
    }
  }

   mTreeOwner = aTreeOwner; // Weak reference per API

   PRInt32 i, n = mChildren.Count();
   for(i = 0; i < n; i++)
      {
      nsIDocShellTreeItem* child = (nsIDocShellTreeItem*) mChildren.ElementAt(i); // doesn't addref the result
      NS_ENSURE_TRUE(child, NS_ERROR_FAILURE);
      PRInt32 childType = ~mItemType; // Set it to not us in case the get fails
      child->GetItemType(&childType); // We don't care if this fails, if it does we won't set the owner
      if(childType == mItemType)
         child->SetTreeOwner(aTreeOwner);
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetChildOffset(PRInt32 aChildOffset)
{
  mChildOffset = aChildOffset;
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetChildOffset(PRInt32 *aChildOffset)
{
  NS_ENSURE_ARG_POINTER(aChildOffset);
  *aChildOffset = mChildOffset;
  return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIDocShellTreeNode
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::GetChildCount(PRInt32 *aChildCount)
{
  NS_ENSURE_ARG_POINTER(aChildCount);
  *aChildCount = mChildren.Count();
  return NS_OK;
}



NS_IMETHODIMP nsDocShell::AddChild(nsIDocShellTreeItem *aChild)
{
   NS_ENSURE_ARG_POINTER(aChild);

   NS_ENSURE_SUCCESS(aChild->SetParent(this), NS_ERROR_FAILURE);
   mChildren.AppendElement(aChild);
   NS_ADDREF(aChild);

   // Set the child's index in the parent's children list 
   // XXX What if the parent had different types of children?
   // XXX in that case docshell hierarchyand SH hierarchy won't match.
   PRInt32 childCount = mChildren.Count();
   aChild->SetChildOffset(childCount-1);

   /* Set the child's global history if the parent has one */
   if (mGlobalHistory) {
      nsCOMPtr<nsIDocShellHistory> dsHistoryChild(do_QueryInterface(aChild));
      if (dsHistoryChild)
 	     dsHistoryChild->SetGlobalHistory(mGlobalHistory);
   }
 

   PRInt32 childType = ~mItemType; // Set it to not us in case the get fails
   aChild->GetItemType(&childType);
   if(childType != mItemType)
      return NS_OK;    
   // Everything below here is only done when the child is the same type.


   aChild->SetTreeOwner(mTreeOwner);

   nsCOMPtr<nsIDocShell> childAsDocShell(do_QueryInterface(aChild));
   if(!childAsDocShell)
      return NS_OK;

   // Do some docShell Specific stuff.
   nsXPIDLString defaultCharset;
   nsXPIDLString forceCharset;
   NS_ENSURE_TRUE(mContentViewer, NS_ERROR_FAILURE);

   nsCOMPtr<nsIMarkupDocumentViewer> muDV = do_QueryInterface(mContentViewer);
   if(muDV)
      {
      NS_ENSURE_SUCCESS(muDV->GetDefaultCharacterSet(getter_Copies(defaultCharset)),
         NS_ERROR_FAILURE);
      NS_ENSURE_SUCCESS(muDV->GetForceCharacterSet(getter_Copies(forceCharset)),
         NS_ERROR_FAILURE);
      }
   nsCOMPtr<nsIContentViewer> childCV;
   NS_ENSURE_SUCCESS(childAsDocShell->GetContentViewer(getter_AddRefs(childCV)),
      NS_ERROR_FAILURE);
   if(childCV)
      {
      nsCOMPtr<nsIMarkupDocumentViewer> childmuDV = do_QueryInterface(childCV);
      if(childmuDV)
         {
         NS_ENSURE_SUCCESS(childmuDV->SetDefaultCharacterSet(defaultCharset), 
            NS_ERROR_FAILURE);
         NS_ENSURE_SUCCESS(childmuDV->SetForceCharacterSet(forceCharset), 
            NS_ERROR_FAILURE);
         }
      }

  // Now take this document's charset and set the parentCharset field of the 
  // child's DocumentCharsetInfo to it. We'll later use that field, in the 
  // loading process, for the charset choosing algorithm.
  // If we fail, at any point, we just return NS_OK.
  // This code has some performance impact. But this will be reduced when 
  // the current charset will finally be stored as an Atom, avoiding the
  // alias resolution extra look-up.

  // we are NOT going to propagate the charset is this Chrome's docshell
  if (mItemType == nsIDocShellTreeItem::typeChrome) return NS_OK;

  nsresult res = NS_OK;

  // get the child's docCSInfo object
  nsCOMPtr<nsIDocumentCharsetInfo> dcInfo = NULL;
  res = childAsDocShell->GetDocumentCharsetInfo(getter_AddRefs(dcInfo));
  if (NS_FAILED(res) || (!dcInfo)) return NS_OK;

  // get the parent's current charset
  nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(mContentViewer));
  if (!docv) return NS_OK;
  nsCOMPtr<nsIDocument> doc;
  res = docv->GetDocument(*getter_AddRefs(doc));
  if (NS_FAILED(res) || (!doc)) return NS_OK;
  nsAutoString parentCS;
  res = doc->GetDocumentCharacterSet(parentCS);
  if (NS_FAILED(res)) return NS_OK;

  // set the child's parentCharset
  res = dcInfo->SetParentCharset(&parentCS);
  if (NS_FAILED(res)) return NS_OK;

  // printf("### 1 >>> Adding child. Parent CS = %s. ItemType = %d.\n", parentCS.ToNewCString(), mItemType);

  return NS_OK;
}

NS_IMETHODIMP nsDocShell::RemoveChild(nsIDocShellTreeItem *aChild)
{
   NS_ENSURE_ARG_POINTER(aChild);

   if(mChildren.RemoveElement(aChild))
      {
      aChild->SetParent(nsnull);
      aChild->SetTreeOwner(nsnull);
      NS_RELEASE(aChild);
      }
   else
      NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_INVALID_ARG);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetChildAt(PRInt32 aIndex, nsIDocShellTreeItem** aChild)
{
   NS_ENSURE_ARG_POINTER(aChild);
   NS_ENSURE_ARG_RANGE(aIndex, 0, mChildren.Count() - 1);

   *aChild = (nsIDocShellTreeItem*) mChildren.ElementAt(aIndex);
   NS_IF_ADDREF(*aChild);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::FindChildWithName(const PRUnichar *aName, 
   PRBool aRecurse, PRBool aSameType, nsIDocShellTreeItem* aRequestor, 
   nsIDocShellTreeItem **_retval)
{
   NS_ENSURE_ARG(aName);
   NS_ENSURE_ARG_POINTER(_retval);
  
   *_retval = nsnull;  // if we don't find one, we return NS_OK and a null result 

   nsAutoString name(aName);
   nsXPIDLString childName;
   PRInt32 i, n = mChildren.Count();
   for(i = 0; i < n; i++) 
      {
      nsIDocShellTreeItem* child = (nsIDocShellTreeItem*) mChildren.ElementAt(i); // doesn't addref the result
      NS_ENSURE_TRUE(child, NS_ERROR_FAILURE);
      PRInt32 childType;
      child->GetItemType(&childType);
      
      if(aSameType && (childType != mItemType))
         continue;

      child->GetName(getter_Copies(childName));
      if(name.EqualsWithConversion(childName))
         {
         *_retval = child;
         NS_ADDREF(*_retval);
         break;
         }

      if(childType != mItemType) //Only ask it to check children if it is same type
         continue;

      if(aRecurse && (aRequestor != child)) // Only ask the child if it isn't the requestor
         {
         // See if child contains the shell with the given name
         nsCOMPtr<nsIDocShellTreeNode> childAsNode(do_QueryInterface(child));
         if(child)
            {
            NS_ENSURE_SUCCESS(childAsNode->FindChildWithName(aName, PR_TRUE, 
            aSameType, NS_STATIC_CAST(nsIDocShellTreeItem*, this), _retval),
               NS_ERROR_FAILURE);
            }
         }
      if(*_retval)   // found it
         return NS_OK;
      }
   return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIDocShellHistory
//*****************************************************************************   
NS_IMETHODIMP
nsDocShell::GetChildSHEntry(PRInt32 aChildOffset, nsISHEntry ** aResult)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;
  
  //
  // A nsISHEntry for a child is *only* available when the parent is in
  // the progress of loading a document too...
  //
  if (LSHE) {
    nsCOMPtr<nsISHContainer> container(do_QueryInterface(LSHE));
    if (container) {
      rv = container->GetChildAt(aChildOffset, aResult);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsDocShell::AddChildSHEntry(nsISHEntry * aCloneRef, nsISHEntry * aNewEntry,
                            PRInt32 aChildOffset)
{
  nsresult rv;
  
  if (LSHE) {
    /* You get here if you are currently building a 
     * hierarchy ie.,you just visited a frameset page
     */
    nsCOMPtr<nsISHContainer>  container(do_QueryInterface(LSHE, &rv));
    if(container)
      rv = container->AddChild(aNewEntry, aChildOffset);
  }
  else if (mSessionHistory) {
    /* You are currently in the rootDocShell.
     * You will get here when a subframe has a new url
     * to load and you have walked up the tree all the 
     * way to the top  
     */
    PRInt32 index=-1;
    nsCOMPtr<nsISHEntry> currentEntry;
    mSessionHistory->GetIndex(&index);
    if (index < 0)
      return NS_ERROR_FAILURE;

    rv = mSessionHistory->GetEntryAtIndex(index, PR_FALSE,
                                          getter_AddRefs(currentEntry));
    if (currentEntry) {
      nsCOMPtr<nsISHEntry> nextEntry; //(do_CreateInstance(NS_SHENTRY_PROGID));
      //   NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
      rv = CloneAndReplace(currentEntry, aCloneRef, aNewEntry,
                           getter_AddRefs(nextEntry));
      
      if (NS_SUCCEEDED(rv)) {
        rv = mSessionHistory->AddEntry(nextEntry, PR_TRUE);
      }
    }
  }
  else {
    /* You will get here when you are in a subframe and
     * a new url has been loaded on you. 
     * The OSHE in this subframe will be the previous url's
     * OSHE. This OSHE will be used as the identification
     * for this subframe in the  CloneAndReplace function.
     */

    nsCOMPtr<nsIDocShellHistory> parent(do_QueryInterface(mParent, &rv));
    if (parent) {
      if (!aCloneRef) {
        aCloneRef = OSHE;
      }
      rv = parent->AddChildSHEntry(aCloneRef, aNewEntry, aChildOffset);
    }

  }
  return rv;
}

NS_IMETHODIMP nsDocShell::SetGlobalHistory(nsIGlobalHistory* aGlobalHistory)
{
    mGlobalHistory = aGlobalHistory; 
    return NS_OK;
}
 	
NS_IMETHODIMP nsDocShell::GetGlobalHistory(nsIGlobalHistory** aGlobalHistory)
{
    NS_ENSURE_ARG_POINTER(aGlobalHistory);
 
    *aGlobalHistory = mGlobalHistory;
    NS_IF_ADDREF(*aGlobalHistory);
    return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIWebNavigation
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::GetCanGoBack(PRBool* aCanGoBack)
{
#ifndef SH_IN_FRAMES
   NS_ENSURE_ARG_POINTER(aCanGoBack);
   *aCanGoBack = PR_FALSE;
   if (mSessionHistory == nsnull) {
      return NS_OK;
   }
   
   NS_ENSURE_STATE(mSessionHistory);  

   PRInt32 index = -1;
   NS_ENSURE_SUCCESS(mSessionHistory->GetIndex(&index), NS_ERROR_FAILURE);
   if(index > 0)
      *aCanGoBack = PR_TRUE;
#else
  if (mSessionHistory) {
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mSessionHistory));

    if (webNav) {
      return webNav->GetCanGoBack(aCanGoBack);
    }
  }
    else {
    nsCOMPtr<nsIDocShellTreeItem> root;
	GetSameTypeRootTreeItem(getter_AddRefs(root));
	NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);
    nsCOMPtr<nsIWebNavigation> rootAsWebnav(do_QueryInterface(root));  
    if (rootAsWebnav) {
       rootAsWebnav->GetCanGoBack(aCanGoBack);
 	}
  }
#endif 
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetCanGoForward(PRBool* aCanGoForward)
{
#ifndef SH_IN_FRAMES
   NS_ENSURE_ARG_POINTER(aCanGoForward);
   *aCanGoForward = PR_FALSE;
   if (mSessionHistory == nsnull) {
      return NS_OK;
   }

   NS_ENSURE_STATE(mSessionHistory);  

   PRInt32 index = -1;
   PRInt32 count = -1;

   NS_ENSURE_SUCCESS(mSessionHistory->GetIndex(&index), NS_ERROR_FAILURE);
   NS_ENSURE_SUCCESS(mSessionHistory->GetCount(&count), NS_ERROR_FAILURE);

   if((index >= 0) && (index < (count - 1)))
      *aCanGoForward = PR_TRUE;
#else
  if (mSessionHistory) {
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mSessionHistory));

    if (webNav) {
      return webNav->GetCanGoForward(aCanGoForward);
    }
  }
  else {
	nsCOMPtr<nsIDocShellTreeItem> root;
	GetSameTypeRootTreeItem(getter_AddRefs(root));
	NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);
    nsCOMPtr<nsIWebNavigation> rootAsWebnav(do_QueryInterface(root));  
    if (rootAsWebnav) {
       rootAsWebnav->GetCanGoForward(aCanGoForward);
 	}
  }
#endif 
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GoBack()
{
#ifndef SH_IN_FRAMES
   if (mSessionHistory == nsnull) {
      return NS_OK;
   }
   nsCOMPtr<nsIDocShellTreeItem> root;
   GetSameTypeRootTreeItem(getter_AddRefs(root));
   if(root.get() != NS_STATIC_CAST(nsIDocShellTreeItem*, this))
      {
      nsCOMPtr<nsIWebNavigation> rootAsNav(do_QueryInterface(root));
      return rootAsNav->GoBack();
      }

   NS_ENSURE_STATE(mSessionHistory);

 
   UpdateCurrentSessionHistory();  

   nsCOMPtr<nsISHEntry> previousEntry;

   NS_ENSURE_SUCCESS(mSessionHistory->GetPreviousEntry(PR_TRUE, 
      getter_AddRefs(previousEntry)), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(previousEntry, NS_ERROR_FAILURE);

  
   NS_ENSURE_SUCCESS(LoadHistoryEntry(previousEntry), NS_ERROR_FAILURE);
#else
  if (mSessionHistory) {
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mSessionHistory));

    if (webNav) {
      return webNav->GoBack();
    }
  }
  else {
	  	nsCOMPtr<nsIDocShellTreeItem> root;
	GetSameTypeRootTreeItem(getter_AddRefs(root));
	NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);
    nsCOMPtr<nsIWebNavigation> rootAsWebnav(do_QueryInterface(root));  
    if (rootAsWebnav) {
       rootAsWebnav->GoBack();
 	}
  }
#endif 
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GoForward()
{
#ifndef SH_IN_FRAMES
   if (mSessionHistory == nsnull) {
      return NS_OK;
   }
   nsCOMPtr<nsIDocShellTreeItem> root;
   GetSameTypeRootTreeItem(getter_AddRefs(root));
   if(root.get() != NS_STATIC_CAST(nsIDocShellTreeItem*, this))
      {
      nsCOMPtr<nsIWebNavigation> rootAsNav(do_QueryInterface(root));
      return rootAsNav->GoForward();
      }

   NS_ENSURE_STATE(mSessionHistory);
   
   UpdateCurrentSessionHistory();  

   nsCOMPtr<nsISHEntry> nextEntry;

   NS_ENSURE_SUCCESS(mSessionHistory->GetNextEntry(PR_TRUE, 
      getter_AddRefs(nextEntry)), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(nextEntry, NS_ERROR_FAILURE);
   

   NS_ENSURE_SUCCESS(LoadHistoryEntry(nextEntry), NS_ERROR_FAILURE);
#else
  if (mSessionHistory) {
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mSessionHistory));

    if (webNav) {
      return webNav->GoForward();
    }
  }
  else {
	  	nsCOMPtr<nsIDocShellTreeItem> root;
	GetSameTypeRootTreeItem(getter_AddRefs(root));
	NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);
    nsCOMPtr<nsIWebNavigation> rootAsWebnav(do_QueryInterface(root));  
    if (rootAsWebnav) {
       rootAsWebnav->GoForward();
 	}
  }
#endif 
   return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GotoIndex(PRInt32 aIndex)
{
  if (mSessionHistory) {
      nsCOMPtr<nsIWebNavigation>webNav(do_QueryInterface(mSessionHistory));
      if (webNav) {
          return webNav->GotoIndex(aIndex);
	  }
  }
  else {
 	nsCOMPtr<nsIDocShellTreeItem> root;
	GetSameTypeRootTreeItem(getter_AddRefs(root));
	NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);
    nsCOMPtr<nsIWebNavigation> rootAsWebnav(do_QueryInterface(root));  
    if (rootAsWebnav) {
       rootAsWebnav->GotoIndex(aIndex);
 	}
  } 
	return NS_OK;
}


NS_IMETHODIMP nsDocShell::LoadURI(const PRUnichar* aURI)
{
   nsCOMPtr<nsIURI> uri;

   nsresult rv = CreateFixupURI(aURI, getter_AddRefs(uri));

   if(NS_ERROR_UNKNOWN_PROTOCOL == rv)
      {
      // we weren't able to find a protocol handler
      nsCOMPtr<nsIPrompt> prompter;
      nsCOMPtr<nsIStringBundle> stringBundle;
      GetPromptAndStringBundle(getter_AddRefs(prompter), 
         getter_AddRefs(stringBundle));

      NS_ENSURE_TRUE(stringBundle, NS_ERROR_FAILURE);

      nsXPIDLString messageStr;
      NS_ENSURE_SUCCESS(stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("protocolNotFound").GetUnicode(), 
         getter_Copies(messageStr)), NS_ERROR_FAILURE);

      nsAutoString uriString(aURI);
      PRInt32 colon = uriString.FindChar(':');
      // extract the scheme
      nsAutoString scheme;
      uriString.Left(scheme, colon);
      nsCAutoString cScheme;
      cScheme.AssignWithConversion(scheme);

      PRUnichar *msg = nsTextFormatter::smprintf(messageStr, cScheme.GetBuffer());
      if (!msg) return NS_ERROR_OUT_OF_MEMORY;

      prompter->Alert(nsnull, msg);
      nsTextFormatter::smprintf_free(msg);
      } // end unknown protocol

   if(!uri)
      return NS_ERROR_FAILURE;

   NS_ENSURE_SUCCESS(LoadURI(uri, nsnull), NS_ERROR_FAILURE);
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::Reload(PRInt32 aReloadType)
{  
#ifdef SH_IN_FRAMES
   // XXX Honor the reload type
   //NS_ENSURE_STATE(mCurrentURI);

   // XXXTAB Convert reload type to our type
   nsDocShellInfoLoadType type = nsIDocShellLoadInfo::loadReloadNormal;
   if ( aReloadType == nsIWebNavigation::loadReloadBypassProxyAndCache )
   	type = nsIDocShellLoadInfo::loadReloadBypassProxyAndCache;
#if 0
   nsCOMPtr<nsISHEntry> entry;
   if (OSHE) {
	   /* We should fall here in most cases including subframes & refreshes */
	   entry = OSHE;
   } else if (mSessionHistory) {
	   /* In case we fail above, as a last ditch effort, we 
	    * reload the whole page.
		*/
	  PRInt32 index = -1;
      NS_ENSURE_SUCCESS(mSessionHistory->GetIndex(&index), NS_ERROR_FAILURE);
      NS_ENSURE_SUCCESS(mSessionHistory->GetEntryAtIndex(index, PR_FALSE,
          getter_AddRefs(entry)), NS_ERROR_FAILURE);
      NS_ENSURE_TRUE(entry, NS_ERROR_FAILURE);
   }
   else {
	   //May be one of those <META> charset reloads in a composer or Messenger
      return InternalLoad(mCurrentURI, mReferrerURI, nsnull, nsnull, 
      nsnull, type);

   }
#else
   // OK. Atleast for the heck of it, pollmann says that he doesn't crash
   // in bug 45297 if he just did the following, instead of the one in #if 0.
   // If this really keeps the crash from re-occuring, may be this can stay. However 
   // there is no major difference between this one and the one inside #if 0
   
   return InternalLoad(mCurrentURI, mReferrerURI, nsnull, nsnull, 
      nsnull, type);
#endif /* 0 */

	   
  // return LoadHistoryEntry(entry, type);


#else

   // XXX Honor the reload type
   NS_ENSURE_STATE(mCurrentURI);

   // XXXTAB Convert reload type to our type
   nsDocShellInfoLoadType type = nsIDocShellLoadInfo::loadReloadNormal;
   if ( aReloadType == nsIWebNavigation::loadReloadBypassProxyAndCache )
   	type = nsIDocShellLoadInfo::loadReloadBypassProxyAndCache;

   UpdateCurrentSessionHistory();

   NS_ENSURE_SUCCESS(InternalLoad(mCurrentURI, mReferrerURI, nsnull, nsnull, 
      nsnull, type), NS_ERROR_FAILURE);
   return NS_OK;
#endif  /* SH_IN_FRAMES  */
 
}

NS_IMETHODIMP nsDocShell::Stop()
{
   // Cancel any timers that were set for this loader.
   CancelRefreshURITimers();

   if(mContentViewer)
      mContentViewer->Stop();

   if(mLoadCookie)
      {
      nsCOMPtr<nsIURILoader> uriLoader = do_GetService(NS_URI_LOADER_PROGID);
      if(uriLoader)
         uriLoader->Stop(mLoadCookie);
      }

   PRInt32 n;
   PRInt32 count = mChildren.Count();
   for(n = 0; n < count; n++)
      {
      nsIDocShellTreeItem* shell = (nsIDocShellTreeItem*)mChildren.ElementAt(n);
      nsCOMPtr<nsIWebNavigation> shellAsNav(do_QueryInterface(shell));
      if(shellAsNav)
         shellAsNav->Stop();
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetDocument(nsIDOMDocument* aDocument,
   const PRUnichar* aContentType)
{
   //XXX First Checkin
   NS_ERROR("Not Yet Implemented");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::GetDocument(nsIDOMDocument** aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  NS_ENSURE_STATE(mContentViewer);

  return mContentViewer->GetDOMDocument(aDocument);
}
	
NS_IMETHODIMP nsDocShell::GetCurrentURI(PRUnichar** aCurrentURI)
{
   NS_ENSURE_ARG_POINTER(aCurrentURI);
   
   if(!mCurrentURI)
      {
      *aCurrentURI = nsnull;
      return NS_OK;
      }

   char* spec;
   NS_ENSURE_SUCCESS(mCurrentURI->GetSpec(&spec), NS_ERROR_FAILURE);

   *aCurrentURI = NS_ConvertASCIItoUCS2(spec).ToNewUnicode();

   if(spec)
      nsCRT::free(spec);

   return NS_OK;
}
	
NS_IMETHODIMP nsDocShell::SetSessionHistory(nsISHistory* aSessionHistory)
{
   mSessionHistory = aSessionHistory;

#if defined(SH_IN_FRAMES)
   if (mSessionHistory) {
     mSessionHistory->SetRootDocShell(this);
   }
#endif /* SH_IN_FRAMES */

   return NS_OK;
}
	
NS_IMETHODIMP nsDocShell::GetSessionHistory(nsISHistory** aSessionHistory)
{
   NS_ENSURE_ARG_POINTER(aSessionHistory);

   if (mSessionHistory) {
     *aSessionHistory = mSessionHistory;
     NS_IF_ADDREF(*aSessionHistory);
   }
   else {
	nsCOMPtr<nsIDocShellTreeItem> root;
	GetSameTypeRootTreeItem(getter_AddRefs(root));
	NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);
    nsCOMPtr<nsIWebNavigation> rootAsWebnav(do_QueryInterface(root));  
    if (rootAsWebnav) {
       rootAsWebnav->GetSessionHistory(aSessionHistory);
 	}
   }
   return NS_OK;
}
//*****************************************************************************
// nsDocShell::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::InitWindow(nativeWindow parentNativeWindow,
   nsIWidget* parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)   
{
   NS_ENSURE_ARG(parentWidget);  // DocShells must get a widget for a parent

   SetParentWidget(parentWidget);
   SetPositionAndSize(x, y, cx, cy, PR_FALSE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::Create()
{
   NS_ENSURE_STATE(!mContentViewer);
   mPrefs = do_GetService(NS_PREF_PROGID);
   //GlobalHistory is now set in SetGlobalHistory
 //  mGlobalHistory = do_GetService(NS_GLOBALHISTORY_PROGID);

   // i don't want to read this pref in every time we load a url
   // so read it in once here and be done with it...
   mPrefs->GetBoolPref("network.protocols.useSystemDefaults", &mUseExternalProtocolHandler);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::Destroy()
{
   // Stop any URLs that are currently being loaded...
   Stop();
   if(mDocLoader)
      {
      mDocLoader->Destroy();
      mDocLoader->SetContainer(nsnull);
      }

   SetDocLoaderObserver(nsnull);

  // Save the state of the current document, before destroying the window.
  // This is needed to capture the state of a frameset when the new document
  // causes the frameset to be destroyed...
  PersistLayoutHistoryState();

   // Remove this docshell from its parent's child list
   nsCOMPtr<nsIDocShellTreeNode> docShellParentAsNode(do_QueryInterface(mParent));
   if(docShellParentAsNode)
      docShellParentAsNode->RemoveChild(this);

   mContentViewer = nsnull;

   DestroyChildren();

   mDocLoader = nsnull;
   mDocLoaderObserver = nsnull;
   mParentWidget = nsnull;
   mPrefs = nsnull;
   mCurrentURI = nsnull;

   if(mScriptGlobal)
      {
      mScriptGlobal->SetDocShell(nsnull);
      mScriptGlobal = nsnull;
      }
   if(mScriptContext)
      {
      mScriptContext->SetOwner(nsnull);
      mScriptContext = nsnull;
      }

   mScriptGlobal = nsnull;
   mScriptContext = nsnull;
   mSessionHistory = nsnull;
   SetTreeOwner(nsnull);

  SetLoadCookie(nsnull);

   if(mInitInfo)
      {
      delete mInitInfo;
      mInitInfo = nsnull;
      }

   if(mContentListener)
      {
      mContentListener->DocShell(nsnull);
      NS_RELEASE(mContentListener);
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetPosition(PRInt32 x, PRInt32 y)
{
   if(mContentViewer)
      NS_ENSURE_SUCCESS(mContentViewer->Move(x, y), NS_ERROR_FAILURE);
   else if(InitInfo())
      {
      mInitInfo->x = x;
      mInitInfo->y = y;
      }
   else
      NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetPosition(PRInt32* aX, PRInt32* aY)
{
   PRInt32 dummyHolder;
   return GetPositionAndSize(aX, aY, &dummyHolder, &dummyHolder);
}

NS_IMETHODIMP nsDocShell::SetSize(PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
   PRInt32 x = 0, y = 0;
   GetPosition(&x, &y);
   return SetPositionAndSize(x, y, aCX, aCY, aRepaint);
}

NS_IMETHODIMP nsDocShell::GetSize(PRInt32* aCX, PRInt32* aCY)
{
   PRInt32 dummyHolder;
   return GetPositionAndSize(&dummyHolder, &dummyHolder, aCX, aCY);
}

NS_IMETHODIMP nsDocShell::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
   PRInt32 cy, PRBool fRepaint)
{
   if(mContentViewer)
      {
      //XXX Border figured in here or is that handled elsewhere?
      nsRect bounds(x, y, cx, cy);

      NS_ENSURE_SUCCESS(mContentViewer->SetBounds(bounds), NS_ERROR_FAILURE);
      }
   else if(InitInfo())
      {
      mInitInfo->x = x;
      mInitInfo->y = y;
      mInitInfo->cx = cx;
      mInitInfo->cy = cy;
      }
   else
      NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetPositionAndSize(PRInt32* x, PRInt32* y, PRInt32* cx,
   PRInt32* cy)
{
   if(mContentViewer)
      {
      nsRect bounds;

      NS_ENSURE_SUCCESS(mContentViewer->GetBounds(bounds), NS_ERROR_FAILURE);

      if(x)
         *x = bounds.x;
      if(y)
         *y = bounds.y;
      if(cx)
         *cx = bounds.width;
      if(cy)
         *cy = bounds.height;
      }
   else if(InitInfo())
      {
      if(x)
         *x = mInitInfo->x;
      if(y)
         *y = mInitInfo->y;
      if(cx)
         *cx = mInitInfo->cx;
      if(cy)
         *cy = mInitInfo->cy;
      }
   else
      NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::Repaint(PRBool aForce)
{
   nsCOMPtr<nsIDocumentViewer> docViewer(do_QueryInterface(mContentViewer));
   NS_ENSURE_TRUE(docViewer, NS_ERROR_FAILURE);

   nsCOMPtr<nsIPresContext> context;
   docViewer->GetPresContext(*getter_AddRefs(context));
   NS_ENSURE_TRUE(context, NS_ERROR_FAILURE);

   nsCOMPtr<nsIPresShell> shell;
   context->GetShell(getter_AddRefs(shell));
   NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

   nsCOMPtr<nsIViewManager> viewManager;
   shell->GetViewManager(getter_AddRefs(viewManager));
   NS_ENSURE_TRUE(viewManager, NS_ERROR_FAILURE);

   // what about aForce ?
   NS_ENSURE_SUCCESS(viewManager->UpdateAllViews(0), NS_ERROR_FAILURE);
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetParentWidget(nsIWidget** parentWidget)
{
   NS_ENSURE_ARG_POINTER(parentWidget);

   *parentWidget = mParentWidget;
   NS_IF_ADDREF(*parentWidget);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetParentWidget(nsIWidget* aParentWidget)
{
   NS_ENSURE_STATE(!mContentViewer);

   mParentWidget = aParentWidget;

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetParentNativeWindow(nativeWindow* parentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(parentNativeWindow);

   if(mParentWidget)
      *parentNativeWindow = mParentWidget->GetNativeData(NS_NATIVE_WIDGET);
   else
      *parentNativeWindow = nsnull;

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetParentNativeWindow(nativeWindow parentNativeWindow)
{
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDocShell::GetVisibility(PRBool* aVisibility)
{
   NS_ENSURE_ARG_POINTER(aVisibility);
   if(!mContentViewer)
      {
      *aVisibility = PR_FALSE;
      return NS_OK;
      }

   // get the pres shell
   nsCOMPtr<nsIPresShell> presShell;
   NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

   // get the view manager
   nsCOMPtr<nsIViewManager> vm;
   NS_ENSURE_SUCCESS(presShell->GetViewManager(getter_AddRefs(vm)), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

   // get the root view
   nsIView *rootView=nsnull; // views are not ref counted
   NS_ENSURE_SUCCESS(vm->GetRootView(rootView), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(rootView, NS_ERROR_FAILURE);

   // convert the view's visibility attribute to a bool
   nsViewVisibility vis;
   NS_ENSURE_TRUE(rootView->GetVisibility(vis), NS_ERROR_FAILURE);
   *aVisibility = nsViewVisibility_kHide==vis ? PR_FALSE : PR_TRUE;

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetVisibility(PRBool aVisibility)
{
   if(!mContentViewer)
      return NS_OK;
   if(aVisibility)
      {
      NS_ENSURE_SUCCESS(EnsureContentViewer(), NS_ERROR_FAILURE);
      mContentViewer->Show();
      }
   else if(mContentViewer)
      mContentViewer->Hide();

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetMainWidget(nsIWidget** aMainWidget)
{
   // We don't create our own widget, so simply return the parent one. 
   return GetParentWidget(aMainWidget);
}

NS_IMETHODIMP nsDocShell::SetFocus()
{
   nsCOMPtr<nsIWidget> mainWidget;
   GetMainWidget(getter_AddRefs(mainWidget));
   if(mainWidget)
      mainWidget->SetFocus();
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::FocusAvailable(nsIBaseWindow* aCurrentFocus, 
   PRBool* aTookFocus)
{
   NS_ENSURE_ARG_POINTER(aTookFocus);

   // Next person we should call is first the parent otherwise the 
   // docshell tree owner.
   nsCOMPtr<nsIBaseWindow> nextCallWin(do_QueryInterface(mParent));
   if(!nextCallWin)
      {
      nextCallWin = do_QueryInterface(mTreeOwner);
      }

   //If the current focus is us, offer it to the next owner.
   if(aCurrentFocus == NS_STATIC_CAST(nsIBaseWindow*, this))
      {
      if(nextCallWin)
        { 
        nsresult ret = nextCallWin->FocusAvailable(aCurrentFocus, aTookFocus);
        if (NS_SUCCEEDED(ret) && *aTookFocus)
          return NS_OK;
        }

        if (!mChildren.Count())
           {
           //If we don't have children and our parent didn't want 
           //the focus then we should just stop now.
           return NS_OK;
           }
      }

   //Otherwise, check the chilren and offer it to the next sibling.
   PRInt32 i;
   PRInt32 n = mChildren.Count();
   for(i = 0; i < n; i++)
      {
      nsCOMPtr<nsIBaseWindow> 
         child(do_QueryInterface((nsISupports*)mChildren.ElementAt(i)));
      //If we have focus we offer it to our first child.
      if(aCurrentFocus == NS_STATIC_CAST(nsIBaseWindow*, this))
        {
        if(NS_SUCCEEDED(child->SetFocus()))
           {
           *aTookFocus = PR_TRUE;
           return NS_OK;
           }
        else 
           {
           return NS_ERROR_FAILURE;
           } 
        }
      //If we don't have focus, find the child that does then
      //offer focus to the next one.
      if (child.get() == aCurrentFocus)
         {
         while(++i < n)
            {
            child = do_QueryInterface((nsISupports*)mChildren.ElementAt(i));
            if(NS_SUCCEEDED(child->SetFocus()))
               {
               *aTookFocus = PR_TRUE;
               return NS_OK;
               }
            else 
               {
               return NS_ERROR_FAILURE;
               } 
            }
         }
      }

   //Reached the end of our child list.  Call again to offer focus
   //upwards and to start at the beginning of our child list if
   //no one above us wants focus.
   return FocusAvailable(this, aTookFocus);
}

NS_IMETHODIMP nsDocShell::GetTitle(PRUnichar** aTitle)
{
   NS_ENSURE_ARG_POINTER(aTitle);

   *aTitle = mTitle.ToNewUnicode();
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetTitle(const PRUnichar* aTitle)
{
   // Store local title
   mTitle = aTitle;

   nsCOMPtr<nsIDocShellTreeItem> parent;
   GetSameTypeParent(getter_AddRefs(parent));

   // When title is set on the top object it should then be passed to the 
   // tree owner.
   if(!parent)
      {
      nsCOMPtr<nsIBaseWindow> treeOwnerAsWin(do_QueryInterface(mTreeOwner));
      if (treeOwnerAsWin)
          treeOwnerAsWin->SetTitle(aTitle);
      }

   if(mGlobalHistory && mCurrentURI)
      {
      nsXPIDLCString url;
      mCurrentURI->GetSpec(getter_Copies(url));
      mGlobalHistory->SetPageTitle(url, aTitle);
      }


   // Update SessionHistory too with Title. Otherwise entry for current page
   // has previous page's title.
   if(mSessionHistory)
      {
      PRInt32 index = -1;
      mSessionHistory->GetIndex(&index);
      nsCOMPtr<nsISHEntry>   shEntry;
      mSessionHistory->GetEntryAtIndex(index, PR_FALSE, getter_AddRefs(shEntry));
      if(shEntry)
         shEntry->SetTitle(mTitle.GetUnicode());      
      }
   

   return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIScrollable
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::GetCurScrollPos(PRInt32 scrollOrientation, 
   PRInt32* curPos)
{
   NS_ENSURE_ARG_POINTER(curPos);

   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);
   if (!scrollView)
   {
       return NS_ERROR_FAILURE;
   }

   nscoord x, y;
   NS_ENSURE_SUCCESS(scrollView->GetScrollPosition(x, y), NS_ERROR_FAILURE);

   switch(scrollOrientation)
      {
      case ScrollOrientation_X:
         *curPos = x;
         return NS_OK;

      case ScrollOrientation_Y:
         *curPos = y;
         return NS_OK;

      default:
         NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_INVALID_ARG);
      }
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::SetCurScrollPos(PRInt32 scrollOrientation, 
   PRInt32 curPos)
{
   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);
   if (!scrollView)
   {
       return NS_ERROR_FAILURE;
   }

   PRInt32 other;
   PRInt32 x;
   PRInt32 y;

   GetCurScrollPos(scrollOrientation, &other);

   switch(scrollOrientation)
      {
      case ScrollOrientation_X:
         x = curPos;
         y = other;
         break;

      case ScrollOrientation_Y:
         x = other;
         y = curPos;
         break;

      default:
         NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_INVALID_ARG);
      }

   NS_ENSURE_SUCCESS(scrollView->ScrollTo(x, y, NS_VMREFRESH_IMMEDIATE),
      NS_ERROR_FAILURE);
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetCurScrollPosEx(PRInt32 curHorizontalPos, 
   PRInt32 curVerticalPos)
{
   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);
   if (!scrollView)
   {
       return NS_ERROR_FAILURE;
   }

   NS_ENSURE_SUCCESS(scrollView->ScrollTo(curHorizontalPos, curVerticalPos, 
      NS_VMREFRESH_IMMEDIATE), NS_ERROR_FAILURE);
   return NS_OK;
}

// XXX This is wrong
NS_IMETHODIMP nsDocShell::GetScrollRange(PRInt32 scrollOrientation,
   PRInt32* minPos, PRInt32* maxPos)
{
   NS_ENSURE_ARG_POINTER(minPos && maxPos);

   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);
   if (!scrollView)
   {
       return NS_ERROR_FAILURE;
   }

   PRInt32 cx;
   PRInt32 cy;
   
   NS_ENSURE_SUCCESS(scrollView->GetContainerSize(&cx, &cy), NS_ERROR_FAILURE);
   *minPos = 0;
   
   switch(scrollOrientation)
      {
      case ScrollOrientation_X:
         *maxPos = cx;
         return NS_OK;

      case ScrollOrientation_Y:
         *maxPos = cy;
         return NS_OK;

      default:
         NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_INVALID_ARG);
      }

   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::SetScrollRange(PRInt32 scrollOrientation,
   PRInt32 minPos, PRInt32 maxPos)
{
   //XXX First Check
  /*
  Retrieves or Sets the valid ranges for the thumb.  When maxPos is set to 
  something less than the current thumb position, curPos is set = to maxPos.

  @return NS_OK - Setting or Getting completed successfully.
        NS_ERROR_INVALID_ARG - returned when curPos is not within the
          minPos and maxPos.
  */
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::SetScrollRangeEx(PRInt32 minHorizontalPos,
   PRInt32 maxHorizontalPos, PRInt32 minVerticalPos, PRInt32 maxVerticalPos)
{
   //XXX First Check
  /*
  Retrieves or Sets the valid ranges for the thumb.  When maxPos is set to 
  something less than the current thumb position, curPos is set = to maxPos.

  @return NS_OK - Setting or Getting completed successfully.
        NS_ERROR_INVALID_ARG - returned when curPos is not within the
          minPos and maxPos.
  */
   return NS_ERROR_FAILURE;
}

// Get scroll setting for this document only
//
// One important client is nsCSSFrameConstructor::ConstructRootFrame()
NS_IMETHODIMP nsDocShell::GetCurrentScrollbarPreferences(PRInt32 scrollOrientation,
   PRInt32* scrollbarPref)
{
   NS_ENSURE_ARG_POINTER(scrollbarPref);
   switch(scrollOrientation) {
     case ScrollOrientation_X:
       *scrollbarPref = mCurrentScrollbarPref.x;
       return NS_OK;

     case ScrollOrientation_Y:
       *scrollbarPref = mCurrentScrollbarPref.y;
       return NS_OK;

     default:
       NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_INVALID_ARG);
   }
   return NS_ERROR_FAILURE;
}

// This returns setting for all documents in this webshell
NS_IMETHODIMP nsDocShell::GetDefaultScrollbarPreferences(PRInt32 scrollOrientation,
   PRInt32* scrollbarPref)
{
   NS_ENSURE_ARG_POINTER(scrollbarPref);
   switch(scrollOrientation) {
     case ScrollOrientation_X:
       *scrollbarPref = mDefaultScrollbarPref.x;
       return NS_OK;

     case ScrollOrientation_Y:
       *scrollbarPref = mDefaultScrollbarPref.y;
       return NS_OK;

     default:
       NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_INVALID_ARG);
   }
   return NS_ERROR_FAILURE;
}

// Set scrolling preference for this document only.
//
// There are three possible values stored in the shell:
//  1) NS_STYLE_OVERFLOW_HIDDEN = no scrollbars
//  2) NS_STYLE_OVERFLOW_AUTO = scrollbars appear if needed
//  3) NS_STYLE_OVERFLOW_SCROLL = scrollbars always
//
// XXX Currently OVERFLOW_SCROLL isn't honored,
//     as it is not implemented by Gfx scrollbars
// XXX setting has no effect after the root frame is created
//     as it is not implemented by Gfx scrollbars
//
// One important client is HTMLContentSink::StartLayout()
NS_IMETHODIMP nsDocShell::SetCurrentScrollbarPreferences(PRInt32 scrollOrientation,
   PRInt32 scrollbarPref)
{
   switch(scrollOrientation) {
     case ScrollOrientation_X:
       mCurrentScrollbarPref.x = scrollbarPref;
       return NS_OK;

     case ScrollOrientation_Y:
       mCurrentScrollbarPref.y = scrollbarPref;
       return NS_OK;

     default:
       NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_INVALID_ARG);
   }
   return NS_ERROR_FAILURE;
}

// Set scrolling preference for all documents in this shell
// One important client is nsHTMLFrameInnerFrame::CreateWebShell()
NS_IMETHODIMP nsDocShell::SetDefaultScrollbarPreferences(PRInt32 scrollOrientation,
   PRInt32 scrollbarPref)
{
   switch(scrollOrientation) {
     case ScrollOrientation_X:
       mDefaultScrollbarPref.x = scrollbarPref;
       return NS_OK;

     case ScrollOrientation_Y:
       mDefaultScrollbarPref.y = scrollbarPref;
       return NS_OK;

     default:
       NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_INVALID_ARG);
   }
   return NS_ERROR_FAILURE;
}

// Reset 'current' scrollbar settings to 'default'.
// This must be called before every document load or else
// frameset scrollbar settings (e.g. <IFRAME SCROLLING="no">
// will not be preserved.
//
// One important client is HTMLContentSink::StartLayout()
NS_IMETHODIMP nsDocShell::ResetScrollbarPreferences()
{
  mCurrentScrollbarPref = mDefaultScrollbarPref;
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetScrollbarVisibility(PRBool* verticalVisible,
   PRBool* horizontalVisible)
{
   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);
   if (!scrollView)
   {
       return NS_ERROR_FAILURE;
   }

   PRBool vertVisible;
   PRBool horizVisible;

   NS_ENSURE_SUCCESS(scrollView->GetScrollbarVisibility(&vertVisible,
      &horizVisible), NS_ERROR_FAILURE);

   if(verticalVisible)
      *verticalVisible = vertVisible;
   if(horizontalVisible)
      *horizontalVisible = horizVisible;

   return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsITextScroll
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::ScrollByLines(PRInt32 numLines)
{
   nsCOMPtr<nsIScrollableView> scrollView;

   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)),
      NS_ERROR_FAILURE);
   if (!scrollView)
   {
       return NS_ERROR_FAILURE;
   }

   NS_ENSURE_SUCCESS(scrollView->ScrollByLines(0, numLines), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::ScrollByPages(PRInt32 numPages)
{
   nsCOMPtr<nsIScrollableView> scrollView;

   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)),
      NS_ERROR_FAILURE);
   if (!scrollView)
   {
       return NS_ERROR_FAILURE;
   }

   NS_ENSURE_SUCCESS(scrollView->ScrollByPages(numPages), NS_ERROR_FAILURE);

   return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIScriptGlobalObjectOwner
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::GetScriptGlobalObject(nsIScriptGlobalObject** aGlobal)
{
   NS_ENSURE_ARG_POINTER(aGlobal);
   NS_ENSURE_SUCCESS(EnsureScriptEnvironment(), NS_ERROR_FAILURE);

   *aGlobal = mScriptGlobal;
   NS_IF_ADDREF(*aGlobal);
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::ReportScriptError(nsIScriptError *errorObject)
{
   nsresult rv;

   if (errorObject == nsnull)
      return NS_ERROR_NULL_POINTER;

   // Get the console service, where we're going to register the error.
   nsCOMPtr<nsIConsoleService> consoleService
      (do_GetService("mozilla.consoleservice.1"));

   if (consoleService != nsnull)
      {
      rv = consoleService->LogMessage(errorObject);
      if (NS_SUCCEEDED(rv))
         {
         return NS_OK;
         }
      else
         {
         return rv;
         }
      }
   else
      {
      return NS_ERROR_NOT_AVAILABLE;
      }
}

//*****************************************************************************
// nsDocShell::nsIRefreshURI
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::RefreshURI(nsIURI *aURI, PRInt32 aDelay, PRBool aRepeat)
{
   NS_ENSURE_ARG(aURI);

   nsRefreshTimer* refreshTimer = new nsRefreshTimer();
   NS_ENSURE_TRUE(refreshTimer, NS_ERROR_OUT_OF_MEMORY);

   nsCOMPtr<nsISupports> dataRef = refreshTimer; // Get the ref count to 1

   refreshTimer->mDocShell = this;
   refreshTimer->mURI = aURI;
   refreshTimer->mDelay = aDelay;
   refreshTimer->mRepeat = aRepeat;

   if (!mRefreshURIList)
   {
      NS_ENSURE_SUCCESS(NS_NewISupportsArray(getter_AddRefs(mRefreshURIList)),
         NS_ERROR_FAILURE);
   }
   
   nsCOMPtr<nsITimer> timer = do_CreateInstance("component://netscape/timer");
   NS_ENSURE_TRUE(timer, NS_ERROR_FAILURE);
    
   mRefreshURIList->AppendElement(timer);    // owning timer ref
   timer->Init(refreshTimer, aDelay);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::CancelRefreshURITimers()
{
   if (!mRefreshURIList) return NS_OK;

   PRUint32 n;
   mRefreshURIList->Count(&n);

   while (n)
   {
      nsCOMPtr<nsISupports> element;
      mRefreshURIList->GetElementAt(0, getter_AddRefs(element));
      nsCOMPtr<nsITimer> timer(do_QueryInterface(element));

      mRefreshURIList->RemoveElementAt(0);    // bye bye owning timer ref

      if (timer)
         timer->Cancel();
      n--;
   }

   return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIContentViewerContainer
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::Embed(nsIContentViewer* aContentViewer, 
                                    const char      * aCommand,
                                    nsISupports     * aExtraInfo)
{
#ifdef SH_IN_FRAMES
	// Save the LayoutHistoryState of the previous document, before
	// setting up new document
	PersistLayoutHistoryState();

   nsresult rv = SetupNewViewer(aContentViewer);

   // XXX What if SetupNewViewer fails?

   OSHE = LSHE;
/*   
   PRBool updateHistory = PR_TRUE;

    // Determine if this type of load should update history   
    switch(mLoadType)
    {
    case nsIDocShellLoadInfo::loadHistory:
    case nsIDocShellLoadInfo::loadReloadNormal:
    case nsIDocShellLoadInfo::loadReloadBypassCache:
    case nsIDocShellLoadInfo::loadReloadBypassProxy:
    case nsIDocShellLoadInfo::loadReloadBypassProxyAndCache:
        updateHistory = PR_FALSE;
        break;
    default:
        break;
    } 
*/
  if (OSHE) {
    nsCOMPtr<nsILayoutHistoryState> layoutState;

    rv = OSHE->GetLayoutHistoryState(getter_AddRefs(layoutState));
    if (layoutState && (mLoadType != nsIDocShellLoadInfo::loadNormalReplace)) {
      // This is a SH load. That's why there is a LayoutHistoryState in OSHE
      nsCOMPtr<nsIPresShell> presShell;
      rv = GetPresShell(getter_AddRefs(presShell));
      if (NS_SUCCEEDED(rv) && presShell) {
        rv = presShell->SetHistoryState(layoutState);
      }
    }
  }
    return NS_OK;
#else
   return SetupNewViewer(aContentViewer);
#endif  /* SH_IN_FRAMES */
}

//*****************************************************************************
// nsDocShell::nsIWebProgressListener
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::OnProgressChange(nsIWebProgress *aProgress, nsIRequest *aRequest,
                             PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress,
                             PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::OnStateChange(nsIWebProgress *aProgress, nsIRequest *aRequest,
                          PRInt32 aStateFlags, nsresult aStatus)
{
  // Clear the LSHE reference to indicate document loading has finished
  // one way or another.
  if ((aStateFlags & flag_stop) && (aStateFlags & flag_is_network)) {
    LSHE = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::OnLocationChange(nsIWebProgress *aProgress, 
                             nsIRequest *aRequest,
                             nsIURI *aURI)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsDocShell::OnStatusChange(nsIWebProgress* aWebProgress,
                           nsIRequest* aRequest,
                           nsresult aStatus,
                           const PRUnichar* aMessage)
{
    return NS_OK;
}

//*****************************************************************************
// nsDocShell: Content Viewer Management
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::EnsureContentViewer()
{
   if(mContentViewer)
      return NS_OK;

   return CreateAboutBlankContentViewer();
}

NS_IMETHODIMP nsDocShell::EnsureDeviceContext()
{
   if(mDeviceContext)
      return NS_OK;

   mDeviceContext = do_CreateInstance(kDeviceContextCID);
   NS_ENSURE_TRUE(mDeviceContext, NS_ERROR_FAILURE);

   nsCOMPtr<nsIWidget> widget;
   GetMainWidget(getter_AddRefs(widget));
   NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

   mDeviceContext->Init(widget->GetNativeData(NS_NATIVE_WIDGET));
   float dev2twip;
   mDeviceContext->GetDevUnitsToTwips(dev2twip);
   mDeviceContext->SetDevUnitsToAppUnits(dev2twip);
   float twip2dev;
   mDeviceContext->GetTwipsToDevUnits(twip2dev);
   mDeviceContext->SetAppUnitsToDevUnits(twip2dev);
   mDeviceContext->SetGamma(1.0f);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::CreateAboutBlankContentViewer()
{
   // XXX
   NS_ERROR("Not Implemented yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::CreateContentViewer(const char* aContentType, 
   nsIChannel* aOpenedChannel, nsIStreamListener** aContentHandler)
{
   // Can we check the content type of the current content viewer
   // and reuse it without destroying it and re-creating it?

   nsCOMPtr<nsILoadGroup> loadGroup(do_GetInterface(mLoadCookie));
   NS_ENSURE_TRUE(loadGroup, NS_ERROR_FAILURE);

   // Instantiate the content viewer object
   nsCOMPtr<nsIContentViewer> viewer;
   if(NS_FAILED(NewContentViewerObj(aContentType, aOpenedChannel, loadGroup, 
      aContentHandler, getter_AddRefs(viewer))))
      return NS_ERROR_FAILURE;

   // let's try resetting the load group if we need to...
   nsCOMPtr<nsILoadGroup> currentLoadGroup;
   NS_ENSURE_SUCCESS(aOpenedChannel->GetLoadGroup(getter_AddRefs(currentLoadGroup)),
      NS_ERROR_FAILURE);

   if(currentLoadGroup.get() != loadGroup.get())
      {
      nsLoadFlags loadAttribs = 0;

      //Cancel any URIs that are currently loading...
      /// XXX: Need to do this eventually      Stop();
      //
      // Retarget the document to this loadgroup...
      //
      if(currentLoadGroup)
         currentLoadGroup->RemoveChannel(aOpenedChannel, nsnull, nsnull, nsnull);
      
      aOpenedChannel->SetLoadGroup(loadGroup);

      // Mark the channel as being a document URI...
      aOpenedChannel->GetLoadAttributes(&loadAttribs);
      loadAttribs |= nsIChannel::LOAD_DOCUMENT_URI;

      aOpenedChannel->SetLoadAttributes(loadAttribs);

      loadGroup->AddChannel(aOpenedChannel, nsnull);
      }
#ifdef SH_IN_FRAMES
   NS_ENSURE_SUCCESS(Embed(viewer, "", (nsISupports *) nsnull), NS_ERROR_FAILURE);
#else
   NS_ENSURE_SUCCESS(SetupNewViewer(viewer), NS_ERROR_FAILURE);
#endif  /* SH_IN_FRAMES */

   mEODForCurrentDocument = PR_FALSE; // clear the current flag
   return NS_OK;
}

nsresult nsDocShell::NewContentViewerObj(const char* aContentType,
   nsIChannel* aOpenedChannel, nsILoadGroup* aLoadGroup, 
   nsIStreamListener** aContentHandler, nsIContentViewer** aViewer)
{
  //XXX This should probably be some category thing....
  char id[256];
  PR_snprintf(id, sizeof(id), NS_DOCUMENT_LOADER_FACTORY_PROGID_PREFIX "%s/%s",
             (const char*)((viewSource == mViewMode) ? "view-source" : "view"),
             aContentType);

  // Create an instance of the document-loader-factory
  nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory(do_CreateInstance(id));
  if(!docLoaderFactory)
  {
    // try again after loading plugins
    nsresult err;
    NS_WITH_SERVICE(nsIPluginHost, pluginHost, kPluginManagerCID, &err);

    if(NS_FAILED(err))
      return NS_ERROR_FAILURE;

    pluginHost->LoadPlugins();

    docLoaderFactory = do_CreateInstance(id);

    if(!docLoaderFactory)
      return NS_ERROR_FAILURE;
  }

  // Now create an instance of the content viewer
  NS_ENSURE_SUCCESS(docLoaderFactory->CreateInstance(
                    (viewSource == mViewMode) ? "view-source" : "view",
                    aOpenedChannel, aLoadGroup, aContentType,
                    NS_STATIC_CAST(nsIContentViewerContainer*, this), nsnull, 
                    aContentHandler, aViewer), NS_ERROR_FAILURE);

  (*aViewer)->SetContainer(NS_STATIC_CAST(nsIContentViewerContainer*, this));

  return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetupNewViewer(nsIContentViewer* aNewViewer)
{
   //
   // Copy content viewer state from previous or parent content viewer.
   //
   // The following logic is mirrored in nsHTMLDocument::StartDocumentLoad!
   //
   // Do NOT to maintain a reference to the old content viewer outside
   // of this "copying" block, or it will not be destroyed until the end of
   // this routine and all <SCRIPT>s and event handlers fail! (bug 20315)
   //
   // In this block of code, if we get an error result, we return it
   // but if we get a null pointer, that's perfectly legal for parent
   // and parentContentViewer.
   //

   PRInt32 x = 0;
   PRInt32 y = 0;
   PRInt32 cx = 0;
   PRInt32 cy = 0;

   // This will get the size from the current content viewer or from the
   // Init settings
   GetPositionAndSize(&x, &y, &cx, &cy);

   nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
   NS_ENSURE_SUCCESS(GetSameTypeParent(getter_AddRefs(parentAsItem)), 
      NS_ERROR_FAILURE);
   nsCOMPtr<nsIDocShell> parent(do_QueryInterface(parentAsItem));

   if(mContentViewer || parent)
      {  
      nsCOMPtr<nsIMarkupDocumentViewer> oldMUDV;
      if(mContentViewer)
         { 
         // Get any interesting state from old content viewer
         // XXX: it would be far better to just reuse the document viewer ,
         //      since we know we're just displaying the same document as before
         oldMUDV = do_QueryInterface(mContentViewer);
         }
      else
         { 
         // No old content viewer, so get state from parent's content viewer
         nsCOMPtr<nsIContentViewer> parentContentViewer;
         parent->GetContentViewer(getter_AddRefs(parentContentViewer));
         oldMUDV = do_QueryInterface(parentContentViewer);
         }

      nsXPIDLString defaultCharset;
      nsXPIDLString forceCharset;
      nsXPIDLString hintCharset;
      PRInt32 hintCharsetSource;

      nsCOMPtr<nsIMarkupDocumentViewer> newMUDV(do_QueryInterface(aNewViewer));
      if(oldMUDV && newMUDV)
         {
         NS_ENSURE_SUCCESS(oldMUDV->GetDefaultCharacterSet(getter_Copies(defaultCharset)),
            NS_ERROR_FAILURE);
         NS_ENSURE_SUCCESS(oldMUDV->GetForceCharacterSet(getter_Copies(forceCharset)),
            NS_ERROR_FAILURE);
         NS_ENSURE_SUCCESS(oldMUDV->GetHintCharacterSet(getter_Copies(hintCharset)),
            NS_ERROR_FAILURE);
         NS_ENSURE_SUCCESS(oldMUDV->GetHintCharacterSetSource(&hintCharsetSource),
            NS_ERROR_FAILURE);

         // set the old state onto the new content viewer
         NS_ENSURE_SUCCESS(newMUDV->SetDefaultCharacterSet(defaultCharset),
            NS_ERROR_FAILURE);
         NS_ENSURE_SUCCESS(newMUDV->SetForceCharacterSet(forceCharset), 
            NS_ERROR_FAILURE);
         NS_ENSURE_SUCCESS(newMUDV->SetHintCharacterSet(hintCharset),
            NS_ERROR_FAILURE);
         NS_ENSURE_SUCCESS(newMUDV->SetHintCharacterSetSource(hintCharsetSource),
            NS_ERROR_FAILURE);
         }
      }
   mContentViewer = nsnull;
   // End copying block (Don't hold content/document viewer ref beyond here!!)

   if(mScriptContext)
      mScriptContext->GC();

   mContentViewer = aNewViewer;

   nsCOMPtr<nsIWidget> widget;
   NS_ENSURE_SUCCESS(GetMainWidget(getter_AddRefs(widget)), NS_ERROR_FAILURE);

   nsRect bounds(x, y, cx, cy);
   NS_ENSURE_SUCCESS(EnsureDeviceContext(), NS_ERROR_FAILURE);
   if(NS_FAILED(mContentViewer->Init(widget,
      mDeviceContext, bounds)))
      {
      mContentViewer = nsnull;
      NS_ERROR("ContentViewer Initialization failed");
      return NS_ERROR_FAILURE;
      }

// XXX: It looks like the LayoutState gets restored again in Embed()
//      right after the call to SetupNewViewer(...)
#ifndef SH_IN_FRAMES
    // Restore up any HistoryLayoutState this page might have.
    nsresult rv = NS_OK;
	PRBool updateHistory = PR_TRUE;

    // Determine if this type of load should update history   
    switch(mLoadType)
    {
    case nsIDocShellLoadInfo::loadHistory:
    case nsIDocShellLoadInfo::loadReloadNormal:
    case nsIDocShellLoadInfo::loadReloadBypassCache:
    case nsIDocShellLoadInfo::loadReloadBypassProxy:
    case nsIDocShellLoadInfo::loadReloadBypassProxyAndCache:
        updateHistory = PR_FALSE;
        break;
    default:
        break;
    } 
    if (mSessionHistory && !updateHistory) {
      PRInt32 index = 0;
      mSessionHistory->GetIndex(&index);
      if (-1 < index) {
 
        nsCOMPtr<nsISHEntry> entry;
        rv = mSessionHistory->GetEntryAtIndex(index, PR_FALSE, getter_AddRefs(entry));
        if (NS_SUCCEEDED(rv) && entry) {
 
          nsCOMPtr<nsILayoutHistoryState> layoutState;
          rv = entry->GetLayoutHistoryState(getter_AddRefs(layoutState));
          if (NS_SUCCEEDED(rv) && layoutState) {
 
            nsCOMPtr<nsIPresShell> presShell;
            rv = GetPresShell(getter_AddRefs(presShell));
            if (NS_SUCCEEDED(rv) && presShell) {
 
              rv = presShell->SetHistoryState(layoutState);
            }
          }
        }
      }
    }
#endif  /* SH_IN_FRAMES */

   mContentViewer->Show();

   // Now that we have switched documents, forget all of our children
   DestroyChildren();

   return NS_OK;
}

//*****************************************************************************
// nsDocShell: Site Loading
//*****************************************************************************   
#ifdef SH_IN_FRAMES
NS_IMETHODIMP nsDocShell::InternalLoad(nsIURI* aURI, nsIURI* aReferrer,
   nsISupports* aOwner, const char* aWindowTarget, nsIInputStream* aPostData, 
   nsDocShellInfoLoadType aLoadType, nsISHEntry * aSHEntry)
#else
NS_IMETHODIMP nsDocShell::InternalLoad(nsIURI* aURI, nsIURI* aReferrer,
   nsISupports* aOwner, const char* aWindowTarget, nsIInputStream* aPostData, 
   nsDocShellInfoLoadType aLoadType)
#endif
{
    // Check to see if the new URI is an anchor in the existing document.
  if (aLoadType == nsIDocShellLoadInfo::loadNormal ||
    aLoadType == nsIDocShellLoadInfo::loadNormalReplace ||
    aLoadType == nsIDocShellLoadInfo::loadHistory ||
    aLoadType == nsIDocShellLoadInfo::loadLink)
    {
        PRBool wasAnchor = PR_FALSE;
        NS_ENSURE_SUCCESS(ScrollIfAnchor(aURI, &wasAnchor), NS_ERROR_FAILURE);
        if(wasAnchor)
        {
            mLoadType = aLoadType;
            OnNewURI(aURI, nsnull, mLoadType);
		    /* Clear out LSHE so that further anchor visits get
			 * recorded in SH and SH won't misbehave. i think this is  
			 * sufficient for now to take care of any Sh mis-behaviors.
			 * Other option is to call OnEndDocumentLoad() with a dummy channel
			 * and uriloader and letting everybody know that we are done
			 *  with this target load. I don't think that is necessay
			 * considering that nsIDocLoaderObserver is on its way out.
			 *  Hopefully  the following is sufficient.
			 */
			LSHE = nsnull;
			/* Set the title for the SH entry for this target url. so that
			 * SH menus in go/back/forward buttons won't be empty for this.
			 */
		    if(mSessionHistory)
			{
               PRInt32 index = -1;
               mSessionHistory->GetIndex(&index);
               nsCOMPtr<nsISHEntry>   shEntry;
               mSessionHistory->GetEntryAtIndex(index, PR_FALSE, getter_AddRefs(shEntry));
               if(shEntry)
                 shEntry->SetTitle(mTitle.GetUnicode());      
			}
   
            return NS_OK;
        }
    }

    NS_ENSURE_SUCCESS(StopLoad(), NS_ERROR_FAILURE);
    // Cancel any timers that were set for this loader.
    CancelRefreshURITimers();

    mLoadType = aLoadType;
#ifdef SH_IN_FRAMES
// XXX: I think that LSHE should *always* be set to the new Entry.
//      Even if it is null...
//   if (aSHEntry)
	   LSHE = aSHEntry;
#endif

    nsURILoadCommand  loadCmd = nsIURILoader::viewNormal;
    if(nsIDocShellLoadInfo::loadLink == aLoadType)
        loadCmd = nsIURILoader::viewUserClick;
    NS_ENSURE_SUCCESS(DoURILoad(aURI, aReferrer, aOwner, loadCmd, aWindowTarget, 
        aPostData), NS_ERROR_FAILURE);

    return NS_OK;
}

NS_IMETHODIMP nsDocShell::CreateFixupURI(const PRUnichar* aStringURI, 
   nsIURI** aURI)
{
   *aURI = nsnull;
   nsAutoString uriString(aStringURI);
   uriString.Trim(" ");  // Cleanup the empty spaces that might be on each end.

   // Just try to create an URL out of it
   NS_NewURI(aURI, uriString, nsnull);
   if(*aURI)
      return NS_OK;

   // Check for if it is a file URL
   FileURIFixup(uriString.GetUnicode(), aURI);
   if(*aURI)
      return NS_OK;

   // See if it is a keyword
   KeywordURIFixup(uriString.GetUnicode(), aURI);
   if(*aURI)
      return NS_OK;

   // See if a protocol needs to be added
   PRInt32 checkprotocol = uriString.Find("://",0);
   // if no scheme (protocol) is found, assume http or ftp.
   if (checkprotocol == -1) {
      // find host name
      PRInt32 hostPos = uriString.FindCharInSet("./:");
      if (hostPos == -1) 
         hostPos = uriString.Length();

      // extract host name
      nsAutoString hostSpec;
      uriString.Left(hostSpec, hostPos);

      // insert url spec corresponding to host name
      if (hostSpec.EqualsIgnoreCase("ftp")) 
         uriString.InsertWithConversion("ftp://", 0, 6);
      else 
         uriString.InsertWithConversion("http://", 0, 7);
   } // end if checkprotocol
   return NS_NewURI(aURI, uriString, nsnull);
}

NS_IMETHODIMP nsDocShell::FileURIFixup(const PRUnichar* aStringURI, 
   nsIURI** aURI)
{
   nsAutoString uriSpecIn(aStringURI);
   nsAutoString uriSpecOut(aStringURI);

   ConvertFileToStringURI(uriSpecIn, uriSpecOut);

   if(0 == uriSpecOut.Find("file:", 0))
      {
      // if this is file url, we need to  convert the URI
      // from Unicode to the FS charset
      nsCAutoString inFSCharset;
      NS_ENSURE_SUCCESS(ConvertStringURIToFileCharset(uriSpecOut, inFSCharset),
         NS_ERROR_FAILURE);

      if(NS_SUCCEEDED(NS_NewURI(aURI, inFSCharset.GetBuffer(), nsnull)))
         return NS_OK;
      } 
   return NS_ERROR_FAILURE;
}

#define FILE_PROTOCOL "file://"

NS_IMETHODIMP nsDocShell::ConvertFileToStringURI(nsString& aIn, nsString& aOut)
{
#ifdef XP_PC
   // Check for \ in the url-string or just a drive (PC)
   if(kNotFound != aIn.FindChar(PRUnichar('\\')) || ((aIn.Length() == 2 ) && (aIn.Last() == PRUnichar(':') || aIn.Last() == PRUnichar('|'))))
      {
#elif XP_UNIX
   // Check if it starts with / or \ (UNIX)
   const PRUnichar * up = aIn.GetUnicode();
   if((PRUnichar('/') == *up) || (PRUnichar('\\') == *up))
      {
#else
   if(0) 
      {  
      // Do nothing (All others for now) 
#endif

#ifdef XP_PC
      // Translate '\' to '/'
      aOut.ReplaceChar(PRUnichar('\\'), PRUnichar('/'));
      aOut.ReplaceChar(PRUnichar(':'), PRUnichar('|'));
#endif

      // Build the file URL
      aOut.InsertWithConversion(FILE_PROTOCOL,0);
      }

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::ConvertStringURIToFileCharset(nsString& aIn, 
   nsCString& aOut)
{
   aOut = "";
   // for file url, we need to convert the nsString to the file system
   // charset before we pass to NS_NewURI
   static nsAutoString fsCharset;
   // find out the file system charset first
   if(0 == fsCharset.Length())
      {
      fsCharset.AssignWithConversion("ISO-8859-1"); // set the fallback first.
      nsCOMPtr<nsIPlatformCharset> plat(do_GetService(kPlatformCharsetCID));
      NS_ENSURE_TRUE(plat, NS_ERROR_FAILURE);
      NS_ENSURE_SUCCESS(plat->GetCharset(kPlatformCharsetSel_FileName, fsCharset),
         NS_ERROR_FAILURE);
      }
   // We probably should cache ccm here.
   // get a charset converter from the manager
   nsCOMPtr<nsICharsetConverterManager> ccm(do_GetService(kCharsetConverterManagerCID));
   NS_ENSURE_TRUE(ccm, NS_ERROR_FAILURE);
   
   nsCOMPtr<nsIUnicodeEncoder> fsEncoder;
   NS_ENSURE_SUCCESS(ccm->GetUnicodeEncoder(&fsCharset, 
      getter_AddRefs(fsEncoder)), NS_ERROR_FAILURE);

   PRInt32 bufLen = 0;
   NS_ENSURE_SUCCESS(fsEncoder->GetMaxLength(aIn.GetUnicode(), aIn.Length(),
      &bufLen), NS_ERROR_FAILURE);
   aOut.SetCapacity(bufLen+1);
   PRInt32 srclen = aIn.Length();
   NS_ENSURE_SUCCESS(fsEncoder->Convert(aIn.GetUnicode(), &srclen, 
      (char*)aOut.GetBuffer(), &bufLen), NS_ERROR_FAILURE);

   ((char*)aOut.GetBuffer())[bufLen]='\0';
   aOut.SetLength(bufLen);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::KeywordURIFixup(const PRUnichar* aStringURI, 
   nsIURI** aURI)
{
   NS_ENSURE_STATE(mPrefs);

   PRBool keywordsEnabled = PR_FALSE;
   NS_ENSURE_SUCCESS(mPrefs->GetBoolPref("keyword.enabled", &keywordsEnabled),
      NS_ERROR_FAILURE);

   if(!keywordsEnabled)
      return NS_ERROR_FAILURE;

   // These are keyword formatted strings
   // "what is mozilla"
   // "what is mozilla?"
   // "?mozilla"
   // "?What is mozilla"

   // These are not keyword formatted strings
   // "www.blah.com" - anything with a dot in it 
   // "nonQualifiedHost:80" - anything with a colon in it
   // "nonQualifiedHost?"
   // "nonQualifiedHost?args"
   // "nonQualifiedHost?some args"

   nsAutoString uriString(aStringURI);
   if(uriString.FindChar('.') == -1 && uriString.FindChar(':') == -1)
      {
      PRInt32 qMarkLoc = uriString.FindChar('?');
      PRInt32 spaceLoc = uriString.FindChar(' ');

      PRBool keyword = PR_FALSE;
      if(qMarkLoc == 0)
         keyword = PR_TRUE;
      else if((spaceLoc > 0) && ((qMarkLoc == -1) || (spaceLoc < qMarkLoc)))
         keyword = PR_TRUE;

      if(keyword)
         {
         nsCAutoString keywordSpec("keyword:");
         char *utf8Spec = uriString.ToNewUTF8String();
         if(utf8Spec)
            {
            char* escapedUTF8Spec = nsEscape(utf8Spec, url_Path);
            if(escapedUTF8Spec) 
               {
               keywordSpec.Append(escapedUTF8Spec);
               NS_NewURI(aURI, keywordSpec.GetBuffer(), nsnull);
               nsMemory::Free(escapedUTF8Spec);
               } // escapedUTF8Spec
            nsMemory::Free(utf8Spec);
            } // utf8Spec
         } // keyword 
      } // FindChar

   if(*aURI)
      return NS_OK;

   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::GetCurrentDocumentOwner(nsISupports** aOwner)
{
    nsresult rv;
    *aOwner = nsnull;
    nsCOMPtr<nsIDocument> document;
    //-- Get the current document
    if (mContentViewer)
    {
        nsCOMPtr<nsIDocumentViewer> docViewer(do_QueryInterface(mContentViewer));
        if (!docViewer) return NS_ERROR_FAILURE;
        rv = docViewer->GetDocument(*getter_AddRefs(document));
    }
    else //-- If there's no document loaded yet, look at the parent (frameset)
    {
        nsCOMPtr<nsIDocShellTreeItem> parentItem;
        rv = GetSameTypeParent(getter_AddRefs(parentItem));
        if (NS_FAILED(rv) || !parentItem) return rv;
        nsCOMPtr<nsIDOMWindow> parentWindow(do_GetInterface(parentItem));
        if (!parentWindow) return NS_OK;
        nsCOMPtr<nsIDOMDocument> parentDomDoc;
        rv = parentWindow->GetDocument(getter_AddRefs(parentDomDoc));
        if (!parentDomDoc) return NS_OK;
        document = do_QueryInterface(parentDomDoc);
    }

    //-- Get the document's principal
    nsCOMPtr<nsIPrincipal> principal;
    rv = document->GetPrincipal(getter_AddRefs(principal));
    if (NS_FAILED(rv) || !principal) return NS_ERROR_FAILURE;
    rv = principal->QueryInterface(NS_GET_IID(nsISupports),(void**)aOwner);
    return rv;
}

NS_IMETHODIMP nsDocShell::DoURILoad(nsIURI* aURI, nsIURI* aReferrerURI,  
   nsISupports* aOwner, nsURILoadCommand aLoadCmd, const char* aWindowTarget, 
   nsIInputStream* aPostData)
{

  // if the load cmd is a user click....and we are supposed to try using
  // external default protocol handlers....then try to see if we have one for
  // this protocol
  if (mUseExternalProtocolHandler && aLoadCmd == nsIURILoader::viewUserClick)
  {
    nsXPIDLCString urlScheme;
    aURI->GetScheme(getter_Copies(urlScheme));
    // don't do it for javascript urls!
    if (urlScheme && nsCRT::strcasecmp("javascript", urlScheme))
    {
      nsCOMPtr<nsIExternalProtocolService> extProtService (do_GetService(NS_EXTERNALPROTOCOLSERVICE_PROGID));
      PRBool haveHandler = PR_FALSE;
      extProtService->ExternalProtocolHandlerExists(urlScheme, &haveHandler);
      if (haveHandler)
        return extProtService->LoadUrl(aURI);
    }
  }
   nsCOMPtr<nsIURILoader> uriLoader(do_GetService(NS_URI_LOADER_PROGID));
   NS_ENSURE_TRUE(uriLoader, NS_ERROR_FAILURE);

   // we need to get the load group from our load cookie so we can pass it into open uri...
   nsCOMPtr<nsILoadGroup> loadGroup;
   NS_ENSURE_SUCCESS(
      uriLoader->GetLoadGroupForContext(NS_STATIC_CAST(nsIDocShell*, this),
      getter_AddRefs(loadGroup)), NS_ERROR_FAILURE);

   // open a channel for the url
   nsCOMPtr<nsIChannel> channel;
   nsresult rv;
   rv = NS_OpenURI(getter_AddRefs(channel), aURI, nsnull, loadGroup, 
                     NS_STATIC_CAST(nsIInterfaceRequestor*, this));
   if(NS_FAILED(rv))
      {
      if(NS_ERROR_DOM_RETVAL_UNDEFINED == rv) // if causing the channel changed the
         return NS_OK;                        // dom and there is nothing else to do
      else
         return NS_ERROR_FAILURE;
      }
   channel->SetOriginalURI(aURI);
   
   nsCOMPtr<nsIHTTPChannel> httpChannel(do_QueryInterface(channel));
   if(httpChannel)
      {
      // figure out if we need to set the post data stream on the channel...
      // right now, this is only done for http channels.....
      if(aPostData)
         {
         // XXX it's a bit of a hack to rewind the postdata stream here but
         // it has to be done in case the post data is being reused multiple
         // times.
         nsCOMPtr<nsIRandomAccessStore> postDataRandomAccess(do_QueryInterface(aPostData));
         if (postDataRandomAccess)
         {
             postDataRandomAccess->Seek(PR_SEEK_SET, 0);
         }

	     nsCOMPtr<nsIAtom> method = NS_NewAtom ("POST");
         httpChannel->SetRequestMethod(method);
         httpChannel->SetUploadStream(aPostData);
         }
      // Set the referrer explicitly
      if(aReferrerURI) // Referrer is currenly only set for link clicks here.
         httpChannel->SetReferrer(aReferrerURI, 
                                    nsIHTTPChannel::REFERRER_LINK_CLICK);
      }
      else
      {
        // If an owner was not provided, we want to inherit the principal from the current document iff we 
        // are dealing with a JS or a data url.
        nsCOMPtr<nsISupports> owner = aOwner;
        nsCOMPtr<nsIStreamIOChannel> ioChannel(do_QueryInterface(channel));
        if(ioChannel) // Might be a javascript: URL load, need to set owner
        {
           static const char jsSchemeName[] = "javascript";
           char* scheme;
           aURI->GetScheme(&scheme);
           if (PL_strcasecmp(scheme, jsSchemeName) == 0)
           {
             if (!owner)  // only try to call GetCurrentDocumentOwner if we are a JS url or a data url (hence the code duplication)
               GetCurrentDocumentOwner(getter_AddRefs(owner));
              channel->SetOwner(owner);
           }
           if (scheme)
               nsCRT::free(scheme);
        }
        else
        { // Also set owner for data: URLs
           nsCOMPtr<nsIDataChannel> dataChannel(do_QueryInterface(channel));
           if (dataChannel)
           {
             if (!owner)
               GetCurrentDocumentOwner(getter_AddRefs(owner));
             channel->SetOwner(owner);
           }
        }
      }

   NS_ENSURE_SUCCESS(DoChannelLoad(channel, aLoadCmd, aWindowTarget, uriLoader), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::DoChannelLoad(nsIChannel *aChannel, nsURILoadCommand aLoadCmd, 
                                       const char* aWindowTarget, nsIURILoader *aURILoader)
{
   // Mark the channel as being a document URI...
   nsLoadFlags loadAttribs = 0;
   (void)aChannel->GetLoadAttributes(&loadAttribs);
   loadAttribs |= nsIChannel::LOAD_DOCUMENT_URI;
  
  	switch ( mLoadType )
  	{
  	 case nsIDocShellLoadInfo::loadHistory:
  	 		loadAttribs |= nsIChannel::VALIDATE_NEVER;
  	 		break;
  	 		
  	 case nsIDocShellLoadInfo::loadReloadNormal:
  	 			loadAttribs |= nsIChannel::FORCE_VALIDATION;
  	 		break;
  	 		
  	 case nsIDocShellLoadInfo::loadReloadBypassProxyAndCache:
  	 		loadAttribs |= nsIChannel::FORCE_RELOAD;
  	 		break;
  	 case nsIDocShellLoadInfo::loadRefresh:
  	 		loadAttribs |= nsIChannel::FORCE_RELOAD;
  	 		break;
     case nsIDocShellLoadInfo::loadNormal:
     case nsIDocShellLoadInfo::loadLink:
		   // Set cache checking flags
		   if ( mPrefs )
		   {
		   		PRInt32 prefSetting;
		   		if ( NS_SUCCEEDED( 	mPrefs->GetIntPref( "browser.cache.check_doc_frequency" , &prefSetting) ) )
		   		{
		   			switch ( prefSetting )
		   			{
		   				case 0:
		   					loadAttribs |= nsIChannel::VALIDATE_ONCE_PER_SESSION;
		   					break;
		   				case 1:
		   					loadAttribs |= nsIChannel::VALIDATE_ALWAYS;
		   					break;
		   				case 2:
		   					loadAttribs |= nsIChannel::VALIDATE_NEVER;
		   					break;
		   			}
		   		}
			   }
			break;
   }
   
   (void) aChannel->SetLoadAttributes(loadAttribs);

   NS_ENSURE_SUCCESS(aURILoader->OpenURI(aChannel, aLoadCmd,
      aWindowTarget, NS_STATIC_CAST(nsIDocShell*, this)), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::ScrollIfAnchor(nsIURI* aURI, PRBool* aWasAnchor)
{
    NS_ASSERTION(aURI, "null uri arg");
    NS_ASSERTION(aWasAnchor, "null anchor arg");

    if (aURI == nsnull || aWasAnchor == nsnull)
    {
        return NS_ERROR_FAILURE;
    }

    *aWasAnchor = PR_FALSE;
 
    if (!mCurrentURI)
    {
        return NS_OK;
    }

    nsresult rv;

    // NOTE: we assume URIs are absolute for comparison purposes

    nsXPIDLCString currentSpec;
    NS_ENSURE_SUCCESS(mCurrentURI->GetSpec(getter_Copies(currentSpec)), NS_ERROR_FAILURE);
 
    nsXPIDLCString newSpec;
    NS_ENSURE_SUCCESS(aURI->GetSpec(getter_Copies(newSpec)), NS_ERROR_FAILURE);

    // Search for hash marks in the current URI and the new URI and
    // take a copy of everything to the left of the hash for
    // comparison.

    const char kHash = '#';

    // Split the new URI into a left and right part
    nsAutoString sNew; sNew.AssignWithConversion(newSpec);
    nsAutoString sNewLeft;
    nsAutoString sNewRef;
    PRInt32 hashNew = sNew.FindChar(kHash);
    if (hashNew == 0)
    {
        return NS_OK; // Strange URI 
    }
    else if (hashNew > 0)
    {
        sNew.Left(sNewLeft, hashNew);
        sNew.Right(sNewRef, sNew.Length() - hashNew - 1);
    }
    else
    {
        sNewLeft = sNew;
    }

    // Split the current URI in a left and right part
    nsAutoString sCurrent; sCurrent.AssignWithConversion(currentSpec);
    nsAutoString sCurrentLeft;
    PRInt32 hashCurrent = sCurrent.FindChar(kHash);
    if (hashCurrent == 0)
    {
        return NS_OK; // Strange URI 
    }
    else if (hashCurrent > 0)
    {
        sCurrent.Left(sCurrentLeft, hashCurrent);
    }
    else
    {
        sCurrentLeft = sCurrent;
    }

    // Exit when there are no anchors
    if (hashNew <= 0 && hashCurrent <= 0)
    {
        return NS_OK;
    }

    // Compare the URIs.
    //
    // NOTE: this is a case sensitive comparison because some parts of the
    // URI are case sensitive, and some are not. i.e. the domain name
    // is case insensitive but the the paths are not.
    //
    // This means that comparing "http://www.ABC.com/" to "http://www.abc.com/"
    // will fail this test.
 
    if (sCurrentLeft.CompareWithConversion(sNewLeft, PR_FALSE, -1) != 0)
    {
        return NS_OK; // URIs not the same
    }

    // Both the new and current URIs refer to the same page. We can now
    // browse to the hash stored in the new URI.

    if (!sNewRef.IsEmpty())
    {
        nsCOMPtr<nsIPresShell> shell = nsnull;
        rv = GetPresShell(getter_AddRefs(shell));
        if (NS_SUCCEEDED(rv) && shell)
        {
            *aWasAnchor = PR_TRUE;

            char *str = sNewRef.ToNewCString();

            // nsUnescape modifies the string that is passed into it.
            nsUnescape(str);

            sNewRef.AssignWithConversion(str);

            nsMemory::Free(str);

            rv = shell->GoToAnchor(sNewRef);
        }
    }
    else
    {
        // A bit of a hack - scroll to the top of the page.
        SetCurScrollPosEx(0, 0);
        *aWasAnchor = PR_TRUE;
    }

    return NS_OK;
}


NS_IMETHODIMP
nsDocShell::OnNewURI(nsIURI *aURI, nsIChannel *aChannel, nsDocShellInfoLoadType aLoadType)
{
    NS_ASSERTION(aURI, "uri is null");

    UpdateCurrentGlobalHistory();
    PRBool updateHistory = PR_TRUE;

    // Determine if this type of load should update history   
    switch(aLoadType)
    {
    case nsIDocShellLoadInfo::loadHistory:
    case nsIDocShellLoadInfo::loadReloadNormal:
    case nsIDocShellLoadInfo::loadReloadBypassCache:
    case nsIDocShellLoadInfo::loadReloadBypassProxy:
    case nsIDocShellLoadInfo::loadReloadBypassProxyAndCache:
        updateHistory = PR_FALSE;
        break;

    case nsIDocShellLoadInfo::loadNormal:
    case nsIDocShellLoadInfo::loadRefresh:
    case nsIDocShellLoadInfo::loadNormalReplace:
    case nsIDocShellLoadInfo::loadLink:
        break;

    default:
        NS_ERROR("Need to update case");
        break;
    } 

  if (updateHistory) { // Page load not from SH
    // Update session history if necessary...
    if (!LSHE && (mItemType == typeContent)) {
      /* This is  a fresh page getting loaded for the first time
       *.Create a Entry for it and add it to SH, if this is the
       * rootDocShell
       */
      (void) AddToSessionHistory(aURI, aChannel, getter_AddRefs(LSHE));
    }

    // Update Global history if necessary...
    updateHistory = PR_FALSE;
    ShouldAddToGlobalHistory(aURI, &updateHistory);
    if(updateHistory) {
      AddToGlobalHistory(aURI);
    }
  }

   SetCurrentURI(aURI);
   nsCOMPtr<nsIHTTPChannel> httpChannel(do_QueryInterface(aChannel));
   if(httpChannel)
   {
      nsCOMPtr<nsIURI> referrer;
      httpChannel->GetReferrer(getter_AddRefs(referrer));
      SetReferrerURI(referrer);

      nsXPIDLCString refreshHeader;
      nsCOMPtr<nsIAtom> refreshAtom ( dont_AddRef( NS_NewAtom("refresh") ) );

      httpChannel -> GetResponseHeader (refreshAtom, getter_Copies (refreshHeader));

      if (refreshHeader)
      {
        nsCOMPtr<nsIURI> baseURI = mCurrentURI;

        PRInt32 millis = -1;
        nsAutoString uriAttrib;
        nsString result; result.AssignWithConversion (refreshHeader);

        PRInt32 semiColon = result.FindCharInSet(";,");
        nsAutoString token;
        if (semiColon > -1)
            result.Left(token, semiColon);
        else
            token = result;
  
        PRBool done = PR_FALSE;
        while (!done && !token.IsEmpty()) {
            token.CompressWhitespace();
            if (millis == -1 && nsCRT::IsAsciiDigit(token.First())) {
                PRInt32 i = 0;
                PRUnichar value = nsnull;
                for ( ; i < (PRInt32) token.Length (); i++)
                {
                    value = token[i];
                    if (!nsCRT::IsAsciiDigit(value)) {
                        i = -1;
                        break;
                     }
                }
            
                if (i > -1) {
                    PRInt32 err;
                    millis = token.ToInteger(&err) * 1000;
                } else {
                    done = PR_TRUE;
                }
            } else {
                   done = PR_TRUE;
            }
            if (done) {
                PRInt32 loc = token.FindChar('=');
                    if (loc > -1)
                        token.Cut(0, loc+1);
                     token.Trim(" \"'");
                     uriAttrib = token;
            } else {
                // Increment to the next token.
                    if (semiColon > -1) {
                        semiColon++;
                        PRInt32 semiColon2 = result.FindCharInSet(";,", semiColon);
                        if (semiColon2 == -1) semiColon2 = result.Length();
                        result.Mid(token, semiColon, semiColon2 - semiColon);
                        semiColon = semiColon2;
                     } else {
                         done = PR_TRUE;
                    }
            }
        } // end while

        nsCOMPtr<nsIURI> uri;
        if (!uriAttrib.Length()) {
            uri = baseURI;
        } else {
            NS_NewURI(getter_AddRefs(uri), uriAttrib, baseURI);
        }

        RefreshURI (uri, millis, PR_FALSE);
      }
   }

   mInitialPageLoad = PR_FALSE;
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::OnLoadingSite(nsIChannel* aChannel)
{
    nsCOMPtr<nsIURI> uri;
    aChannel->GetOriginalURI(getter_AddRefs(uri));
    NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

    OnNewURI(uri, aChannel, mLoadType);

    return NS_OK;
}

void nsDocShell::SetCurrentURI(nsIURI* aURI)
{
   mCurrentURI = aURI; //This assignment addrefs
   
   nsCOMPtr<nsIDocumentLoader> loader(do_GetInterface(mLoadCookie));
   
   NS_ASSERTION(loader, "No document loader");
   if (loader) {
     loader->FireOnLocationChange(nsnull, nsnull, aURI);
   }
}

void nsDocShell::SetReferrerURI(nsIURI* aURI)
{
   mReferrerURI = aURI; // This assigment addrefs
}

//*****************************************************************************
// nsDocShell: Session History
//*****************************************************************************   
PRBool nsDocShell::ShouldAddToSessionHistory(nsIURI* aURI)
{
  nsresult rv;
  nsXPIDLCString buffer;
  nsCAutoString schemeStr;

  rv = aURI->GetScheme(getter_Copies(buffer));
  if (NS_FAILED(rv)) return PR_FALSE;

  schemeStr = buffer;
  if(schemeStr.Equals("about")) {
    rv = aURI->GetPath(getter_Copies(buffer));
    if (NS_FAILED(rv)) return PR_FALSE;

    schemeStr = buffer;
    if(schemeStr.Equals("blank")) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

nsresult nsDocShell::AddToSessionHistory(nsIURI *aURI,
                                         nsIChannel *aChannel,
                                         nsISHEntry **aNewEntry)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsISHEntry> entry;
  PRBool shouldPersist;

  shouldPersist = ShouldAddToSessionHistory(aURI);

  //
  // If the entry is being replaced in SH, then just use the
  // current entry...
  //
  if(mSessionHistory && nsIDocShellLoadInfo::loadNormalReplace == mLoadType) {
    PRInt32 index = 0;
    mSessionHistory->GetIndex(&index);
    mSessionHistory->GetEntryAtIndex(index, PR_FALSE, getter_AddRefs(entry));
  }

  // Create a new entry if necessary.
  if(!entry) {
    entry = do_CreateInstance(NS_SHENTRY_PROGID);

    if (!entry) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // Get the post data
  nsCOMPtr<nsIInputStream> inputStream;
  if (aChannel) {
    nsCOMPtr<nsIHTTPChannel> httpChannel(do_QueryInterface(aChannel));

    if(httpChannel) {
      httpChannel->GetUploadStream(getter_AddRefs(inputStream));
    }
  }

  //Title is set in nsDocShell::SetTitle()
  entry->Create(aURI,           // uri
                nsnull,         // Title
                nsnull,         // DOMDocument
                inputStream,    // Post data stream
                nsnull);        // LayoutHistory state
  //
  // Add the new entry to session history.
  //
  // If no Session History component is available in the parent DocShell
  // heirarchy, then AddChildSHEntry(...) will fail and the new entry
  // will be deleted when it loses scope...
  //
  if (mLoadType != nsIDocShellLoadInfo::loadNormalReplace) {
     if (mSessionHistory)	  
         rv = mSessionHistory->AddEntry(entry, shouldPersist);
     else 
         rv = AddChildSHEntry(nsnull, entry, mChildOffset);
  }

  // Return the new SH entry...
  if (aNewEntry) {
    *aNewEntry = nsnull;
    if (NS_SUCCEEDED(rv)) {
      *aNewEntry = entry;
      NS_ADDREF(*aNewEntry);
    }
  }

  return rv;
}

 /* 
  * Save the HistoryLayoutState for this page before we leave it.
  */
NS_IMETHODIMP nsDocShell::UpdateCurrentSessionHistory()
{
   nsresult rv = NS_OK;
   if(!mInitialPageLoad && mSessionHistory) {
  
     PRInt32 index = 0;
     mSessionHistory->GetIndex(&index);
     if (-1 < index) {
 
       nsCOMPtr<nsISHEntry> entry;
       rv = mSessionHistory->GetEntryAtIndex(index, PR_FALSE, getter_AddRefs(entry));
       if (NS_SUCCEEDED(rv) && entry) {
 
         nsCOMPtr<nsIPresShell> shell;
         rv = GetPresShell(getter_AddRefs(shell));            
         if (NS_SUCCEEDED(rv) && shell) {
 
           nsCOMPtr<nsILayoutHistoryState> layoutState;
           rv = shell->CaptureHistoryState(getter_AddRefs(layoutState), PR_TRUE);
           if (NS_SUCCEEDED(rv) && layoutState) {
 
             rv = entry->SetLayoutHistoryState(layoutState);
           }
         }
       }
     }
   }
   return rv;
   
}

#ifdef SH_IN_FRAMES
NS_IMETHODIMP nsDocShell::LoadHistoryEntry(nsISHEntry* aEntry, nsDocShellInfoLoadType aLoadType)
#else
NS_IMETHODIMP nsDocShell::LoadHistoryEntry(nsISHEntry* aEntry)
#endif
{
   nsCOMPtr<nsIURI> uri;
   nsCOMPtr<nsIInputStream> postData;
   PRBool repost = PR_TRUE;

   NS_ENSURE_TRUE(aEntry, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(aEntry->GetURI(getter_AddRefs(uri)), NS_ERROR_FAILURE);
   NS_ENSURE_SUCCESS(aEntry->GetPostData(getter_AddRefs(postData)),
      NS_ERROR_FAILURE);

   /* Ask whether to repost form post data */
   if (postData) {
       nsCOMPtr<nsIPrompt> prompter;
       nsCOMPtr<nsIStringBundle> stringBundle;
       GetPromptAndStringBundle(getter_AddRefs(prompter), 
          getter_AddRefs(stringBundle));
 
       if (stringBundle && prompter) {
          nsXPIDLString messageStr;
          nsresult rv = stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("repost").GetUnicode(), 
          getter_Copies(messageStr));
          
		  if (NS_SUCCEEDED(rv) && messageStr) {
             prompter->Confirm(nsnull, messageStr, &repost);
		     if (!repost)
                postData = nsnull;
		  }
	   }
    }
    

#ifdef SH_IN_FRAMES
   NS_ENSURE_SUCCESS(InternalLoad(uri, nsnull, nsnull, nsnull, postData, aLoadType, aEntry),
      NS_ERROR_FAILURE);
#else
   NS_ENSURE_SUCCESS(InternalLoad(uri, nsnull, nsnull, nsnull, postData, nsIDocShellLoadInfo::loadHistory),
      NS_ERROR_FAILURE);
#endif 

   return NS_OK;
}

/*
NS_IMETHODIMP
nsDocShell::GetSHEForChild(PRInt32 aChildOffset, nsISHEntry ** aResult)
{
    if (OSHE) {
		nsCOMPtr<nsISHContainer> container(do_QueryInterface(OSHE));
		if (container)
           return container->GetChildAt(aChildOffset, aResult);
	}
    return NS_ERROR_FAILURE;

}
*/
NS_IMETHODIMP
nsDocShell::PersistLayoutHistoryState()
{
	nsresult rv = NS_OK;
	if (OSHE) {
      nsCOMPtr<nsIPresShell> shell;

      rv = GetPresShell(getter_AddRefs(shell));            
      if (NS_SUCCEEDED(rv) && shell) {
         nsCOMPtr<nsILayoutHistoryState> layoutState;
         rv = shell->CaptureHistoryState(getter_AddRefs(layoutState), PR_TRUE);
         if (NS_SUCCEEDED(rv) && layoutState) {
             rv = OSHE->SetLayoutHistoryState(layoutState);
         }
      }

	}
	return rv;
}

#if 0
NS_IMETHODIMP
nsDocShell::CloneAndReplace(nsISHEntry * src, nsISHEntry * cloneRef,
							nsISHEntry * replaceEntry, nsISHEntry * dest)
{
	nsresult result;
	if (!src || !replaceEntry || !cloneRef || !dest)
		return NS_ERROR_FAILURE;
//    NS_ENSURE_ARG_POINTER(dest, NS_ERROR_FAILURE);
//	static  PRBool firstTime = PR_TRUE;
//	static nsISHEntry * rootSHEntry = nsnull;
  
	if (src == cloneRef) {
		// release the original object before assigning a new one.
	   NS_RELEASE(dest);
       dest = replaceEntry;
	}
	else {
    nsCOMPtr<nsIURI> uri;
	nsCOMPtr<nsIInputStream> postdata;
	nsCOMPtr<nsILayoutHistoryState> LHS;
	PRUnichar * title=nsnull;
	nsCOMPtr<nsISHEntry> parent;

	src->GetURI(getter_AddRefs(uri));
	src->GetPostData(getter_AddRefs(postdata));
	src->GetTitle(&title);
	src->GetLayoutHistoryState(getter_AddRefs(LHS));
	//XXX Is this correct? parent is a weak ref in nsISHEntry
	src->GetParent(getter_AddRefs(parent));

	// XXX do we care much about valid values for these uri, title etc....
	dest->SetURI(uri);
	dest->SetPostData(postdata);
	dest->SetLayoutHistoryState(LHS);
	dest->SetTitle(title);
	dest->SetParent(parent);
	}
	/*
	if (firstTime) {
		// Save the root of the hierarchy in the result parameter
		rootSHEntry = dest;
		firstTime = PR_FALSE;
	}
    */
	PRInt32 childCount= 0;

	nsCOMPtr<nsISHContainer> srcContainer(do_QueryInterface(src));
	if (!srcContainer)
		return NS_ERROR_FAILURE;
	nsCOMPtr<nsISHContainer> destContainer(do_QueryInterface(dest));
	if (!destContainer)
		return NS_ERROR_FAILURE;
	srcContainer->GetChildCount(&childCount);
	for(PRInt32 i = 0; i<childCount; i++) {
		nsCOMPtr<nsISHEntry> srcChild;
		srcContainer->GetChildAt(i, getter_AddRefs(srcChild));
		if (!srcChild)
			return NS_ERROR_FAILURE;
		nsCOMPtr<nsISHEntry>  destChild(do_CreateInstance(NS_SHENTRY_PROGID));
		if (!NS_SUCCEEDED(result))
			return result;
		result = CloneAndReplace(srcChild, cloneRef, replaceEntry, destChild);
		if (!NS_SUCCEEDED(result))
			return result;
		result = destContainer->AddChild(destChild, i);
		if (!NS_SUCCEEDED(result))
			return result;
	}

    
	return result;

}
#else
NS_IMETHODIMP
nsDocShell::CloneAndReplace(nsISHEntry * src, nsISHEntry * cloneRef,
							nsISHEntry * replaceEntry, nsISHEntry ** resultEntry)
{
	nsresult result = NS_OK;
	NS_ENSURE_ARG_POINTER(resultEntry);
	if (!src || !replaceEntry || !cloneRef)
		return NS_ERROR_FAILURE;
//    NS_ENSURE_ARG_POINTER(dest, NS_ERROR_FAILURE);
//	static  PRBool firstTime = PR_TRUE;
//	static nsISHEntry * rootSHEntry = nsnull;
    nsISHEntry * dest = *resultEntry;
	dest = (nsISHEntry *) nsnull;

	if (src == cloneRef) {
		// release the original object before assigning a new one.
	   //NS_RELEASE(dest);
		
      dest = replaceEntry;
	  *resultEntry = dest;
	  NS_IF_ADDREF(*resultEntry);
	}
	else {
    nsCOMPtr<nsIURI> uri;
	nsCOMPtr<nsIInputStream> postdata;
	nsCOMPtr<nsILayoutHistoryState> LHS;
	PRUnichar * title=nsnull;
	nsCOMPtr<nsISHEntry> parent;
	result = nsComponentManager::CreateInstance(NS_SHENTRY_PROGID, NULL,
			NS_GET_IID(nsISHEntry), (void **) &dest);
	if (!NS_SUCCEEDED(result))
      return result;

	src->GetURI(getter_AddRefs(uri));
	src->GetPostData(getter_AddRefs(postdata));
	src->GetTitle(&title);
	src->GetLayoutHistoryState(getter_AddRefs(LHS));
	//XXX Is this correct? parent is a weak ref in nsISHEntry
	src->GetParent(getter_AddRefs(parent));

	// XXX do we care much about valid values for these uri, title etc....
	dest->SetURI(uri);
	dest->SetPostData(postdata);
	dest->SetLayoutHistoryState(LHS);
	dest->SetTitle(title);
	dest->SetParent(parent);
	*resultEntry = dest;
	
	}
	*resultEntry = dest;
	/*
	if (firstTime) {
		// Save the root of the hierarchy in the result parameter
		rootSHEntry = dest;
		firstTime = PR_FALSE;
	}
    */
	PRInt32 childCount= 0;

	nsCOMPtr<nsISHContainer> srcContainer(do_QueryInterface(src));
	if (!srcContainer)
		return NS_ERROR_FAILURE;
	nsCOMPtr<nsISHContainer> destContainer(do_QueryInterface(dest));
	if (!destContainer)
		return NS_ERROR_FAILURE;
	srcContainer->GetChildCount(&childCount);
	for(PRInt32 i = 0; i<childCount; i++) {
		nsCOMPtr<nsISHEntry> srcChild;
		srcContainer->GetChildAt(i, getter_AddRefs(srcChild));
		if (!srcChild)
			return NS_ERROR_FAILURE;
		nsCOMPtr<nsISHEntry>  destChild;
		if (!NS_SUCCEEDED(result))
			return result;
		result = CloneAndReplace(srcChild, cloneRef, replaceEntry, getter_AddRefs(destChild));
		if (!NS_SUCCEEDED(result))
			return result;
		result = destContainer->AddChild(destChild, i);
		if (!NS_SUCCEEDED(result))
			return result;
	}

    
	return result;

}
#endif  /* 0 */

//*****************************************************************************
// nsDocShell: Global History
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::ShouldAddToGlobalHistory(nsIURI* aURI, PRBool* aShouldAdd)
{
   *aShouldAdd = PR_FALSE;
   if(!mGlobalHistory || !aURI || (typeContent != mItemType))
      return NS_OK;

   nsXPIDLCString scheme;
   NS_ENSURE_SUCCESS(aURI->GetScheme(getter_Copies(scheme)), NS_ERROR_FAILURE);
   
   nsAutoString schemeStr; schemeStr.AssignWithConversion(scheme);

   // The model is really if we don't know differently then add which basically
   // means we are suppose to try all the things we know not to allow in and
   // then if we don't bail go on and allow it in.  But here lets compare
   // against the most common case we know to allow in and go on and say yes
   // to it.
   if(schemeStr.EqualsWithConversion("http") || schemeStr.EqualsWithConversion("https"))
      {
      *aShouldAdd = PR_TRUE;
      return NS_OK;
      }

   if(schemeStr.EqualsWithConversion("about") || schemeStr.EqualsWithConversion("imap") ||
      schemeStr.EqualsWithConversion("news") || schemeStr.EqualsWithConversion("mailbox"))
      return NS_OK;

   *aShouldAdd = PR_TRUE;

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::AddToGlobalHistory(nsIURI* aURI)
{
   NS_ENSURE_STATE(mGlobalHistory);

   nsXPIDLCString spec;
   NS_ENSURE_SUCCESS(aURI->GetSpec(getter_Copies(spec)), NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(mGlobalHistory->AddPage(spec, nsnull, PR_Now()), 
      NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::UpdateCurrentGlobalHistory()
{
   // XXX Add code here that needs to update the current history item
   return NS_OK;
}

//*****************************************************************************
// nsDocShell: Helper Routines
//*****************************************************************************   

nsresult nsDocShell::SetLoadCookie(nsISupports *aCookie)
{
  // Remove the DocShell as a listener of the old WebProgress...
  if (mLoadCookie) {
    nsCOMPtr<nsIWebProgress> webProgress(do_QueryInterface(mLoadCookie));

    if (webProgress) {
      webProgress->RemoveProgressListener(this);
    }
  }

  mLoadCookie = aCookie;

  // Add the DocShell as a listener to the new WebProgress...
  if (mLoadCookie) {
    nsCOMPtr<nsIWebProgress> webProgress(do_QueryInterface(mLoadCookie));

    if (webProgress) {
      webProgress->AddProgressListener(this);
    }
  }
  return NS_OK;
}


nsresult nsDocShell::GetLoadCookie(nsISupports **aResult)
{
  *aResult = mLoadCookie;
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}


nsDocShellInitInfo* nsDocShell::InitInfo()
{
   if(mInitInfo)
      return mInitInfo;
   return mInitInfo = new nsDocShellInitInfo();
}

#define DIALOG_STRING_URI "chrome://global/locale/appstrings.properties"

NS_IMETHODIMP nsDocShell::GetPromptAndStringBundle(nsIPrompt** aPrompt,
   nsIStringBundle** aStringBundle)
{
   NS_ENSURE_SUCCESS(GetInterface(NS_GET_IID(nsIPrompt), (void**)aPrompt), NS_ERROR_FAILURE);

   nsCOMPtr<nsILocaleService> localeService(do_GetService(NS_LOCALESERVICE_PROGID));
   NS_ENSURE_TRUE(localeService, NS_ERROR_FAILURE);

   nsCOMPtr<nsILocale> locale;
   localeService->GetSystemLocale(getter_AddRefs(locale));
   NS_ENSURE_TRUE(locale, NS_ERROR_FAILURE);

   nsCOMPtr<nsIStringBundleService> stringBundleService(do_GetService(NS_STRINGBUNDLE_PROGID));
   NS_ENSURE_TRUE(stringBundleService, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(stringBundleService->CreateBundle(DIALOG_STRING_URI, locale, 
      getter_AddRefs(aStringBundle)), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetChildOffset(nsIDOMNode *aChild, nsIDOMNode* aParent,
   PRInt32* aOffset)
{
   NS_ENSURE_ARG_POINTER(aChild || aParent);
   
   nsCOMPtr<nsIDOMNodeList> childNodes;
   NS_ENSURE_SUCCESS(aParent->GetChildNodes(getter_AddRefs(childNodes)),
      NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(childNodes, NS_ERROR_FAILURE);

   PRInt32 i=0;

   for( ; PR_TRUE; i++)
      {
      nsCOMPtr<nsIDOMNode> childNode;
      NS_ENSURE_SUCCESS(childNodes->Item(i, getter_AddRefs(childNode)), 
         NS_ERROR_FAILURE);
      NS_ENSURE_TRUE(childNode, NS_ERROR_FAILURE);

      if(childNode.get() == aChild)
         {
         *aOffset = i;
         return NS_OK;
         }
      }

   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::GetRootScrollableView(nsIScrollableView** aOutScrollView)
{
   NS_ENSURE_ARG_POINTER(aOutScrollView);
   
   nsCOMPtr<nsIPresShell> shell;
   NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(shell)), NS_ERROR_FAILURE);

   nsCOMPtr<nsIViewManager> viewManager;
   NS_ENSURE_SUCCESS(shell->GetViewManager(getter_AddRefs(viewManager)),
      NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(viewManager->GetRootScrollableView(aOutScrollView),
      NS_ERROR_FAILURE);

   if (*aOutScrollView == nsnull)
   {
      return NS_ERROR_FAILURE;
   }
   return NS_OK;
} 

NS_IMETHODIMP nsDocShell::EnsureContentListener()
{
   if(mContentListener)
      return NS_OK;
   
   mContentListener = new nsDSURIContentListener();
   NS_ENSURE_TRUE(mContentListener, NS_ERROR_OUT_OF_MEMORY);

   NS_ADDREF(mContentListener);
   mContentListener->DocShell(this);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::EnsureScriptEnvironment()
{
   if(mScriptContext)
      return NS_OK;

   NS_NewScriptGlobalObject(getter_AddRefs(mScriptGlobal));
   NS_ENSURE_TRUE(mScriptGlobal, NS_ERROR_FAILURE);

   mScriptGlobal->SetDocShell(NS_STATIC_CAST(nsIDocShell*, this));
   mScriptGlobal->SetGlobalObjectOwner(
      NS_STATIC_CAST(nsIScriptGlobalObjectOwner*, this));

   NS_CreateScriptContext(mScriptGlobal, getter_AddRefs(mScriptContext));
   NS_ENSURE_TRUE(mScriptContext, NS_ERROR_FAILURE);

   return NS_OK;
}

PRBool nsDocShell::IsFrame()
{
   if(mParent)
      {
      PRInt32 parentType = ~mItemType;  // Not us
      mParent->GetItemType(&parentType);
      if(parentType == mItemType)   // This is a frame
         return PR_TRUE;
      }

   return PR_FALSE;
}

//*****************************************************************************
//***    nsRefreshTimer: Object Management
//*****************************************************************************

nsRefreshTimer::nsRefreshTimer() : mRepeat(PR_FALSE), mDelay(0)
{
  NS_INIT_REFCNT();
}

nsRefreshTimer::~nsRefreshTimer()
{
}

//*****************************************************************************
// nsRefreshTimer::nsISupports
//*****************************************************************************   

NS_IMPL_THREADSAFE_ADDREF(nsRefreshTimer)
NS_IMPL_THREADSAFE_RELEASE(nsRefreshTimer)

NS_INTERFACE_MAP_BEGIN(nsRefreshTimer)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITimerCallback)
   NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
NS_INTERFACE_MAP_END_THREADSAFE

///*****************************************************************************
// nsRefreshTimer::nsITimerCallback
//*****************************************************************************   

NS_IMETHODIMP_(void) nsRefreshTimer::Notify(nsITimer *aTimer)
{
    NS_ASSERTION(mDocShell, "DocShell is somehow null");

    if(mDocShell)
    {
        nsCOMPtr<nsIDocShellLoadInfo> loadInfo;        
        mDocShell -> CreateLoadInfo (getter_AddRefs (loadInfo));

        loadInfo  -> SetLoadType(nsIDocShellLoadInfo::loadRefresh);
        mDocShell -> LoadURI(mURI, loadInfo);
    }

   /*
    * LoadURL(...) will cancel all refresh timers... This causes the Timer and
    * its refreshData instance to be released...
    */
}

