/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vi:tw=2:ts=2:et:sw=2:
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#ifdef XP_OS2_VACPP
// XXX every other file that pulls in _os2.h has no problem with HTMX there;
// this one does; the problem may lie with the order of the headers below,
// which is why this fix is here instead of in _os2.h
typedef unsigned long HMTX;
#endif

#ifdef DEBUG
#undef NOISY_LINKS
#undef NOISY_WEBSHELL_LEAKS
#else
#undef NOISY_LINKS
#undef NOISY_WEBSHELL_LEAKS
#endif

#define NOISY_WEBSHELL_LEAKS
#ifdef NOISY_WEBSHELL_LEAKS
#undef DETECT_WEBSHELL_LEAKS
#define DETECT_WEBSHELL_LEAKS
#endif

#include "nsDocShell.h"
#include "nsIWebShell.h"
#include "nsWebShell.h"
#include "nsIWebBrowserChrome.h"
#include "nsIInterfaceRequestor.h"
#include "nsIDocumentLoader.h"
#include "nsIDocumentLoaderObserver.h"
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
#include "nsDOMEvent.h"
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

#include "nsIHTTPChannel.h" // add this to the ick include list...we need it to QI for post data interface
#include "nsHTTPEnums.h"


#include "nsILocaleService.h"
#include "nsIStringBundle.h"
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kCStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kPlatformCharsetCID,  NS_PLATFORMCHARSET_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID,  NS_ICHARSETCONVERTERMANAGER_CID);


#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIProtocolHandler.h"

//XXX for nsIPostData; this is wrong; we shouldn't see the nsIDocument type
#include "nsIDocument.h"

#define SH_IN_FRAMES 1

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

static NS_DEFINE_CID(kGlobalHistoryCID, NS_GLOBALHISTORY_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

//----------------------------------------------------------------------

//----------------------------------------------------------------------

// Class IID's
static NS_DEFINE_IID(kEventQueueServiceCID,   NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kChildCID,               NS_CHILD_CID);
static NS_DEFINE_IID(kDocLoaderServiceCID,    NS_DOCUMENTLOADER_SERVICE_CID);
static NS_DEFINE_IID(kWebShellCID,            NS_WEB_SHELL_CID);

// IID's
static NS_DEFINE_IID(kIContentViewerContainerIID,
                     NS_ICONTENTVIEWERCONTAINER_IID);
static NS_DEFINE_IID(kIDocumentLoaderIID,     NS_IDOCUMENTLOADER_IID);
static NS_DEFINE_IID(kIFactoryIID,            NS_IFACTORY_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kRefreshURIIID,          NS_IREFRESHURI_IID);

static NS_DEFINE_IID(kIWidgetIID,             NS_IWIDGET_IID);
static NS_DEFINE_IID(kIPluginManagerIID,      NS_IPLUGINMANAGER_IID);
static NS_DEFINE_IID(kIPluginHostIID,         NS_IPLUGINHOST_IID);
static NS_DEFINE_IID(kCPluginManagerCID,      NS_PLUGINMANAGER_CID);
static NS_DEFINE_IID(kIDocumentViewerIID,     NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_IID(kITimerCallbackIID,      NS_ITIMERCALLBACK_IID);
static NS_DEFINE_IID(kIWebShellContainerIID,  NS_IWEB_SHELL_CONTAINER_IID);
static NS_DEFINE_IID(kIClipboardCommandsIID,  NS_ICLIPBOARDCOMMANDS_IID);
static NS_DEFINE_IID(kIEventQueueServiceIID,  NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kIDOMHTMLDocumentIID,    NS_IDOMHTMLDOCUMENT_IID);
static NS_DEFINE_CID(kCDOMRangeCID,           NS_RANGE_CID);
// XXX not sure
static NS_DEFINE_IID(kILinkHandlerIID,        NS_ILINKHANDLER_IID);

//----------------------------------------------------------------------

// Note: operator new zeros our memory
nsWebShell::nsWebShell() : nsDocShell()
{
#ifdef DETECT_WEBSHELL_LEAKS
  // We're counting the number of |nsWebShells| to help find leaks
  ++gNumberOfWebShells;
#endif
#ifdef NOISY_WEBSHELL_LEAKS
  printf("WEBSHELL+ = %ld\n", gNumberOfWebShells);
#endif

  NS_INIT_REFCNT();
  mThreadEventQueue = nsnull;
  InitFrameData();
  mItemType = typeContent;
  mCharsetReloadState = eCharsetReloadInit;
  mHistoryState = nsnull;
  mFiredUnloadEvent = PR_FALSE;
  mBounds.SetRect(0, 0, 0, 0);
}

nsWebShell::~nsWebShell()
{
   Destroy();

  // Stop any pending document loads and destroy the loader...
  if (nsnull != mDocLoader) {
    mDocLoader->Stop();
    mDocLoader->RemoveObserver((nsIDocumentLoaderObserver*)this);
    mDocLoader->SetContainer(nsnull);
    mDocLoader->Destroy();
    NS_RELEASE(mDocLoader);
  }
  // Cancel any timers that were set for this loader.
  CancelRefreshURITimers();

  ++mRefCnt; // following releases can cause this destructor to be called
             // recursively if the refcount is allowed to remain 0

  NS_IF_RELEASE(mThreadEventQueue);
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

#ifdef DETECT_WEBSHELL_LEAKS
  // We're counting the number of |nsWebShells| to help find leaks
  --gNumberOfWebShells;
#endif
#ifdef NOISY_WEBSHELL_LEAKS
  printf("WEBSHELL- = %ld\n", gNumberOfWebShells);
#endif
}

void nsWebShell::InitFrameData()
{
  SetMarginWidth(-1);    
  SetMarginHeight(-1);
}

nsresult
nsWebShell::FireUnloadForChildren()
{
  nsresult rv = NS_OK;

  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsIDocShell* shell = (nsIDocShell*) mChildren.ElementAt(i);
    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(shell));
    rv = webShell->FireUnloadEvent();
  }

  return rv;
}

