/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 *   Dan Rosen <dr@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsDocShell.h"
#include "nsIWebShell.h"
#include "nsWebShell.h"
#include "nsIWebBrowserChrome.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebProgress.h"
#include "nsIDocumentLoader.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIClipboardCommands.h"
#include "nsILinkHandler.h"
#include "nsIStreamListener.h"
#include "nsIPrompt.h"
#include "nsNetUtil.h"
#include "nsIDNSService.h"
#include "nsISocketProvider.h"
#include "nsIRefreshURI.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIDOMEvent.h"
#include "nsPresContext.h"
#include "nsIComponentManager.h"
#include "nsIEventQueueService.h"
#include "nsCRT.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "plevent.h"
#include "prprf.h"
#include "nsIPluginHost.h"
#include "nsplugin.h"
#include "nsIPluginManager.h"
#include "nsCDefaultURIFixup.h"
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
#include "nsIDOMRange.h"
#include "nsIURIContentListener.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLAnchorElement.h"
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
#include "nsTextFormatter.h"
#include "nsPIDOMWindow.h"
#include "nsPICommandUpdater.h"
#include "nsIController.h"
#include "nsIFocusController.h"
#include "nsGUIEvent.h"
#include "nsISHistoryInternal.h"

#include "nsIHttpChannel.h"
#include "nsIUploadChannel.h"
#include "nsISeekableStream.h"

#include "nsILocaleService.h"
#include "nsIStringBundle.h"

#include "nsICachingChannel.h"

//XXX for nsIPostData; this is wrong; we shouldn't see the nsIDocument type
#include "nsIDocument.h"
#include "nsITextToSubURI.h"

#include "nsIExternalProtocolService.h"
#include "nsCExternalHandlerService.h"

// Used in the fixup code
#include "prnetdb.h"

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

//----------------------------------------------------------------------

static PRBool
IsOffline()
{
    PRBool offline = PR_TRUE;
    nsCOMPtr<nsIIOService> ios(do_GetIOService());
    if (ios)
        ios->GetOffline(&offline);
    return offline;
}

//----------------------------------------------------------------------

// Note: operator new zeros our memory
nsWebShell::nsWebShell() : nsDocShell()
{
#ifdef DEBUG
  // We're counting the number of |nsWebShells| to help find leaks
  ++gNumberOfWebShells;
#endif
#ifdef DEBUG
    printf("++WEBSHELL == %ld\n", gNumberOfWebShells);
#endif

  mThread = nsnull;
  InitFrameData();
  mItemType = typeContent;
  mCharsetReloadState = eCharsetReloadInit;
}

