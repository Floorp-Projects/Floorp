/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 cin et: */
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
#include "nsWebShell.h"
#include "nsIWebBrowserChrome2.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebProgress.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIClipboardCommands.h"
#include "nsILinkHandler.h"
#include "nsIStreamListener.h"
#include "nsIPrompt.h"
#include "nsNetUtil.h"
#include "nsIRefreshURI.h"
#include "nsIDOMEvent.h"
#include "nsPresContext.h"
#include "nsIComponentManager.h"
#include "nsCRT.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsReadableUtils.h"
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
#include "nsIHttpChannelInternal.h"
#include "nsIUploadChannel.h"
#include "nsISeekableStream.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"

#include "nsILocaleService.h"
#include "nsIStringBundle.h"

#include "nsICachingChannel.h"

#include "nsIDocument.h"
#include "nsITextToSubURI.h"

#include "nsIExternalProtocolService.h"
#include "nsCExternalHandlerService.h"

#include "nsIIDNService.h"

#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsITimer.h"
#include "nsIScriptSecurityManager.h"
#include "nsContentPolicyUtils.h"

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

//-----------------------------------------------------------------------------

#define PREF_PINGS_ENABLED           "browser.send_pings"
#define PREF_PINGS_MAX_PER_LINK      "browser.send_pings.max_per_link"
#define PREF_PINGS_REQUIRE_SAME_HOST "browser.send_pings.require_same_host"

// Check prefs to see if pings are enabled and if so what restrictions might
// be applied.
//
// @param maxPerLink
//   This parameter returns the number of pings that are allowed per link click
//
// @param requireSameHost
//   This parameter returns PR_TRUE if pings are restricted to the same host as
//   the document in which the click occurs.  If the same host restriction is
//   imposed, then we still allow for pings to cross over to different
//   protocols and ports for flexibility and because it is not possible to send
//   a ping via FTP.
//
// @returns
//   PR_TRUE if pings are enabled and PR_FALSE otherwise.
//
static PRBool
PingsEnabled(PRInt32 *maxPerLink, PRBool *requireSameHost)
{
  PRBool allow = PR_FALSE;

  *maxPerLink = 1;
  *requireSameHost = PR_TRUE;

  nsCOMPtr<nsIPrefBranch> prefs =
      do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    PRBool val;
    if (NS_SUCCEEDED(prefs->GetBoolPref(PREF_PINGS_ENABLED, &val)))
      allow = val;
    if (allow) {
      prefs->GetIntPref(PREF_PINGS_MAX_PER_LINK, maxPerLink);
      prefs->GetBoolPref(PREF_PINGS_REQUIRE_SAME_HOST, requireSameHost);
    }
  }

  return allow;
}

static PRBool
CheckPingURI(nsIURI* uri, nsIContent* content)
{
  if (!uri)
    return PR_FALSE;

  // Check with nsIScriptSecurityManager
  nsCOMPtr<nsIScriptSecurityManager> ssmgr =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(ssmgr, PR_FALSE);

  nsresult rv =
    ssmgr->CheckLoadURIWithPrincipal(content->NodePrincipal(), uri,
                                     nsIScriptSecurityManager::STANDARD);
  if (NS_FAILED(rv)) {
    return PR_FALSE;
  }

  // Ignore non-HTTP(S)
  PRBool match;
  if ((NS_FAILED(uri->SchemeIs("http", &match)) || !match) &&
      (NS_FAILED(uri->SchemeIs("https", &match)) || !match)) {
    return PR_FALSE;
  }

  // Check with contentpolicy
  PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
  nsIURI* docURI = nsnull;
  nsIDocument* doc = content->GetOwnerDoc();
  if (doc) {
    docURI = doc->GetDocumentURI();
  }
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_OTHER,
                                 uri,
                                 docURI,
                                 content,
                                 EmptyCString(), // mime hint
                                 nsnull, //extra
                                 &shouldLoad);
  return NS_SUCCEEDED(rv) && NS_CP_ACCEPTED(shouldLoad);
}

typedef void (* ForEachPingCallback)(void *closure, nsIContent *content,
                                     nsIURI *uri, nsIIOService *ios);

