/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:ts=2:et:sw=2:
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com> 
 *   Dan Rosen <dr@netscape.com>
 */

#ifdef XP_OS2_VACPP
// XXX every other file that pulls in _os2.h has no problem with HTMX there;
// this one does; the problem may lie with the order of the headers below,
// which is why this fix is here instead of in _os2.h
typedef unsigned long HMTX;
#endif

#include "nsDocShell.h"
#include "nsIWebShell.h"
#include "nsWebShell.h"
#include "nsIWebBrowserChrome.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebProgress.h"
#include "nsIDocumentLoader.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIClipboardCommands.h"
#include "nsILinkHandler.h"
#include "nsIStreamListener.h"
#include "nsIPrompt.h"
#include "nsNetUtil.h"
#include "nsIProtocolHandler.h"
#include "nsIDNSService.h"
#include "nsIRefreshURI.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIDOMEvent.h"
#include "nsIPresContext.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsCRT.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"
#include "plevent.h"
#include "prprf.h"
#include "nsIPluginHost.h"
#include "nsplugin.h"
//#include "nsPluginsCID.h"
#include "nsIPluginManager.h"
#include "nsIPref.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "nsIContent.h"
#include "prlog.h"
#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsIWebShellServices.h"
#include "nsIGlobalHistory.h"
#include "prmem.h"
#include "prthread.h"
#include "nsXPIDLString.h"
#include "nsDOMError.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLDocument.h"
#include "nsLayoutCID.h"
#include "nsIDOMRange.h"
#include "nsIURIContentListener.h"
#include "nsIDOMDocument.h"
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsCURILoader.h"
#include "nsIDOMWindowInternal.h"
#include "nsEscape.h"
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"
#include "nsISocketTransportService.h"
#include "nsILayoutHistoryState.h"
#include "nsTextFormatter.h"
#include "nsPIDOMWindow.h"
#include "nsIController.h"
#include "nsIFocusController.h"
#include "nsGUIEvent.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsIFileStream.h"
#include "nsISHistoryInternal.h"

#include "nsIHttpChannel.h" // add this to the ick include list...we need it to QI for post data interface
#include "nsIUploadChannel.h"

#include "nsILocaleService.h"
#include "nsIStringBundle.h"

#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIProtocolHandler.h"
#include "nsICachingChannel.h"

//XXX for nsIPostData; this is wrong; we shouldn't see the nsIDocument type
#include "nsIDocument.h"
#include "nsITextToSubURI.h"

#ifdef NS_DEBUG
/**
 * Note: the log module is created during initialization which
 * means that you cannot perform logging before then.
 */
static PRLogModuleInfo* gLogModule = PR_NewLogModule("webshell");
#endif

#define WEB_TRACE_CALLS        0x1
#define WEB_TRACE_HISTORY      0x2

#define WEB_LOG_TEST(_lm,_bit) (PRIntn((_lm)->level) & (_bit))

#ifdef NS_DEBUG
#define WEB_TRACE(_bit,_args)            \
  PR_BEGIN_MACRO                         \
    if (WEB_LOG_TEST(gLogModule,_bit)) { \
      PR_LogPrint _args;                 \
    }                                    \
  PR_END_MACRO
#else
#define WEB_TRACE(_bit,_args)
#endif

//static NS_DEFINE_CID(kGlobalHistoryCID, NS_GLOBALHISTORY_CID);
//static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

//----------------------------------------------------------------------

//----------------------------------------------------------------------

// Class IID's
static NS_DEFINE_IID(kEventQueueServiceCID,   NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kCDOMRangeCID,           NS_RANGE_CID);

//----------------------------------------------------------------------

// Note: operator new zeros our memory
nsWebShell::nsWebShell() : nsDocShell()
{
#ifdef DEBUG
  // We're counting the number of |nsWebShells| to help find leaks
  ++gNumberOfWebShells;
#endif
#ifdef DEBUG
    printf("WEBSHELL+ = %ld\n", gNumberOfWebShells);
#endif

  NS_INIT_REFCNT();
  mThread = nsnull;
  InitFrameData();
  mItemType = typeContent;
  mCharsetReloadState = eCharsetReloadInit;
  mHistoryState = nsnull;
  mBounds.SetRect(0, 0, 0, 0);
}

nsWebShell::~nsWebShell()
{
   Destroy();

  // Stop any pending document loads and destroy the loader...
  if (nsnull != mDocLoader) {
    mDocLoader->Stop();
    mDocLoader->SetContainer(nsnull);
    mDocLoader->Destroy();
    NS_RELEASE(mDocLoader);
  }
  // Cancel any timers that were set for this loader.
  CancelRefreshURITimers();

  ++mRefCnt; // following releases can cause this destructor to be called
             // recursively if the refcount is allowed to remain 0

  mContentViewer=nsnull;
  mDeviceContext=nsnull;
  NS_IF_RELEASE(mContainer);

  if (mScriptGlobal) {
    mScriptGlobal->SetDocShell(nsnull);
    mScriptGlobal = nsnull;
  }
  if (mScriptContext) {
    mScriptContext->SetOwner(nsnull);
    mScriptContext = nsnull;
  }

  InitFrameData();

#ifdef DEBUG
  // We're counting the number of |nsWebShells| to help find leaks
  --gNumberOfWebShells;
#endif
#ifdef DEBUG
  printf("WEBSHELL- = %ld\n", gNumberOfWebShells);
#endif
}

void nsWebShell::InitFrameData()
{
  SetMarginWidth(-1);    
  SetMarginHeight(-1);
}

NS_IMPL_ADDREF_INHERITED(nsWebShell, nsDocShell)
NS_IMPL_RELEASE_INHERITED(nsWebShell, nsDocShell)