nsWebShell::~nsWebShell()
{
   Destroy();

  // Stop any pending document loads and destroy the loader...
  if (mDocLoader) {
    mDocLoader->Stop();
    mDocLoader->SetContainer(nsnull);
    mDocLoader->Destroy();
    mDocLoader = nsnull;
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
  printf("--WEBSHELL == %ld\n", gNumberOfWebShells);
#endif
}

void nsWebShell::InitFrameData()
{
  SetMarginWidth(-1);    
  SetMarginHeight(-1);
}

nsresult
nsWebShell::EnsureCommandHandler()
{
  if (!mCommandManager)
  {
    mCommandManager = do_CreateInstance("@mozilla.org/embedcomp/command-manager;1");
    if (!mCommandManager) return NS_ERROR_OUT_OF_MEMORY;
    
    nsCOMPtr<nsPICommandUpdater>       commandUpdater = do_QueryInterface(mCommandManager);
    if (!commandUpdater) return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsIDOMWindow> domWindow = do_GetInterface(NS_STATIC_CAST(nsIInterfaceRequestor *, this));
#ifdef DEBUG
    nsresult rv =
#endif
    commandUpdater->Init(domWindow);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Initting command manager failed");
  }
  
  return mCommandManager ? NS_OK : NS_ERROR_FAILURE;
}



NS_IMPL_ADDREF_INHERITED(nsWebShell, nsDocShell)
NS_IMPL_RELEASE_INHERITED(nsWebShell, nsDocShell)

NS_INTERFACE_MAP_BEGIN(nsWebShell)
   NS_INTERFACE_MAP_ENTRY(nsIWebShell)
   NS_INTERFACE_MAP_ENTRY(nsIWebShellServices)
   NS_INTERFACE_MAP_ENTRY(nsIWebShellContainer)
   NS_INTERFACE_MAP_ENTRY(nsILinkHandler)
   NS_INTERFACE_MAP_ENTRY(nsIClipboardCommands)
NS_INTERFACE_MAP_END_INHERITING(nsDocShell)

NS_IMETHODIMP
nsWebShell::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
   NS_ENSURE_ARG_POINTER(aInstancePtr);
   nsresult rv = NS_OK;
   *aInstancePtr = nsnull;

   if(aIID.Equals(NS_GET_IID(nsILinkHandler)))
      {
      // Note: If we ever allow for registering other link handlers,
      // we need to make sure that link handler implementations take
      // the necessary precautions to prevent the security compromise
      // that is blocked by nsWebSell::OnLinkClickSync().

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
   else if(aIID.Equals(NS_GET_IID(nsICommandManager)))
      {
      NS_ENSURE_SUCCESS(EnsureCommandHandler(), NS_ERROR_FAILURE);
      NS_ENSURE_SUCCESS(mCommandManager->QueryInterface(NS_GET_IID(nsICommandManager),
         aInstancePtr), NS_ERROR_FAILURE);
      return NS_OK;
      }

   if (!*aInstancePtr || NS_FAILED(rv))
     return nsDocShell::GetInterface(aIID,aInstancePtr);
   else
     return rv;
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

nsEventStatus PR_CALLBACK
nsWebShell::HandleEvent(nsGUIEvent *aEvent)
{
  return nsEventStatus_eIgnore;
}


/**
 * Document Load methods
 */
NS_IMETHODIMP
nsWebShell::GetDocumentLoader(nsIDocumentLoader*& aResult)
{
  aResult = mDocLoader;
  if (!mDocLoader)
    return NS_ERROR_FAILURE;
  NS_ADDREF(aResult);
  return NS_OK;
}

//----------------------------------------------------------------------
// Web Shell Services API

//This functions is only called when a new charset is detected in loading a document. 
//Its name should be changed to "CharsetReloadDocument"
NS_IMETHODIMP
nsWebShell::ReloadDocument(const char* aCharset,
                           PRInt32 aSource)
{

  // XXX hack. kee the aCharset and aSource wait to pick it up
  nsCOMPtr<nsIContentViewer> cv;
  NS_ENSURE_SUCCESS(GetContentViewer(getter_AddRefs(cv)), NS_ERROR_FAILURE);
  if (cv)
  {
    nsCOMPtr<nsIMarkupDocumentViewer> muDV = do_QueryInterface(cv);  
    if (muDV)
    {
      PRInt32 hint;
      muDV->GetHintCharacterSetSource(&hint);
      if( aSource > hint ) 
      {
         muDV->SetHintCharacterSet(nsDependentCString(aCharset));
         muDV->SetHintCharacterSetSource(aSource);
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
                   nsLinkVerb aVerb, nsIURI* aURI,
                   const PRUnichar* aTargetSpec, nsIInputStream* aPostDataStream = 0, 
                   nsIInputStream* aHeadersDataStream = 0);
  ~OnLinkClickEvent();

  void HandleEvent() {
    mHandler->OnLinkClickSync(mContent, mVerb, mURI,
                              mTargetSpec.get(), mPostDataStream,
                              mHeadersDataStream,
                              nsnull, nsnull);
  }

  nsWebShell*              mHandler;
  nsCOMPtr<nsIURI>         mURI;
  nsString                 mTargetSpec;
  nsCOMPtr<nsIInputStream> mPostDataStream;
  nsCOMPtr<nsIInputStream> mHeadersDataStream;
  nsCOMPtr<nsIContent>     mContent;
  nsLinkVerb               mVerb;
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
                                   nsIURI* aURI,
                                   const PRUnichar* aTargetSpec,
                                   nsIInputStream* aPostDataStream,
                                   nsIInputStream* aHeadersDataStream)
{
  mHandler = aHandler;
  NS_ADDREF(aHandler);
  mURI = aURI;
  mTargetSpec.Assign(aTargetSpec);
  mPostDataStream = aPostDataStream;
  mHeadersDataStream = aHeadersDataStream;
  mContent = aContent;
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
  NS_IF_RELEASE(mHandler);
}

//----------------------------------------

NS_IMETHODIMP
nsWebShell::OnLinkClick(nsIContent* aContent,
                        nsLinkVerb aVerb,
                        nsIURI* aURI,
                        const PRUnichar* aTargetSpec,
                        nsIInputStream* aPostDataStream,
                        nsIInputStream* aHeadersDataStream)
{
  OnLinkClickEvent* ev;

  ev = new OnLinkClickEvent(this, aContent, aVerb, aURI,
                            aTargetSpec, aPostDataStream, aHeadersDataStream);
  if (!ev) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult
nsWebShell::GetEventQueue(nsIEventQueue **aQueue)
{
  NS_ENSURE_ARG_POINTER(aQueue);
  *aQueue = 0;

  nsCOMPtr<nsIEventQueueService> eventService(do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID));
  if (eventService)
    eventService->GetThreadEventQueue(mThread, aQueue);
  return *aQueue ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWebShell::OnLinkClickSync(nsIContent *aContent,
                            nsLinkVerb aVerb,
                            nsIURI* aURI,
                            const PRUnichar* aTargetSpec,
                            nsIInputStream* aPostDataStream,
                            nsIInputStream* aHeadersDataStream,
                            nsIDocShell** aDocShell,
                            nsIRequest** aRequest)
{
  PRBool earlyReturn = PR_FALSE;
  {
    // defer to an external protocol handler if necessary...
    nsCOMPtr<nsIExternalProtocolService> extProtService = do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID);
    if (extProtService) {
      nsCAutoString scheme;
      aURI->GetScheme(scheme);
      if (!scheme.IsEmpty()) {
        // if the URL scheme does not correspond to an exposed protocol, then we
        // need to hand this link click over to the external protocol handler.
        PRBool isExposed;
        nsresult rv = extProtService->IsExposedProtocol(scheme.get(), &isExposed);
        if (NS_SUCCEEDED(rv) && !isExposed) {
          rv = extProtService->LoadUrl(aURI);
          if (NS_SUCCEEDED(rv))
            earlyReturn = PR_TRUE;
          else
            NS_WARNING("failed to launch external protocol handler");
        }
      }
    }
  }
  if (earlyReturn)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(aContent));
  NS_ENSURE_TRUE(node, NS_ERROR_UNEXPECTED);

  PRBool isJS = PR_FALSE;
  PRBool isData = PR_FALSE;

  aURI->SchemeIs("javascript", &isJS);
  aURI->SchemeIs("data", &isData);

  if (isJS || isData) {
    nsCOMPtr<nsIDocument> sourceDoc = aContent->GetDocument();

    if (!sourceDoc) {
      // The source is in a 'zombie' document, or not part of a
      // document any more. Don't let it execute any javascript in the
      // new document.

      return NS_OK;
    }

    nsCOMPtr<nsIPresShell> presShell;
    GetPresShell(getter_AddRefs(presShell));
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    if (presShell->GetDocument() != sourceDoc) {
      // The source is not in the current document, don't let it
      // execute any javascript in the current document.

      return NS_OK;
    }
  }

  // Get the owner document of the link that was clicked, this will be
  // the document that the link is in, or the last document that the
  // link was in. From that document, we'll get the URI to use as the
  // referer, since the current URI in this webshell/docshell may be a
  // new document that we're in the process of loading.
  nsCOMPtr<nsIDOMDocument> refererOwnerDoc;
  node->GetOwnerDocument(getter_AddRefs(refererOwnerDoc));

  nsCOMPtr<nsIDocument> refererDoc(do_QueryInterface(refererOwnerDoc));
  NS_ENSURE_TRUE(refererDoc, NS_ERROR_UNEXPECTED);

  nsIURI *referer = refererDoc->GetDocumentURI();

  // referer could be null here in some odd cases, but that's ok,
  // we'll just load the link w/o sending a referer in those cases.

  nsAutoString target(aTargetSpec);

  // If this is an anchor element, grab its type property to use as a hint
  nsAutoString typeHint;
  nsCOMPtr<nsIDOMHTMLAnchorElement> anchor(do_QueryInterface(aContent));
  if (anchor) {
    anchor->GetType(typeHint);
  }
  
  // Initialize the DocShell / Request
  if (aDocShell) {
    *aDocShell = nsnull;
  }
  if (aRequest) {
    *aRequest = nsnull;
  }

  switch(aVerb) {
    case eLinkVerb_New:
      target.AssignLiteral("_blank");
      // Fall into replace case
    case eLinkVerb_Undefined:
      // Fall through, this seems like the most reasonable action
    case eLinkVerb_Replace:
      {
        return InternalLoad(aURI,               // New URI
                            referer,            // Referer URI
                            nsnull,             // No onwer
                            PR_TRUE,            // Inherit owner from document
                            target.get(),       // Window target
                            NS_LossyConvertUCS2toASCII(typeHint).get(),
                            aPostDataStream,    // Post data stream
                            aHeadersDataStream, // Headers stream
                            LOAD_LINK,          // Load type
                            nsnull,             // No SHEntry
                            PR_TRUE,            // first party site
                            aDocShell,          // DocShell out-param
                            aRequest);          // Request out-param
      }
      break;
    case eLinkVerb_Embed:
      // XXX TODO Should be similar to the HTML IMG ALT attribute handling
      //          in NS 4.x
    default:
      NS_ABORT_IF_FALSE(0,"unexpected link verb");
      return NS_ERROR_UNEXPECTED;
  }
}

NS_IMETHODIMP
nsWebShell::OnOverLink(nsIContent* aContent,
                       nsIURI* aURI,
                       const PRUnichar* aTargetSpec)
{
  nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(mTreeOwner));
  nsresult rv = NS_ERROR_FAILURE;

  if (browserChrome)  {
    nsCOMPtr<nsITextToSubURI> textToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    // use url origin charset to unescape the URL
    nsCAutoString charset;
    rv = aURI->GetOriginCharset(charset);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString spec;
    rv = aURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString uStr;
    rv = textToSubURI->UnEscapeURIForUI(charset, spec, uStr);    

    if (NS_SUCCEEDED(rv))
      rv = browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_LINK, uStr.get());
  }
  return rv;
}

NS_IMETHODIMP
nsWebShell::OnLeaveLink()
{
  nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(mTreeOwner));
  nsresult rv = NS_ERROR_FAILURE;

  if (browserChrome)  {
      rv = browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_LINK,
                                    EmptyString().get());
  }
  return rv;
}

