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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsDocShell.h"
#include "nsIComponentManager.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDocumentViewer.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIDeviceContext.h"
#include "nsCURILoader.h"
#include "nsLayoutCID.h"
#include "nsNetUtil.h"
#include "nsRect.h"
#include "prprf.h"
#include "nsIFrame.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsXPIDLString.h"
#include "nsIChromeEventHandler.h"
#include "nsIDOMWindow.h"
#include "nsIWebBrowserChrome.h"

#ifdef XXX_NS_DEBUG       // XXX: we'll need a logging facility for debugging
#define WEB_TRACE(_bit,_args)            \
  PR_BEGIN_MACRO                         \
    if (WEB_LOG_TEST(gLogModule,_bit)) { \
      PR_LogPrint _args;                 \
    }                                    \
  PR_END_MACRO
#else
#define WEB_TRACE(_bit,_args)
#endif




//*****************************************************************************
//***    nsDocShell: Object Management
//*****************************************************************************

nsDocShell::nsDocShell() : 
  mContentListener(nsnull),
  mInitInfo(nsnull), 
  mMarginWidth(0), 
  mMarginHeight(0),
  mItemType(typeContent),
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

//*****************************************************************************
// nsDocShell::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(nsDocShell)
NS_IMPL_RELEASE(nsDocShell)

NS_INTERFACE_MAP_BEGIN(nsDocShell)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocShell)
   NS_INTERFACE_MAP_ENTRY(nsIDocShell)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeItem)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeNode)
   NS_INTERFACE_MAP_ENTRY(nsIWebNavigation)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
   NS_INTERFACE_MAP_ENTRY(nsIScrollable)
   NS_INTERFACE_MAP_ENTRY(nsITextScroll)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObjectOwner)
NS_INTERFACE_MAP_END

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
   else
      return QueryInterface(aIID, aSink);

   NS_IF_ADDREF(((nsISupports*)*aSink));
   return NS_OK;   
}

//*****************************************************************************
// nsDocShell::nsIDocShell
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::LoadURI(nsIURI* aUri, 
   nsIPresContext* presContext)
{
   //NS_ENSURE_ARG(aUri);  // Done in LoadURIVia for us.

   return LoadURIVia(aUri, presContext, 0);
}