NS_IMETHODIMP
nsWebShell::FireUnloadEvent()
{
  nsresult rv = NS_OK;

  if (!mFiredUnloadEvent) {
    mFiredUnloadEvent = PR_TRUE;

    if (mScriptGlobal) {
      nsIDocumentViewer* docViewer;
      if (mContentViewer && NS_SUCCEEDED(mContentViewer->QueryInterface(kIDocumentViewerIID, (void**)&docViewer))) {
        nsIPresContext *presContext;
        if (NS_SUCCEEDED(docViewer->GetPresContext(presContext))) {
          nsEventStatus status = nsEventStatus_eIgnore;
          nsMouseEvent event;
          event.eventStructType = NS_EVENT;
          event.message = NS_PAGE_UNLOAD;
          rv = mScriptGlobal->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

          NS_RELEASE(presContext);
        }
        NS_RELEASE(docViewer);
      }
    }
  }
  //Fire child unloads now while our data is intact.
  rv = FireUnloadForChildren();

  return rv;
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
   NS_INTERFACE_MAP_ENTRY(nsIDocumentLoaderObserver)
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
   else if(aIID.Equals(NS_GET_IID(nsIDOMWindowInternal)))
      {
      NS_ENSURE_SUCCESS(EnsureScriptEnvironment(), NS_ERROR_FAILURE);
      NS_ENSURE_SUCCESS(mScriptGlobal->QueryInterface(NS_GET_IID(nsIDOMWindowInternal),
         aInstancePtr), NS_ERROR_FAILURE);
      return NS_OK;
      }
   else if(aIID.Equals(NS_GET_IID(nsIDOMWindow)))
      {
      NS_ENSURE_SUCCESS(EnsureScriptEnvironment(), NS_ERROR_FAILURE);
      NS_ENSURE_SUCCESS(mScriptGlobal->QueryInterface(NS_GET_IID(nsIDOMWindow),
         aInstancePtr), NS_ERROR_FAILURE);
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
   NS_ENSURE_SUCCESS(FireUnloadEvent(), NS_ERROR_FAILURE);
   NS_ENSURE_SUCCESS(nsDocShell::SetupNewViewer(aViewer), NS_ERROR_FAILURE);

   // Set mFiredUnloadEvent = PR_FALSE so that the unload handler for the
   // *new* document will fire.
   mFiredUnloadEvent = PR_FALSE;

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
#ifdef SH_IN_FRAMES
	return nsDocShell::Embed(aContentViewer, aCommand, aExtraInfo);
#else
   return SetupNewViewer(aContentViewer);
#endif /* SH_IN_FRAMES */
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
   NS_NewURI(getter_AddRefs(mReferrerURI), nsLiteralString(aReferrer), nsnull);
}

NS_IMETHODIMP
nsWebShell::SetURL(const PRUnichar* aURL)
{
  nsCOMPtr<nsIURI> uri;
  NS_ENSURE_SUCCESS(NS_NewURI(getter_AddRefs(uri), nsLiteralString(aURL),
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

   nsCOMPtr<nsISHEntry> entry;

   NS_ENSURE_SUCCESS(mSessionHistory->GetEntryAtIndex(aIndex, PR_TRUE, 
      getter_AddRefs(entry)), NS_ERROR_FAILURE);
   NS_ENSURE_TRUE(entry, NS_ERROR_FAILURE);

   UpdateCurrentSessionHistory();  
#ifdef SH_IN_FRAMES
   NS_ENSURE_SUCCESS(LoadHistoryEntry(entry, nsIDocShellLoadInfo::loadHistory), NS_ERROR_FAILURE);
#else
   NS_ENSURE_SUCCESS(LoadHistoryEntry(entry), NS_ERROR_FAILURE);
#endif

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

   nsCOMPtr<nsISHEntry> entry;

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
        muDV->SetHintCharacterSet(NS_ConvertASCIItoUCS2(aCharset).GetUnicode());
        muDV->SetHintCharacterSetSource((PRInt32)aSource);
        if(eCharsetReloadRequested != mCharsetReloadState) 
        {
          mCharsetReloadState = eCharsetReloadRequested;
          LoadURI(NS_ConvertASCIItoUCS2(aURL).GetUnicode());
        }
      }
    }
  }
  return NS_OK;
}

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
         muDV->SetHintCharacterSet(NS_ConvertASCIItoUCS2(aCharset).GetUnicode());
         muDV->SetHintCharacterSetSource((PRInt32)aSource);
         if(eCharsetReloadRequested != mCharsetReloadState) 
         {
            mCharsetReloadState = eCharsetReloadRequested;
            return Reload(nsIWebNavigation::loadReloadNormal);
         }
      }
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsWebShell::StopDocumentLoad(void)
{
  if(eCharsetReloadRequested != mCharsetReloadState) 
  {
     Stop();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetRendering(PRBool aRender)
{
  if(eCharsetReloadRequested != mCharsetReloadState) 
  {
    if (mContentViewer) {
       mContentViewer->SetEnableRendering(aRender);
    }
  }
  return NS_OK;
}

//----------------------------------------------------------------------

// WebShell link handling

struct OnLinkClickEvent : public PLEvent {
  OnLinkClickEvent(nsWebShell* aHandler, nsIContent* aContent,
                   nsLinkVerb aVerb, const PRUnichar* aURLSpec,
                   const PRUnichar* aTargetSpec, nsIInputStream* aPostDataStream = 0);
  ~OnLinkClickEvent();

  void HandleEvent() {
    mHandler->HandleLinkClickEvent(mContent, mVerb, mURLSpec->GetUnicode(),
                                   mTargetSpec->GetUnicode(), mPostDataStream);
  }

  nsWebShell*  mHandler;
  nsString*    mURLSpec;
  nsString*    mTargetSpec;
  nsIInputStream* mPostDataStream;
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
                                   nsIInputStream* aPostDataStream)
{
  nsIEventQueue* eventQueue;

  mHandler = aHandler;
  NS_ADDREF(aHandler);
  mURLSpec = new nsString(aURLSpec);
  mTargetSpec = new nsString(aTargetSpec);
  mPostDataStream = aPostDataStream;
  NS_IF_ADDREF(mPostDataStream);
  mContent = aContent;
  NS_IF_ADDREF(mContent);
  mVerb = aVerb;

  PL_InitEvent(this, nsnull,
               (PLHandleEventProc) ::HandlePLEvent,
               (PLDestroyEventProc) ::DestroyPLEvent);

  eventQueue = aHandler->GetEventQueue();
  eventQueue->PostEvent(this);
  NS_RELEASE(eventQueue);
}

OnLinkClickEvent::~OnLinkClickEvent()
{
  NS_IF_RELEASE(mContent);
  NS_IF_RELEASE(mHandler);
  NS_IF_RELEASE(mPostDataStream);
  if (nsnull != mURLSpec) delete mURLSpec;
  if (nsnull != mTargetSpec) delete mTargetSpec;

}

//----------------------------------------

NS_IMETHODIMP
nsWebShell::OnLinkClick(nsIContent* aContent,
                        nsLinkVerb aVerb,
                        const PRUnichar* aURLSpec,
                        const PRUnichar* aTargetSpec,
                        nsIInputStream* aPostDataStream)
{
  OnLinkClickEvent* ev;
  nsresult rv = NS_OK;

  ev = new OnLinkClickEvent(this, aContent, aVerb, aURLSpec,
                            aTargetSpec, aPostDataStream);
  if (nsnull == ev) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  return rv;
}

nsIEventQueue* nsWebShell::GetEventQueue(void)
{
  NS_PRECONDITION(nsnull != mThreadEventQueue, "EventQueue for thread is null");
  NS_ADDREF(mThreadEventQueue);
  return mThreadEventQueue;
}

void
nsWebShell::HandleLinkClickEvent(nsIContent *aContent,
                                 nsLinkVerb aVerb,
                                 const PRUnichar* aURLSpec,
                                 const PRUnichar* aTargetSpec,
                                 nsIInputStream* aPostDataStream)
{
  nsCAutoString target; target.AssignWithConversion(aTargetSpec);

  switch(aVerb) {
    case eLinkVerb_New:
      target = "_blank";
      // Fall into replace case
    case eLinkVerb_Undefined:
      // Fall through, this seems like the most reasonable action
    case eLinkVerb_Replace:
      {
        // for now, just hack the verb to be view-link-clicked
        // and down in the load document code we'll detect this and
        // set the correct uri loader command
        nsCOMPtr<nsIURI> uri;
        NS_NewURI(getter_AddRefs(uri), nsLiteralString(aURLSpec), nsnull);

#ifdef SH_IN_FRAMES
		InternalLoad(uri, mCurrentURI, nsnull, PR_TRUE, target, aPostDataStream, nsIDocShellLoadInfo::loadLink, nsnull); 
#else
        InternalLoad(uri, mCurrentURI, nsnull, PR_TRUE, target, aPostDataStream, nsIDocShellLoadInfo::loadLink); 
#endif  /* SH_IN_FRAMES */
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

   if(browserChrome)
      browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_LINK, aURLSpec);

   return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetLinkState(const char* aLinkURI, nsLinkState& aState)
{
  aState = eLinkState_Unvisited;

   if(mGlobalHistory)
      {
      PRInt64 lastVisitDate;
      NS_ENSURE_SUCCESS(mGlobalHistory->GetLastVisitDate(aLinkURI,
         &lastVisitDate), NS_ERROR_FAILURE);

      // a last-visit-date of zero means we've never seen it before; so
      // if it's not zero, we must've seen it.
      if(!LL_IS_ZERO(lastVisitDate))
         aState = eLinkState_Visited;

      // XXX how to tell if eLinkState_OutOfDate?
      }

   return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP
nsWebShell::OnStartDocumentLoad(nsIDocumentLoader* loader,
                                nsIURI* aURL,
                                const char* aCommand)
{
  nsresult rv = NS_ERROR_FAILURE;

   if(loader == mDocLoader)
      {
      nsCOMPtr<nsIDocumentLoaderObserver> dlObserver;

      if(!mDocLoaderObserver && mParent)
         {
         /* If this is a frame (in which case it would have a parent && doesn't
          * have a documentloaderObserver, get it from the rootWebShell
          */
         nsCOMPtr<nsIDocShellTreeItem> rootItem;
         GetSameTypeRootTreeItem(getter_AddRefs(rootItem));
         nsCOMPtr<nsIDocShell> rootDocShell(do_QueryInterface(rootItem));
         if(rootDocShell)
            rootDocShell->GetDocLoaderObserver(getter_AddRefs(dlObserver));
         }
      else
         dlObserver = do_QueryInterface(mDocLoaderObserver);  // we need this to addref
      /*
       * Fire the OnStartDocumentLoad of the webshell observer
       */
      if(mContainer && dlObserver)
         dlObserver->OnStartDocumentLoad(mDocLoader, aURL, aCommand);
      }
   return rv;
}



NS_IMETHODIMP
nsWebShell::OnEndDocumentLoad(nsIDocumentLoader* loader,
                              nsIChannel* channel,
                              nsresult aStatus)
{
   if(loader != mDocLoader)
      return NS_OK;

   nsresult rv = NS_OK;
   if(!channel)
      return NS_ERROR_NULL_POINTER;

   nsCOMPtr<nsIURI> aURL;
   NS_ENSURE_SUCCESS(channel->GetURI(getter_AddRefs(aURL)), NS_ERROR_FAILURE);
  
   // clean up reload state for meta charset
   if(eCharsetReloadRequested == mCharsetReloadState)
      mCharsetReloadState = eCharsetReloadStopOrigional;
   else 
      mCharsetReloadState = eCharsetReloadInit;

   /* one of many safeguards that prevent death and destruction if
      someone is so very very rude as to bring this window down
      during this load handler. */
   nsCOMPtr<nsIWebShell> kungFuDeathGrip(this);

   // Clear the LSHE reference in docshell to indicate document loading
   // is done one way or another.
   LSHE = nsnull;
   if(mScriptGlobal && !mEODForCurrentDocument && NS_SUCCEEDED(aStatus))
      {
      if(mContentViewer)
         {
         nsCOMPtr<nsIPresContext> presContext;
         GetPresContext(getter_AddRefs(presContext));
         if(presContext)
            {
            nsEventStatus status = nsEventStatus_eIgnore;
            nsMouseEvent event;
            event.eventStructType = NS_EVENT;
            event.message = NS_PAGE_LOAD;
            rv = mScriptGlobal->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
            }
         }
      }

   mEODForCurrentDocument = PR_TRUE;
   nsCOMPtr<nsIDocumentLoaderObserver> dlObserver;

   if(!mDocLoaderObserver && mParent)
      {
      /* If this is a frame (in which case it would have a parent && doesn't
       * have a documentloaderObserver, get it from the rootWebShell
       */
      nsCOMPtr<nsIDocShellTreeItem> rootItem;
      GetSameTypeRootTreeItem(getter_AddRefs(rootItem));
      nsCOMPtr<nsIDocShell> rootDocShell(do_QueryInterface(rootItem));

      if(rootDocShell)
         rootDocShell->GetDocLoaderObserver(getter_AddRefs(dlObserver));
      }
   else
      dlObserver = do_QueryInterface(mDocLoaderObserver);  // we need this to addref

   /*
    * Fire the OnEndDocumentLoad of the DocLoaderobserver
    */
   if(dlObserver && aURL)
      dlObserver->OnEndDocumentLoad(mDocLoader, channel, aStatus);

   if(mDocLoader == loader && NS_FAILED(aStatus))
      {
      nsXPIDLCString host;
      nsresult hostResult = aURL->GetHost(getter_Copies(host));
      if (NS_SUCCEEDED(hostResult) && host)
      {      
        CBufDescriptor buf((const char *)host, PR_TRUE, PL_strlen(host) + 1);
        nsCAutoString hostStr(buf);
        PRInt32 dotLoc = hostStr.FindChar('.');

        nsXPIDLCString scheme;
        aURL->GetScheme(getter_Copies(scheme));

        PRUint32 schemeLen = PL_strlen((const char*)scheme);
        CBufDescriptor schemeBuf((const char*)scheme, PR_TRUE, schemeLen+1, schemeLen);
        nsCAutoString schemeStr(schemeBuf);


      if(aStatus == NS_ERROR_UNKNOWN_HOST ||
         aStatus == NS_ERROR_CONNECTION_REFUSED ||
         aStatus == NS_ERROR_NET_TIMEOUT)
         {
         PRBool keywordsEnabled = PR_FALSE;
         NS_ENSURE_SUCCESS(mPrefs->GetBoolPref("keyword.enabled", &keywordsEnabled),
            NS_ERROR_FAILURE);
         
         // we should only perform a keyword search under the following conditions:
         // (1) the url scheme is http (or https)
         // (2) the url does not have a protocol scheme
         // If we don't enforce such a policy, then we end up doing keyword searchs on urls
         // we don't intend like imap, file, mailbox, etc. This could lead to a security
         // problem where we send data to the keyword server that we shouldn't be. 
         // Someone needs to clean up keywords in general so we can determine on a per url basis
         // if we want keywords enabled...this is just a bandaid...
         static const char httpSchemeName[] = "http";

         if (schemeStr.IsEmpty()  || !schemeStr.Find(httpSchemeName) )
           keywordsEnabled = PR_TRUE; // keep keywors enabled for this url
         else
           keywordsEnabled = PR_FALSE; // all other cases, disable keywords for this url.

         if(keywordsEnabled && (-1 == dotLoc))
            {
            // only send non-qualified hosts to the keyword server
            nsAutoString keywordSpec; keywordSpec.AssignWithConversion("keyword:");
            keywordSpec.Append(NS_ConvertUTF8toUCS2(host));
            return LoadURI(keywordSpec.GetUnicode());
            } // end keywordsEnabled
         }

      // Doc failed to load because the host was not found.
      if(aStatus == NS_ERROR_UNKNOWN_HOST)
         {
         // Try our www.*.com trick.
         nsCAutoString retryHost;


         if(schemeStr.Find("http") == 0)
            {
            if(-1 == dotLoc)
               {
               retryHost = "www.";
               retryHost += hostStr;
               retryHost += ".com";
               } 
            else 
               {
               PRInt32 hostLen = hostStr.Length();
               if(((hostLen - dotLoc) == 3) || ((hostLen - dotLoc) == 4))
                  {
                  retryHost = "www.";
                  retryHost += hostStr;
                  }
               }
            }

         if(!retryHost.IsEmpty())
            {
            NS_ENSURE_SUCCESS(aURL->SetHost(retryHost.GetBuffer()), 
               NS_ERROR_FAILURE);
            nsXPIDLCString aSpec;
            NS_ENSURE_SUCCESS(aURL->GetSpec(getter_Copies(aSpec)),
               NS_ERROR_FAILURE);
            // reload the url
            return LoadURI(NS_ConvertASCIItoUCS2(aSpec).GetUnicode());
            } // retry

         // throw a DNS failure dialog
         nsCOMPtr<nsIPrompt> prompter;
         nsCOMPtr<nsIStringBundle> stringBundle;
         GetPromptAndStringBundle(getter_AddRefs(prompter), 
            getter_AddRefs(stringBundle));
         NS_ENSURE_TRUE(stringBundle, NS_ERROR_FAILURE);

         nsXPIDLString messageStr;
         NS_ENSURE_SUCCESS(stringBundle->GetStringFromName(
            NS_ConvertASCIItoUCS2("dnsNotFound").GetUnicode(), 
            getter_Copies(messageStr)), NS_ERROR_FAILURE);

         if (host) {
             PRUnichar *msg = nsTextFormatter::smprintf(messageStr, (const char*)host);
             if (!msg) return NS_ERROR_OUT_OF_MEMORY;

             prompter->Alert(nsnull, msg);
             nsTextFormatter::smprintf_free(msg);
             }
         } 
      else if(aStatus == NS_ERROR_CONNECTION_REFUSED)
         {// Doc failed to load because we couldn't connect to the server.
         // throw a connection failure dialog
         PRInt32 port = -1;
         NS_ENSURE_SUCCESS(aURL->GetPort(&port), NS_ERROR_FAILURE);
         
           nsCOMPtr<nsIPrompt> prompter;
           nsCOMPtr<nsIStringBundle> stringBundle;
           GetPromptAndStringBundle(getter_AddRefs(prompter), 
              getter_AddRefs(stringBundle));
           NS_ENSURE_TRUE(stringBundle, NS_ERROR_FAILURE);

           nsXPIDLString messageStr;
           NS_ENSURE_SUCCESS(stringBundle->GetStringFromName(
              NS_ConvertASCIItoUCS2("connectionFailure").GetUnicode(), 
              getter_Copies(messageStr)), NS_ERROR_FAILURE);

           // build up the host:port string.
           nsCAutoString combo(host);
           if (port > 0) {
               combo.Append(':');
               combo.AppendInt(port);
           }
           PRUnichar *msg = nsTextFormatter::smprintf(messageStr, combo.GetBuffer());
           if (!msg) return NS_ERROR_OUT_OF_MEMORY;

           prompter->Alert(nsnull, msg);
           nsTextFormatter::smprintf_free(msg);
           } 
        else if(aStatus == NS_ERROR_NET_TIMEOUT)
           {// Doc failed to load because the socket function timed out.
           // throw a timeout dialog
           nsCOMPtr<nsIPrompt> prompter;
           nsCOMPtr<nsIStringBundle> stringBundle;
           GetPromptAndStringBundle(getter_AddRefs(prompter), 
              getter_AddRefs(stringBundle));
           NS_ENSURE_TRUE(stringBundle, NS_ERROR_FAILURE);


           nsXPIDLString messageStr;
           NS_ENSURE_SUCCESS(stringBundle->GetStringFromName(
              NS_ConvertASCIItoUCS2("netTimeout").GetUnicode(), 
              getter_Copies(messageStr)), NS_ERROR_FAILURE);

           PRUnichar *msg = nsTextFormatter::smprintf(messageStr, (const char*)host);
           if (!msg) return NS_ERROR_OUT_OF_MEMORY;

           prompter->Alert(nsnull, msg);            
           nsTextFormatter::smprintf_free(msg);
           } // end NS_ERROR_NET_TIMEOUT
        } // if we have a host
      } // end mDocLoader == loader

      return rv;
}

NS_IMETHODIMP
nsWebShell::OnStartURLLoad(nsIDocumentLoader* loader,
                           nsIChannel* channel)
{
  nsresult rv;

  nsCOMPtr<nsIURI> aURL;
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;


  // Stop loading of the earlier document completely when the document url
  // load starts.  Now we know that this url is valid and available.
  nsLoadFlags loadFlags = 0;
  channel -> GetLoadAttributes (&loadFlags);

  PRBool equals = PR_FALSE;
  if (NS_SUCCEEDED(aURL->Equals(mCurrentURI, &equals)) && equals && !(loadFlags & nsIChannel::LOAD_REPLACE))
    Stop();

  /*
   *Fire the OnStartDocumentLoad of the webshell observer
   */
  if ((nsnull != mContainer) && (nsnull != mDocLoaderObserver))
  {
    mDocLoaderObserver->OnStartURLLoad(mDocLoader, channel);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::OnProgressURLLoad(nsIDocumentLoader* loader,
                              nsIChannel* channel,
                              PRUint32 aProgress,
                              PRUint32 aProgressMax)
{
  /*
   *Fire the OnStartDocumentLoad of the webshell observer and container...
   */
  if ((nsnull != mContainer) && (nsnull != mDocLoaderObserver))
  {
     mDocLoaderObserver->OnProgressURLLoad(mDocLoader, channel, aProgress, aProgressMax);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::OnStatusURLLoad(nsIDocumentLoader* loader,
                            nsIChannel* channel,
                            nsString& aMsg)
{
  /*
   *Fire the OnStartDocumentLoad of the webshell observer and container...
   */
  if ((nsnull != mContainer) && (nsnull != mDocLoaderObserver))
  {
     mDocLoaderObserver->OnStatusURLLoad(mDocLoader, channel, aMsg);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::OnEndURLLoad(nsIDocumentLoader* loader,
                         nsIChannel* channel,
                         nsresult aStatus)
{
#if 0
  const char* spec;
  aURL->GetSpec(&spec);
  printf("nsWebShell::OnEndURLLoad:%p: loader=%p url=%s status=%d\n", this, loader, spec, aStatus);
#endif
  /*
   *Fire the OnEndDocumentLoad of the webshell observer
   */
  if ((nsnull != mContainer) && (nsnull != mDocLoaderObserver))
  {
      mDocLoaderObserver->OnEndURLLoad(mDocLoader, channel, aStatus);
  }
  return NS_OK;
}

//----------------------------------------------------
NS_IMETHODIMP
nsWebShell::CanCutSelection(PRBool* aResult)
{
  nsresult rv = NS_OK;

  if (nsnull == aResult) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    *aResult = PR_FALSE;
  }

  return rv;
}

NS_IMETHODIMP
nsWebShell::CanCopySelection(PRBool* aResult)
{
  nsresult rv = NS_OK;

  if (nsnull == aResult) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    *aResult = PR_FALSE;
  }

  return rv;
}

NS_IMETHODIMP
nsWebShell::CanPasteSelection(PRBool* aResult)
{
  nsresult rv = NS_OK;

  if (nsnull == aResult) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    *aResult = PR_FALSE;
  }

  return rv;
}

NS_IMETHODIMP
nsWebShell::CutSelection(void)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWebShell::CopySelection(void)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWebShell::PasteSelection(void)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWebShell::SelectAll(void)
{
  nsresult rv;

  nsCOMPtr<nsIDocumentViewer> docViewer;
  rv = mContentViewer->QueryInterface(kIDocumentViewerIID,
                                      getter_AddRefs(docViewer));
  if (NS_FAILED(rv) || !docViewer) return rv;

  nsCOMPtr<nsIPresShell> presShell;
  rv = docViewer->GetPresShell(*getter_AddRefs(presShell));
  if (NS_FAILED(rv) || !presShell) return rv;
  
  nsCOMPtr<nsISelectionController> selCon = do_QueryInterface(presShell);
  if (NS_FAILED(rv) || !selCon) return rv;

  nsCOMPtr<nsIDOMSelection> selection;
  rv = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_FAILED(rv) || !selection) return rv;

  // Get the document object
  nsCOMPtr<nsIDocument> doc;
  rv = docViewer->GetDocument(*getter_AddRefs(doc));
  if (NS_FAILED(rv) || !doc) return rv;


  nsCOMPtr<nsIDOMHTMLDocument> htmldoc;
  rv = doc->QueryInterface(kIDOMHTMLDocumentIID, (void**)&htmldoc);
  if (NS_FAILED(rv) || !htmldoc) return rv;

  nsCOMPtr<nsIDOMHTMLElement>bodyElement;
  rv = htmldoc->GetBody(getter_AddRefs(bodyElement));
  if (NS_FAILED(rv) || !bodyElement) return rv;

  nsCOMPtr<nsIDOMNode>bodyNode = do_QueryInterface(bodyElement);
  if (!bodyNode) return NS_ERROR_NO_INTERFACE;

#if 0
  rv = selection->Collapse(bodyNode, 0);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMRange>range;
  rv = selection->GetRangeAt(0, getter_AddRefs(range));
  if (NS_FAILED(rv) || !range) return rv;
#endif

  rv = selection->ClearSelection();
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMRange> range;
  rv = nsComponentManager::CreateInstance(kCDOMRangeCID, nsnull,
                                          NS_GET_IID(nsIDOMRange),
                                          getter_AddRefs(range));

  rv = range->SelectNodeContents(bodyNode);
  if (NS_FAILED(rv)) return rv;

  rv = selection->AddRange(range);
  return rv;
}

NS_IMETHODIMP
nsWebShell::SelectNone(void)
{
  return NS_ERROR_FAILURE;
}


//----------------------------------------------------
NS_IMETHODIMP
nsWebShell::FindNext(const PRUnichar * aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound)
{
  return NS_ERROR_FAILURE;
}

//*****************************************************************************
// nsWebShell::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP nsWebShell::Create()
{
     // Cache the PL_EventQueue of the current UI thread...
  //
  // Since this call must be made on the UI thread, we know the Event Queue
  // will be associated with the current thread...
  //
  nsCOMPtr<nsIEventQueueService> eventService(do_GetService(kEventQueueServiceCID));
  NS_ENSURE_TRUE(eventService, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(eventService->GetThreadEventQueue(NS_CURRENT_THREAD,
   &mThreadEventQueue), NS_ERROR_FAILURE);

  WEB_TRACE(WEB_TRACE_CALLS,
            ("nsWebShell::Init: this=%p", this));

  // HACK....force the uri loader to give us a load cookie for this webshell...then get it's
  // doc loader and store it...as more of the docshell lands, we'll be able to get rid
  // of this hack...
  nsCOMPtr<nsIURILoader> uriLoader = do_GetService(NS_URI_LOADER_PROGID);
  uriLoader->GetDocumentLoaderForContext(NS_STATIC_CAST( nsISupports*, (nsIWebShell *) this), &mDocLoader);

  // Set the webshell as the default IContentViewerContainer for the loader...
  mDocLoader->SetContainer(NS_STATIC_CAST(nsIContentViewerContainer*, (nsIWebShell*)this));

  //Register ourselves as an observer for the new doc loader
  mDocLoader->AddObserver((nsIDocumentLoaderObserver*)this);

   return nsDocShell::Create();
}

NS_IMETHODIMP nsWebShell::Destroy()
{
  nsresult rv = NS_OK;

  //Fire unload event before we blow anything away.
  rv = FireUnloadEvent();

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

#ifdef DETECT_WEBSHELL_LEAKS
unsigned long nsWebShell::gNumberOfWebShells = 0;
#endif
