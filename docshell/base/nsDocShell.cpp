/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 3 -*-
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
#include "nsILayoutHistoryState.h"
#include "nsILocaleService.h"
#include "nsIPlatformCharset.h"
#include "nsITimer.h"

// For reporting errors with the console service.
// These can go away if error reporting is propagated up past nsDocShell.
#include "nsIConsoleService.h"
#include "nsIScriptError.h"

static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_CID(kPlatformCharsetCID, NS_PLATFORMCHARSET_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

//*****************************************************************************
//***    nsDocShell: Object Management
//*****************************************************************************

nsDocShell::nsDocShell() : 
  mContentListener(nsnull),
  mWebProgressListener(nsnull),
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

NS_IMETHODIMP nsDocShell::Create(nsISupports* aOuter, const nsIID& aIID, 
  void** ppv)
{
  NS_ENSURE_ARG_POINTER(ppv);
  NS_ENSURE_NO_AGGREGATION(aOuter);

  nsDocShell* docShell = new  nsDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(docShell);
  nsresult rv = docShell->QueryInterface(aIID, ppv);
  NS_RELEASE(docShell);  
  return rv;
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
   NS_INTERFACE_MAP_ENTRY(nsIWebNavigation)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgress)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
   NS_INTERFACE_MAP_ENTRY(nsIScrollable)
   NS_INTERFACE_MAP_ENTRY(nsITextScroll)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObjectOwner)
   NS_INTERFACE_MAP_ENTRY(nsIRefreshURI)
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
   else if(aIID.Equals(NS_GET_IID(nsIWebProgressListener)) &&
      NS_SUCCEEDED(EnsureWebProgressListener()))
      *aSink = mWebProgressListener;
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
   else
      return QueryInterface(aIID, aSink);

   NS_IF_ADDREF(((nsISupports*)*aSink));
   return NS_OK;   
}