NS_IMETHODIMP nsDocShell::LoadURIVia(nsIURI* aUri, 
   nsIPresContext* aPresContext, PRUint32 aAdapterBinding)
{
   NS_ENSURE_ARG(aUri);

   // If Anchor, goto anchor
   // Stop
   // URI Load

   nsCOMPtr<nsIURILoader> uriLoader = do_GetService(NS_URI_LOADER_PROGID);
   NS_ENSURE_TRUE(uriLoader, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(EnsureContentListener(), NS_ERROR_FAILURE);
   mContentListener->SetPresContext(aPresContext);

   // first, open a channel for the url
   nsCOMPtr<nsIChannel> channel;
   nsCOMPtr<nsIInterfaceRequestor> capabilities (do_QueryInterface(NS_STATIC_CAST(nsIDocShell *, this)));

   // we need to get the load group from our load cookie so we can pass it into open uri...
   nsCOMPtr<nsILoadGroup> loadGroup;
   NS_ENSURE_SUCCESS(uriLoader->GetLoadGroupForContext(NS_STATIC_CAST(nsISupports *, (nsIDocShell *) this), getter_AddRefs(loadGroup)),
                     NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(NS_OpenURI(getter_AddRefs(channel), aUri, loadGroup, capabilities), NS_ERROR_FAILURE);


   NS_ENSURE_SUCCESS(uriLoader->OpenURIVia(channel, nsIURILoader::viewNormal, nsnull, 
      NS_STATIC_CAST(nsIDocShell*, this), aAdapterBinding), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetCurrentURI(nsIURI** aURI)
{
   NS_ENSURE_ARG_POINTER(aURI);
   
   *aURI = mCurrentURI;
   NS_IF_ADDREF(*aURI);

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

   // (3) Tell the content viewer container to embed the content viewer.
   //     (This step causes everything to be set up for an initial flow.)
   NS_ENSURE_SUCCESS(Embed(documentViewer, "view", nsnull), NS_ERROR_FAILURE);

   // XXX: It would be great to get rid of this dummy channel!
   const nsAutoString uriString = "about:blank";
   nsCOMPtr<nsIURI> uri;
   NS_ENSURE_SUCCESS(NS_NewURI(getter_AddRefs(uri), uriString), NS_ERROR_FAILURE);
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
   //XXX: SetScrolling doesn't make any sense
   //SetScrolling(-1, PR_FALSE);
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

NS_IMETHODIMP nsDocShell::GetPrefs(nsIPref** aPrefs)
{
   NS_ENSURE_ARG_POINTER(aPrefs);

   *aPrefs = mPrefs;
   NS_IF_ADDREF(*aPrefs);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetPrefs(nsIPref* aPrefs)
{
  // null aPrefs is ok
  mPrefs = aPrefs;    // this assignment does an addref
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetZoom(float* zoom)
{
   NS_ENSURE_ARG_POINTER(zoom);
   NS_ENSURE_STATE(mContentViewer);

   nsCOMPtr<nsIPresContext> presContext;
   NS_ENSURE_SUCCESS(GetPresContext(getter_AddRefs(presContext)), 
      NS_ERROR_FAILURE);

   nsCOMPtr<nsIDeviceContext> deviceContext;
   NS_ENSURE_SUCCESS(presContext->GetDeviceContext(getter_AddRefs(deviceContext)),
      NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(deviceContext->GetZoom(*zoom), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetZoom(float zoom)
{
   NS_ENSURE_STATE(mContentViewer);

   nsCOMPtr<nsIPresContext> presContext;
   NS_ENSURE_SUCCESS(GetPresContext(getter_AddRefs(presContext)), 
      NS_ERROR_FAILURE);

   nsCOMPtr<nsIDeviceContext> deviceContext;
   NS_ENSURE_SUCCESS(presContext->GetDeviceContext(getter_AddRefs(deviceContext)),
      NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(deviceContext->SetZoom(zoom), NS_ERROR_FAILURE);

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

NS_IMETHODIMP
nsDocShell::GetMarginWidth(PRInt32* aWidth)
{
  NS_ENSURE_ARG_POINTER(aWidth);

  *aWidth = mMarginWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetMarginWidth(PRInt32 aWidth)
{
  mMarginWidth = aWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetMarginHeight(PRInt32* aHeight)
{
  NS_ENSURE_ARG_POINTER(aHeight);

  *aHeight = mMarginHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetMarginHeight(PRInt32 aHeight)
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
   NS_ENSURE_SUCCESS(FindChildWithName(aName, PR_TRUE, reqAsTreeItem, _retval), 
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
   PRBool aRecurse, nsIDocShellTreeItem* aRequestor, 
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
      child->GetName(getter_Copies(childName));
      if(name.Equals(childName))
         {
         *_retval = child;
         NS_ADDREF(*_retval);
         break;
         }

      if(aRecurse && (aRequestor != child)) // Only ask the child if it isn't the requestor
         {
         PRInt32 childType;
         child->GetItemType(&childType);

         if(childType == mItemType) //Only ask it to check children if it is same type
            {
            // See if child contains the shell with the given name
            nsCOMPtr<nsIDocShellTreeNode> childAsNode(do_QueryInterface(child));
            if(child)
               {
               NS_ENSURE_SUCCESS(childAsNode->FindChildWithName(aName, PR_TRUE,
                  NS_STATIC_CAST(nsIDocShellTreeItem*, this), _retval),
                  NS_ERROR_FAILURE);
               }
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
   //XXX First Checkin
   NS_ERROR("Not Yet Implemented");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::GetCanGoForward(PRBool* aCanGoForward)
{
   //XXX First Checkin
   NS_ERROR("Not Yet Implemented");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::GoBack()
{
   //XXX First Checkin
   NS_ERROR("Not Yet Implemented");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::GoForward()
{
   //XXX First Checkin
   NS_ERROR("Not Yet Implemented");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::LoadURI(const PRUnichar* aURI)
{
   // Mangle URL
   // If anchor goto Anchor
   // Stop Current Loads
   // URI Load
   //XXX First Checkin
   NS_ERROR("Not Yet Implemented");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::Reload()
{
   //XXX First Checkin
   NS_ERROR("Not Yet Implemented");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::Stop()
{
   if(mContentViewer)
      mContentViewer->Stop();

   if(mContentListener->mLoadCookie)
      {
      nsCOMPtr<nsIURILoader> uriLoader = do_GetService(NS_URI_LOADER_PROGID);
      if(uriLoader)
         uriLoader->Stop(mContentListener->mLoadCookie);
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

  nsCOMPtr<nsIPresShell> presShell;
  NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)), NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument>doc;
  NS_ENSURE_SUCCESS(presShell->GetDocument(getter_AddRefs(doc)), NS_ERROR_FAILURE);
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

   nsAutoString strSpec(spec);
   *aCurrentURI = strSpec.ToNewUnicode();

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

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::Destroy()
{
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

NS_IMETHODIMP nsDocShell::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
   if(mContentViewer)
      {
      PRInt32 x;
      PRInt32 y;

      NS_ENSURE_SUCCESS(GetPosition(&x, &y), NS_ERROR_FAILURE);

      //XXX Border figured in here or is that handled elsewhere?
      nsRect bounds(x, y, cx, cy);

      NS_ENSURE_SUCCESS(mContentViewer->SetBounds(bounds), NS_ERROR_FAILURE);
      }
   else if(InitInfo())
      {
      mInitInfo->cx = cx;
      mInitInfo->cy = cy;
      }
   else
      NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_FAILURE);

   return NS_OK;
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
   
   //XXX First Check
  /** 
   * Tell the window to repaint itself
   * @param aForce - if true, repaint immediately
   *                 if false, the window may defer repainting as it sees fit.
   */

   /* XXX Implement Tell our view to repaint

     if (mWindow) {
    mWindow->Invalidate(aForce);
  }

  nsresult rv;
  nsCOMPtr<nsIViewManager> viewManager;
  rv = GetViewManager(getter_AddRefs(viewManager));
  if (NS_FAILED(rv)) { return rv; }
  if (!viewManager) { return NS_ERROR_NULL_POINTER; }

  //XXX: what about aForce?
  rv = viewManager->UpdateAllViews(0);
  return rv;

   */
   return NS_ERROR_FAILURE;
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

NS_IMETHODIMP nsDocShell::GetMainWidget(nsIWidget** mainWidget)
{
   NS_ENSURE_ARG_POINTER(mainWidget);

   // For now we don't create our own widget, so simply return the parent one. 
   *mainWidget = mParentWidget;
   NS_IF_ADDREF(*mainWidget);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetFocus()
{
   //XXX First Check
  /**
  * Give the window focus.
  */

   /* XXX implement

     if (mWindow) {
    mWindow->SetFocus();
  }

  return NS_OK;

   */
   return NS_ERROR_FAILURE;
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

NS_IMETHODIMP nsDocShell::GetScrollbarPreferences(PRInt32 scrollOrientation,
   PRInt32* scrollbarPref)
{
   NS_ENSURE_ARG_POINTER(scrollbarPref);

   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);

   // XXX This is all evil, we need to convert.  We don't know our prefs
   // are the same as this interfaces.
 /*  nsScrollPreference scrollPref;

   NS_ENSURE_SUCCESS(scrollView->GetScrollPreference(scrollPref), 
      NS_ERROR_FAILURE);

   *scrollbarPref = scrollPref; */

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetScrollbarPreferences(PRInt32 scrollOrientation,
   PRInt32 scrollbarPref)
{
   nsCOMPtr<nsIScrollableView> scrollView;
   NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)), 
      NS_ERROR_FAILURE);

   // XXX This is evil.  We should do a mapping, we don't know our prefs
   // are the same as this interface.  In fact it doesn't compile
  /* nsScrollPreference scrollPref = scrollbarPref;
   NS_ENSURE_SUCCESS(scrollView->SetScrollPreference(scrollPref), 
      NS_ERROR_FAILURE);  */

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

NS_IMETHODIMP nsDocShell::ReportScriptError(const char* aErrorString, 
   const char* aFileName, PRInt32 aLineNo, const char* aLineBuf)
{
   //XXX Needs some international work.
   nsAutoString error;
   error.SetString("JavaScript Error: ");
   error.Append(aErrorString);
   error += "\n";

   if(aFileName)
      {
      error += "URL: ";
      error += aFileName;
      error += "\n";
      }

   if(aLineNo)
      {
      error += "LineNo: ";
      error.Append(aLineNo, 10);
      error += "\n";
      }

   if(aLineBuf)
      {
      error += "Line text: '";
      error += aLineBuf;
      error += "'\n";
      }

   // XXX Should not do a printf
   char* errorStr = error.ToNewCString();
   if(errorStr)
      {
      printf("%s\n", errorStr);
      nsCRT::free(errorStr);
      }

   //XXXEMBEDDING Call embedding app with the error.

   // XXX Turn it off for now...there should be an Error method too
   //Alert(error.GetUnicode());

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

NS_IMETHODIMP nsDocShell::HandleUnknownContentType(nsIDocumentLoader* aLoader,
                                      nsIChannel* channel,
                                      const char *aContentType,
                                      const char *aCommand)
{
  NS_ENSURE_SUCCESS(PR_FALSE, NS_ERROR_NOT_IMPLEMENTED);
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

NS_IMETHODIMP nsDocShell::GetTarget(const PRUnichar* aName, nsIDocShellTreeItem** aShell)
{
   nsAutoString name(aName);

   if(!name.Length() || name.EqualsIgnoreCase("_self"))
      {
      *aShell = NS_STATIC_CAST(nsIDocShellTreeItem*, this);
      }
   else if(name.EqualsIgnoreCase("_blank"))
      {
      NS_ENSURE_SUCCESS(CreateTargetLocation(nsnull, aShell), NS_ERROR_FAILURE);
      }
   else if(name.EqualsIgnoreCase("_parent"))
      {
      GetSameTypeParent(aShell);
      if(!*aShell)
         *aShell = NS_STATIC_CAST(nsIDocShellTreeItem*, this);
      }
   else if(name.EqualsIgnoreCase("_top"))
      {
      NS_ENSURE_SUCCESS(GetSameTypeRootTreeItem(aShell), NS_ERROR_FAILURE);
      }
   else if(name.EqualsIgnoreCase("_content"))
      {
      if(mTreeOwner)
         mTreeOwner->FindItemWithName(aName, nsnull, aShell);
      else
         {
         NS_ERROR("Someone isn't setting up the tree owner.  You might like to try that."
            "Things will.....you know, work.");
         }
      }
   else
      {
      NS_ENSURE_SUCCESS(FindItemWithName(aName, nsnull, aShell), NS_ERROR_FAILURE);
      if(!*aShell)
         {
         NS_ENSURE_SUCCESS(CreateTargetLocation(aName, aShell), NS_ERROR_FAILURE);
         }
      }

   NS_IF_ADDREF(*aShell);

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::CreateTargetLocation(const PRUnichar* aName, 
   nsIDocShellTreeItem** aShell)
{
   return mTreeOwner->GetNewWindow(nsIWebBrowserChrome::allChrome, aShell);
}

void nsDocShell::SetCurrentURI(nsIURI* aUri)
{
   mCurrentURI = aUri; //This assignment addrefs
}

NS_IMETHODIMP nsDocShell::EnsureContentViewer()
{
   if(mContentViewer)
      return NS_OK;

   return CreateAboutBlankContentViewer();
}

NS_IMETHODIMP nsDocShell::CreateAboutBlankContentViewer()
{
   // XXX
   NS_ERROR("Not Implemented yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::CreateContentViewer(const char* aContentType, 
   nsURILoadCommand aCommand, nsIChannel* aOpenedChannel, 
   nsIStreamListener** aContentHandler)
{
   //XXX We should return NS_ERROR_FAILURE if we can't create a new 
   // one and then leave our window in tact....
   //XXXQ Can we check the content type of the current content viewer
   // and reuse it without destroying it and re-creating it?

   // XXXIMPL Do cleanup....

   // Instantiate the content viewer object
   NS_ENSURE_SUCCESS(NewContentViewerObj(aContentType, aCommand, aOpenedChannel,
      aContentHandler), NS_ERROR_FAILURE);

   //XXXIMPL Do stuff found in embed here.  Don't call embed it is going away.

   return NS_ERROR_FAILURE;
}

nsresult nsDocShell::NewContentViewerObj(const char* aContentType,
   nsURILoadCommand aCommand, nsIChannel* aOpenedChannel, 
   nsIStreamListener** aContentHandler)
{
   //XXX This should probably be some category thing....
   char id[256];
   PR_snprintf(id, sizeof(id), NS_DOCUMENT_LOADER_FACTORY_PROGID_PREFIX "%s/%s",
      aCommand , aContentType);

   // Create an instance of the document-loader-factory
   nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory(do_CreateInstance(id));
   NS_ENSURE_TRUE(docLoaderFactory, NS_ERROR_FAILURE);

   nsCOMPtr<nsILoadGroup> loadGroup(do_QueryInterface(mContentListener->mLoadCookie));
   // Now create an instance of the content viewer
   nsXPIDLCString strCommand;
   // go to the uri loader and ask it to convert the uri load command into a old
   // world style string
   nsresult rv = NS_OK;
   NS_WITH_SERVICE(nsIURILoader, pURILoader, NS_URI_LOADER_PROGID, &rv);
   if (NS_SUCCEEDED(rv))
    NS_ENSURE_SUCCESS(pURILoader->GetStringForCommand(aCommand, getter_Copies(strCommand)), NS_ERROR_FAILURE);
   
   NS_ENSURE_SUCCESS(docLoaderFactory->CreateInstance(strCommand, aOpenedChannel,
      loadGroup, aContentType, NS_STATIC_CAST(nsIContentViewerContainer*, this),
      nsnull /*XXXQ Need ExtraInfo???*/,
      aContentHandler, getter_AddRefs(mContentViewer)), NS_ERROR_FAILURE);

   return rv;
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
/*   if(NS_FAILED(mContentViewer->Init(widget->GetNativeData(NS_NATIVE_WIDGET),
      mDeviceContext, mPrefs, bounds, nsScrollPreference_kAuto)))
      {
      mContentViewer = nsnull;
      NS_ERROR("ContentViewer Initialization failed");
      return NS_ERROR_FAILURE;
      }   

   /*
    XXXTAB Don't think we need this in new world
    // If the history state has been set by session history,
    // set it on the pres shell now that we have a content
    // viewer.
   if(mContentViewer && mHistoryState)
      {
      nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(mContentViewer));
      if(docv)
         {
         nsCOMPtr<nsIPresShell> shell;
         docv->GetPresShell(*getter_AddRefs(shell));
         if(shell)
            shell->SetHistoryState(mHistoryState);
         }
      }
   */
   NS_ERROR("Uncomment above");
   mContentViewer->Show();

   // Now that we have switched documents, forget all of our children
   DestroyChildren();

   return NS_OK;
}

NS_IMETHODIMP nsDocShell::StopCurrentLoads()
{
   // XXX
   NS_ERROR("Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsDocShell::AddCurrentSiteToHistories()
{
   // XXX
   NS_ERROR("Not Yet Implemented");
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

NS_IMETHODIMP nsDocShell::DestroyChildren()
{
   PRInt32 i, n = mChildren.Count();
   nsCOMPtr<nsIDocShellTreeItem>   shell;
   for (i = 0; i < n; i++) 
      {
      shell = dont_AddRef((nsIDocShellTreeItem*)mChildren.ElementAt(i));
      if(!NS_WARN_IF_FALSE(shell, "docshell has null child"))
         shell->SetParent(nsnull);
      }
   mChildren.Clear();
   return NS_OK;
}

nsresult nsDocShell::GetPrimaryFrameFor(nsIContent* content, nsIFrame** frame)
{
   //XXX Implement
   return NS_ERROR_FAILURE;
}   