NS_INTERFACE_MAP_BEGIN(nsWebShell)
#if 0 // inherits from nsDocShell:
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebShell)
#endif
   NS_INTERFACE_MAP_ENTRY(nsIWebShell)
   NS_INTERFACE_MAP_ENTRY(nsIWebShellServices)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIContentViewerContainer, nsIWebShell)
   NS_INTERFACE_MAP_ENTRY(nsIWebShellContainer)
   NS_INTERFACE_MAP_ENTRY(nsILinkHandler)
   NS_INTERFACE_MAP_ENTRY(nsIClipboardCommands)
#if 0 // inherits from nsDocShell:
   NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObjectOwner)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
   NS_INTERFACE_MAP_ENTRY(nsIDocShell)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeItem)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeNode)
   NS_INTERFACE_MAP_ENTRY(nsIWebNavigation)
   NS_INTERFACE_MAP_ENTRY(nsIRefreshURI)
   NS_INTERFACE_MAP_ENTRY(nsIScrollable)
#endif
NS_INTERFACE_MAP_END_INHERITING(nsDocShell)

NS_IMETHODIMP
nsWebShell::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
   NS_ENSURE_ARG_POINTER(aInstancePtr);
   nsresult rv = NS_OK;
   *aInstancePtr = nsnull;

   if(aIID.Equals(NS_GET_IID(nsILinkHandler)))
      {
      *aInstancePtr = NS_STATIC_CAST(nsILinkHandler*, this);
      NS_ADDREF((nsISupports*)*aInstancePtr);
      return NS_OK;
      }
   else if(aIID.Equals(NS_GET_IID(nsIScriptGlobalObjectOwner)))
      {
      *aInstancePtr = NS_STATIC_CAST(nsIScriptGlobalObjectOwner*, this);
      NS_ADDREF((nsISupports*)*aInstancePtr);
      return NS_OK;
      }
   else if(aIID.Equals(NS_GET_IID(nsIScriptGlobalObject)))
      {
      NS_ENSURE_SUCCESS(EnsureScriptEnvironment(), NS_ERROR_FAILURE);
      *aInstancePtr = mScriptGlobal;
      NS_ADDREF((nsISupports*)*aInstancePtr);
      return NS_OK;
      }
   else if(aIID.Equals(NS_GET_IID(nsIDOMWindowInternal)) ||
           aIID.Equals(NS_GET_IID(nsIDOMWindow)))

      {
      NS_ENSURE_SUCCESS(EnsureScriptEnvironment(), NS_ERROR_FAILURE);
      NS_ENSURE_SUCCESS(mScriptGlobal->QueryInterface(aIID, aInstancePtr),
                        NS_ERROR_FAILURE);
      return NS_OK;
      }

   if (!*aInstancePtr || NS_FAILED(rv))
     return nsDocShell::GetInterface(aIID,aInstancePtr);
   else
     return rv;
}

NS_IMETHODIMP
nsWebShell::SetupNewViewer(nsIContentViewer* aViewer)
{
   NS_ENSURE_SUCCESS(nsDocShell::SetupNewViewer(aViewer), NS_ERROR_FAILURE);

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
            shell->SetHistoryState((nsILayoutHistoryState*)mHistoryState);
         }
      }
   return NS_OK;
}

NS_IMETHODIMP
nsWebShell::Embed(nsIContentViewer* aContentViewer,
                  const char* aCommand,
                  nsISupports* aExtraInfo)
{
	return nsDocShell::Embed(aContentViewer, aCommand, aExtraInfo);
}