NS_IMETHODIMP
nsWebShell::GetLinkState(nsIURI* aLinkURI, nsLinkState& aState)
{
  if (!aLinkURI) {
    // No uri means not a link
    aState = eLinkState_NotLink;
    return NS_OK;
  }
    
  aState = eLinkState_Unvisited;

  // no history, leave state unchanged
  if (!mGlobalHistory)
    return NS_OK;

  PRBool isVisited;
  NS_ENSURE_SUCCESS(mGlobalHistory->IsVisited(aLinkURI, &isVisited),
                    NS_ERROR_FAILURE);
  if (isVisited)
    aState = eLinkState_Visited;
  
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

  // Test if this is the top frame or a subframe
  PRBool isTopFrame = PR_TRUE;
  nsCOMPtr<nsIDocShellTreeItem> targetParentTreeItem;
  rv = GetSameTypeParent(getter_AddRefs(targetParentTreeItem));
  if (NS_SUCCEEDED(rv) && targetParentTreeItem) 
  {
    isTopFrame = PR_FALSE;
  }

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
      DisplayLoadError(aStatus, url, nsnull);
      return NS_OK;
    }  

    if (sURIFixup)
    {
      //
      // Try and make an alternative URI from the old one
      //
      nsCOMPtr<nsIURI> newURI;

      nsCAutoString oldSpec;
      url->GetSpec(oldSpec);
      
      //
      // First try keyword fixup
      //
      if (aStatus == NS_ERROR_UNKNOWN_HOST)  
      {
        PRBool keywordsEnabled = PR_FALSE;

        if (mPrefs && NS_FAILED(mPrefs->GetBoolPref("keyword.enabled", &keywordsEnabled)))
            keywordsEnabled = PR_FALSE;

        nsCAutoString host;
        url->GetHost(host);

        nsCAutoString scheme;
        url->GetScheme(scheme);

        PRInt32 dotLoc = host.FindChar('.');

        // we should only perform a keyword search under the following conditions:
        // (0) Pref keyword.enabled is true
        // (1) the url scheme is http (or https)
        // (2) the url does not have a protocol scheme
        // If we don't enforce such a policy, then we end up doing keyword searchs on urls
        // we don't intend like imap, file, mailbox, etc. This could lead to a security
        // problem where we send data to the keyword server that we shouldn't be. 
        // Someone needs to clean up keywords in general so we can determine on a per url basis
        // if we want keywords enabled...this is just a bandaid...
        if (keywordsEnabled && !scheme.IsEmpty() &&
           (scheme.Find("http") != 0)) {
            keywordsEnabled = PR_FALSE;
        }

        // Don't perform fixup on an IP address
        PRNetAddr addr;
        if(PR_StringToNetAddr(host.get(), &addr) == PR_SUCCESS) {
            keywordsEnabled = PR_FALSE;
        }

        if(keywordsEnabled && (-1 == dotLoc)) {
          // only send non-qualified hosts to the keyword server
          nsCAutoString keywordSpec("keyword:");
          keywordSpec += host;

          NS_NewURI(getter_AddRefs(newURI),
                    keywordSpec, nsnull);
        } // end keywordsEnabled
      }

      //
      // Now try change the address, e.g. turn http://foo into http://www.foo.com
      //
      if (aStatus == NS_ERROR_UNKNOWN_HOST ||
          aStatus == NS_ERROR_NET_RESET)
      {
        PRBool doCreateAlternate = PR_TRUE;
        
        // Skip fixup for anything except a normal document load operation on
        // the topframe.
        
        if (mLoadType != LOAD_NORMAL || !isTopFrame)
        {
          doCreateAlternate = PR_FALSE;
        }
        else
        {
          // Test if keyword lookup produced a new URI or not
          if (newURI)
          {
            PRBool sameURI = PR_FALSE;
            url->Equals(newURI, &sameURI);
            if (!sameURI)
            {
              // Keyword lookup made a new URI so no need to try an
              // alternate one.
              doCreateAlternate = PR_FALSE;
            }
          }
        }
        if (doCreateAlternate)
        {
          newURI = nsnull;
          sURIFixup->CreateFixupURI(oldSpec,
              nsIURIFixup::FIXUP_FLAGS_MAKE_ALTERNATE_URI, getter_AddRefs(newURI));
        }
      }

      //
      // Did we make a new URI that is different to the old one? If so load it.
      //
      if (newURI)
      {
        // Make sure the new URI is different from the old one, otherwise
        // there's little point trying to load it again.
        PRBool sameURI = PR_FALSE;
        url->Equals(newURI, &sameURI);
        if (!sameURI)
        {
          nsCAutoString newSpec;
          newURI->GetSpec(newSpec);
          NS_ConvertUTF8toUCS2 newSpecW(newSpec);

          // This seems evil, since it is modifying the original URL
          rv = url->SetSpec(newSpec);
          if (NS_FAILED(rv)) return rv;

          return LoadURI(newSpecW.get(),      // URI string
                         LOAD_FLAGS_NONE, // Load flags
                         nsnull,          // Referring URI
                         nsnull,          // Post data stream
                         nsnull);         // Headers stream
        }
      }
    }

    //
    // Well, fixup didn't work :-(
    // It is time to throw an error dialog box, and be done with it...
    //

    // Errors to be shown only on top-level frames
    if ((aStatus == NS_ERROR_UNKNOWN_HOST || 
         aStatus == NS_ERROR_CONNECTION_REFUSED ||
         aStatus == NS_ERROR_UNKNOWN_PROXY_HOST || 
         aStatus == NS_ERROR_PROXY_CONNECTION_REFUSED) &&
            (isTopFrame || mUseErrorPages)) {
      DisplayLoadError(aStatus, url, nsnull);
    }
    // Errors to be shown for any frame
    else if (aStatus == NS_ERROR_NET_TIMEOUT ||
             aStatus == NS_ERROR_REDIRECT_LOOP ||
             aStatus == NS_ERROR_UNKNOWN_SOCKET_TYPE ||
             aStatus == NS_ERROR_NET_INTERRUPT ||
             aStatus == NS_ERROR_NET_RESET) {
      DisplayLoadError(aStatus, url, nsnull);
    }
    else if (aStatus == NS_ERROR_DOCUMENT_NOT_CACHED) {
      /* A document that was requested to be fetched *only* from
       * the cache is not in cache. May be this is one of those 
       * postdata results. Throw a  dialog to the user,
       * saying that the page has expired from cache and ask if 
       * they wish to refetch the page from the net. Do this only
       * if the request is a form post.
       */
      nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
      nsCAutoString method;
      if (httpChannel)
        httpChannel->GetRequestMethod(method);
      if (method.Equals("POST") && !IsOffline()) {
        nsCOMPtr<nsIPrompt> prompter;
        PRBool repost;
        nsCOMPtr<nsIStringBundle> stringBundle;
        GetPromptAndStringBundle(getter_AddRefs(prompter), 
                                  getter_AddRefs(stringBundle));
   
        if (stringBundle && prompter) {
          nsXPIDLString messageStr;
          nsresult rv = stringBundle->GetStringFromName(NS_LITERAL_STRING("repost").get(), 
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
                nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(channel));
                if (uploadChannel) {
                  uploadChannel->GetUploadStream(getter_AddRefs(inputStream));
                }
              }
            }
            nsCOMPtr<nsISeekableStream> postDataSeekable(do_QueryInterface(inputStream));
            if (postDataSeekable)
            {
               postDataSeekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
            }
            InternalLoad(url,                               // URI
                         referrer,                          // Referring URI
                         nsnull,                            // Owner
                         PR_TRUE,                           // Inherit owner
                         nsnull,                            // No window target
                         nsnull,                            // No type hint
                         inputStream,                       // Post data stream
                         nsnull,                            // No headers stream
                         LOAD_RELOAD_BYPASS_PROXY_AND_CACHE,// Load type
                         nsnull,                            // No SHEntry
                         PR_TRUE,                           // first party site
                         nsnull,                            // No nsIDocShell
                         nsnull);                           // No nsIRequest
          }
        }
      }
      else {
        DisplayLoadError(aStatus, url, nsnull);
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
nsWebShell::GetControllerForCommand ( const char * inCommand, nsIController** outController )
{
  NS_ENSURE_ARG_POINTER(outController);
  *outController = nsnull;
  
  nsresult rv = NS_ERROR_FAILURE;
  
  nsCOMPtr<nsPIDOMWindow> window ( do_QueryInterface(mScriptGlobal) );
  if ( window ) {
    nsIFocusController *focusController = window->GetRootFocusController();
    if ( focusController )
      rv = focusController->GetControllerForCommand ( inCommand, outController );
  } // if window

  return rv;
  
} // GetControllerForCommand


nsresult
nsWebShell::IsCommandEnabled ( const char * inCommand, PRBool* outEnabled )
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
nsWebShell::DoCommand ( const char * inCommand )
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
  return IsCommandEnabled ( "cmd_cut", aResult );
}