//*****************************************************************************
// nsDocShell::nsIDocShell
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::LoadURI(nsIURI* aURI, nsIDocShellLoadInfo* aLoadInfo)
{
   NS_ENSURE_ARG(aURI);

   nsCOMPtr<nsIURI> referrer;
   PRBool replace = PR_FALSE;
   if(aLoadInfo)
      {
      aLoadInfo->GetReferrer(getter_AddRefs(referrer));
      aLoadInfo->GetReplaceSessionHistorySlot(&replace);
      }
      

   NS_ENSURE_SUCCESS(InternalLoad(aURI, referrer, nsnull, nsnull, 
      replace ? loadNormalReplace : loadNormal), NS_ERROR_FAILURE);

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

// SetDocument is only meaningful for doc shells that support DOM documents.  Not all do.
NS_IMETHODIMP
nsDocShell::SetDocument(nsIDOMDocument *aDOMDoc, nsIDOMElement *aRootNode)
{

   // The tricky part is bypassing the normal load process and just putting a document into
   // the webshell.  This is particularly nasty, since webshells don't normally even know
   // about their documents

   // (1) Create a document viewer 
   nsCOMPtr<nsIContentViewer> documentViewer;
   nsCOMPtr<nsIDocumentLoaderFactory> docFactory;
   static NS_DEFINE_CID(kLayoutDocumentLoaderFactoryCID, NS_LAYOUT_DOCUMENT_LOADER_FACTORY_CID);
   NS_ENSURE_SUCCESS(nsComponentManager::CreateInstance(kLayoutDocumentLoaderFactoryCID, nsnull, 
                                                       NS_GET_IID(nsIDocumentLoaderFactory),
                                                       (void**)getter_AddRefs(docFactory)),
                    NS_ERROR_FAILURE);

   nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDOMDoc);
   NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
   NS_ENSURE_SUCCESS(docFactory->CreateInstanceForDocument(NS_STATIC_CAST(nsIContentViewerContainer*, this),
                                                          doc,
                                                          "view",
                                                          getter_AddRefs(documentViewer)),
                    NS_ERROR_FAILURE); 

   // (2) Feed the docshell to the content viewer
   NS_ENSURE_SUCCESS(documentViewer->SetContainer((nsIDocShell*)this), 
      NS_ERROR_FAILURE);

   // (3) Tell the content viewer container to setup the content viewer.
   //     (This step causes everything to be set up for an initial flow.)
   NS_ENSURE_SUCCESS(SetupNewViewer(documentViewer), NS_ERROR_FAILURE);

   // XXX: It would be great to get rid of this dummy channel!
   nsCOMPtr<nsIURI> uri;
   NS_ENSURE_SUCCESS(NS_NewURI(getter_AddRefs(uri), NS_ConvertASCIItoUCS2("about:blank")), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(uri, NS_ERROR_OUT_OF_MEMORY);

   nsCOMPtr<nsIChannel> dummyChannel;
   NS_ENSURE_SUCCESS(NS_OpenURI(getter_AddRefs(dummyChannel), uri, nsnull), NS_ERROR_FAILURE);

   // (4) fire start document load notification
   nsCOMPtr<nsIStreamListener> outStreamListener;
   NS_ENSURE_SUCCESS(doc->StartDocumentLoad("view", dummyChannel, nsnull, 
      NS_STATIC_CAST(nsIContentViewerContainer*, this), 
      getter_AddRefs(outStreamListener)), NS_ERROR_FAILURE);
   NS_ENSURE_SUCCESS(FireStartDocumentLoad(mDocLoader, uri, "load"), NS_ERROR_FAILURE);

   // (5) hook up the document and its content
   nsCOMPtr<nsIContent> rootContent = do_QueryInterface(aRootNode);
   NS_ENSURE_TRUE(doc, NS_ERROR_OUT_OF_MEMORY);
   NS_ENSURE_SUCCESS(rootContent->SetDocument(doc, PR_FALSE), NS_ERROR_FAILURE);
   doc->SetRootContent(rootContent);

   // (6) reflow the document
   PRInt32 i;
   PRInt32 ns = doc->GetNumberOfShells();
   for (i = 0; i < ns; i++) 
   {
    nsCOMPtr<nsIPresShell> shell(dont_AddRef(doc->GetShellAt(i)));
    if (shell) 
    {
      // Make shell an observer for next time
      NS_ENSURE_SUCCESS(shell->BeginObservingDocument(), NS_ERROR_FAILURE);

      // Resize-reflow this time
      nsCOMPtr<nsIDocumentViewer> docViewer = do_QueryInterface(documentViewer);
      NS_ENSURE_TRUE(docViewer, NS_ERROR_OUT_OF_MEMORY);
      nsCOMPtr<nsIPresContext> presContext;
      NS_ENSURE_SUCCESS(docViewer->GetPresContext(*(getter_AddRefs(presContext))), NS_ERROR_FAILURE);
      NS_ENSURE_TRUE(presContext, NS_ERROR_OUT_OF_MEMORY);
      float p2t;
      presContext->GetScaledPixelsToTwips(&p2t);

      nsRect r;
      NS_ENSURE_SUCCESS(GetPosition(&r.x, &r.y), NS_ERROR_FAILURE);;
      NS_ENSURE_SUCCESS(GetSize(&r.width, &r.height), NS_ERROR_FAILURE);;
      NS_ENSURE_SUCCESS(shell->InitialReflow(NSToCoordRound(r.width * p2t), NSToCoordRound(r.height * p2t)), NS_ERROR_FAILURE);

      // Now trigger a refresh
      nsCOMPtr<nsIViewManager> vm;
      NS_ENSURE_SUCCESS(shell->GetViewManager(getter_AddRefs(vm)), NS_ERROR_FAILURE);
      if (vm) 
      {
        PRBool enabled;
        documentViewer->GetEnableRendering(&enabled);
        if (enabled) {
          vm->EnableRefresh(NS_VMREFRESH_IMMEDIATE);
        }
        NS_ENSURE_SUCCESS(vm->SetWindowDimensions(NSToCoordRound(r.width * p2t), 
                                                  NSToCoordRound(r.height * p2t)), 
                          NS_ERROR_FAILURE);
      }
    }
   }

   // (7) fire end document load notification
   nsresult rv = NS_OK;
   NS_ENSURE_SUCCESS(FireEndDocumentLoad(mDocLoader, dummyChannel, rv), NS_ERROR_FAILURE);
   NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);  // test the resulting out-param separately

   return NS_OK;
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
   NS_ENSURE_ARG_POINTER(aPresContext);
   *aPresContext = nsnull;

   NS_ENSURE_TRUE(mContentViewer, NS_ERROR_FAILURE);
   
   nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(mContentViewer));
   NS_ENSURE_TRUE(docv, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(docv->GetPresContext(*aPresContext), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetPresShell(nsIPresShell** aPresShell)
{
   NS_ENSURE_ARG_POINTER(aPresShell);
   *aPresShell = nsnull;
   
   nsCOMPtr<nsIPresContext> presContext;
   NS_ENSURE_SUCCESS(GetPresContext(getter_AddRefs(presContext)), 
      NS_ERROR_FAILURE);
   if(!presContext)
      return NS_OK;    

   NS_ENSURE_SUCCESS(presContext->GetShell(aPresShell), NS_ERROR_FAILURE);

   return NS_OK;
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
      Reload(reloadNormal);

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
   if(mName.Equals(aName))
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
   mTreeOwner = aTreeOwner; // Weak reference per API
   // Don't automatically set the progress based on the tree owner for frames
   if(!IsFrame()) 
      {
      nsCOMPtr<nsIWebProgressListener> progressListener(do_QueryInterface(aTreeOwner));
      mOwnerProgressListener = progressListener; // Weak reference per API
      }
   else
      mOwnerProgressListener = nsnull;

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
      if(name.Equals(childName))
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
// nsDocShell::nsIWebNavigation
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::GetCanGoBack(PRBool* aCanGoBack)
{
   NS_ENSURE_STATE(mSessionHistory);  
   NS_ENSURE_ARG_POINTER(aCanGoBack);
   *aCanGoBack = PR_FALSE;

   PRInt32 index = -1;
   NS_ENSURE_SUCCESS(mSessionHistory->GetIndex(&index), NS_ERROR_FAILURE);
   if(index > 0)
      *aCanGoBack = PR_TRUE;

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetCanGoForward(PRBool* aCanGoForward)
{
   NS_ENSURE_STATE(mSessionHistory);  
   NS_ENSURE_ARG_POINTER(aCanGoForward);
   *aCanGoForward = PR_FALSE;

   PRInt32 index = -1;
   PRInt32 count = -1;

   NS_ENSURE_SUCCESS(mSessionHistory->GetIndex(&index), NS_ERROR_FAILURE);
   NS_ENSURE_SUCCESS(mSessionHistory->GetCount(&count), NS_ERROR_FAILURE);

   if((index >= 0) && (index < (count - 1)))
      *aCanGoForward = PR_TRUE;

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GoBack()
{
   nsCOMPtr<nsIDocShellTreeItem> root;
   GetSameTypeRootTreeItem(getter_AddRefs(root));
   if(root.get() != NS_STATIC_CAST(nsIDocShellTreeItem*, this))
      {
      nsCOMPtr<nsIWebNavigation> rootAsNav(do_QueryInterface(root));
      return rootAsNav->GoBack();
      }

   NS_ENSURE_STATE(mSessionHistory);

   nsCOMPtr<nsISHEntry> previousEntry;

   NS_ENSURE_SUCCESS(mSessionHistory->GetPreviousEntry(PR_TRUE, 
      getter_AddRefs(previousEntry)), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(previousEntry, NS_ERROR_FAILURE);

   UpdateCurrentSessionHistory();  

   NS_ENSURE_SUCCESS(LoadHistoryEntry(previousEntry), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GoForward()
{
   nsCOMPtr<nsIDocShellTreeItem> root;
   GetSameTypeRootTreeItem(getter_AddRefs(root));
   if(root.get() != NS_STATIC_CAST(nsIDocShellTreeItem*, this))
      {
      nsCOMPtr<nsIWebNavigation> rootAsNav(do_QueryInterface(root));
      return rootAsNav->GoForward();
      }

   NS_ENSURE_STATE(mSessionHistory);
   
   nsCOMPtr<nsISHEntry> nextEntry;

   NS_ENSURE_SUCCESS(mSessionHistory->GetNextEntry(PR_TRUE, 
      getter_AddRefs(nextEntry)), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(nextEntry, NS_ERROR_FAILURE);

   UpdateCurrentSessionHistory();  

   NS_ENSURE_SUCCESS(LoadHistoryEntry(nextEntry), NS_ERROR_FAILURE);

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
      GetInterface(NS_GET_IID(nsIPrompt), getter_AddRefs(prompter));
      
      nsCOMPtr<nsIStringBundle> stringBundle;
      GetStringBundle(getter_AddRefs(stringBundle));

      NS_ENSURE_TRUE(stringBundle, NS_ERROR_FAILURE);

      nsXPIDLString messageStr;
      NS_ENSURE_SUCCESS(stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("protocolNotFound").GetUnicode(), 
         getter_Copies(messageStr)), NS_ERROR_FAILURE);

      nsAutoString uriString(aURI);
      PRInt32 colon = uriString.FindChar(':');
      // extract the scheme
      nsAutoString scheme;
      uriString.Left(scheme, colon);

      nsAutoString dnsMsg(scheme);
      dnsMsg.AppendWithConversion(' ');
      dnsMsg.Append(messageStr);

      prompter->Alert(dnsMsg.GetUnicode());
      } // end unknown protocol

   if(!uri)
      return NS_ERROR_FAILURE;

   NS_ENSURE_SUCCESS(LoadURI(uri, nsnull), NS_ERROR_FAILURE);
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::Reload(PRInt32 aReloadType)
{  
   // XXX Honor the reload type
   NS_ENSURE_STATE(mCurrentURI);

   // XXXTAB Convert reload type to our type
   loadType type = loadReloadNormal;
   
   NS_ENSURE_SUCCESS(InternalLoad(mCurrentURI, mReferrerURI, nsnull, nsnull, 
      type), NS_ERROR_FAILURE);

   return NS_OK;
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

  nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(mContentViewer));
  NS_ENSURE_TRUE(docv, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument>doc;
  NS_ENSURE_SUCCESS(docv->GetDocument(*getter_AddRefs(doc)), NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(doc, NS_ERROR_NULL_POINTER);

  // the result's addref comes from this QueryInterface call
  NS_ENSURE_SUCCESS(CallQueryInterface(doc.get(), aDocument), NS_ERROR_FAILURE);

  return NS_OK;
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
   return NS_OK;
}
	
NS_IMETHODIMP nsDocShell::GetSessionHistory(nsISHistory** aSessionHistory)
{
   NS_ENSURE_ARG_POINTER(aSessionHistory);

   *aSessionHistory = mSessionHistory;
   NS_IF_ADDREF(*aSessionHistory);
   return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIWebProgress
//*****************************************************************************

NS_IMETHODIMP nsDocShell::AddProgressListener(nsIWebProgressListener* aListener)
{
   if(!mWebProgressListenerList)
      NS_ENSURE_SUCCESS(NS_NewISupportsArray(getter_AddRefs(mWebProgressListenerList)),
         NS_ERROR_FAILURE);

   // Make sure it isn't already in the list...  This is bad!
   NS_ENSURE_ARG(mWebProgressListenerList->IndexOf(aListener) == -1);

   NS_ENSURE_SUCCESS(mWebProgressListenerList->AppendElement(aListener),
      NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::RemoveProgressListener(nsIWebProgressListener* aListener)
{
   NS_ENSURE_STATE(mWebProgressListenerList);
   NS_ENSURE_ARG(aListener);

   NS_ENSURE_TRUE(mWebProgressListenerList->RemoveElement(aListener),
      NS_ERROR_INVALID_ARG);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetProgressStatusFlags(PRInt32* aProgressStatusFlags)
{
   //XXXTAB First Check
   //XXX First Check
	/*
	Current connection Status of the browser.  This will be one of the enumerated
	connection progress steps.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::GetCurSelfProgress(PRInt32* curSelfProgress)
{
   //XXXTAB First Check
   //XXX First Check
	/*
	The current position of progress.  This is between 0 and maxSelfProgress.
	This is the position of only this progress object.  It doesn not include
	the progress of all children.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::GetMaxSelfProgress(PRInt32* maxSelfProgress)
{
   //XXXTAB First Check
   //XXX First Check
	/*
	The maximum position that progress will go to.  This sets a relative
	position point for the current progress to relate to.  This is the max
	position of only this progress object.  It does not include the progress of
	all the children.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::GetCurTotalProgress(PRInt32* curTotalProgress)
{
   //XXXTAB First Check
   //XXX First Check
	/*
	The current position of progress for this object and all children added
	together.  This is between 0 and maxTotalProgress.
	*/
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::GetMaxTotalProgress(PRInt32* maxTotalProgress)
{
   //XXXTAB First Check
   //XXX First Check
	/*
	The maximum position that progress will go to for the max of this progress
	object and all children.  This sets the relative position point for the
	current progress to relate to.
	*/
   return NS_ERROR_FAILURE;
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
   mWebProgressListenerList = nsnull;

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
   mLoadCookie = nsnull;
   SetTreeOwner(nsnull);

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

   if(mWebProgressListener)
      {
      mWebProgressListener->DocShell(nsnull);
      NS_RELEASE(mWebProgressListener);
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
         return nextCallWin->FocusAvailable(aCurrentFocus, aTookFocus);
      return NS_OK;
      }

   //Otherwise, check the chilren and offer it to the next sibling.
   PRInt32 i;
   PRInt32 n = mChildren.Count();
   for(i = 0; i < n; i++)
      {
      nsCOMPtr<nsIBaseWindow> 
         child(do_QueryInterface((nsISupports*)mChildren.ElementAt(i)));
      if(child.get() == aCurrentFocus)
         {
         while(++i < n)
            {
            child = do_QueryInterface((nsISupports*)mChildren.ElementAt(i));
            if(NS_SUCCEEDED(child->SetFocus()))
               {
               *aTookFocus = PR_TRUE;
               return NS_OK;
               }
            }
         }
      }
   if(nextCallWin)
      return nextCallWin->FocusAvailable(aCurrentFocus, aTookFocus);
   return NS_OK;
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
      NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

      treeOwnerAsWin->SetTitle(aTitle);
      }

   EnsureGlobalHistory();

   if(mGlobalHistory && mCurrentURI)
      {
      nsXPIDLCString url;
      mCurrentURI->GetSpec(getter_Copies(url));
      mGlobalHistory->SetPageTitle(url, aTitle);
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

   NS_ENSURE_SUCCESS(scrollView->ScrollByLines(numLines), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::ScrollByPages(PRInt32 numPages)
{
   nsCOMPtr<nsIScrollableView> scrollView;

   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)),
      NS_ERROR_FAILURE);

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

   nsITimer* timer = nsnull;
   NS_NewTimer(&timer);
   NS_ENSURE_TRUE(timer, NS_ERROR_FAILURE);

   mRefreshURIList.AppendElement(timer);
   timer->Init(refreshTimer, aDelay);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::CancelRefreshURITimers()
{
   PRInt32 n = mRefreshURIList.Count();
   nsCOMPtr<nsITimer> timer;

   while(n)
      {
      timer = dont_AddRef((nsITimer*)mRefreshURIList.ElementAt(0));
      mRefreshURIList.RemoveElementAt(0);

      if(timer)
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
   return SetupNewViewer(aContentViewer);
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

   NS_ENSURE_SUCCESS(SetupNewViewer(viewer), NS_ERROR_FAILURE);
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
      return NS_ERROR_FAILURE;

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

   mContentViewer->Show();

   // Now that we have switched documents, forget all of our children
   DestroyChildren();

   return NS_OK;
}

//*****************************************************************************
// nsDocShell: Site Loading
//*****************************************************************************   
  
NS_IMETHODIMP nsDocShell::InternalLoad(nsIURI* aURI, nsIURI* aReferrer,
   const char* aWindowTarget, nsIInputStream* aPostData, loadType aLoadType)
{
   PRBool wasAnchor = PR_FALSE;
   NS_ENSURE_SUCCESS(ScrollIfAnchor(aURI, &wasAnchor), NS_ERROR_FAILURE);
   if(wasAnchor)
      {
      SetCurrentURI(aURI);
      return NS_OK;
      }
   
   NS_ENSURE_SUCCESS(StopCurrentLoads(), NS_ERROR_FAILURE);

   mLoadType = aLoadType;

   nsURILoadCommand  loadCmd = nsIURILoader::viewNormal;
   if(loadLink == aLoadType)
      loadCmd = nsIURILoader::viewUserClick;
   NS_ENSURE_SUCCESS(DoURILoad(aURI, aReferrer, loadCmd, aWindowTarget, 
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
   NS_NewURI(aURI, uriString.GetUnicode(), nsnull);
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
   PRInt32 colon = -1;
   PRInt32 fSlash = uriString.FindChar('/');
   PRUnichar port;
   // if no scheme (protocol) is found, assume http.
   if((colon=uriString.FindChar(':') == -1) ||// no colon at all
      ((fSlash > -1) && (colon > fSlash)) ||// the only colon comes after the first slash
      ((colon < (((PRInt32)uriString.Length())-1)) && // the first char after the first colon is a digit (i.e. a port)
       ((port=uriString.CharAt(colon+1)) <= '9') && (port > '0'))) 
      {
      // find host name
      PRInt32 hostPos = uriString.FindCharInSet("./:");
      if(hostPos == -1) 
         hostPos = uriString.Length();

      // extract host name
      nsAutoString hostSpec;
      uriString.Left(hostSpec, hostPos);

      // insert url spec corresponding to host name
      if(hostSpec.EqualsIgnoreCase("ftp")) 
         uriString.InsertWithConversion("ftp://", 0, 6);
      else 
         uriString.InsertWithConversion("http://", 0, 7);
      } // end if colon

   return NS_NewURI(aURI, uriString.GetUnicode(), nsnull);
}

NS_IMETHODIMP nsDocShell::FileURIFixup(const PRUnichar* aStringURI, 
   nsIURI** aURI)
{
   nsAutoString uriSpec(aStringURI);

   ConvertFileToStringURI(uriSpec, uriSpec);

   if(0 == uriSpec.Find("file:", 0))
      {
      // if this is file url, we need to  convert the URI
      // from Unicode to the FS charset
      nsCAutoString inFSCharset;
      NS_ENSURE_SUCCESS(ConvertStringURIToFileCharset(uriSpec, inFSCharset),
         NS_ERROR_FAILURE);

      if(NS_SUCCEEDED(NS_NewURI(aURI, inFSCharset.GetBuffer(), nsnull)))
         return NS_OK;
      } 
   return NS_ERROR_FAILURE;
}

#define FILE_PROTOCOL "file://"

NS_IMETHODIMP nsDocShell::ConvertFileToStringURI(nsString& aIn, nsString& aOut)
{
   aOut = aIn;
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
               nsAllocator::Free(escapedUTF8Spec);
               } // escapedUTF8Spec
            nsAllocator::Free(utf8Spec);
            } // utf8Spec
         } // keyword 
      } // FindChar

   if(*aURI)
      return NS_OK;

   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::DoURILoad(nsIURI* aURI, nsIURI* aReferrerURI, 
   nsURILoadCommand aLoadCmd, const char* aWindowTarget, 
   nsIInputStream* aPostData)
{
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

   //XXX Wrong, but needed for now
   channel->SetOriginalURI(aReferrerURI ? aReferrerURI : aURI);
   
   // Mark the channel as being a document URI...
   nsLoadFlags loadAttribs = 0;
   channel->GetLoadAttributes(&loadAttribs);
   loadAttribs |= nsIChannel::LOAD_DOCUMENT_URI;
   channel->SetLoadAttributes(loadAttribs);

   nsCOMPtr<nsIHTTPChannel> httpChannel(do_QueryInterface(channel));
   if(httpChannel)
      {
      // figure out if we need to set the post data stream on the channel...
      // right now, this is only done for http channels.....
      if(aPostData)
         {
         httpChannel->SetRequestMethod(HM_POST);
         httpChannel->SetPostDataStream(aPostData);
         }
      // Set the referrer explicitly
      if(aReferrerURI) // Referrer is currenly only set for link clicks here.
         httpChannel->SetReferrer(aReferrerURI, 
                                    nsIHTTPChannel::REFERRER_LINK_CLICK);
      }

   NS_ENSURE_SUCCESS(uriLoader->OpenURI(channel, aLoadCmd,
      aWindowTarget, NS_STATIC_CAST(nsIDocShell*, this)), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::StopCurrentLoads()
{
   StopLoad();
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::ScrollIfAnchor(nsIURI* aURI, PRBool* aWasAnchor)
{
   *aWasAnchor = PR_FALSE;

   //XXXTAB Implement this
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::OnLoadingSite(nsIChannel* aChannel)
{
   nsCOMPtr<nsIURI> uri;
   aChannel->GetURI(getter_AddRefs(uri));
   NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

   UpdateCurrentSessionHistory();
   UpdateCurrentGlobalHistory();

   PRBool updateHistory = PR_TRUE;

   // Determine if this type of load should update history   
   switch(mLoadType)
      {
      case loadHistory:
      case loadReloadNormal:
      case loadReloadBypassCache:
      case loadReloadBypassProxy:
      case loadRelaodBypassProxyAndCache:
         updateHistory = PR_FALSE;
         break;

      case loadNormal:
      case loadNormalReplace:
      case loadLink:
         break;
      
      default:
         NS_ERROR("Need to update case");
         break;
      } 

   if(updateHistory)
      {
      PRBool shouldAdd = PR_FALSE;

      ShouldAddToSessionHistory(uri, &shouldAdd);
      if(shouldAdd)
         AddToSessionHistory(uri);

      shouldAdd = PR_FALSE;
      ShouldAddToGlobalHistory(uri, &shouldAdd);
      if(shouldAdd)
         AddToGlobalHistory(uri);
      }

   SetCurrentURI(uri);
   nsCOMPtr<nsIHTTPChannel> httpChannel(do_QueryInterface(aChannel));
   if(httpChannel)
      {
      nsCOMPtr<nsIURI> referrer;
      httpChannel->GetReferrer(getter_AddRefs(referrer));
      SetReferrerURI(referrer);
      }

   mInitialPageLoad = PR_FALSE;
   return NS_OK;
}

void nsDocShell::SetCurrentURI(nsIURI* aURI)
{
   mCurrentURI = aURI; //This assignment addrefs
   FireOnLocationChange(aURI);
}

void nsDocShell::SetReferrerURI(nsIURI* aURI)
{
   mReferrerURI = aURI; // This assigment addrefs
}

//*****************************************************************************
// nsDocShell: Session History
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::ShouldAddToSessionHistory(nsIURI* aURI, 
   PRBool* aShouldAdd)
{
   *aShouldAdd = PR_FALSE;

   if((!mSessionHistory) || (IsFrame() && mInitialPageLoad))
      return NS_OK;

   if(mCurrentURI)
      {
      PRBool equals = PR_TRUE;
      mCurrentURI->Equals(aURI, &equals);
      if(equals)
         return NS_OK;
      }
      
   *aShouldAdd = PR_TRUE;
 
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::ShouldPersistInSessionHistory(nsIURI* aURI,
   PRBool* aShouldAdd)
{
   *aShouldAdd = PR_FALSE;
   if(!aURI)
      return NS_OK;

   nsXPIDLCString scheme;
   NS_ENSURE_SUCCESS(aURI->GetScheme(getter_Copies(scheme)), NS_ERROR_FAILURE);
   
   nsAutoString schemeStr(scheme);

   if(schemeStr.Equals("about"))
      {
      nsXPIDLCString path;
      NS_ENSURE_SUCCESS(aURI->GetPath(getter_Copies(path)), NS_ERROR_FAILURE);
      nsAutoString pathStr(path);
      if(pathStr.Equals("blank"))
         return NS_OK;
      }

   *aShouldAdd = PR_TRUE;

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::AddToSessionHistory(nsIURI* aURI)
{
   PRBool shouldPersist = PR_FALSE;
   ShouldPersistInSessionHistory(aURI, &shouldPersist);

   nsCOMPtr<nsISHEntry> entry;
   if(loadNormalReplace == mLoadType)
      {
      PRInt32 index = 0;
      mSessionHistory->GetIndex(&index);
      mSessionHistory->GetEntryAtIndex(index, PR_FALSE, getter_AddRefs(entry));
      }

   if(!entry)
      entry = do_CreateInstance(NS_SHENTRY_PROGID);
   NS_ENSURE_TRUE(entry, NS_ERROR_FAILURE);

   nsCOMPtr<nsIInputStream> inputStream;  // XXX Need to get this from somewhere
   nsCOMPtr<nsILayoutHistoryState> layoutState; // XXX Need to get this from somewhere

   NS_ENSURE_SUCCESS(entry->Create(aURI, mTitle.GetUnicode(), nsnull, 
      inputStream, layoutState), NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(mSessionHistory->AddEntry(entry, shouldPersist),
      NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::UpdateCurrentSessionHistory()
{
   if(mInitialPageLoad || !mSessionHistory)
      return NS_OK;

   // XXXTAB
   //NS_ERROR("Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::LoadHistoryEntry(nsISHEntry* aEntry)
{
   nsCOMPtr<nsIURI> uri;
   nsCOMPtr<nsIInputStream> postData;

   NS_ENSURE_SUCCESS(aEntry->GetURI(getter_AddRefs(uri)), NS_ERROR_FAILURE);
   NS_ENSURE_SUCCESS(aEntry->GetPostData(getter_AddRefs(postData)),
      NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(InternalLoad(uri, nsnull, nsnull, postData, loadHistory),
      NS_ERROR_FAILURE);

   return NS_OK;
}

//*****************************************************************************
// nsDocShell: Global History
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::EnsureGlobalHistory()
{
   if(mGlobalHistory)
      return NS_OK;

   mGlobalHistory = do_GetService(NS_GLOBALHISTORY_PROGID);
   if(!mGlobalHistory)
      return NS_ERROR_FAILURE;

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::ShouldAddToGlobalHistory(nsIURI* aURI, 
   PRBool* aShouldAdd)
{
   *aShouldAdd = PR_FALSE;
   if(!aURI || (typeContent != mItemType))
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
   if(NS_FAILED(EnsureGlobalHistory()))
      return NS_ERROR_FAILURE;  // XXX REMOVE THIS!!!!

   NS_ENSURE_SUCCESS(EnsureGlobalHistory(), NS_ERROR_FAILURE);

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
// nsDocShell: WebProgressListener Firing
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::EnsureWebProgressListener()
{
   if(mWebProgressListener)   
      return NS_OK;

   mWebProgressListener = new nsDSWebProgressListener();
   NS_ENSURE_TRUE(mWebProgressListener, NS_ERROR_OUT_OF_MEMORY);

   NS_ADDREF(mWebProgressListener);
   mWebProgressListener->DocShell(this);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::FireOnProgressChange(nsIChannel* aChannel,
   PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, 
   PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
   if(mOwnerProgressListener)
      mOwnerProgressListener->OnProgressChange(aChannel, aCurSelfProgress, 
         aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress);

   if(!mWebProgressListenerList)
      return NS_OK;

   PRUint32 count = 0;
   mWebProgressListenerList->Count(&count);
   for(PRUint32 x = 0; x < count; x++)
      {
      nsCOMPtr<nsISupports> element;
      mWebProgressListenerList->GetElementAt(x, getter_AddRefs(element));
      nsCOMPtr<nsIWebProgressListener> listener(do_QueryInterface(element));
      if(!listener) 
         continue;
      listener->OnProgressChange(aChannel, aCurSelfProgress, aMaxSelfProgress,
         aCurTotalProgress, aMaxTotalProgress);
      }
   return NS_OK;
}
      
NS_IMETHODIMP nsDocShell::FireOnChildProgressChange(nsIChannel* aChannel,
   PRInt32 aCurChildProgress, PRInt32 aMaxChildProgress)
{
   if(mOwnerProgressListener)
      mOwnerProgressListener->OnChildProgressChange(aChannel, aCurChildProgress, 
         aMaxChildProgress);

   if(!mWebProgressListenerList)
      return NS_OK;

   PRUint32 count = 0;
   mWebProgressListenerList->Count(&count);
   for(PRUint32 x = 0; x < count; x++)
      {
      nsCOMPtr<nsISupports> element;
      mWebProgressListenerList->GetElementAt(x, getter_AddRefs(element));
      nsCOMPtr<nsIWebProgressListener> listener(do_QueryInterface(element));
      if(!listener) 
         continue;
      listener->OnChildProgressChange(aChannel, aCurChildProgress, 
         aMaxChildProgress);
      }
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::FireOnStatusChange(nsIChannel* aChannel,
   PRInt32 aProgressStatusFlags)
{
   if(mOwnerProgressListener)
      mOwnerProgressListener->OnStatusChange(aChannel, aProgressStatusFlags); 

   if(!mWebProgressListenerList)
      return NS_OK;

   PRUint32 count = 0;
   mWebProgressListenerList->Count(&count);
   for(PRUint32 x = 0; x < count; x++)
      {
      nsCOMPtr<nsISupports> element;
      mWebProgressListenerList->GetElementAt(x, getter_AddRefs(element));
      nsCOMPtr<nsIWebProgressListener> listener(do_QueryInterface(element));
      if(!listener) 
         continue;
      listener->OnStatusChange(aChannel, aProgressStatusFlags);
      }
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::FireOnChildStatusChange(nsIChannel* aChannel,
   PRInt32 aProgressStatusFlags)
{
   if(mOwnerProgressListener)
      mOwnerProgressListener->OnStatusChange(aChannel, aProgressStatusFlags); 

   if(!mWebProgressListenerList)
      return NS_OK;

   PRUint32 count = 0;
   mWebProgressListenerList->Count(&count);
   for(PRUint32 x = 0; x < count; x++)
      {
      nsCOMPtr<nsISupports> element;
      mWebProgressListenerList->GetElementAt(x, getter_AddRefs(element));
      nsCOMPtr<nsIWebProgressListener> listener(do_QueryInterface(element));
      if(!listener) 
         continue;
      listener->OnChildStatusChange(aChannel, aProgressStatusFlags);
      }
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::FireOnLocationChange(nsIURI* aURI)
{
   if(mOwnerProgressListener)
      mOwnerProgressListener->OnLocationChange(aURI);

   if(!mWebProgressListenerList)
      return NS_OK;

   PRUint32 count = 0;
   mWebProgressListenerList->Count(&count);
   for(PRUint32 x = 0; x < count; x++)
      {
      nsCOMPtr<nsISupports> element;
      mWebProgressListenerList->GetElementAt(x, getter_AddRefs(element));
      nsCOMPtr<nsIWebProgressListener> listener(do_QueryInterface(element));
      if(!listener) 
         continue;
      listener->OnLocationChange(aURI);
      }
   return NS_OK;
}

//*****************************************************************************
// nsDocShell: Helper Routines
//*****************************************************************************   

nsDocShellInitInfo* nsDocShell::InitInfo()
{
   if(mInitInfo)
      return mInitInfo;
   return mInitInfo = new nsDocShellInitInfo();
}

#define DIALOG_STRING_URI "chrome://global/locale/appstrings.properties"

NS_IMETHODIMP nsDocShell::GetStringBundle(nsIStringBundle** aStringBundle)
{
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

NS_IMETHODIMP
nsDocShell::FireStartDocumentLoad(nsIDocumentLoader* aLoader,
                                      nsIURI           * aURL,  //XXX: should be the channel?
                                      const char       * aCommand)
{
  NS_ENSURE_ARG_POINTER(aLoader);
  NS_ENSURE_ARG_POINTER(aURL);
  NS_ENSURE_ARG_POINTER(aCommand);

  nsCOMPtr<nsIDocumentViewer> docViewer;
  if (mScriptGlobal && (aLoader == mDocLoader.get())) 
  {
    docViewer = do_QueryInterface(mContentViewer);
    if (docViewer)
    {
      nsCOMPtr<nsIPresContext> presContext;
      NS_ENSURE_SUCCESS(docViewer->GetPresContext(*(getter_AddRefs(presContext))), NS_ERROR_FAILURE);
      if (presContext)
      {
        nsEventStatus status = nsEventStatus_eIgnore;
        nsMouseEvent event;
        event.eventStructType = NS_EVENT;
        event.message = NS_PAGE_UNLOAD;
        NS_ENSURE_SUCCESS(mScriptGlobal->HandleDOMEvent(presContext, 
                                                        &event, 
                                                        nsnull, 
                                                        NS_EVENT_FLAG_INIT, 
                                                        &status),
                          NS_ERROR_FAILURE);
      }
    }
  }

  if (aLoader == mDocLoader.get()) 
  {
    nsCOMPtr<nsIDocumentLoaderObserver> dlObserver;

    if (!mDocLoaderObserver && mParent) 
    {
      /* If this is a frame (in which case it would have a parent && doesn't
       * have a documentloaderObserver, get it from the rootWebShell
       */
      nsCOMPtr<nsIDocShellTreeItem> rootTreeItem;
      NS_ENSURE_SUCCESS(GetSameTypeRootTreeItem(getter_AddRefs(rootTreeItem)), 
         NS_ERROR_FAILURE);

      nsCOMPtr<nsIDocShell> rootAsDocShell(do_QueryInterface(rootTreeItem));
      if(rootAsDocShell)
         NS_ENSURE_SUCCESS(rootAsDocShell->GetDocLoaderObserver(getter_AddRefs(dlObserver)), 
            NS_ERROR_FAILURE);
    }
    else
    {
      dlObserver = do_QueryInterface(mDocLoaderObserver);  // we need this to addref
    }
    /*
     * Fire the OnStartDocumentLoad of the webshell observer
     */
    /* XXX This code means "notify dlObserver only if we're the top level webshell.
           I don't know why that would be, can't subdocument have doc loader observers?
     */
    if (/*(nsnull != mContainer) && */(nsnull != dlObserver))
    {
       NS_ENSURE_SUCCESS(dlObserver->OnStartDocumentLoad(mDocLoader, aURL, aCommand), 
                         NS_ERROR_FAILURE);
    }
  }

  return NS_OK;
}



NS_IMETHODIMP
nsDocShell::FireEndDocumentLoad(nsIDocumentLoader* aLoader,
                                    nsIChannel       * aChannel,
                                    nsresult           aStatus)
{
  NS_ENSURE_ARG_POINTER(aLoader);
  NS_ENSURE_ARG_POINTER(aChannel);

  nsCOMPtr<nsIURI> aURL;
  NS_ENSURE_SUCCESS(aChannel->GetURI(getter_AddRefs(aURL)), NS_ERROR_FAILURE);

  if (aLoader == mDocLoader.get())
  {
    if (mScriptGlobal && mContentViewer)
    {
      nsCOMPtr<nsIDocumentViewer> docViewer;
      docViewer = do_QueryInterface(mContentViewer);
      if (docViewer)
      {
        nsCOMPtr<nsIPresContext> presContext;
        NS_ENSURE_SUCCESS(docViewer->GetPresContext(*(getter_AddRefs(presContext))), NS_ERROR_FAILURE);
        if (presContext)
        {
          nsEventStatus status = nsEventStatus_eIgnore;
          nsMouseEvent event;
          event.eventStructType = NS_EVENT;
          event.message = NS_PAGE_LOAD;
          NS_ENSURE_SUCCESS(mScriptGlobal->HandleDOMEvent(presContext, 
                                                          &event, 
                                                          nsnull, 
                                                          NS_EVENT_FLAG_INIT, 
                                                          &status),
                            NS_ERROR_FAILURE);
        }
      }
    }

    // Fire the EndLoadURL of the web shell container
    /* XXX: what replaces mContainer?
    if (nsnull != aURL) 
    {
      nsAutoString urlString;
      char* spec;
      rv = aURL->GetSpec(&spec);
      if (NS_SUCCEEDED(rv)) 
      {
        urlString = spec;
        if (nsnull != mContainer) {
          rv = mContainer->EndLoadURL(this, urlString.GetUnicode(), 0);
        }
        nsCRT::free(spec);
      }
    }
    */

    nsCOMPtr<nsIDocumentLoaderObserver> dlObserver;
    if (!mDocLoaderObserver && mParent) 
    {
      // If this is a frame (in which case it would have a parent && doesn't
      // have a documentloaderObserver, get it from the rootWebShell
      nsCOMPtr<nsIDocShellTreeItem> rootTreeItem;
      NS_ENSURE_SUCCESS(GetSameTypeRootTreeItem(getter_AddRefs(rootTreeItem)), 
         NS_ERROR_FAILURE);

      nsCOMPtr<nsIDocShell> rootAsDocShell(do_QueryInterface(rootTreeItem));
      if(rootAsDocShell)
         NS_ENSURE_SUCCESS(rootAsDocShell->GetDocLoaderObserver(getter_AddRefs(dlObserver)),
            NS_ERROR_FAILURE);
    }
    else
    {
      /* Take care of the Trailing slash situation */
/* XXX: session history stuff, should be taken care of external to the docshell
      if (mSHist)
        CheckForTrailingSlash(aURL);
*/
      dlObserver = do_QueryInterface(mDocLoaderObserver);  // we need this to addref
    }

    /*
     * Fire the OnEndDocumentLoad of the DocLoaderobserver
     */
    if (dlObserver && aURL) {
       NS_ENSURE_SUCCESS(dlObserver->OnEndDocumentLoad(mDocLoader, aChannel, aStatus), 
                         NS_ERROR_FAILURE);
    }

    /* put the new document in the doc tree */
    NS_ENSURE_SUCCESS(InsertDocumentInDocTree(), NS_ERROR_FAILURE);
  }

  return NS_OK;
}

NS_IMETHODIMP nsDocShell::InsertDocumentInDocTree()
{
   nsCOMPtr<nsIDocShell> parent(do_QueryInterface(mParent));
  if(parent)
  {
    // Get the document object for the parent
    nsCOMPtr<nsIContentViewer> parentContentViewer;
    NS_ENSURE_SUCCESS(parent->GetContentViewer(getter_AddRefs(parentContentViewer)), 
                      NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(parentContentViewer, NS_ERROR_FAILURE);
    nsCOMPtr<nsIDocumentViewer> parentDocViewer;
    parentDocViewer = do_QueryInterface(parentContentViewer);
    NS_ENSURE_TRUE(parentDocViewer, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDocument> parentDoc;
    NS_ENSURE_SUCCESS(parentDocViewer->GetDocument(*getter_AddRefs(parentDoc)), NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(parentDoc, NS_ERROR_FAILURE);

    // Get the document object for this
    nsCOMPtr<nsIDocumentViewer> docViewer;
    docViewer = do_QueryInterface(mContentViewer);
    NS_ENSURE_TRUE(docViewer, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDocument> doc;
    NS_ENSURE_SUCCESS(docViewer->GetDocument(*getter_AddRefs(doc)), NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

    doc->SetParentDocument(parentDoc);
    parentDoc->AddSubDocument(doc);
  }
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
      mDocShell->LoadURI(mURI, nsnull);
  /*
   * LoadURL(...) will cancel all refresh timers... This causes the Timer and
   * its refreshData instance to be released...
   */
}