NS_IMETHODIMP
nsWebShell::SetContainer(nsIWebShellContainer* aContainer)
{
  NS_IF_RELEASE(mContainer);
  mContainer = aContainer;
  NS_IF_ADDREF(mContainer);

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetContainer(nsIWebShellContainer*& aResult)
{
  aResult = mContainer;
  NS_IF_ADDREF(mContainer);
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetTopLevelWindow(nsIWebShellContainer** aTopLevelWindow)
{
   NS_ENSURE_ARG_POINTER(aTopLevelWindow);
   *aTopLevelWindow = nsnull;

   nsCOMPtr<nsIWebShell> rootWebShell;

   GetRootWebShellEvenIfChrome(getter_AddRefs(rootWebShell));
   if(!rootWebShell)
      return NS_OK;
   
   nsCOMPtr<nsIWebShellContainer> rootContainer;
   rootWebShell->GetContainer(*aTopLevelWindow);

   return NS_OK;
}

nsEventStatus PR_CALLBACK
nsWebShell::HandleEvent(nsGUIEvent *aEvent)
{
  return nsEventStatus_eIgnore;
}

NS_IMETHODIMP
nsWebShell::GetRootWebShell(nsIWebShell*& aResult)
{
   nsCOMPtr<nsIDocShellTreeItem> top;
   GetSameTypeRootTreeItem(getter_AddRefs(top));
   nsCOMPtr<nsIWebShell> topAsWebShell(do_QueryInterface(top));
   aResult = topAsWebShell;
   NS_IF_ADDREF(aResult);
   return NS_OK;
}

void
nsWebShell::GetRootWebShellEvenIfChrome(nsIWebShell** aResult)
{
   nsCOMPtr<nsIDocShellTreeItem> top;
   GetRootTreeItem(getter_AddRefs(top));
   nsCOMPtr<nsIWebShell> topAsWebShell(do_QueryInterface(top));
   *aResult = topAsWebShell;
   NS_IF_ADDREF(*aResult);
}

/*
NS_IMETHODIMP
nsWebShell::SetParent(nsIWebShell* aParent)
{
   nsCOMPtr<nsIDocShellTreeItem> parentAsTreeItem(do_QueryInterface(aParent));

   mParent = parentAsTreeItem.get();
   return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetParent(nsIWebShell*& aParent)
{
   nsCOMPtr<nsIDocShellTreeItem> parent;
   NS_ENSURE_SUCCESS(GetSameTypeParent(getter_AddRefs(parent)), NS_ERROR_FAILURE);

   if(parent)
      parent->QueryInterface(NS_GET_IID(nsIWebShell), (void**)&aParent);
   else
      aParent = nsnull;
   return NS_OK;
}
*/
NS_IMETHODIMP
nsWebShell::GetReferrer(nsIURI **aReferrer)
{
   *aReferrer = mReferrerURI;
   NS_IF_ADDREF(*aReferrer);
   return NS_OK;
}

void
nsWebShell::SetReferrer(const PRUnichar* aReferrer)
{
   NS_NewURI(getter_AddRefs(mReferrerURI), nsDependentString(aReferrer), nsnull);
}

NS_IMETHODIMP
nsWebShell::SetURL(const PRUnichar* aURL)
{
  nsCOMPtr<nsIURI> uri;
  NS_ENSURE_SUCCESS(NS_NewURI(getter_AddRefs(uri), nsDependentString(aURL),
                              nsnull),
                    NS_ERROR_FAILURE);
  SetCurrentURI(uri);
  return NS_OK;
}

/**
 * Document Load methods
 */
NS_IMETHODIMP
nsWebShell::GetDocumentLoader(nsIDocumentLoader*& aResult)
{
  aResult = mDocLoader;
  NS_IF_ADDREF(mDocLoader);
  return (nsnull != mDocLoader) ? NS_OK : NS_ERROR_FAILURE;
}

//----------------------------------------

// History methods

NS_IMETHODIMP nsWebShell::GoTo(PRInt32 aIndex)
{
   NS_ENSURE_STATE(mSessionHistory);
   NS_ENSURE_TRUE(!IsFrame(), NS_ERROR_FAILURE);
   nsCOMPtr<nsIHistoryEntry> entry;

   mSessionHistory->GetEntryAtIndex(aIndex, PR_TRUE, getter_AddRefs(entry));
   NS_ENSURE_TRUE(entry, NS_ERROR_FAILURE);
    nsCOMPtr<nsISHEntry> shEntry(do_QueryInterface(entry));
    NS_ENSURE_TRUE(shEntry, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(LoadHistoryEntry(shEntry, LOAD_HISTORY), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetHistoryLength(PRInt32& aResult)
{
   NS_ENSURE_STATE(mSessionHistory);
   NS_ENSURE_TRUE(!IsFrame(), NS_ERROR_FAILURE);
   
   NS_ENSURE_SUCCESS(mSessionHistory->GetCount(&aResult), NS_ERROR_FAILURE);
   return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetHistoryIndex(PRInt32& aResult)
{
   NS_ENSURE_STATE(mSessionHistory);
   NS_ENSURE_TRUE(!IsFrame(), NS_ERROR_FAILURE);
   
   NS_ENSURE_SUCCESS(mSessionHistory->GetIndex(&aResult), NS_ERROR_FAILURE);
   return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetURL(PRInt32 aIndex, const PRUnichar** aURLResult)
{
   NS_ENSURE_STATE(mSessionHistory);
   NS_ENSURE_TRUE(!IsFrame(), NS_ERROR_FAILURE);

   nsCOMPtr<nsIHistoryEntry> entry;

   NS_ENSURE_SUCCESS(mSessionHistory->GetEntryAtIndex(aIndex, PR_TRUE, 
      getter_AddRefs(entry)), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(entry, NS_ERROR_FAILURE);

   nsCOMPtr<nsIURI> uri;

   entry->GetURI(getter_AddRefs(uri));

   NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

   nsXPIDLCString spec;
   uri->GetSpec(getter_Copies(spec));

   *aURLResult = NS_ConvertASCIItoUCS2(spec).ToNewUnicode();

   return NS_OK;
}

//----------------------------------------

//----------------------------------------------------------------------

// WebShell container implementation

NS_IMETHODIMP
nsWebShell::SetHistoryState(nsISupports* aLayoutHistoryState)
{
  mHistoryState = aLayoutHistoryState;
  return NS_OK;
}

//----------------------------------------------------------------------
// Web Shell Services API

NS_IMETHODIMP
nsWebShell::LoadDocument(const char* aURL,
                         const char* aCharset,
                         nsCharsetSource aSource)
{
  // XXX hack. kee the aCharset and aSource wait to pick it up
  nsCOMPtr<nsIContentViewer> cv;
  NS_ENSURE_SUCCESS(GetContentViewer(getter_AddRefs(cv)), NS_ERROR_FAILURE);
  if (cv)
  {
    nsCOMPtr<nsIMarkupDocumentViewer> muDV = do_QueryInterface(cv);  
    if (muDV)
    {
      nsCharsetSource hint;
      muDV->GetHintCharacterSetSource((PRInt32 *)(&hint));
      if( aSource > hint ) 
      {
        muDV->SetHintCharacterSet(NS_ConvertASCIItoUCS2(aCharset).get());
        muDV->SetHintCharacterSetSource((PRInt32)aSource);
        if(eCharsetReloadRequested != mCharsetReloadState) 
        {
          mCharsetReloadState = eCharsetReloadRequested;
          LoadURI(NS_ConvertASCIItoUCS2(aURL).get(), LOAD_FLAGS_NONE);
        }
      }
    }
  }
  return NS_OK;
}

//This functions is only called when a new charset is detected in loading a document. 
//Its name should be changed to "CharsetReloadDocument"
NS_IMETHODIMP
nsWebShell::ReloadDocument(const char* aCharset,
                           nsCharsetSource aSource)
{

  // XXX hack. kee the aCharset and aSource wait to pick it up
  nsCOMPtr<nsIContentViewer> cv;
  NS_ENSURE_SUCCESS(GetContentViewer(getter_AddRefs(cv)), NS_ERROR_FAILURE);
  if (cv)
  {
    nsCOMPtr<nsIMarkupDocumentViewer> muDV = do_QueryInterface(cv);  
    if (muDV)
    {
      nsCharsetSource hint;
      muDV->GetHintCharacterSetSource((PRInt32 *)(&hint));
      if( aSource > hint ) 
      {
         muDV->SetHintCharacterSet(NS_ConvertASCIItoUCS2(aCharset).get());
         muDV->SetHintCharacterSetSource((PRInt32)aSource);
         if(eCharsetReloadRequested != mCharsetReloadState) 
         {
            mCharsetReloadState = eCharsetReloadRequested;
            return Reload(LOAD_FLAGS_CHARSET_CHANGE);
         }
      }
    }
  }
  //return failer if this request is not accepted due to mCharsetReloadState
  return NS_ERROR_WEBSHELL_REQUEST_REJECTED;
}


NS_IMETHODIMP
nsWebShell::StopDocumentLoad(void)
{
  if(eCharsetReloadRequested != mCharsetReloadState) 
  {
    Stop(nsIWebNavigation::STOP_ALL);
    return NS_OK;
  }
  //return failer if this request is not accepted due to mCharsetReloadState
  return NS_ERROR_WEBSHELL_REQUEST_REJECTED;
}

NS_IMETHODIMP
nsWebShell::SetRendering(PRBool aRender)
{
  if(eCharsetReloadRequested != mCharsetReloadState) 
  {
    if (mContentViewer) {
       mContentViewer->SetEnableRendering(aRender);
       return NS_OK;
    }
  }
  //return failer if this request is not accepted due to mCharsetReloadState
  return NS_ERROR_WEBSHELL_REQUEST_REJECTED;
}

//----------------------------------------------------------------------

// WebShell link handling

struct OnLinkClickEvent : public PLEvent {
  OnLinkClickEvent(nsWebShell* aHandler, nsIContent* aContent,
                   nsLinkVerb aVerb, const PRUnichar* aURLSpec,
                   const PRUnichar* aTargetSpec, nsIInputStream* aPostDataStream = 0, 
                   nsIInputStream* aHeadersDataStream = 0);
  ~OnLinkClickEvent();

  void HandleEvent() {
    mHandler->HandleLinkClickEvent(mContent, mVerb, mURLSpec->get(),
                                   mTargetSpec->get(), mPostDataStream,
                                   mHeadersDataStream);
  }

  nsWebShell*  mHandler;
  nsString*    mURLSpec;
  nsString*    mTargetSpec;
  nsIInputStream* mPostDataStream;
  nsIInputStream* mHeadersDataStream;
  nsIContent*     mContent;
  nsLinkVerb      mVerb;
};

static void PR_CALLBACK HandlePLEvent(OnLinkClickEvent* aEvent)
{
  aEvent->HandleEvent();
}

static void PR_CALLBACK DestroyPLEvent(OnLinkClickEvent* aEvent)
{
  delete aEvent;
}

OnLinkClickEvent::OnLinkClickEvent(nsWebShell* aHandler,
                                   nsIContent *aContent,
                                   nsLinkVerb aVerb,
                                   const PRUnichar* aURLSpec,
                                   const PRUnichar* aTargetSpec,
                                   nsIInputStream* aPostDataStream,
                                   nsIInputStream* aHeadersDataStream)
{
  mHandler = aHandler;
  NS_ADDREF(aHandler);
  mURLSpec = new nsString(aURLSpec);
  mTargetSpec = new nsString(aTargetSpec);
  mPostDataStream = aPostDataStream;
  NS_IF_ADDREF(mPostDataStream);
  mHeadersDataStream = aHeadersDataStream;
  NS_IF_ADDREF(mHeadersDataStream);
  mContent = aContent;
  NS_IF_ADDREF(mContent);
  mVerb = aVerb;

  PL_InitEvent(this, nsnull,
               (PLHandleEventProc) ::HandlePLEvent,
               (PLDestroyEventProc) ::DestroyPLEvent);

  nsCOMPtr<nsIEventQueue> eventQueue;
  aHandler->GetEventQueue(getter_AddRefs(eventQueue));
  NS_ASSERTION(eventQueue, "no event queue");
  if (eventQueue)
    eventQueue->PostEvent(this);
}

OnLinkClickEvent::~OnLinkClickEvent()
{
  NS_IF_RELEASE(mContent);
  NS_IF_RELEASE(mHandler);
  NS_IF_RELEASE(mPostDataStream);
  NS_IF_RELEASE(mHeadersDataStream);
  if (nsnull != mURLSpec) delete mURLSpec;
  if (nsnull != mTargetSpec) delete mTargetSpec;

}

//----------------------------------------

NS_IMETHODIMP
nsWebShell::OnLinkClick(nsIContent* aContent,
                        nsLinkVerb aVerb,
                        const PRUnichar* aURLSpec,
                        const PRUnichar* aTargetSpec,
                        nsIInputStream* aPostDataStream,
                        nsIInputStream* aHeadersDataStream)
{
  OnLinkClickEvent* ev;
  nsresult rv = NS_OK;

  ev = new OnLinkClickEvent(this, aContent, aVerb, aURLSpec,
                            aTargetSpec, aPostDataStream, aHeadersDataStream);
  if (nsnull == ev) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  return rv;
}

nsresult
nsWebShell::GetEventQueue(nsIEventQueue **aQueue)
{
  NS_ENSURE_ARG_POINTER(aQueue);
  *aQueue = 0;

  nsCOMPtr<nsIEventQueueService> eventService(do_GetService(kEventQueueServiceCID));
  if (eventService)
    eventService->GetThreadEventQueue(mThread, aQueue);
  return *aQueue ? NS_OK : NS_ERROR_FAILURE;
}

void
nsWebShell::HandleLinkClickEvent(nsIContent *aContent,
                                 nsLinkVerb aVerb,
                                 const PRUnichar* aURLSpec,
                                 const PRUnichar* aTargetSpec,
                                 nsIInputStream* aPostDataStream,
                                 nsIInputStream* aHeadersDataStream)
{
  nsresult rv;
  nsAutoString target(aTargetSpec);

  switch(aVerb) {
    case eLinkVerb_New:
      target.AssignWithConversion("_blank");
      // Fall into replace case
    case eLinkVerb_Undefined:
      // Fall through, this seems like the most reasonable action
    case eLinkVerb_Replace:
      {
        // for now, just hack the verb to be view-link-clicked
        // and down in the load document code we'll detect this and
        // set the correct uri loader command
        nsCOMPtr<nsIURI> uri;
        NS_NewURI(getter_AddRefs(uri), nsDependentString(aURLSpec), nsnull);

        // No URI object? This may indicate the URLspec is for an
        // unrecognized protocol. Embedders might still be interested
        // in handling the click, so we fire a notification before
        // throwing the click away.
        if (!uri && NS_SUCCEEDED(EnsureContentListener()))
        {
            nsCOMPtr<nsIURIContentListener> listener = do_QueryInterface(mContentListener);
            nsCAutoString spec; spec.AssignWithConversion(aURLSpec);
            PRBool abort = PR_FALSE;
            uri = do_CreateInstance(kSimpleURICID, &rv);
            NS_ASSERTION(NS_SUCCEEDED(rv), "can't create simple uri");
            if (NS_SUCCEEDED(rv))
            {
                rv = uri->SetSpec(spec);
                NS_ASSERTION(NS_SUCCEEDED(rv), "spec is invalid");
                if (NS_SUCCEEDED(rv))
                {
                    listener->OnStartURIOpen(uri, &abort);
                }
            }
            return;
        }

        rv = InternalLoad(uri,                // New URI
                          mCurrentURI,        // Referer URI
                          nsnull,             // No onwer
                          PR_TRUE,            // Inherit owner from document
                          target.get(),       // Window target
                          aPostDataStream,    // Post data stream
                          aHeadersDataStream, // Headers stream
                          LOAD_LINK,          // Load type
                          nsnull);            // No SHEntry
      }
      break;
    case eLinkVerb_Embed:
      // XXX TODO Should be similar to the HTML IMG ALT attribute handling
      //          in NS 4.x
    default:
      NS_ABORT_IF_FALSE(0,"unexpected link verb");
  }
}

NS_IMETHODIMP
nsWebShell::OnOverLink(nsIContent* aContent,
                       const PRUnichar* aURLSpec,
                       const PRUnichar* aTargetSpec)
{
    nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(mTreeOwner));
  nsresult rv = NS_ERROR_FAILURE;

  if(browserChrome)  {
    nsCOMPtr<nsITextToSubURI> textToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    PRUnichar *uStr;
    nsString aSpec(aURLSpec);
    PRInt32 pos = aSpec.FindChar(':');
    nsAutoString scheme;
    static const char kMailToURI[] = "mailto";
    // If the scheme is mailtourl then should not convert from a document charset
    if ((pos == (PRInt32)(sizeof kMailToURI - 1)) && (aSpec.Left(scheme, pos) != -1) &&
         scheme.EqualsIgnoreCase(kMailToURI)) {
      // use UTF-8 instead and apply URL escape.
      rv = textToSubURI->UnEscapeAndConvert("UTF-8",NS_ConvertUCS2toUTF8(aURLSpec).get(), &uStr);
    }
    else  {
      // use doc charset instead and apply URL escape.
      nsCOMPtr<nsIPresShell> presShell;
      nsCOMPtr<nsIDocument> doc;
      nsDocShell::GetPresShell(getter_AddRefs(presShell));
      NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
      presShell->GetDocument(getter_AddRefs(doc));
      NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
      nsAutoString charset;
      NS_ENSURE_SUCCESS(doc->GetDocumentCharacterSet(charset), NS_ERROR_FAILURE);
      rv = textToSubURI->UnEscapeAndConvert(NS_ConvertUCS2toUTF8(charset.get()).get(),
             NS_ConvertUCS2toUTF8(aURLSpec).get(), &uStr);    
    }

    if (NS_SUCCEEDED(rv))   {
      rv = browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_LINK, uStr);
      nsMemory::Free(uStr);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsWebShell::GetLinkState(const char* aLinkURI, nsLinkState& aState)
{
  aState = eLinkState_Unvisited;

   if(mGlobalHistory)
      {
        PRBool isVisited;
        NS_ENSURE_SUCCESS(mGlobalHistory->IsVisited(aLinkURI, &isVisited),
                          NS_ERROR_FAILURE);
        if (isVisited)
          aState = eLinkState_Visited;
      }

   return NS_OK;
}

//----------------------------------------------------------------------
nsresult nsWebShell::EndPageLoad(nsIWebProgress *aProgress,
                                 nsIChannel* channel,
                                 nsresult aStatus)
{
  nsresult rv = NS_OK;

  if(!channel)
    return NS_ERROR_NULL_POINTER;
    
  nsCOMPtr<nsIURI> url;
  rv = channel->GetURI(getter_AddRefs(url));
  if (NS_FAILED(rv)) return rv;
  
  // clean up reload state for meta charset
  if(eCharsetReloadRequested == mCharsetReloadState)
    mCharsetReloadState = eCharsetReloadStopOrigional;
  else 
    mCharsetReloadState = eCharsetReloadInit;

  //
  // one of many safeguards that prevent death and destruction if
  // someone is so very very rude as to bring this window down
  // during this load handler.
  //
  nsCOMPtr<nsIWebShell> kungFuDeathGrip(this);
  nsDocShell::EndPageLoad(aProgress, channel, aStatus);

  //
  // If the page load failed, then deal with the error condition...
  // Errors are handled as follows:
  //   1. Check to see if it a file not found error.
  //   2. Send the URI to a keyword server (if enabled)
  //   3. If the error was DNS failure, then add www and .com to the URI
  //      (if appropriate).
  //   4. Throw an error dialog box...
  //

  if(url && NS_FAILED(aStatus)) {
    if (aStatus == NS_ERROR_FILE_NOT_FOUND) {
      nsCOMPtr<nsIPrompt> prompter;
      nsCOMPtr<nsIStringBundle> stringBundle;
      GetPromptAndStringBundle(getter_AddRefs(prompter), 
                                getter_AddRefs(stringBundle));
       if (stringBundle && prompter) {
        nsXPIDLString messageStr;
        nsresult rv = stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("fileNotFound").get(), 
                                                      getter_Copies(messageStr));
          
        if (NS_SUCCEEDED(rv) && messageStr) {
          nsXPIDLCString spec;
          url->GetPath(getter_Copies(spec));

          PRUnichar *msg = nsTextFormatter::smprintf(messageStr, (const char*)spec);
          if (!msg) return NS_ERROR_OUT_OF_MEMORY;
          
          prompter->Alert(nsnull, msg);
          nsTextFormatter::smprintf_free(msg);
        }
      }
      return NS_OK;
    }  
      
    nsXPIDLCString host;

    rv = url->GetHost(getter_Copies(host));
    if (!host) {
      return rv;
    }

    CBufDescriptor buf((const char *)host, PR_TRUE, PL_strlen(host) + 1);
    nsCAutoString hostStr(buf);
    PRInt32 dotLoc = hostStr.FindChar('.');

    nsXPIDLCString scheme;
    url->GetScheme(getter_Copies(scheme));

    PRUint32 schemeLen = PL_strlen((const char*)scheme);
    CBufDescriptor schemeBuf((const char*)scheme, PR_TRUE, schemeLen+1, schemeLen);
    nsCAutoString schemeStr(schemeBuf);

    //
    // First try sending the request to a keyword server (if enabled)...
    //
    if(aStatus == NS_ERROR_UNKNOWN_HOST ||
       aStatus == NS_ERROR_CONNECTION_REFUSED ||
       aStatus == NS_ERROR_NET_TIMEOUT)
    {
      PRBool keywordsEnabled = PR_FALSE;

      if(mPrefs) {
        rv = mPrefs->GetBoolPref("keyword.enabled", &keywordsEnabled);
        if (NS_FAILED(rv)) return rv;
      }

      // we should only perform a keyword search under the following conditions:
      // (0) Pref keyword.enabled is true
      // (1) the url scheme is http (or https)
      // (2) the url does not have a protocol scheme
      // If we don't enforce such a policy, then we end up doing keyword searchs on urls
      // we don't intend like imap, file, mailbox, etc. This could lead to a security
      // problem where we send data to the keyword server that we shouldn't be. 
      // Someone needs to clean up keywords in general so we can determine on a per url basis
      // if we want keywords enabled...this is just a bandaid...
      if (keywordsEnabled && !schemeStr.IsEmpty() &&
         (schemeStr.Find("http") != 0)) {
          keywordsEnabled = PR_FALSE;
      }

      if(keywordsEnabled && (-1 == dotLoc)) {
        // only send non-qualified hosts to the keyword server
        nsAutoString keywordSpec; keywordSpec.AssignWithConversion("keyword:");
        keywordSpec.Append(NS_ConvertUTF8toUCS2(host));

        return LoadURI(keywordSpec.get(), LOAD_FLAGS_NONE);
      } // end keywordsEnabled
    }

    //
    // Next, try rewriting the URI using our www.*.com trick
    //
    if(aStatus == NS_ERROR_UNKNOWN_HOST) {
      // Try our www.*.com trick.
      nsCAutoString retryHost;


      if(schemeStr.Find("http") == 0) {
        if(-1 == dotLoc) {
          retryHost = "www.";
          retryHost += hostStr;
          retryHost += ".com";
        } else {
          PRInt32 hostLen = hostStr.Length();
          if(((hostLen - dotLoc) == 3) || ((hostLen - dotLoc) == 4)) {
            retryHost = "www.";
            retryHost += hostStr;
          }
        }
      }

      if(!retryHost.IsEmpty()) {
        // This seems evil, since it is modifying the original URL
        rv = url->SetHost(retryHost.get());
        if (NS_FAILED(rv)) return rv;
        
        rv = url->GetSpec(getter_Copies(host));
        if (NS_FAILED(rv)) return rv;

        // reload the url
        return LoadURI(NS_ConvertASCIItoUCS2(host).get(), LOAD_FLAGS_NONE);
      } // retry
    }

    //
    // Well, none of the URI rewriting tricks worked :-(
    // It is time to throw an error dialog box, and be done with it...
    //

    // Doc failed to load because the host was not found.
    if(aStatus == NS_ERROR_UNKNOWN_HOST) {
      // throw a DNS failure dialog
      nsCOMPtr<nsIPrompt> prompter;
      nsCOMPtr<nsIStringBundle> stringBundle;
      
      rv = GetPromptAndStringBundle(getter_AddRefs(prompter),
                                    getter_AddRefs(stringBundle));
      if (!stringBundle) {
        return rv;
      }

      nsXPIDLString messageStr;
      rv = stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("dnsNotFound").get(),
                                           getter_Copies(messageStr));
      if (NS_FAILED(rv)) return rv;

      if (host) {
        PRUnichar *msg = nsTextFormatter::smprintf(messageStr, (const char*)host);
        if (!msg) return NS_ERROR_OUT_OF_MEMORY;

        prompter->Alert(nsnull, msg);
        nsTextFormatter::smprintf_free(msg);
      }
    }
    //
    // Doc failed to load because we couldn't connect to the server.
    // throw a connection failure dialog
    //
    else if(aStatus == NS_ERROR_CONNECTION_REFUSED) {
      PRInt32 port = -1;

      rv = url->GetPort(&port);
      if (NS_FAILED(rv)) return rv;

      nsCOMPtr<nsIPrompt> prompter;
      nsCOMPtr<nsIStringBundle> stringBundle;

      rv = GetPromptAndStringBundle(getter_AddRefs(prompter), 
                                    getter_AddRefs(stringBundle));
      if (!stringBundle) {
        return rv;
      }

      nsXPIDLString messageStr;
      rv = stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("connectionFailure").get(),
                                           getter_Copies(messageStr));
      if (NS_FAILED(rv)) return rv;

      // build up the host:port string.
      nsCAutoString combo(host);
      if (port > 0) {
        combo.Append(':');
        combo.AppendInt(port);
      }
      
      PRUnichar *msg = nsTextFormatter::smprintf(messageStr, combo.get());
      if (!msg) return NS_ERROR_OUT_OF_MEMORY;

      prompter->Alert(nsnull, msg);
      nsTextFormatter::smprintf_free(msg);
    }
    //
    // Doc failed to load because the socket function timed out.
    // throw a timeout dialog
    //
    else if(aStatus == NS_ERROR_NET_TIMEOUT) {
      nsCOMPtr<nsIPrompt> prompter;
      nsCOMPtr<nsIStringBundle> stringBundle;

      rv = GetPromptAndStringBundle(getter_AddRefs(prompter),
                                    getter_AddRefs(stringBundle));
      if (!stringBundle) {
        return rv;
      }

      nsXPIDLString messageStr;
      rv = stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("netTimeout").get(),
                                           getter_Copies(messageStr));
      if (NS_FAILED(rv)) return rv;

      PRUnichar *msg = nsTextFormatter::smprintf(messageStr, (const char*)host);
      if (!msg) return NS_ERROR_OUT_OF_MEMORY;

      prompter->Alert(nsnull, msg);
      nsTextFormatter::smprintf_free(msg);
    } // end NS_ERROR_NET_TIMEOUT
    else if (aStatus == NS_ERROR_DOCUMENT_NOT_CACHED) {
      /* A document that was requested to be fetched *only* from
       * the cache is not in cache. May be this is one of those 
       * postdata results. Throw a  dialog to the user,
       * saying that the page has expired from cache and ask if 
       * they wish to refetch the page from the net.
       */
      nsCOMPtr<nsIPrompt> prompter;
      PRBool repost;
      nsCOMPtr<nsIStringBundle> stringBundle;
      GetPromptAndStringBundle(getter_AddRefs(prompter), 
                                getter_AddRefs(stringBundle));
 
      if (stringBundle && prompter) {
        nsXPIDLString messageStr;
        nsresult rv = stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("repost").get(), 
                                                      getter_Copies(messageStr));
          
        if (NS_SUCCEEDED(rv) && messageStr) {
          prompter->Confirm(nsnull, messageStr, &repost);
          /* If the user pressed cancel in the dialog, 
           * return failure. Don't try to load the page with out 
           * the post data. 
           */
          if (!repost)
            return NS_OK;

          // The user wants to repost the data to the server. 
          // If the page was loaded due to a back/forward/go
          // operation, update the session history index.
          // This is similar to the updating done in 
          // nsDocShell::OnNewURI() for regular pages          
          nsCOMPtr<nsISHistory> rootSH=mSessionHistory;
          if (!mSessionHistory) {
            nsCOMPtr<nsIDocShellTreeItem> root;
            //Get the root docshell
            GetSameTypeRootTreeItem(getter_AddRefs(root));
            if (root) {
              // QI root to nsIWebNavigation
              nsCOMPtr<nsIWebNavigation> rootAsWebnav(do_QueryInterface(root));
              if (rootAsWebnav) {
               // Get the handle to SH from the root docshell          
               rootAsWebnav->GetSessionHistory(getter_AddRefs(rootSH));             
              }
            }
          }  // mSessionHistory

          if (rootSH && (mLoadType & LOAD_CMD_HISTORY)) {
            nsCOMPtr<nsISHistoryInternal> shInternal(do_QueryInterface(rootSH));
            if (shInternal)
             shInternal->UpdateIndex();
          }
          /* The user does want to repost the data to the server.
           * Initiate a new load again.
           */
          /* Get the postdata if any from the channel */
          nsCOMPtr<nsIInputStream> inputStream;
          nsCOMPtr<nsIURI> referrer;
          if (channel) {
            nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
 
            if(httpChannel) {
              httpChannel->GetReferrer(getter_AddRefs(referrer));
              httpChannel->GetUploadStream(getter_AddRefs(inputStream));
            }
          }
          nsCOMPtr<nsIRandomAccessStore> postDataRandomAccess(do_QueryInterface(inputStream));
          if (postDataRandomAccess)
          {
             postDataRandomAccess->Seek(PR_SEEK_SET, 0);
          }
          InternalLoad(url,                               // URI
                       referrer,                          // Refering URI
                       nsnull,                            // Owner
                       PR_TRUE,                           // Inherit owner
                       nsnull,                            // No window target
                       inputStream,                       // Post data stream
                       nsnull,                            // No headers stream
                       LOAD_RELOAD_BYPASS_PROXY_AND_CACHE,// Load type
                       nsnull);                           // No SHEntry
            
          }
        }
    }
  } // if we have a host

  return NS_OK;
}

//
// Routines for selection and clipboard
//

#ifdef XP_MAC
#pragma mark -
#endif

nsresult
nsWebShell::GetControllerForCommand ( const nsAReadableString & inCommand, nsIController** outController )
{
  NS_ENSURE_ARG_POINTER(outController);
  *outController = nsnull;
  
  nsresult rv = NS_ERROR_FAILURE;
  
  nsCOMPtr<nsPIDOMWindow> window ( do_QueryInterface(mScriptGlobal) );
  if ( window ) {
    nsCOMPtr<nsIFocusController> focusController;
    rv = window->GetRootFocusController ( getter_AddRefs(focusController) );
    if ( focusController )
      rv = focusController->GetControllerForCommand ( inCommand, outController );
  } // if window

  return rv;
  
} // GetControllerForCommand


nsresult
nsWebShell::IsCommandEnabled ( const nsAReadableString & inCommand, PRBool* outEnabled )
{
  NS_ENSURE_ARG_POINTER(outEnabled);
  *outEnabled = PR_FALSE;

  nsresult rv = NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIController> controller;
  rv = GetControllerForCommand ( inCommand, getter_AddRefs(controller) );
  if ( controller )
    rv = controller->IsCommandEnabled(inCommand, outEnabled);
  
  return rv;
}


nsresult
nsWebShell::DoCommand ( const nsAReadableString & inCommand )
{
  nsresult rv = NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIController> controller;
  rv = GetControllerForCommand ( inCommand, getter_AddRefs(controller) );
  if ( controller )
    rv = controller->DoCommand(inCommand);
  
  return rv;
}


NS_IMETHODIMP
nsWebShell::CanCutSelection(PRBool* aResult)
{
  return IsCommandEnabled ( NS_LITERAL_STRING("cmd_cut"), aResult );
}

NS_IMETHODIMP
nsWebShell::CanCopySelection(PRBool* aResult)
{
  return IsCommandEnabled ( NS_LITERAL_STRING("cmd_copy"), aResult );
}

NS_IMETHODIMP
nsWebShell::CanCopyLinkLocation(PRBool* aResult)
{
  return IsCommandEnabled ( NS_LITERAL_STRING("cmd_copyLink"), aResult );
}

NS_IMETHODIMP
nsWebShell::CanCopyImageLocation(PRBool* aResult)
{
  return IsCommandEnabled ( NS_LITERAL_STRING("cmd_copyImageLocation"),
                            aResult );
}

NS_IMETHODIMP
nsWebShell::CanCopyImageContents(PRBool* aResult)
{
  return IsCommandEnabled ( NS_LITERAL_STRING("cmd_copyImageContents"),
                            aResult );
}

NS_IMETHODIMP
nsWebShell::CanPaste(PRBool* aResult)
{
  return IsCommandEnabled ( NS_LITERAL_STRING("cmd_paste"), aResult );
}

NS_IMETHODIMP
nsWebShell::CutSelection(void)
{
  return DoCommand ( NS_LITERAL_STRING("cmd_cut") );
}

NS_IMETHODIMP
nsWebShell::CopySelection(void)
{
  return DoCommand ( NS_LITERAL_STRING("cmd_copy") );
}

NS_IMETHODIMP
nsWebShell::CopyLinkLocation(void)
{
  return DoCommand ( NS_LITERAL_STRING("cmd_copyLink") );
}

NS_IMETHODIMP
nsWebShell::CopyImageLocation(void)
{
  return DoCommand ( NS_LITERAL_STRING("cmd_copyImageLocation") );
}

NS_IMETHODIMP
nsWebShell::CopyImageContents(void)
{
  return DoCommand ( NS_LITERAL_STRING("cmd_copyImageContents") );
}

NS_IMETHODIMP
nsWebShell::Paste(void)
{
  return DoCommand ( NS_LITERAL_STRING("cmd_paste") );
}

NS_IMETHODIMP
nsWebShell::SelectAll(void)
{
  return DoCommand ( NS_LITERAL_STRING("cmd_selectAll") );
}


//
// SelectNone
//
// Collapses the current selection, insertion point ends up at beginning
// of previous selection.
//
NS_IMETHODIMP
nsWebShell::SelectNone(void)
{
  return DoCommand ( NS_LITERAL_STRING("cmd_selectNone") );
}


//----------------------------------------------------
NS_IMETHODIMP
nsWebShell::FindNext(const PRUnichar * aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound)
{
  return NS_ERROR_FAILURE;
}

#ifdef XP_MAC
#pragma mark -
#endif

//*****************************************************************************
// nsWebShell::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP nsWebShell::Create()
{
  // Remember the current thread (in current and forseeable implementations,
  // it'll just be the unique UI thread)
  //
  // Since this call must be made on the UI thread, we know the Event Queue
  // will be associated with the current thread...
  //
  mThread = PR_GetCurrentThread();

  WEB_TRACE(WEB_TRACE_CALLS,
            ("nsWebShell::Init: this=%p", this));

  // HACK....force the uri loader to give us a load cookie for this webshell...then get it's
  // doc loader and store it...as more of the docshell lands, we'll be able to get rid
  // of this hack...
  nsCOMPtr<nsIURILoader> uriLoader = do_GetService(NS_URI_LOADER_CONTRACTID);
  uriLoader->GetDocumentLoaderForContext(NS_STATIC_CAST( nsISupports*, (nsIWebShell *) this), &mDocLoader);

  // Set the webshell as the default IContentViewerContainer for the loader...
  mDocLoader->SetContainer(NS_STATIC_CAST(nsIContentViewerContainer*, (nsIWebShell*)this));

   return nsDocShell::Create();
}

NS_IMETHODIMP nsWebShell::Destroy()
{
  nsDocShell::Destroy();

  SetContainer(nsnull);

  return NS_OK;
}

NS_IMETHODIMP nsWebShell::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
   PRInt32 cy, PRBool fRepaint)
{
   mBounds.SetRect(x, y, cx, cy);
   return nsDocShell::SetPositionAndSize(x, y, cx, cy, fRepaint);   
}

NS_IMETHODIMP nsWebShell::GetPositionAndSize(PRInt32* x, PRInt32* y, 
   PRInt32* cx, PRInt32* cy)
{
   if(x)
      *x = mBounds.x;
   if(y)
      *y = mBounds.y;
   if(cx)
      *cx = mBounds.width;
   if(cy)
      *cy = mBounds.height;
      
   return NS_OK; 
}

#ifdef DEBUG
unsigned long nsWebShell::gNumberOfWebShells = 0;
#endif