NS_IMETHODIMP
nsWebShell::CanCopySelection(PRBool* aResult)
{
  return IsCommandEnabled ( "cmd_copy", aResult );
}

NS_IMETHODIMP
nsWebShell::CanCopyLinkLocation(PRBool* aResult)
{
  return IsCommandEnabled ( "cmd_copyLink", aResult );
}

NS_IMETHODIMP
nsWebShell::CanCopyImageLocation(PRBool* aResult)
{
  return IsCommandEnabled ( "cmd_copyImageLocation",
                            aResult );
}

NS_IMETHODIMP
nsWebShell::CanCopyImageContents(PRBool* aResult)
{
  return IsCommandEnabled ( "cmd_copyImageContents",
                            aResult );
}

NS_IMETHODIMP
nsWebShell::CanPaste(PRBool* aResult)
{
  return IsCommandEnabled ( "cmd_paste", aResult );
}

NS_IMETHODIMP
nsWebShell::CutSelection(void)
{
  return DoCommand ( "cmd_cut" );
}

NS_IMETHODIMP
nsWebShell::CopySelection(void)
{
  return DoCommand ( "cmd_copy" );
}

NS_IMETHODIMP
nsWebShell::CopyLinkLocation(void)
{
  return DoCommand ( "cmd_copyLink" );
}