static void
ForEachPing(nsIContent *content, ForEachPingCallback callback, void *closure)
{
  // NOTE: Using nsIDOMNSHTMLAnchorElement2::GetPing isn't really worth it here
  //       since we'd still need to parse the resulting string.  Instead, we
  //       just parse the raw attribute.  It might be nice if the content node
  //       implemented an interface that exposed an enumeration of nsIURIs.

  // Make sure we are dealing with either an <A> or <AREA> element in the HTML
  // or XHTML namespace.
  if (!content->IsNodeOfType(nsINode::eHTML))
    return;
  nsIAtom *nameAtom = content->Tag();
  if (!nameAtom->EqualsUTF8(NS_LITERAL_CSTRING("a")) &&
      !nameAtom->EqualsUTF8(NS_LITERAL_CSTRING("area")))
    return;

  nsCOMPtr<nsIAtom> pingAtom = do_GetAtom("ping");
  if (!pingAtom)
    return;

  nsAutoString value;
  content->GetAttr(kNameSpaceID_None, pingAtom, value);
  if (value.IsEmpty())
    return;

  nsCOMPtr<nsIIOService> ios = do_GetIOService();
  if (!ios)
    return;

  nsIDocument *doc = content->GetOwnerDoc();
  if (!doc)
    return;

  // value contains relative URIs split on spaces (U+0020)
  const PRUnichar *start = value.BeginReading();
  const PRUnichar *end   = value.EndReading();
  const PRUnichar *iter  = start;
  for (;;) {
    if (iter < end && *iter != ' ') {
      ++iter;
    } else {  // iter is pointing at either end or a space
      while (*start == ' ' && start < iter)
        ++start;
      if (iter != start) {
        nsCOMPtr<nsIURI> uri, baseURI = content->GetBaseURI();
        ios->NewURI(NS_ConvertUTF16toUTF8(Substring(start, iter)),
                    doc->GetDocumentCharacterSet().get(),
                    baseURI, getter_AddRefs(uri));
        if (CheckPingURI(uri, content)) {
          callback(closure, content, uri, ios);
        }
      }
      start = iter = iter + 1;
      if (iter >= end)
        break;
    }
  }
}

//----------------------------------------------------------------------

// We wait this many milliseconds before killing the ping channel...
#define PING_TIMEOUT 10000

static void
OnPingTimeout(nsITimer *timer, void *closure)
{
  nsILoadGroup *loadGroup = NS_STATIC_CAST(nsILoadGroup *, closure);
  loadGroup->Cancel(NS_ERROR_ABORT);
  loadGroup->Release();
}

// Check to see if two URIs have the same host or not
static PRBool
IsSameHost(nsIURI *uri1, nsIURI *uri2)
{
  nsCAutoString host1, host2;
  uri1->GetAsciiHost(host1);
  uri2->GetAsciiHost(host2);
  return host1.Equals(host2);
}

class nsPingListener : public nsIStreamListener
                     , public nsIInterfaceRequestor
                     , public nsIChannelEventSink
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK

  nsPingListener(PRBool requireSameHost, nsIContent* content)
    : mRequireSameHost(requireSameHost),
      mContent(content)
  {}

private:
  PRBool mRequireSameHost;
  nsCOMPtr<nsIContent> mContent;
};

NS_IMPL_ISUPPORTS4(nsPingListener, nsIStreamListener, nsIRequestObserver,
                   nsIInterfaceRequestor, nsIChannelEventSink)

NS_IMETHODIMP
nsPingListener::OnStartRequest(nsIRequest *request, nsISupports *context)
{
  return NS_OK;
}

NS_IMETHODIMP
nsPingListener::OnDataAvailable(nsIRequest *request, nsISupports *context,
                                nsIInputStream *stream, PRUint32 offset,
                                PRUint32 count)
{
  PRUint32 result;
  return stream->ReadSegments(NS_DiscardSegment, nsnull, count, &result);
}

NS_IMETHODIMP
nsPingListener::OnStopRequest(nsIRequest *request, nsISupports *context,
                              nsresult status)
{
  return NS_OK;
}

NS_IMETHODIMP
nsPingListener::GetInterface(const nsIID &iid, void **result)
{
  if (iid.Equals(NS_GET_IID(nsIChannelEventSink))) {
    NS_ADDREF_THIS();
    *result = (nsIChannelEventSink *) this;
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
nsPingListener::OnChannelRedirect(nsIChannel *oldChan, nsIChannel *newChan,
                                  PRUint32 flags)
{
  nsCOMPtr<nsIURI> newURI;
  newChan->GetURI(getter_AddRefs(newURI));

  if (!CheckPingURI(newURI, mContent))
    return NS_ERROR_ABORT;

  if (!mRequireSameHost)
    return NS_OK;

  nsCOMPtr<nsIURI> oldURI;
  oldChan->GetURI(getter_AddRefs(oldURI));
  NS_ENSURE_STATE(oldURI && newURI);

  if (!IsSameHost(oldURI, newURI))
    return NS_ERROR_ABORT;

  return NS_OK;
}

struct SendPingInfo {
  PRInt32 numPings;
  PRInt32 maxPings;
  PRBool  requireSameHost;
  nsIURI *referrer;
};

static void
SendPing(void *closure, nsIContent *content, nsIURI *uri, nsIIOService *ios)
{
  SendPingInfo *info = NS_STATIC_CAST(SendPingInfo *, closure);
  if (info->numPings >= info->maxPings)
    return;

  if (info->requireSameHost) {
    // Make sure the referrer and the given uri share the same origin.  We
    // only require the same hostname.  The scheme and port may differ.
    if (!IsSameHost(uri, info->referrer))
      return;
  }

  nsIDocument *doc = content->GetOwnerDoc();
  if (!doc)
    return;

  nsCOMPtr<nsIChannel> chan;
  ios->NewChannelFromURI(uri, getter_AddRefs(chan));
  if (!chan)
    return;

  // Don't bother caching the result of this URI load.
  chan->SetLoadFlags(nsIRequest::INHIBIT_CACHING);

  nsCOMPtr<nsIHttpChannel> httpChan = do_QueryInterface(chan);
  if (!httpChan)
    return;

  // This is needed in order for 3rd-party cookie blocking to work.
  nsCOMPtr<nsIHttpChannelInternal> httpInternal = do_QueryInterface(httpChan);
  if (httpInternal)
    httpInternal->SetDocumentURI(doc->GetDocumentURI());

  if (info->referrer)
    httpChan->SetReferrer(info->referrer);

  httpChan->SetRequestMethod(NS_LITERAL_CSTRING("POST"));

  // Remove extraneous request headers (to reduce request size)
  httpChan->SetRequestHeader(NS_LITERAL_CSTRING("accept"),
                             EmptyCString(), PR_FALSE);
  httpChan->SetRequestHeader(NS_LITERAL_CSTRING("accept-language"),
                             EmptyCString(), PR_FALSE);
  httpChan->SetRequestHeader(NS_LITERAL_CSTRING("accept-charset"),
                             EmptyCString(), PR_FALSE);
  httpChan->SetRequestHeader(NS_LITERAL_CSTRING("accept-encoding"),
                             EmptyCString(), PR_FALSE);

  nsCOMPtr<nsIUploadChannel> uploadChan = do_QueryInterface(httpChan);
  if (!uploadChan)
    return;

  // To avoid sending an unnecessary Content-Type header, we encode the
  // closing portion of the headers in the POST body.
  NS_NAMED_LITERAL_CSTRING(uploadData, "Content-Length: 0\r\n\r\n");

  nsCOMPtr<nsIInputStream> uploadStream;
  NS_NewPostDataStream(getter_AddRefs(uploadStream), PR_FALSE,
                       uploadData, 0);
  if (!uploadStream)
    return;

  uploadChan->SetUploadStream(uploadStream, EmptyCString(), -1);

  // The channel needs to have a loadgroup associated with it, so that we can
  // cancel the channel and any redirected channels it may create.
  nsCOMPtr<nsILoadGroup> loadGroup =
      do_CreateInstance(NS_LOADGROUP_CONTRACTID);
  if (!loadGroup)
    return;
  chan->SetLoadGroup(loadGroup);

  // Construct a listener that merely discards any response.  If successful at
  // opening the channel, then it is not necessary to hold a reference to the
  // channel.  The networking subsystem will take care of that for us.
  nsCOMPtr<nsIStreamListener> listener =
      new nsPingListener(info->requireSameHost, content);
  if (!listener)
    return;

  // Observe redirects as well:
  nsCOMPtr<nsIInterfaceRequestor> callbacks = do_QueryInterface(listener);
  NS_ASSERTION(callbacks, "oops");
  loadGroup->SetNotificationCallbacks(callbacks);

  chan->AsyncOpen(listener, nsnull);

  // Even if AsyncOpen failed, we still count this as a successful ping.  It's
  // possible that AsyncOpen may have failed after triggering some background
  // process that may have written something to the network.
  info->numPings++;

  // Prevent ping requests from stalling and never being garbage collected...
  nsCOMPtr<nsITimer> timer =
      do_CreateInstance(NS_TIMER_CONTRACTID);
  if (timer) {
    nsresult rv = timer->InitWithFuncCallback(OnPingTimeout, loadGroup,
                                              PING_TIMEOUT,
                                              nsITimer::TYPE_ONE_SHOT);
    if (NS_SUCCEEDED(rv)) {
      // When the timer expires, the callback function will release this
      // reference to the loadgroup.
      NS_STATIC_CAST(nsILoadGroup *, loadGroup.get())->AddRef();
      loadGroup = 0;
    }
  }
  
  // If we failed to setup the timer, then we should just cancel the channel
  // because we won't be able to ensure that it goes away in a timely manner.
  if (loadGroup)
    chan->Cancel(NS_ERROR_ABORT);
}

// Spec: http://whatwg.org/specs/web-apps/current-work/#ping
static void
DispatchPings(nsIContent *content, nsIURI *referrer)
{
  SendPingInfo info;

  if (!PingsEnabled(&info.maxPings, &info.requireSameHost))
    return;
  if (info.maxPings == 0)
    return;

  info.numPings = 0;
  info.referrer = referrer;

  ForEachPing(content, SendPing, &info);
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
    printf("++WEBSHELL %p == %ld\n", (void*) this, gNumberOfWebShells);
#endif

  InitFrameData();
  mItemType = typeContent;
  mCharsetReloadState = eCharsetReloadInit;
}

nsWebShell::~nsWebShell()
{
   Destroy();

  ++mRefCnt; // following releases can cause this destructor to be called
             // recursively if the refcount is allowed to remain 0

  mContentViewer=nsnull;
  mDeviceContext=nsnull;

  InitFrameData();

#ifdef DEBUG
  // We're counting the number of |nsWebShells| to help find leaks
  --gNumberOfWebShells;
#endif
#ifdef DEBUG
  printf("--WEBSHELL %p == %ld\n", (void*) this, gNumberOfWebShells);
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
   NS_INTERFACE_MAP_ENTRY(nsIWebShellServices)
   NS_INTERFACE_MAP_ENTRY(nsILinkHandler)
   NS_INTERFACE_MAP_ENTRY(nsIClipboardCommands)
NS_INTERFACE_MAP_END_INHERITING(nsDocShell)

NS_IMETHODIMP
nsWebShell::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
   NS_PRECONDITION(aInstancePtr, "null out param");

   *aInstancePtr = nsnull;

   if(aIID.Equals(NS_GET_IID(nsICommandManager)))
      {
      NS_ENSURE_SUCCESS(EnsureCommandHandler(), NS_ERROR_FAILURE);
      *aInstancePtr = mCommandManager;
      NS_ADDREF((nsISupports*) *aInstancePtr);
      return NS_OK;
      }

   return nsDocShell::GetInterface(aIID, aInstancePtr);
}

nsEventStatus PR_CALLBACK
nsWebShell::HandleEvent(nsGUIEvent *aEvent)
{
  return nsEventStatus_eIgnore;
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

class OnLinkClickEvent : public nsRunnable {
public:
  OnLinkClickEvent(nsWebShell* aHandler, nsIContent* aContent,
                   nsIURI* aURI,
                   const PRUnichar* aTargetSpec,
                   nsIInputStream* aPostDataStream = 0, 
                   nsIInputStream* aHeadersDataStream = 0);

  NS_IMETHOD Run() {
    nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(mHandler->mScriptGlobal));
    nsAutoPopupStatePusher popupStatePusher(window, mPopupState);

    mHandler->OnLinkClickSync(mContent, mURI,
                              mTargetSpec.get(), mPostDataStream,
                              mHeadersDataStream,
                              nsnull, nsnull);
    return NS_OK;
  }

private:
  nsRefPtr<nsWebShell>     mHandler;
  nsCOMPtr<nsIURI>         mURI;
  nsString                 mTargetSpec;
  nsCOMPtr<nsIInputStream> mPostDataStream;
  nsCOMPtr<nsIInputStream> mHeadersDataStream;
  nsCOMPtr<nsIContent>     mContent;
  PopupControlState        mPopupState;
};

OnLinkClickEvent::OnLinkClickEvent(nsWebShell* aHandler,
                                   nsIContent *aContent,
                                   nsIURI* aURI,
                                   const PRUnichar* aTargetSpec,
                                   nsIInputStream* aPostDataStream,
                                   nsIInputStream* aHeadersDataStream)
  : mHandler(aHandler)
  , mURI(aURI)
  , mTargetSpec(aTargetSpec)
  , mPostDataStream(aPostDataStream)
  , mHeadersDataStream(aHeadersDataStream)
  , mContent(aContent)
{
  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(mHandler->mScriptGlobal));

  mPopupState = window->GetPopupControlState();
}

//----------------------------------------

NS_IMETHODIMP
nsWebShell::OnLinkClick(nsIContent* aContent,
                        nsIURI* aURI,
                        const PRUnichar* aTargetSpec,
                        nsIInputStream* aPostDataStream,
                        nsIInputStream* aHeadersDataStream)
{
  NS_ASSERTION(NS_IsMainThread(), "wrong thread");
  nsCOMPtr<nsIRunnable> ev =
      new OnLinkClickEvent(this, aContent, aURI, aTargetSpec,
                           aPostDataStream, aHeadersDataStream);
  return NS_DispatchToCurrentThread(ev);
}

NS_IMETHODIMP
nsWebShell::OnLinkClickSync(nsIContent *aContent,
                            nsIURI* aURI,
                            const PRUnichar* aTargetSpec,
                            nsIInputStream* aPostDataStream,
                            nsIInputStream* aHeadersDataStream,
                            nsIDocShell** aDocShell,
                            nsIRequest** aRequest)
{
  // Initialize the DocShell / Request
  if (aDocShell) {
    *aDocShell = nsnull;
  }
  if (aRequest) {
    *aRequest = nsnull;
  }

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
          return extProtService->LoadUrl(aURI);
        }
      }
    }
  }

  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(aContent));
  NS_ENSURE_TRUE(node, NS_ERROR_UNEXPECTED);

  PRBool inherit;
  nsresult rv = URIInheritsSecurityContext(aURI, &inherit);
  if (NS_FAILED(rv) || inherit) {
    nsCOMPtr<nsIDocument> sourceDoc = aContent->GetDocument();

    if (!sourceDoc) {
      // The source is in a 'zombie' document, or not part of a
      // document any more. Don't let it perform loads in this docshell.
      // XXXbz why only for the inherit case?

      return NS_OK;
    }

    nsCOMPtr<nsIPresShell> presShell;
    GetPresShell(getter_AddRefs(presShell));
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    if (presShell->GetDocument() != sourceDoc) {
      // The source is not in the current document, don't let it load anything
      // that would inherit the principals of the current document.

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
  
  rv = InternalLoad(aURI,               // New URI
                    referer,            // Referer URI
                    nsnull,             // No onwer
                    INTERNAL_LOAD_FLAGS_INHERIT_OWNER, // Inherit owner from document
                    target.get(),       // Window target
                    NS_LossyConvertUTF16toASCII(typeHint).get(),
                    aPostDataStream,    // Post data stream
                    aHeadersDataStream, // Headers stream
                    LOAD_LINK,          // Load type
                    nsnull,             // No SHEntry
                    PR_TRUE,            // first party site
                    aDocShell,          // DocShell out-param
                    aRequest);          // Request out-param
  if (NS_SUCCEEDED(rv)) {
    DispatchPings(aContent, referer);
  }
  return rv;
}

NS_IMETHODIMP
nsWebShell::OnOverLink(nsIContent* aContent,
                       nsIURI* aURI,
                       const PRUnichar* aTargetSpec)
{
  nsCOMPtr<nsIWebBrowserChrome2> browserChrome2 = do_GetInterface(mTreeOwner);
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIWebBrowserChrome> browserChrome;
  if (!browserChrome2) {
    browserChrome = do_GetInterface(mTreeOwner);
    if (!browserChrome)
      return rv;
  }

  nsCOMPtr<nsITextToSubURI> textToSubURI =
      do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  // use url origin charset to unescape the URL
  nsCAutoString charset;
  rv = aURI->GetOriginCharset(charset);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString spec;
  rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString uStr;
  rv = textToSubURI->UnEscapeURIForUI(charset, spec, uStr);    
  NS_ENSURE_SUCCESS(rv, rv);

  if (browserChrome2) {
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aContent);
    rv = browserChrome2->SetStatusWithContext(nsIWebBrowserChrome::STATUS_LINK,
                                              uStr, element);
  } else {
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

  // Save a pointer to the currently-loading history entry.
  // nsDocShell::EndPageLoad will clear mLSHE, but we may need this history
  // entry further down in this method.
  nsCOMPtr<nsISHEntry> loadingSHE = mLSHE;
  
  //
  // one of many safeguards that prevent death and destruction if
  // someone is so very very rude as to bring this window down
  // during this load handler.
  //
  nsCOMPtr<nsIDocShell> kungFuDeathGrip(this);
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
  //   1. Check to see if it's a file not found error or bad content
  //      encoding error.
  //   2. Send the URI to a keyword server (if enabled)
  //   3. If the error was DNS failure, then add www and .com to the URI
  //      (if appropriate).
  //   4. Throw an error dialog box...
  //

  if (url && NS_FAILED(aStatus))
  {
    if (aStatus == NS_ERROR_FILE_NOT_FOUND ||
        aStatus == NS_ERROR_INVALID_CONTENT_ENCODING)
    {
      DisplayLoadError(aStatus, url, nsnull, channel);
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
      if (aStatus == NS_ERROR_UNKNOWN_HOST && mAllowKeywordFixup)
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

        if(keywordsEnabled && (kNotFound == dotLoc)) {
          // only send non-qualified hosts to the keyword server
          //
          // If this string was passed through nsStandardURL by chance, then it
          // may have been converted from UTF-8 to ACE, which would result in a
          // completely bogus keyword query.  Here we try to recover the
          // original Unicode value, but this is not 100% correct since the
          // value may have been normalized per the IDN normalization rules.
          //
          // Since we don't have access to the exact original string that was
          // entered by the user, this will just have to do.
          //
          PRBool isACE;
          nsCAutoString utf8Host;
          nsCOMPtr<nsIIDNService> idnSrv =
              do_GetService(NS_IDNSERVICE_CONTRACTID);
          if (idnSrv &&
              NS_SUCCEEDED(idnSrv->IsACE(host, &isACE)) && isACE &&
              NS_SUCCEEDED(idnSrv->ConvertACEtoUTF8(host, utf8Host)))
            sURIFixup->KeywordToURI(utf8Host, getter_AddRefs(newURI));
          else
            sURIFixup->KeywordToURI(host, getter_AddRefs(newURI));
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
          NS_ConvertUTF8toUTF16 newSpecW(newSpec);

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
      DisplayLoadError(aStatus, url, nsnull, channel);
    }
    // Errors to be shown for any frame
    else if (aStatus == NS_ERROR_NET_TIMEOUT ||
             aStatus == NS_ERROR_REDIRECT_LOOP ||
             aStatus == NS_ERROR_UNKNOWN_SOCKET_TYPE ||
             aStatus == NS_ERROR_NET_INTERRUPT ||
             aStatus == NS_ERROR_NET_RESET ||
             NS_ERROR_GET_MODULE(aStatus) == NS_ERROR_MODULE_SECURITY) {
      DisplayLoadError(aStatus, url, nsnull, channel);
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
      if (method.Equals("POST") && !NS_IsOffline()) {
        PRBool repost;
        rv = ConfirmRepost(&repost);
        if (NS_FAILED(rv)) return rv;
        // If the user pressed cancel in the dialog, return. Don't try to load
        // the page without the post data.
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
          if (shInternal) {
            rootSH->GetIndex(&mPreviousTransIndex);
            shInternal->UpdateIndex();
            rootSH->GetIndex(&mLoadedTransIndex);
#ifdef DEBUG_PAGE_CACHE
            printf("Previous index: %d, Loaded index: %d\n\n",
                  mPreviousTransIndex, mLoadedTransIndex);
#endif
          }
        }

        // Make it look like we really did honestly finish loading the
        // history page we were loading, since the "reload" load we're
        // about to kick off will reload our current history entry.  This
        // is a bit of a hack, and if the force-load fails I think we'll
        // end up being confused about what page we're on... but we would
        // anyway, since we've updated the session history index above.
        SetHistoryEntry(&mOSHE, loadingSHE);

        // The user does want to repost the data to the server.
        // Initiate a new load again.

        // Get the postdata if any from the channel.
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
                    INTERNAL_LOAD_FLAGS_INHERIT_OWNER, // Inherit owner
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
      else {
        DisplayLoadError(aStatus, url, nsnull, channel);
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
  if (mPrefs) {
    // We've already been created
    return NS_OK;
  }
  
  WEB_TRACE(WEB_TRACE_CALLS,
            ("nsWebShell::Init: this=%p", this));

  return nsDocShell::Create();
}

#ifdef DEBUG
unsigned long nsWebShell::gNumberOfWebShells = 0;
#endif