NS_IMETHODIMP
nsWebShell::CopyImageLocation(void)
{
  return DoCommand ( "cmd_copyImageLocation" );
}

NS_IMETHODIMP
nsWebShell::CopyImageContents(void)
{
  return DoCommand ( "cmd_copyImageContents" );
}

NS_IMETHODIMP
nsWebShell::Paste(void)
{
  return DoCommand ( "cmd_paste" );
}

NS_IMETHODIMP
nsWebShell::SelectAll(void)
{
  return DoCommand ( "cmd_selectAll" );
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
  return DoCommand ( "cmd_selectNone" );
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

  // HACK....force the uri loader to give us a load cookie for this webshell...then get its
  // doc loader and store it...as more of the docshell lands, we'll be able to get rid
  // of this hack...
  nsresult rv;
  nsCOMPtr<nsIURILoader> uriLoader = do_GetService(NS_URI_LOADER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = uriLoader->GetDocumentLoaderForContext(this,
                                              getter_AddRefs(mDocLoader));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContentViewerContainer> shellAsContainer;
  CallQueryInterface(this, NS_STATIC_CAST(nsIContentViewerContainer**, getter_AddRefs(shellAsContainer)));
  // Set the webshell as the default IContentViewerContainer for the loader...
  mDocLoader->SetContainer(shellAsContainer);

  return nsDocShell::Create();
}

NS_IMETHODIMP nsWebShell::Destroy()
{
  nsDocShell::Destroy();

  SetContainer(nsnull);

  return NS_OK;
}

#ifdef DEBUG
unsigned long nsWebShell::gNumberOfWebShells = 0;
#endif
