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
 *   Peter Annema <disttsc@bart.nl>
 *   Dan Rosen <dr@netscape.com>
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
#include "nsDOMCID.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsNetUtil.h"
#include "nsRect.h"
#include "prprf.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIChromeEventHandler.h"
#include "nsIDOMWindowInternal.h"
#include "nsIWebBrowserChrome.h"
#include "nsPoint.h"
#include "nsGfxCIID.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsTextFormatter.h"
#include "nsIHttpEventSink.h"
#include "nsISecurityEventSink.h"
#include "nsScriptSecurityManager.h"
#include "nsDocumentCharsetInfoCID.h"
#include "nsICanvasFrame.h"
#include "nsIPluginViewer.h"

// Local Includes
#include "nsDocShell.h"
#include "nsDocShellLoadInfo.h"
#include "nsCDefaultURIFixup.h"
#include "nsDocShellEnumerator.h"

// Helper Classes
#include "nsDOMError.h"
#include "nsEscape.h"

// Interfaces Needed
#include "nsIUploadChannel.h"
#include "nsIHttpChannel.h"
#include "nsIDataChannel.h"
#include "nsIProgressEventSink.h"
#include "nsIWebProgress.h"
#include "nsILayoutHistoryState.h"
#include "nsITimer.h"
#include "nsIFileStream.h"
#include "nsISHistoryInternal.h"
#include "nsIPrincipal.h"
#include "nsIHistoryEntry.h"

#include "nsPIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsICachingChannel.h"
#include "nsICacheEntryDescriptor.h"

// The following are for bug #13871: Prevent frameset spoofing
#include "nsICodebasePrincipal.h"
#include "nsIHTMLDocument.h"

// For reporting errors with the console service.
// These can go away if error reporting is propagated up past nsDocShell.
#include "nsIConsoleService.h"
#include "nsIScriptError.h"

// used to dispatch urls to default protocol handlers
#include "nsCExternalHandlerService.h"
#include "nsIExternalProtocolService.h"

#include "nsIFocusController.h"

#include "nsITextToSubURI.h"

#include "prlog.h"

// this is going away - see
// http://bugzilla.mozilla.org/show_bug.cgi?id=71482
#include "nsIBrowserHistory.h"

#include "nsIEventStateManager.h"

#include "nsIFrame.h"
#include "nsIStyleContext.h"

// for embedding
#include "nsIEmbeddingSiteWindow.h"
#include "nsIWebBrowserChromeFocus.h"

static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);
static NS_DEFINE_CID(kDocumentCharsetInfoCID, NS_DOCUMENTCHARSETINFO_CID);
static NS_DEFINE_CID(kPluginManagerCID, NS_PLUGINMANAGER_CID);
static NS_DEFINE_CID(kDOMScriptObjectFactoryCID,
                     NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

#if defined(DEBUG_rods) || defined(DEBUG_bryner)
//#define DEBUG_DOCSHELL_FOCUS
#endif
//
// Local function prototypes
//

/**

 * Used in AddHeadersToChannel

 */

static NS_METHOD AHTC_WriteFunc(nsIInputStream * in,
                                void *closure,
                                const char *fromRawSegment,
                                PRUint32 toOffset,
                                PRUint32 count, PRUint32 * writeCount);

#ifdef PR_LOGGING
static PRLogModuleInfo* gDocShellLog;
#endif

//*****************************************************************************
//***    nsDocShellFocusController
//*****************************************************************************

class nsDocShellFocusController
{

public:
  static nsDocShellFocusController* GetInstance() { return &mDocShellFocusControllerSingleton; }
  virtual ~nsDocShellFocusController(){}

  void Focus(nsIDocShell* aDS);
  void ClosingDown(nsIDocShell* aDS);

protected:
  nsDocShellFocusController(){}

  nsIDocShell* mFocusedDocShell; // very weak reference

private:
  static nsDocShellFocusController mDocShellFocusControllerSingleton;
};

nsDocShellFocusController nsDocShellFocusController::mDocShellFocusControllerSingleton;

//*****************************************************************************
//***    nsDocShell: Object Management
//*****************************************************************************

nsDocShell::nsDocShell():
    mContentListener(nsnull),
    mInitInfo(nsnull),
    mMarginWidth(0),
    mMarginHeight(0),
    mItemType(typeContent),
    mCurrentScrollbarPref(-1, -1),
    mDefaultScrollbarPref(-1, -1),
    mAllowSubframes(PR_TRUE),
    mAllowPlugins(PR_TRUE),
    mAllowJavascript(PR_TRUE),
    mAllowMetaRedirects(PR_TRUE),
    mAllowImages(PR_TRUE),
    mFocusDocFirst(PR_FALSE),
    mAppType(nsIDocShell::APP_TYPE_UNKNOWN),
    mBusyFlags(BUSY_FLAGS_NONE),
    mFiredUnloadEvent(PR_FALSE),
    mEODForCurrentDocument(PR_FALSE),
    mURIResultedInDocument(PR_FALSE),
    mUseExternalProtocolHandler(PR_FALSE),
    mDisallowPopupWindows(PR_FALSE),
    mIsBeingDestroyed(PR_FALSE),
    mParent(nsnull),
    mTreeOwner(nsnull),
    mChromeEventHandler(nsnull)
{
    NS_INIT_REFCNT();
#ifdef PR_LOGGING
    if (! gDocShellLog)
        gDocShellLog = PR_NewLogModule("nsDocShell");
#endif
}

nsDocShell::~nsDocShell()
{
    nsDocShellFocusController* dsfc = nsDocShellFocusController::GetInstance();
    if (dsfc) {
      dsfc->ClosingDown(this);
    }
    Destroy();
}

NS_IMETHODIMP
nsDocShell::DestroyChildren()
{
  PRInt32 i, n = mChildren.Count();
  nsCOMPtr<nsIDocShellTreeItem> shell;
  for (i = 0; i < n; i++) {
    shell = dont_AddRef((nsIDocShellTreeItem *) mChildren.ElementAt(i));
    if (!NS_WARN_IF_FALSE(shell, "docshell has null child"))
      shell->SetParent(nsnull);
      shell->SetTreeOwner(nsnull);
      // just clear out the array.  When the nsFrameFrame that holds the subshell is
      // destroyed, then the Destroy() method of that subshell will actually get
      // called.
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
    NS_INTERFACE_MAP_ENTRY(nsIDocCharset)
    NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
    NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObjectOwner)
    NS_INTERFACE_MAP_ENTRY(nsIRefreshURI)
    NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_THREADSAFE

///*****************************************************************************
// nsDocShell::nsIInterfaceRequestor
//*****************************************************************************   
NS_IMETHODIMP nsDocShell::GetInterface(const nsIID & aIID, void **aSink)
{
   NS_ENSURE_ARG_POINTER(aSink);

    if (aIID.Equals(NS_GET_IID(nsIURIContentListener)) &&
        NS_SUCCEEDED(EnsureContentListener())) {
        *aSink = mContentListener;
    }
    else if (aIID.Equals(NS_GET_IID(nsIScriptGlobalObject)) &&
             NS_SUCCEEDED(EnsureScriptEnvironment())) {
        *aSink = mScriptGlobal;
    }
    else if (aIID.Equals(NS_GET_IID(nsIDOMWindowInternal)) &&
             NS_SUCCEEDED(EnsureScriptEnvironment())) {
        NS_ENSURE_SUCCESS(mScriptGlobal->
                          QueryInterface(NS_GET_IID(nsIDOMWindowInternal),
                                         aSink), NS_ERROR_FAILURE);
        return NS_OK;
    }
    else if (aIID.Equals(NS_GET_IID(nsIDOMWindow)) &&
             NS_SUCCEEDED(EnsureScriptEnvironment())) {
        NS_ENSURE_SUCCESS(mScriptGlobal->
                          QueryInterface(NS_GET_IID(nsIDOMWindow), aSink),
                          NS_ERROR_FAILURE);
        return NS_OK;
    }
    else if (aIID.Equals(NS_GET_IID(nsIDOMDocument)) &&
             NS_SUCCEEDED(EnsureContentViewer())) {
        mContentViewer->GetDOMDocument((nsIDOMDocument **) aSink);
    }
    else if (aIID.Equals(NS_GET_IID(nsIPrompt))) {
        nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mTreeOwner));
        if (prompter) {
            *aSink = prompter;
            NS_ADDREF((nsISupports *) * aSink);
            return NS_OK;
        }
        else
            return NS_NOINTERFACE;
    }
    else if (aIID.Equals(NS_GET_IID(nsIAuthPrompt))) {
        nsCOMPtr<nsIAuthPrompt> authPrompter(do_GetInterface(mTreeOwner));
        if (authPrompter) {
            *aSink = authPrompter;
            NS_ADDREF((nsISupports *) * aSink);
            return NS_OK;
        }
        else
            return NS_NOINTERFACE;
    }
    else if (aIID.Equals(NS_GET_IID(nsIProgressEventSink))
             || aIID.Equals(NS_GET_IID(nsIHttpEventSink))
             || aIID.Equals(NS_GET_IID(nsIWebProgress))
             || aIID.Equals(NS_GET_IID(nsISecurityEventSink))) {
        nsCOMPtr<nsIURILoader>
            uriLoader(do_GetService(NS_URI_LOADER_CONTRACTID));
        NS_ENSURE_TRUE(uriLoader, NS_ERROR_FAILURE);
        nsCOMPtr<nsIDocumentLoader> docLoader;
        NS_ENSURE_SUCCESS(uriLoader->
                          GetDocumentLoaderForContext(NS_STATIC_CAST
                                                      (nsIDocShell *, this),
                                                      getter_AddRefs
                                                      (docLoader)),
                          NS_ERROR_FAILURE);
        if (docLoader) {
            nsCOMPtr<nsIInterfaceRequestor>
                requestor(do_QueryInterface(docLoader));
            return requestor->GetInterface(aIID, aSink);
        }
        else
            return NS_ERROR_FAILURE;
    }
    else if (aIID.Equals(NS_GET_IID(nsISHistory))) {
        nsCOMPtr<nsISHistory> shistory;
        nsresult
            rv =
            GetSessionHistory(getter_AddRefs(shistory));
        if (NS_SUCCEEDED(rv) && shistory) {
            *aSink = shistory;
            NS_ADDREF((nsISupports *) * aSink);
            return NS_OK;
        }
        return NS_NOINTERFACE;
    }
    else if (aIID.Equals(NS_GET_IID(nsIWebBrowserFind))) {
        nsresult rv = EnsureFind();
        if (NS_FAILED(rv)) return rv;

        *aSink = mFind;
        NS_ADDREF((nsISupports*)*aSink);
        return NS_OK;
    }
    else {
        return QueryInterface(aIID, aSink);
    }

    NS_IF_ADDREF(((nsISupports *) * aSink));
    return NS_OK;
}

PRUint32
nsDocShell::
ConvertDocShellLoadInfoToLoadType(nsDocShellInfoLoadType aDocShellLoadType)
{
    PRUint32 loadType = LOAD_NORMAL;

    switch (aDocShellLoadType) {
    case nsIDocShellLoadInfo::loadNormal:
        loadType = LOAD_NORMAL;
        break;
    case nsIDocShellLoadInfo::loadNormalReplace:
        loadType = LOAD_NORMAL_REPLACE;
        break;
    case nsIDocShellLoadInfo::loadHistory:
        loadType = LOAD_HISTORY;
        break;
    case nsIDocShellLoadInfo::loadReloadNormal:
        loadType = LOAD_RELOAD_NORMAL;
        break;
    case nsIDocShellLoadInfo::loadReloadCharsetChange:
        loadType = LOAD_RELOAD_CHARSET_CHANGE;
        break;
    case nsIDocShellLoadInfo::loadReloadBypassCache:
        loadType = LOAD_RELOAD_BYPASS_CACHE;
        break;
    case nsIDocShellLoadInfo::loadReloadBypassProxy:
        loadType = LOAD_RELOAD_BYPASS_PROXY;
        break;
    case nsIDocShellLoadInfo::loadReloadBypassProxyAndCache:
        loadType = LOAD_RELOAD_BYPASS_PROXY_AND_CACHE;
        break;
    case nsIDocShellLoadInfo::loadLink:
        loadType = LOAD_LINK;
        break;
    case nsIDocShellLoadInfo::loadRefresh:
        loadType = LOAD_REFRESH;
        break;
    case nsIDocShellLoadInfo::loadBypassHistory:
        loadType = LOAD_BYPASS_HISTORY;
        break;
    }

    return loadType;
}


//*****************************************************************************
// nsDocShell::nsIDocShell
//*****************************************************************************   
NS_IMETHODIMP
nsDocShell::LoadURI(nsIURI * aURI,
                    nsIDocShellLoadInfo * aLoadInfo,
                    PRUint32 aLoadFlags)
{
    nsresult rv;
    nsCOMPtr<nsIURI> referrer;
    nsCOMPtr<nsIInputStream> postStream;
    nsCOMPtr<nsISupports> owner;
    PRBool inheritOwner = PR_FALSE;
    nsCOMPtr<nsISHEntry> shEntry;
    nsXPIDLCString target;
    PRUint32 loadType = MAKE_LOAD_TYPE(LOAD_NORMAL, aLoadFlags);    

    NS_ENSURE_ARG(aURI);

    // Extract the info from the DocShellLoadInfo struct...
    if (aLoadInfo) {
        aLoadInfo->GetReferrer(getter_AddRefs(referrer));

        nsDocShellInfoLoadType lt = nsIDocShellLoadInfo::loadNormal;
        aLoadInfo->GetLoadType(&lt);
        // Get the appropriate loadType from nsIDocShellLoadInfo type
        loadType = ConvertDocShellLoadInfoToLoadType(lt);

        aLoadInfo->GetOwner(getter_AddRefs(owner));
        aLoadInfo->GetInheritOwner(&inheritOwner);
        aLoadInfo->GetSHEntry(getter_AddRefs(shEntry));
        aLoadInfo->GetTarget(getter_Copies(target));
        aLoadInfo->GetPostDataStream(getter_AddRefs(postStream));
    }

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gDocShellLog, PR_LOG_DEBUG)) {
        nsXPIDLCString uristr;
        aURI->GetSpec(getter_Copies(uristr));
        PR_LOG(gDocShellLog, PR_LOG_DEBUG,
               ("nsDocShell[%p]: loading %s with flags 0x%08x",
                this, uristr.get(), aLoadFlags));
    }
#endif

    if (!shEntry && loadType != LOAD_NORMAL_REPLACE && mCurrentURI == nsnull) {
        /* OK. We are in the process of loading a subframe. Checkout the 
         * parent's loadtype. If the parent was loaded thro' a history
         * mechanism, then get the SH entry for the child from the parent.
         * This is done to restore frameset navigation while going back/forward.
         * If the parent was not loaded thro' any history mechanism, we don't
         * have to  do this, since we have nothing to restore. 
         */

        // get the parent
        nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
        GetSameTypeParent(getter_AddRefs(parentAsItem));

        nsCOMPtr<nsIDocShell> parentDS(do_QueryInterface(parentAsItem));
        PRUint32 parentLoadType;

        if (parentDS) {
            // Get the parent's load type
            parentDS->GetLoadType(&parentLoadType);            

            /* Try to get the SHEntry for this subframe from the parent, 
             * only if the parent was loaded thro' a history mechanism, 
             * like back/forward/go/reload
             */
            if (parentAsItem && (parentLoadType & LOAD_CMD_HISTORY) || (parentLoadType & LOAD_CMD_RELOAD)) {
                nsCOMPtr<nsIDocShellHistory> parent(do_QueryInterface(parentAsItem));                
                if (parent) {
                    // Get the ShEntry for the child from the parent
                    parent->GetChildSHEntry(mChildOffset, getter_AddRefs(shEntry));
                    if (shEntry) 
                        loadType = parentLoadType;
                }  // parent
            } 
        } //parentDS
    } // !shEntry

    if (shEntry) {
        // Load is from SH. SH does normal load only
        PR_LOG(gDocShellLog, PR_LOG_DEBUG,
               ("nsDocShell[%p]: loading from session history", this));

        rv = LoadHistoryEntry(shEntry, loadType);
    }
    // Perform the load...
    else {
        // We need an owner (a referring principal). 3 possibilities:
        // (1) If a principal was passed in, that's what we'll use.
        // (2) If the caller has allowed inheriting from the current document,
        //   or if we're being called from chrome (if there's system JS on the stack),
        //   then inheritOwner should be true and InternalLoad will get an owner
        //   from the current document. If none of these things are true, then
        // (3) we pass a null owner into the channel, and an owner will be
        //   created later from the URL.
        if (!owner && !inheritOwner) {
            // See if there's system or chrome JS code running
            nsCOMPtr<nsIScriptSecurityManager> secMan;

            secMan = do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
            if (NS_SUCCEEDED(rv)) {
                nsCOMPtr<nsIPrincipal> sysPrin;
                nsCOMPtr<nsIPrincipal> subjectPrin;

                // Just to compare, not to use!
                rv = secMan->GetSystemPrincipal(getter_AddRefs(sysPrin));
                if (NS_SUCCEEDED(rv)) {
                    rv = secMan->GetSubjectPrincipal(getter_AddRefs(subjectPrin));
                }
                // If there's no subject principal, there's no JS running, so we're in system code.
                if (NS_SUCCEEDED(rv) &&
                    (!subjectPrin || sysPrin.get() == subjectPrin.get())) {
                    inheritOwner = PR_TRUE;
                }
            }
        }

        nsAutoString windowTarget;
        windowTarget.AssignWithConversion(target);

        rv = InternalLoad(aURI,
                          referrer,
                          owner,
                          inheritOwner,
                          windowTarget.get(),
                          postStream,
                          nsnull,         // No headers stream
                          loadType,
                          nsnull);        // No SHEntry
    }

    return rv;
}

NS_IMETHODIMP
nsDocShell::LoadStream(nsIInputStream * aStream, nsIURI * aURI,
                       const char *aContentType, PRInt32 aContentLen,
                       nsIDocShellLoadInfo * aLoadInfo)
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
        if (NS_FAILED(rv))
            return rv;
        // Make sure that the URI spec "looks" like a protocol and path...
        // For now, just use a bogus protocol called "internal"
        rv = uri->SetSpec("internal:load-stream");
        if (NS_FAILED(rv))
            return rv;
    }

    PRUint32 loadType = LOAD_NORMAL;
    if (aLoadInfo) {
        nsDocShellInfoLoadType lt = nsIDocShellLoadInfo::loadNormal;
        (void) aLoadInfo->GetLoadType(&lt);
        // Get the appropriate LoadType from nsIDocShellLoadInfo type
        loadType = ConvertDocShellLoadInfoToLoadType(lt);
    }

    NS_ENSURE_SUCCESS(Stop(nsIWebNavigation::STOP_NETWORK), NS_ERROR_FAILURE);

    mLoadType = loadType;

    // build up a channel for this stream.
    nsCOMPtr<nsIChannel> channel;
    NS_ENSURE_SUCCESS(NS_NewInputStreamChannel
                      (getter_AddRefs(channel), uri, aStream, aContentType,
                       aContentLen), NS_ERROR_FAILURE);

    nsCOMPtr<nsIURILoader>
        uriLoader(do_GetService(NS_URI_LOADER_CONTRACTID));
    NS_ENSURE_TRUE(uriLoader, NS_ERROR_FAILURE);

    NS_ENSURE_SUCCESS(DoChannelLoad(channel, uriLoader), NS_ERROR_FAILURE);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::CreateLoadInfo(nsIDocShellLoadInfo ** aLoadInfo)
{
    nsDocShellLoadInfo *loadInfo = new nsDocShellLoadInfo();
    NS_ENSURE_TRUE(loadInfo, NS_ERROR_OUT_OF_MEMORY);
    nsCOMPtr<nsIDocShellLoadInfo> localRef(loadInfo);

    *aLoadInfo = localRef;
    NS_ADDREF(*aLoadInfo);
    return NS_OK;
}


/*
 * Reset state to a new content model within the current document and the document
 * viewer.  Called by the document before initiating an out of band document.write().
 */
NS_IMETHODIMP
nsDocShell::PrepareForNewContentModel()
{
  mEODForCurrentDocument = PR_FALSE;
  return NS_OK;
}


NS_IMETHODIMP
nsDocShell::FireUnloadNotification()
{
    nsresult rv;

    if (mContentViewer && !mFiredUnloadEvent) {
        mFiredUnloadEvent = PR_TRUE;

        rv = mContentViewer->Unload();

        PRInt32 i, n = mChildren.Count();
        for (i = 0; i < n; i++) {
            nsIDocShellTreeItem* item = (nsIDocShellTreeItem*) mChildren.ElementAt(i);
            if(item) {
                nsCOMPtr<nsIDocShell> shell(do_QueryInterface(item));
                if (shell) {
                    rv = shell->FireUnloadNotification();
                }
            }
        }
    }

    return NS_OK;
}


//
// Bug 13871: Prevent frameset spoofing
// Check if origin document uri is the equivalent to target's principal.
// This takes into account subdomain checking if document.domain is set for
// Nav 4.x compatability.
//
// The following was derived from nsCodeBasePrincipal::Equals but in addition
// to the host PL_strcmp, it accepts a subdomain (nsHTMLDocument::SetDomain)
// if the document.domain was set.
//
static
PRBool SameOrSubdomainOfTarget(nsIURI* aOriginURI, nsIURI* aTargetURI, PRBool aDocumentDomainSet)
{
  nsXPIDLCString targetScheme;
  nsresult rv = aTargetURI->GetScheme(getter_Copies(targetScheme));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && targetScheme, PR_TRUE);

  nsXPIDLCString originScheme;
  rv = aOriginURI->GetScheme(getter_Copies(originScheme));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && originScheme, PR_TRUE);

  if (PL_strcmp(targetScheme, originScheme))
    return PR_FALSE; // Different schemes - check fails

  if (! PL_strcmp(targetScheme, "file"))
    return PR_TRUE; // All file: urls are considered to have the same origin.

  if (! PL_strcmp(targetScheme, "imap") ||
      ! PL_strcmp(targetScheme, "mailbox") ||
      ! PL_strcmp(targetScheme, "news"))
  {

    // Each message is a distinct trust domain; use the whole spec for comparison
    nsXPIDLCString targetSpec;
    rv =aTargetURI->GetSpec(getter_Copies(targetSpec));
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && targetSpec, PR_TRUE);

    nsXPIDLCString originSpec;
    rv = aOriginURI->GetSpec(getter_Copies(originSpec));
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && originSpec, PR_TRUE);

    return (! PL_strcmp(targetSpec, originSpec)); // True if full spec is same, false otherwise
  }

  // Compare ports.
  int targetPort, originPort;
  rv = aTargetURI->GetPort(&targetPort);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv), PR_TRUE);

  rv = aOriginURI->GetPort(&originPort);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv), PR_TRUE);

  if (targetPort != originPort)
    return PR_FALSE; // Different port - check fails

  // Need to check the hosts
  nsXPIDLCString targetHost;
  rv = aTargetURI->GetHost(getter_Copies(targetHost));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && targetHost, PR_TRUE);

  nsXPIDLCString originHost;
  rv = aOriginURI->GetHost(getter_Copies(originHost));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && originHost, PR_TRUE);

  if (!PL_strcmp(targetHost, originHost))
    return PR_TRUE; // Hosts are the same - check passed
  
  // If document.domain was set, do the relaxed check
  // Right align hostnames and compare - ensure preceeding char is . or /
  if (aDocumentDomainSet)
  {
    int targetHostLen = PL_strlen(targetHost);
    int originHostLen = PL_strlen(originHost);
    int prefixChar = originHostLen-targetHostLen-1;

    return ((originHostLen > targetHostLen) &&
            (! PL_strcmp((originHost+prefixChar+1), targetHost)) &&
            (originHost[prefixChar] == '.' || originHost[prefixChar] == '/'));
  }

  return PR_FALSE; // document.domain not set and hosts not same - check failed
}

//
// Bug 13871: Prevent frameset spoofing
//
// This routine answers: 'Is origin's document from same domain as target's document?'
// Be optimistic that domain is same - error cases all answer 'yes'.
//
// We have to compare the URI of the actual document loaded in the origin,
// ignoring any document.domain that was set, with the principal URI of the
// target (including any document.domain that was set).  This puts control
// of loading in the hands of the target, which is more secure. (per Nav 4.x)
//
static
PRBool ValidateOrigin(nsIDocShellTreeItem* aOriginTreeItem, nsIDocShellTreeItem* aTargetTreeItem)
{
  // Get origin document uri (ignoring document.domain)
  nsCOMPtr<nsIWebNavigation> originWebNav(do_QueryInterface(aOriginTreeItem));
  NS_ENSURE_TRUE(originWebNav, PR_TRUE);

  nsCOMPtr<nsIURI> originDocumentURI;
  nsresult rv = originWebNav->GetCurrentURI(getter_AddRefs(originDocumentURI));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && originDocumentURI, PR_TRUE);

  // Get target principal uri (including document.domain)
  nsCOMPtr<nsIDOMDocument> targetDOMDocument(do_GetInterface(aTargetTreeItem));
  NS_ENSURE_TRUE(targetDOMDocument, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument> targetDocument(do_QueryInterface(targetDOMDocument));
  NS_ENSURE_TRUE(targetDocument, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPrincipal> targetPrincipal;
  rv = targetDocument->GetPrincipal(getter_AddRefs(targetPrincipal));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && targetPrincipal, rv);

  nsCOMPtr<nsICodebasePrincipal> targetCodebasePrincipal(do_QueryInterface(targetPrincipal));
  NS_ENSURE_TRUE(targetCodebasePrincipal, PR_TRUE);

  nsCOMPtr<nsIURI> targetPrincipalURI;
  rv = targetCodebasePrincipal->GetURI(getter_AddRefs(targetPrincipalURI));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && targetPrincipalURI, PR_TRUE);

  // Find out if document.domain was set for HTML documents
  PRBool documentDomainSet = PR_FALSE;
  nsCOMPtr<nsIHTMLDocument> targetHTMLDocument(do_QueryInterface(targetDocument));

  // If we don't have an HTML document, fall through with documentDomainSet false
  if (targetHTMLDocument) {
    targetHTMLDocument->WasDomainSet(&documentDomainSet);
  }

  // Is origin same principal or a subdomain of target's document.domain
  // Compare actual URI of origin document, not origin principal's URI. (Per Nav 4.x)
  return SameOrSubdomainOfTarget(originDocumentURI, targetPrincipalURI, documentDomainSet);
}

nsresult nsDocShell::FindTarget(const PRUnichar *aWindowTarget,
                                PRBool *aIsNewWindow,
                                nsIDocShell **aResult)
{
    nsresult rv;
    nsAutoString name(aWindowTarget);
    nsCOMPtr<nsIDocShellTreeItem> treeItem;
    PRBool mustMakeNewWindow = PR_FALSE;

    *aResult      = nsnull;
    *aIsNewWindow = PR_FALSE;

    if(!name.Length() || name.EqualsIgnoreCase("_self"))
    {
        *aResult = this;
    }
    else if (name.EqualsIgnoreCase("_blank") || name.EqualsIgnoreCase("_new"))
    {
        mustMakeNewWindow = PR_TRUE;
        name.AssignWithConversion("");
    }
    else if(name.EqualsIgnoreCase("_parent"))
    {
        GetSameTypeParent(getter_AddRefs(treeItem));
        if(!treeItem)
            *aResult = this;
    }
    else if(name.EqualsIgnoreCase("_top"))
    {
        GetSameTypeRootTreeItem(getter_AddRefs(treeItem));
        if(!treeItem)
            *aResult = this;
    }
    else if(name.EqualsIgnoreCase("_content"))
    {
        if (mTreeOwner) {
            mTreeOwner->FindItemWithName(name.get(), nsnull, 
                                         getter_AddRefs(treeItem));
        } else {
            NS_ERROR("Someone isn't setting up the tree owner.  "
                     "You might like to try that.  "
                     "Things will.....you know, work.");
        }
        // _content should always exist.  If the nsIDocShellTreeOwner did
        // not find one, then create one...
        if (!treeItem) {
            mustMakeNewWindow = PR_TRUE;
        }
    }
    else
    {
        // Try to locate the target window...
        FindItemWithName(name.get(), nsnull, getter_AddRefs(treeItem));

        // The named window cannot be found so it must be created to receive
        // the channel data.

        if (!treeItem) {
            mustMakeNewWindow = PR_TRUE;
        }

        // Bug 13871: Prevent frameset spoofing
        //     See BugSplat 336170, 338737 and XP_FindNamedContextInList in
        //     the classic codebase
        //     Per Nav's behaviour:
        //         - pref controlled: "browser.frame.validate_origin" 
        //           (mValidateOrigin)
        //         - allow load if host of target or target's parent is same
        //           as host of origin
        //         - allow load if target is a top level window

        // Check to see if pref is true
        if (mValidateOrigin && treeItem)
        {

            // Is origin frame from the same domain as target frame?
            if (! ValidateOrigin(this, treeItem))
            {

                // No.  Is origin frame from the same domain as target's parent?
                nsCOMPtr<nsIDocShellTreeItem> targetParentTreeItem;
                
                rv = treeItem->GetSameTypeParent(getter_AddRefs(targetParentTreeItem));
                if (NS_SUCCEEDED(rv) && targetParentTreeItem) 
                {
                    if (! ValidateOrigin(this, targetParentTreeItem)) 
                    {

                        // Neither is from the origin domain, send load to a new window (_blank)
                        mustMakeNewWindow = PR_TRUE;
                        name.AssignWithConversion("");
                    } // else (target's parent from origin domain) allow this load
                } // else (no parent) allow this load since shell is a toplevel window
            } // else (target from origin domain) allow this load
        } // else (pref is false) allow this load
    }

    if (mustMakeNewWindow)
    {
        nsCOMPtr<nsIDOMWindow> newWindow;
        nsCOMPtr<nsIDOMWindowInternal> parentWindow;

        // This DocShell is the parent window
        parentWindow = do_GetInterface(NS_STATIC_CAST(nsIDocShell*, this));
        if (!parentWindow) {
            NS_ASSERTION(0, "Cant get nsIDOMWindowInternal from nsDocShell!");
            return NS_ERROR_FAILURE;
        }

        rv = parentWindow->Open(NS_LITERAL_STRING(""),    // URL to load
                                name,                     // Window name
                                nsDependentString(nsnull),  // Window features
                                getter_AddRefs(newWindow));
        if (NS_FAILED(rv)) return rv;

        // Get the DocShell from the new window...
        nsCOMPtr<nsIScriptGlobalObject> sgo;
        sgo = do_QueryInterface(newWindow, &rv);
        if (NS_FAILED(rv)) return rv;

        // This will AddRef() aResult...
        rv = sgo->GetDocShell(aResult);

        // If all went well, indicate that a new window has been created.
        if (*aResult) {
            *aIsNewWindow = PR_TRUE;

            // if we just open a new window for this link, charset from current docshell 
            // should be kept, as what we did in js openNewWindowWith(url)
            nsCOMPtr<nsIMarkupDocumentViewer> muCV, target_muCV;
            nsCOMPtr<nsIContentViewer> cv, target_cv;
            this->GetContentViewer(getter_AddRefs(cv));
            (*aResult)->GetContentViewer(getter_AddRefs(target_cv));
            if (cv && target_cv) {
              muCV = do_QueryInterface(cv);            
              target_muCV = do_QueryInterface(target_cv);            
              if (muCV && target_muCV) {
                nsXPIDLString defaultCharset;
                rv = muCV->GetDefaultCharacterSet(getter_Copies(defaultCharset));
                if(NS_SUCCEEDED(rv)) {
                  target_muCV->SetDefaultCharacterSet(defaultCharset);
                }
              }
            } 
        }

        return rv;
    }
    else
    {
        if (treeItem)
        {
            NS_ASSERTION(!*aResult, "aResult should be null if treeItem is set!");
            treeItem->QueryInterface(NS_GET_IID(nsIDocShell), (void **)aResult);
        }
        else
        {
            NS_IF_ADDREF(*aResult);
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetEldestPresContext(nsIPresContext** aPresContext)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(aPresContext);
    *aPresContext = nsnull;

    nsCOMPtr<nsIContentViewer> viewer = mContentViewer;
    while (viewer) {
        nsCOMPtr<nsIContentViewer> prevViewer;
        viewer->GetPreviousViewer(getter_AddRefs(prevViewer));
        if (prevViewer)
            viewer = prevViewer;
        else {
            nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(viewer));
            if (docv)
                rv = docv->GetPresContext(*aPresContext);
            break;
        }
    }

    return rv;
}

NS_IMETHODIMP
nsDocShell::GetPresContext(nsIPresContext ** aPresContext)
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

NS_IMETHODIMP
nsDocShell::GetPresShell(nsIPresShell ** aPresShell)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(aPresShell);
    *aPresShell = nsnull;

    nsCOMPtr<nsIPresContext> presContext;
    (void) GetPresContext(getter_AddRefs(presContext));

    if (presContext) {
        rv = presContext->GetShell(aPresShell);
    }

    return rv;
}

NS_IMETHODIMP
nsDocShell::GetEldestPresShell(nsIPresShell** aPresShell)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(aPresShell);
    *aPresShell = nsnull;

    nsCOMPtr<nsIPresContext> presContext;
    (void) GetEldestPresContext(getter_AddRefs(presContext));

    if (presContext) {
        rv = presContext->GetShell(aPresShell);
    }

    return rv;
}

NS_IMETHODIMP
nsDocShell::GetContentViewer(nsIContentViewer ** aContentViewer)
{
    NS_ENSURE_ARG_POINTER(aContentViewer);

    *aContentViewer = mContentViewer;
    NS_IF_ADDREF(*aContentViewer);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetChromeEventHandler(nsIChromeEventHandler * aChromeEventHandler)
{
    // Weak reference. Don't addref.
    mChromeEventHandler = aChromeEventHandler;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetChromeEventHandler(nsIChromeEventHandler ** aChromeEventHandler)
{
    NS_ENSURE_ARG_POINTER(aChromeEventHandler);

    *aChromeEventHandler = mChromeEventHandler;
    NS_IF_ADDREF(*aChromeEventHandler);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetParentURIContentListener(nsIURIContentListener ** aParent)
{
    NS_ENSURE_ARG_POINTER(aParent);
    NS_ENSURE_SUCCESS(EnsureContentListener(), NS_ERROR_FAILURE);

    return mContentListener->GetParentContentListener(aParent);
}

NS_IMETHODIMP
nsDocShell::SetParentURIContentListener(nsIURIContentListener * aParent)
{
    NS_ENSURE_SUCCESS(EnsureContentListener(), NS_ERROR_FAILURE);

    return mContentListener->SetParentContentListener(aParent);
}

NS_IMETHODIMP
nsDocShell::GetCharset(PRUnichar** aCharset)
{
    NS_ENSURE_ARG_POINTER(aCharset);
    *aCharset = nsnull; 

    nsCOMPtr<nsIPresShell> presShell;
    nsCOMPtr<nsIDocument> doc;
    GetPresShell(getter_AddRefs(presShell));
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
    presShell->GetDocument(getter_AddRefs(doc));
    NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
    nsAutoString charset;
    NS_ENSURE_SUCCESS(doc->GetDocumentCharacterSet(charset), NS_ERROR_FAILURE);
    *aCharset = ToNewUnicode(charset);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetCharset(const PRUnichar* aCharset)
{
    // set the default charset
    nsCOMPtr<nsIContentViewer> viewer;
    GetContentViewer(getter_AddRefs(viewer));
    if (viewer) {
      nsCOMPtr<nsIMarkupDocumentViewer> muDV(do_QueryInterface(viewer));
      if (muDV) {
        NS_ENSURE_SUCCESS(muDV->SetDefaultCharacterSet(aCharset),
                                                       NS_ERROR_FAILURE);
      }
    }

    // set the charset override
    nsCOMPtr<nsIDocumentCharsetInfo> dcInfo;
    GetDocumentCharsetInfo(getter_AddRefs(dcInfo));
    if (dcInfo) {
      nsCOMPtr<nsIAtom> csAtom;
      csAtom = dont_AddRef(NS_NewAtom(aCharset));
      dcInfo->SetForcedCharset(csAtom);
    }

    return NS_OK;
} 

NS_IMETHODIMP
nsDocShell::GetDocumentCharsetInfo(nsIDocumentCharsetInfo **
                                   aDocumentCharsetInfo)
{
    NS_ENSURE_ARG_POINTER(aDocumentCharsetInfo);

    // if the mDocumentCharsetInfo does not exist already, we create it now
    if (!mDocumentCharsetInfo) {
        nsresult res =
            nsComponentManager::CreateInstance(kDocumentCharsetInfoCID,
                                               NULL,
                                               NS_GET_IID
                                               (nsIDocumentCharsetInfo),
                                               getter_AddRefs
                                               (mDocumentCharsetInfo));
        if (NS_FAILED(res))
            return NS_ERROR_FAILURE;
    }

    *aDocumentCharsetInfo = mDocumentCharsetInfo;
    NS_IF_ADDREF(*aDocumentCharsetInfo);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetDocumentCharsetInfo(nsIDocumentCharsetInfo *
                                   aDocumentCharsetInfo)
{
    mDocumentCharsetInfo = aDocumentCharsetInfo;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowPlugins(PRBool * aAllowPlugins)
{
    NS_ENSURE_ARG_POINTER(aAllowPlugins);

    *aAllowPlugins = mAllowPlugins;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowPlugins(PRBool aAllowPlugins)
{
    mAllowPlugins = aAllowPlugins;
    //XXX should enable or disable a plugin host
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowJavascript(PRBool * aAllowJavascript)
{
    NS_ENSURE_ARG_POINTER(aAllowJavascript);

    *aAllowJavascript = mAllowJavascript;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowJavascript(PRBool aAllowJavascript)
{
    mAllowJavascript = aAllowJavascript;
    return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetAllowMetaRedirects(PRBool * aReturn)
{
    NS_ENSURE_ARG_POINTER(aReturn);

    *aReturn = mAllowMetaRedirects;
    return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetAllowMetaRedirects(PRBool aValue)
{
    mAllowMetaRedirects = aValue;
    return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetAllowSubframes(PRBool * aAllowSubframes)
{
    NS_ENSURE_ARG_POINTER(aAllowSubframes);

    *aAllowSubframes = mAllowSubframes;
    return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetAllowSubframes(PRBool aAllowSubframes)
{
    mAllowSubframes = aAllowSubframes;
    return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetAllowImages(PRBool * aAllowImages)
{
    NS_ENSURE_ARG_POINTER(aAllowImages);

    *aAllowImages = mAllowImages;
    return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetAllowImages(PRBool aAllowImages)
{
    mAllowImages = aAllowImages;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetDocShellEnumerator(PRInt32 aItemType, PRInt32 aDirection, nsISimpleEnumerator **outEnum)
{
    NS_ENSURE_ARG_POINTER(outEnum);
    *outEnum = nsnull;
    
    nsDocShellEnumerator*   docShellEnum;
    if (aDirection == ENUMERATE_FORWARDS)
        docShellEnum = new nsDocShellForwardsEnumerator;
    else
        docShellEnum = new nsDocShellBackwardsEnumerator;
    
    if (!docShellEnum) return NS_ERROR_OUT_OF_MEMORY;
    
    nsresult rv = docShellEnum->SetEnumDocShellType(aItemType);
    if (NS_FAILED(rv)) return rv;

    rv = docShellEnum->SetEnumerationRootItem((nsIDocShellTreeItem *)this);
    if (NS_FAILED(rv)) return rv;

    rv = docShellEnum->First();
    if (NS_FAILED(rv)) return rv;
    
    docShellEnum->AddRef();     // ensure we don't lose the last ref inside the QueryInterface
    rv = docShellEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)outEnum);
    docShellEnum->Release();
    return rv;
}

NS_IMETHODIMP
nsDocShell::GetAppType(PRUint32 * aAppType)
{
    *aAppType = mAppType;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAppType(PRUint32 aAppType)
{
    mAppType = aAppType;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetZoom(float *zoom)
{
    NS_ENSURE_ARG_POINTER(zoom);
    NS_ENSURE_SUCCESS(EnsureDeviceContext(), NS_ERROR_FAILURE);

    NS_ENSURE_SUCCESS(mDeviceContext->GetZoom(*zoom), NS_ERROR_FAILURE);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetZoom(float zoom)
{
    NS_ENSURE_SUCCESS(EnsureDeviceContext(), NS_ERROR_FAILURE);
    mDeviceContext->SetZoom(zoom);

    // get the pres shell
    nsCOMPtr<nsIPresShell> presShell;
    NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)),
                      NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    // get the view manager
    nsCOMPtr<nsIViewManager> vm;
    NS_ENSURE_SUCCESS(presShell->GetViewManager(getter_AddRefs(vm)),
                      NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

    // get the root scrollable view
    nsIScrollableView *scrollableView = nsnull;
    vm->GetRootScrollableView(&scrollableView);
    if (scrollableView)
        scrollableView->ComputeScrollOffsets();

    // get the root view
    nsIView *rootView = nsnull; // views are not ref counted
    vm->GetRootView(rootView);
    if (rootView)
        vm->UpdateView(rootView, 0);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetMarginWidth(PRInt32 * aWidth)
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
nsDocShell::GetMarginHeight(PRInt32 * aHeight)
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

NS_IMETHODIMP
nsDocShell::GetBusyFlags(PRUint32 * aBusyFlags)
{
    NS_ENSURE_ARG_POINTER(aBusyFlags);

    *aBusyFlags = mBusyFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::TabToTreeOwner(PRBool aForward, PRBool* aTookFocus)
{
    NS_ENSURE_ARG_POINTER(aTookFocus);
    
    nsCOMPtr<nsIWebBrowserChromeFocus> chromeFocus = do_GetInterface(mTreeOwner);
    if (chromeFocus) {
        *aTookFocus = PR_TRUE;
        if (aForward)
            chromeFocus->FocusNextElement();
        else
            chromeFocus->FocusPrevElement();
    } else
        *aTookFocus = PR_FALSE;
    
    return NS_OK;
}


//*****************************************************************************
// nsDocShell::nsIDocShellTreeItem
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::GetName(PRUnichar ** aName)
{
    NS_ENSURE_ARG_POINTER(aName);
    *aName = ToNewUnicode(mName);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetName(const PRUnichar * aName)
{
    mName = aName;              // this does a copy of aName
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::NameEquals(const PRUnichar *aName, PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(aName);
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = mName.Equals(aName);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetItemType(PRInt32 * aItemType)
{
    NS_ENSURE_ARG_POINTER(aItemType);

    *aItemType = mItemType;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetItemType(PRInt32 aItemType)
{
    NS_ENSURE_ARG((aItemType == typeChrome) || (typeContent == aItemType));
    NS_ENSURE_STATE(!mParent);

    mItemType = aItemType;

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetParent(nsIDocShellTreeItem ** aParent)
{
    NS_ENSURE_ARG_POINTER(aParent);

    *aParent = mParent;
    NS_IF_ADDREF(*aParent);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetParent(nsIDocShellTreeItem * aParent)
{
    // null aParent is ok
    /*
       Note this doesn't do an addref on purpose.  This is because the parent
       is an implied lifetime.  We don't want to create a cycle by refcounting
       the parent.
     */
    mParent = aParent;

    nsCOMPtr<nsIURIContentListener>
        parentURIListener(do_GetInterface(aParent));
    if (parentURIListener)
        SetParentURIContentListener(parentURIListener);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSameTypeParent(nsIDocShellTreeItem ** aParent)
{
    NS_ENSURE_ARG_POINTER(aParent);
    *aParent = nsnull;

    if (!mParent)
        return NS_OK;

    PRInt32 parentType;
    NS_ENSURE_SUCCESS(mParent->GetItemType(&parentType), NS_ERROR_FAILURE);

    if (parentType == mItemType) {
        *aParent = mParent;
        NS_ADDREF(*aParent);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetRootTreeItem(nsIDocShellTreeItem ** aRootTreeItem)
{
    NS_ENSURE_ARG_POINTER(aRootTreeItem);
    *aRootTreeItem = NS_STATIC_CAST(nsIDocShellTreeItem *, this);

    nsCOMPtr<nsIDocShellTreeItem> parent;
    NS_ENSURE_SUCCESS(GetParent(getter_AddRefs(parent)), NS_ERROR_FAILURE);
    while (parent) {
        *aRootTreeItem = parent;
        NS_ENSURE_SUCCESS((*aRootTreeItem)->GetParent(getter_AddRefs(parent)),
                          NS_ERROR_FAILURE);
    }
    NS_ADDREF(*aRootTreeItem);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSameTypeRootTreeItem(nsIDocShellTreeItem ** aRootTreeItem)
{
    NS_ENSURE_ARG_POINTER(aRootTreeItem);
    *aRootTreeItem = NS_STATIC_CAST(nsIDocShellTreeItem *, this);

    nsCOMPtr<nsIDocShellTreeItem> parent;
    NS_ENSURE_SUCCESS(GetSameTypeParent(getter_AddRefs(parent)),
                      NS_ERROR_FAILURE);
    while (parent) {
        *aRootTreeItem = parent;
        NS_ENSURE_SUCCESS((*aRootTreeItem)->
                          GetSameTypeParent(getter_AddRefs(parent)),
                          NS_ERROR_FAILURE);
    }
    NS_ADDREF(*aRootTreeItem);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::FindItemWithName(const PRUnichar * aName,
                             nsISupports * aRequestor,
                             nsIDocShellTreeItem ** _retval)
{
    NS_ENSURE_ARG(aName);
    NS_ENSURE_ARG_POINTER(_retval);

    *_retval = nsnull;          // if we don't find one, we return NS_OK and a null result 

    // This QI may fail, but the places where we want to compare, comparing
    // against nsnull serves the same purpose.
    nsCOMPtr<nsIDocShellTreeItem>
        reqAsTreeItem(do_QueryInterface(aRequestor));

    // First we check our name.
    if (mName.EqualsWithConversion(aName)) {
        *_retval = NS_STATIC_CAST(nsIDocShellTreeItem *, this);
        NS_ADDREF(*_retval);
        return NS_OK;
    }

    // Second we check our children making sure not to ask a child if it
    // is the aRequestor.
    NS_ENSURE_SUCCESS(FindChildWithName(aName, PR_TRUE, PR_TRUE, reqAsTreeItem,
                                        _retval), NS_ERROR_FAILURE);
    if (*_retval)
        return NS_OK;

    // Third if we have a parent and it isn't the requestor then we should ask
    // it to do the search.  If it is the requestor we should just stop here
    // and let the parent do the rest.
    // If we don't have a parent, then we should ask the docShellTreeOwner to do
    // the search.
    if (mParent) {
        if (mParent == reqAsTreeItem.get())
            return NS_OK;

        PRInt32 parentType;
        mParent->GetItemType(&parentType);
        if (parentType == mItemType) {
            NS_ENSURE_SUCCESS(mParent->FindItemWithName(aName,
                                                        NS_STATIC_CAST
                                                        (nsIDocShellTreeItem *,
                                                         this), _retval),
                              NS_ERROR_FAILURE);
            return NS_OK;
        }
        // If the parent isn't of the same type fall through and ask tree owner.
    }

    // This QI may fail, but comparing against null serves the same purpose
    nsCOMPtr<nsIDocShellTreeOwner>
        reqAsTreeOwner(do_QueryInterface(aRequestor));

    if (mTreeOwner && (mTreeOwner != reqAsTreeOwner.get())) {
        NS_ENSURE_SUCCESS(mTreeOwner->FindItemWithName(aName,
                                                       NS_STATIC_CAST
                                                       (nsIDocShellTreeItem *,
                                                        this), _retval),
                          NS_ERROR_FAILURE);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetTreeOwner(nsIDocShellTreeOwner ** aTreeOwner)
{
    NS_ENSURE_ARG_POINTER(aTreeOwner);

    *aTreeOwner = mTreeOwner;
    NS_IF_ADDREF(*aTreeOwner);
    return NS_OK;
}

#ifdef DEBUG_DOCSHELL_FOCUS
static void 
PrintDocTree(nsIDocShellTreeNode * aParentNode, int aLevel)
{
  for (PRInt32 i=0;i<aLevel;i++) printf("  ");

  PRInt32 childWebshellCount;
  aParentNode->GetChildCount(&childWebshellCount);
  nsCOMPtr<nsIDocShell> parentAsDocShell(do_QueryInterface(aParentNode));
  nsCOMPtr<nsIDocShellTreeItem> parentAsItem(do_QueryInterface(aParentNode));
  PRInt32 type;
  parentAsItem->GetItemType(&type);
  nsCOMPtr<nsIPresShell> presShell;
  parentAsDocShell->GetPresShell(getter_AddRefs(presShell));
  nsCOMPtr<nsIPresContext> presContext;
  parentAsDocShell->GetPresContext(getter_AddRefs(presContext));
  nsCOMPtr<nsIDocument> doc;
  presShell->GetDocument(getter_AddRefs(doc));

  nsCOMPtr<nsIScriptGlobalObject> sgo;
  doc->GetScriptGlobalObject(getter_AddRefs(sgo));
  nsCOMPtr<nsIDOMWindowInternal> domwin(do_QueryInterface(sgo));

  nsCOMPtr<nsIWidget> widget;
  nsCOMPtr<nsIViewManager> vm;
  presShell->GetViewManager(getter_AddRefs(vm));
  if (vm) {
    vm->GetWidget(getter_AddRefs(widget));
  }
  nsCOMPtr<nsIEventStateManager> esm;
  presContext->GetEventStateManager(getter_AddRefs(esm));
  nsCOMPtr<nsIContent> rootContent;
  doc->GetRootContent(getter_AddRefs(rootContent));

  printf("DS %p  Ty %s  Doc %p DW %p EM %p CN %p\n",  
    parentAsDocShell.get(), 
    type==nsIDocShellTreeItem::typeChrome?"Chr":"Con", 
     doc.get(), domwin.get(), esm.get(), rootContent.get());

  if (childWebshellCount > 0) {
    for (PRInt32 i=0;i<childWebshellCount;i++) {
      nsCOMPtr<nsIDocShellTreeItem> child;
      aParentNode->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShellTreeNode> childAsNode(do_QueryInterface(child));
      PrintDocTree(childAsNode, aLevel+1);
    }
  }
}

static void 
PrintDocTree(nsIDocShellTreeNode * aParentNode)
{
  NS_ASSERTION(aParentNode, "Pointer is null!");

  nsCOMPtr<nsIDocShellTreeItem> item(do_QueryInterface(aParentNode));
  nsCOMPtr<nsIDocShellTreeItem> parentItem;
  item->GetParent(getter_AddRefs(parentItem));
  while (parentItem) {
    nsCOMPtr<nsIDocShellTreeItem>tmp;
    parentItem->GetParent(getter_AddRefs(tmp));
    if (!tmp) {
      break;
    }
    parentItem = tmp;
  }

  if (!parentItem) {
    parentItem = do_QueryInterface(aParentNode);
  }

  if (parentItem) {
    nsCOMPtr<nsIDocShellTreeNode> parentAsNode(do_QueryInterface(parentItem));
    PrintDocTree(parentAsNode, 0);
  }
}
#endif

NS_IMETHODIMP
nsDocShell::SetTreeOwner(nsIDocShellTreeOwner * aTreeOwner)
{
#ifdef DEBUG_DOCSHELL_FOCUS
    nsCOMPtr<nsIDocShellTreeNode> node(do_QueryInterface(aTreeOwner));
    if (node) {
      PrintDocTree(node);
    }
#endif

    // Don't automatically set the progress based on the tree owner for frames
    if (!IsFrame()) {
        nsCOMPtr<nsIWebProgress> webProgress(do_QueryInterface(mLoadCookie));

        if (webProgress) {
            nsCOMPtr<nsIWebProgressListener>
                oldListener(do_QueryInterface(mTreeOwner));
            nsCOMPtr<nsIWebProgressListener>
                newListener(do_QueryInterface(aTreeOwner));

            if (oldListener) {
                webProgress->RemoveProgressListener(oldListener);
            }

            if (newListener) {
                webProgress->AddProgressListener(newListener);
            }
        }
    }

    mTreeOwner = aTreeOwner;    // Weak reference per API

    PRInt32 i, n = mChildren.Count();
    for (i = 0; i < n; i++) {
        nsIDocShellTreeItem *child = (nsIDocShellTreeItem *) mChildren.ElementAt(i);    // doesn't addref the result
        NS_ENSURE_TRUE(child, NS_ERROR_FAILURE);
        PRInt32 childType = ~mItemType; // Set it to not us in case the get fails
        child->GetItemType(&childType); // We don't care if this fails, if it does we won't set the owner
        if (childType == mItemType)
            child->SetTreeOwner(aTreeOwner);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetChildOffset(PRInt32 aChildOffset)
{
    mChildOffset = aChildOffset;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetChildOffset(PRInt32 * aChildOffset)
{
    NS_ENSURE_ARG_POINTER(aChildOffset);
    *aChildOffset = mChildOffset;
    return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIDocShellTreeNode
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::GetChildCount(PRInt32 * aChildCount)
{
    NS_ENSURE_ARG_POINTER(aChildCount);
    *aChildCount = mChildren.Count();
    return NS_OK;
}



NS_IMETHODIMP
nsDocShell::AddChild(nsIDocShellTreeItem * aChild)
{
    NS_ENSURE_ARG_POINTER(aChild);

    NS_ENSURE_SUCCESS(aChild->SetParent(this), NS_ERROR_FAILURE);
    mChildren.AppendElement(aChild);
    NS_ADDREF(aChild);

    // Set the child's index in the parent's children list 
    // XXX What if the parent had different types of children?
    // XXX in that case docshell hierarchyand SH hierarchy won't match.
    PRInt32 childCount = mChildren.Count();
    aChild->SetChildOffset(childCount - 1);

    /* Set the child's global history if the parent has one */
    if (mGlobalHistory) {
        nsCOMPtr<nsIDocShellHistory>
            dsHistoryChild(do_QueryInterface(aChild));
        if (dsHistoryChild)
            dsHistoryChild->SetGlobalHistory(mGlobalHistory);
    }


    PRInt32 childType = ~mItemType;     // Set it to not us in case the get fails
    aChild->GetItemType(&childType);
    if (childType != mItemType)
        return NS_OK;
    // Everything below here is only done when the child is the same type.


    aChild->SetTreeOwner(mTreeOwner);

    nsCOMPtr<nsIDocShell> childAsDocShell(do_QueryInterface(aChild));
    if (!childAsDocShell)
        return NS_OK;

    // Do some docShell Specific stuff.
    nsXPIDLString defaultCharset;
    nsXPIDLString forceCharset;
    NS_ENSURE_TRUE(mContentViewer, NS_ERROR_FAILURE);

    nsCOMPtr<nsIMarkupDocumentViewer> muDV =
        do_QueryInterface(mContentViewer);
    if (muDV) {
        NS_ENSURE_SUCCESS(muDV->
                          GetDefaultCharacterSet(getter_Copies(defaultCharset)),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(muDV->
                          GetForceCharacterSet(getter_Copies(forceCharset)),
                          NS_ERROR_FAILURE);
    }
    nsCOMPtr<nsIContentViewer> childCV;
    NS_ENSURE_SUCCESS(childAsDocShell->
                      GetContentViewer(getter_AddRefs(childCV)),
                      NS_ERROR_FAILURE);
    if (childCV) {
        nsCOMPtr<nsIMarkupDocumentViewer> childmuDV =
            do_QueryInterface(childCV);
        if (childmuDV) {
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
    if (mItemType == nsIDocShellTreeItem::typeChrome)
        return NS_OK;

    nsresult res = NS_OK;

    // get the child's docCSInfo object
    nsCOMPtr<nsIDocumentCharsetInfo> dcInfo = NULL;
    res = childAsDocShell->GetDocumentCharsetInfo(getter_AddRefs(dcInfo));
    if (NS_FAILED(res) || (!dcInfo))
        return NS_OK;

    // get the parent's current charset
    nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(mContentViewer));
    if (!docv)
        return NS_OK;
    nsCOMPtr<nsIDocument> doc;
    res = docv->GetDocument(*getter_AddRefs(doc));
    if (NS_FAILED(res) || (!doc))
        return NS_OK;
    nsAutoString parentCS;
    res = doc->GetDocumentCharacterSet(parentCS);
    if (NS_FAILED(res))
        return NS_OK;

    // set the child's parentCharset
    nsCOMPtr<nsIAtom> parentCSAtom(dont_AddRef(NS_NewAtom(parentCS)));
    res = dcInfo->SetParentCharset(parentCSAtom);
    if (NS_FAILED(res))
        return NS_OK;

    // printf("### 1 >>> Adding child. Parent CS = %s. ItemType = %d.\n", NS_LossyConvertUCS2toASCII(parentCS).get(), mItemType);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::RemoveChild(nsIDocShellTreeItem * aChild)
{
    NS_ENSURE_ARG_POINTER(aChild);

    if (mChildren.RemoveElement(aChild)) {
        aChild->SetParent(nsnull);
        aChild->SetTreeOwner(nsnull);
        NS_RELEASE(aChild);
    }
    else
        NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_INVALID_ARG);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetChildAt(PRInt32 aIndex, nsIDocShellTreeItem ** aChild)
{
    NS_ENSURE_ARG_POINTER(aChild);

    *aChild = (nsIDocShellTreeItem *) mChildren.ElementAt(aIndex);
    NS_IF_ADDREF(*aChild);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::FindChildWithName(const PRUnichar * aName,
                              PRBool aRecurse, PRBool aSameType,
                              nsIDocShellTreeItem * aRequestor,
                              nsIDocShellTreeItem ** _retval)
{
    NS_ENSURE_ARG(aName);
    NS_ENSURE_ARG_POINTER(_retval);

    *_retval = nsnull;          // if we don't find one, we return NS_OK and a null result 

    nsXPIDLString childName;
    PRInt32 i, n = mChildren.Count();
    for (i = 0; i < n; i++) {
        nsIDocShellTreeItem *child = (nsIDocShellTreeItem *) mChildren.ElementAt(i);    // doesn't addref the result
        NS_ENSURE_TRUE(child, NS_ERROR_FAILURE);
        PRInt32 childType;
        child->GetItemType(&childType);

        if (aSameType && (childType != mItemType))
            continue;

        PRBool childNameEquals = PR_FALSE;
        child->NameEquals(aName, &childNameEquals);
        if (childNameEquals) {
            *_retval = child;
            NS_ADDREF(*_retval);
            break;
        }

        if (childType != mItemType)     //Only ask it to check children if it is same type
            continue;

        if (aRecurse && (aRequestor != child))  // Only ask the child if it isn't the requestor
        {
            // See if child contains the shell with the given name
            nsCOMPtr<nsIDocShellTreeNode>
                childAsNode(do_QueryInterface(child));
            if (child) {
                NS_ENSURE_SUCCESS(childAsNode->FindChildWithName(aName, PR_TRUE,
                                                                 aSameType,
                                                                 NS_STATIC_CAST
                                                                 (nsIDocShellTreeItem
                                                                  *, this),
                                                                 _retval),
                                  NS_ERROR_FAILURE);
            }
        }
        if (*_retval)           // found it
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

    
    // A nsISHEntry for a child is *only* available when the parent is in
    // the progress of loading a document too...
    
    if (mLSHE) {
        /* Before looking for the subframe's url, check
         * the expiration status of the parent. If the parent
         * has expired from cache, then subframes will not be 
         * loaded from history in certain situations.  
         */
        PRBool parentExpired=PR_FALSE;
        mLSHE->GetExpirationStatus(&parentExpired);
        
        /* Get the parent's Load Type so that it can be set on the child too.
         * By default give a loadHistory value
         */
        PRUint32 loadType = nsIDocShellLoadInfo::loadHistory;
        mLSHE->GetLoadType(&loadType);  
        // If the user did a shift-reload on this frameset page, 
        // we don't want to load the subframes from history.         
        if (loadType == nsIDocShellLoadInfo::loadReloadBypassCache ||
            loadType == nsIDocShellLoadInfo::loadReloadBypassProxy ||
            loadType == nsIDocShellLoadInfo::loadReloadBypassProxyAndCache)
            return rv;
        
        /* If the user pressed reload and the parent frame has expired
         *  from cache, we do not want to load the child frame from history.
         */
        if (parentExpired && (loadType == nsIDocShellLoadInfo::loadReloadNormal)) {
            // The parent has expired. Return null.
            *aResult = nsnull;
            return rv;
        }

        nsCOMPtr<nsISHContainer> container(do_QueryInterface(mLSHE));
        if (container) {
            // Get the child subframe from session history.
            rv = container->GetChildAt(aChildOffset, aResult);            
            if (*aResult) 
                (*aResult)->SetLoadType(loadType);            
        }
    }
    return rv;
}

NS_IMETHODIMP
nsDocShell::AddChildSHEntry(nsISHEntry * aCloneRef, nsISHEntry * aNewEntry,
                            PRInt32 aChildOffset)
{
    nsresult rv;

    if (mLSHE) {
        /* You get here if you are currently building a 
         * hierarchy ie.,you just visited a frameset page
         */
        nsCOMPtr<nsISHContainer> container(do_QueryInterface(mLSHE, &rv));
        if (container) {
            rv = container->AddChild(aNewEntry, aChildOffset);
        }
    }
    else if (mSessionHistory) {
        /* You are currently in the rootDocShell.
         * You will get here when a subframe has a new url
         * to load and you have walked up the tree all the 
         * way to the top to clone the current SHEntry hierarchy
         * and replace the subframe where a new url was loaded with
         * a new entry.
         */
        PRInt32 index = -1;
        nsCOMPtr<nsIHistoryEntry> currentHE;
        mSessionHistory->GetIndex(&index);
        if (index < 0)
            return NS_ERROR_FAILURE;

        rv = mSessionHistory->GetEntryAtIndex(index, PR_FALSE,
                                              getter_AddRefs(currentHE));
        NS_ENSURE_TRUE(currentHE, NS_ERROR_FAILURE);

        nsCOMPtr<nsISHEntry> currentEntry(do_QueryInterface(currentHE));
        if (currentEntry) {
            PRUint32 cloneID = 0;
            nsCOMPtr<nsISHEntry> nextEntry;  //(do_CreateInstance(NS_SHENTRY_CONTRACTID));
            //   NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
            if (aCloneRef)
                aCloneRef->GetID(&cloneID);
            rv = CloneAndReplace(currentEntry, cloneID, aNewEntry,
                                 getter_AddRefs(nextEntry));

            if (NS_SUCCEEDED(rv)) {
                nsCOMPtr<nsISHistoryInternal>
                    shPrivate(do_QueryInterface(mSessionHistory));
                NS_ENSURE_TRUE(shPrivate, NS_ERROR_FAILURE);
                rv = shPrivate->AddEntry(nextEntry, PR_TRUE);
            }
        }
    }
    else {
        /* You will get here when you are in a subframe and
         * a new url has been loaded on you. 
         * The mOSHE in this subframe will be the previous url's
         * mOSHE. This mOSHE will be used as the identification
         * for this subframe in the  CloneAndReplace function.
         */

        nsCOMPtr<nsIDocShellHistory> parent(do_QueryInterface(mParent, &rv));
        if (parent) {
            if (!aCloneRef) {
                aCloneRef = mOSHE;
            }
            rv = parent->AddChildSHEntry(aCloneRef, aNewEntry, aChildOffset);
        }

    }
    return rv;
}

NS_IMETHODIMP
nsDocShell::SetGlobalHistory(nsIGlobalHistory * aGlobalHistory)
{
    mGlobalHistory = aGlobalHistory;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetGlobalHistory(nsIGlobalHistory ** aGlobalHistory)
{
    NS_ENSURE_ARG_POINTER(aGlobalHistory);

    *aGlobalHistory = mGlobalHistory;
    NS_IF_ADDREF(*aGlobalHistory);
    return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIWebNavigation
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::GetCanGoBack(PRBool * aCanGoBack)
{
    nsCOMPtr<nsIDocShellTreeItem> root;
    //Get the root docshell
    GetSameTypeRootTreeItem(getter_AddRefs(root));
    NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);
    // QI to nsIWebNavigation
    nsCOMPtr<nsIWebNavigation> rootAsWebnav(do_QueryInterface(root));
    if (rootAsWebnav) {
        // Get the handle to SH from the root docshell
        nsCOMPtr<nsISHistory> rootSH;
        rootAsWebnav->GetSessionHistory(getter_AddRefs(rootSH));
        NS_ENSURE_TRUE(rootSH, NS_ERROR_FAILURE);
        // QI SH to nsIWebNavigation
        nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(rootSH));
        if (webNav)
            return webNav->GetCanGoBack(aCanGoBack);
    }
    return NS_ERROR_FAILURE;

}

NS_IMETHODIMP
nsDocShell::GetCanGoForward(PRBool * aCanGoForward)
{
    nsCOMPtr<nsIDocShellTreeItem> root;
    //Get the root docshell
    GetSameTypeRootTreeItem(getter_AddRefs(root));
    NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);
    // QI to nsIWebNavigation
    nsCOMPtr<nsIWebNavigation> rootAsWebnav(do_QueryInterface(root));
    if (rootAsWebnav) {
        // Get the handle to SH from the root docshell
        nsCOMPtr<nsISHistory> rootSH;
        rootAsWebnav->GetSessionHistory(getter_AddRefs(rootSH));
        NS_ENSURE_TRUE(rootSH, NS_ERROR_FAILURE);
        // QI SH to nsIWebNavigation
        nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(rootSH));
        if (webNav)
            return webNav->GetCanGoForward(aCanGoForward);
    }
    return NS_ERROR_FAILURE;

}

NS_IMETHODIMP
nsDocShell::GoBack()
{
    nsCOMPtr<nsIDocShellTreeItem> root;
    //Get the root docshell
    GetSameTypeRootTreeItem(getter_AddRefs(root));
    NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);
    // QI to nsIWebNavigation
    nsCOMPtr<nsIWebNavigation> rootAsWebnav(do_QueryInterface(root));
    if (rootAsWebnav) {
        // Get the handle to SH from the root docshell
        nsCOMPtr<nsISHistory> rootSH;
        rootAsWebnav->GetSessionHistory(getter_AddRefs(rootSH));
        NS_ENSURE_TRUE(rootSH, NS_ERROR_FAILURE);
        // QI SH to nsIWebNavigation
        nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(rootSH));
        if (webNav)
            return webNav->GoBack();
    }
    return NS_ERROR_FAILURE;

}

NS_IMETHODIMP
nsDocShell::GoForward()
{
    nsCOMPtr<nsIDocShellTreeItem> root;
    //Get the root docshell
    GetSameTypeRootTreeItem(getter_AddRefs(root));
    NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);
    // QI to nsIWebNavigation
    nsCOMPtr<nsIWebNavigation> rootAsWebnav(do_QueryInterface(root));
    if (rootAsWebnav) {
        // Get the handle to SH from the root docshell
        nsCOMPtr<nsISHistory> rootSH;
        rootAsWebnav->GetSessionHistory(getter_AddRefs(rootSH));
        NS_ENSURE_TRUE(rootSH, NS_ERROR_FAILURE);
        // QI SH to nsIWebNavigation
        nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(rootSH));
        if (webNav)
            return webNav->GoForward();
    }
    return NS_ERROR_FAILURE;

}

NS_IMETHODIMP nsDocShell::GotoIndex(PRInt32 aIndex)
{
    nsCOMPtr<nsIDocShellTreeItem> root;
    //Get the root docshell
    GetSameTypeRootTreeItem(getter_AddRefs(root));
    NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);
    // QI to nsIWebNavigation
    nsCOMPtr<nsIWebNavigation> rootAsWebnav(do_QueryInterface(root));
    if (rootAsWebnav) {
        // Get the handle to SH from the root docshell
        nsCOMPtr<nsISHistory> rootSH;
        rootAsWebnav->GetSessionHistory(getter_AddRefs(rootSH));
        NS_ENSURE_TRUE(rootSH, NS_ERROR_FAILURE);
        // QI SH to nsIWebNavigation
        nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(rootSH));
        if (webNav)
            return webNav->GotoIndex(aIndex);
    }
    return NS_ERROR_FAILURE;

}


NS_IMETHODIMP
nsDocShell::LoadURI(const PRUnichar * aURI, PRUint32 aLoadFlags)
{
    nsCOMPtr<nsIURI> uri;

    nsresult rv = CreateFixupURI(aURI, getter_AddRefs(uri));

    if (NS_ERROR_UNKNOWN_PROTOCOL == rv) {
        // we weren't able to find a protocol handler
        nsCOMPtr<nsIPrompt> prompter;
        nsCOMPtr<nsIStringBundle> stringBundle;
        GetPromptAndStringBundle(getter_AddRefs(prompter),
                                 getter_AddRefs(stringBundle));

        NS_ENSURE_TRUE(stringBundle, NS_ERROR_FAILURE);

        nsXPIDLString messageStr;
        NS_ENSURE_SUCCESS(stringBundle->
                          GetStringFromName(NS_ConvertASCIItoUCS2
                                            ("protocolNotFound").get(),
                                            getter_Copies(messageStr)),
                          NS_ERROR_FAILURE);

        nsAutoString uriString(aURI);
        PRInt32 colon = uriString.FindChar(':');
        // extract the scheme
        nsAutoString scheme;
        uriString.Left(scheme, colon);
        nsCAutoString cScheme;
        cScheme.AssignWithConversion(scheme);

        PRUnichar *msg = nsTextFormatter::smprintf(messageStr, cScheme.get());
        if (!msg)
            return NS_ERROR_OUT_OF_MEMORY;

        prompter->Alert(nsnull, msg);
        nsTextFormatter::smprintf_free(msg);
    }                           // end unknown protocol

    if (!uri)
        return NS_ERROR_FAILURE;

    NS_ENSURE_SUCCESS(LoadURI(uri, nsnull, aLoadFlags), NS_ERROR_FAILURE);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::Reload(PRUint32 aReloadFlags)
{
    NS_ASSERTION(((aReloadFlags & 0xf) == 0),
                 "Reload command not updated to use load flags!");

    // XXXTAB Convert reload type to our type
    LoadType type = LOAD_RELOAD_NORMAL;
    if (aReloadFlags & LOAD_FLAGS_BYPASS_CACHE &&
        aReloadFlags & LOAD_FLAGS_BYPASS_PROXY)
        type = LOAD_RELOAD_BYPASS_PROXY_AND_CACHE;
    else if (aReloadFlags & LOAD_FLAGS_CHARSET_CHANGE)
        type = LOAD_RELOAD_CHARSET_CHANGE;

    nsresult rv;
    /* If you change this part of code, make sure bug 45297 does not re-occur */
    if (mOSHE)
        rv = LoadHistoryEntry(mOSHE, type);
    else if (mLSHE)              // In case a reload happened before the current load is done
        rv = LoadHistoryEntry(mLSHE, type);
    else
        rv = InternalLoad(mCurrentURI,
                          mReferrerURI,
                          nsnull,         // No owner
                          PR_TRUE,        // Inherit owner from document
                          nsnull,         // No window target
                          nsnull,         // No post data
                          nsnull,         // No headers data
                          type,           // Load type
                          nsnull);        // No SHEntry
    return rv;
}

NS_IMETHODIMP
nsDocShell::Stop(PRUint32 aStopFlags)
{
    if (nsIWebNavigation::STOP_CONTENT & aStopFlags) {
        if (mContentViewer)
            mContentViewer->Stop();

    }

    if (nsIWebNavigation::STOP_NETWORK & aStopFlags) {
        // Cancel any timers that were set for this loader.
        CancelRefreshURITimers();

        if (mLoadCookie) {
            nsCOMPtr<nsIURILoader> uriLoader;

            uriLoader = do_GetService(NS_URI_LOADER_CONTRACTID);
            if (uriLoader)
                uriLoader->Stop(mLoadCookie);
        }
    }

    PRInt32 n;
    PRInt32 count = mChildren.Count();
    for (n = 0; n < count; n++) {
        nsIDocShellTreeItem *shell =
            (nsIDocShellTreeItem *) mChildren.ElementAt(n);
        nsCOMPtr<nsIWebNavigation> shellAsNav(do_QueryInterface(shell));
        if (shellAsNav)
            shellAsNav->Stop(aStopFlags);
    }

    return NS_OK;
}

/*
NS_IMETHODIMP nsDocShell::SetDocument(nsIDOMDocument* aDocument,
   const PRUnichar* aContentType)
{
   //XXX First Checkin
   NS_ERROR("Not Yet Implemented");
   return NS_ERROR_FAILURE;
}
*/

NS_IMETHODIMP
nsDocShell::GetDocument(nsIDOMDocument ** aDocument)
{
    NS_ENSURE_ARG_POINTER(aDocument);
    NS_ENSURE_STATE(mContentViewer);

    return mContentViewer->GetDOMDocument(aDocument);
}

NS_IMETHODIMP
nsDocShell::GetCurrentURI(nsIURI ** aURI)
{
    NS_ENSURE_ARG_POINTER(aURI);

    *aURI = mCurrentURI;
    NS_IF_ADDREF(*aURI);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetSessionHistory(nsISHistory * aSessionHistory)
{

    NS_ENSURE_TRUE(aSessionHistory, NS_ERROR_FAILURE);
    // make sure that we are the root docshell and
    // set a handle to root docshell in SH.

    nsCOMPtr<nsIDocShellTreeItem> root;
    /* Get the root docshell. If *this* is the root docshell
     * then save a handle to *this* in SH. SH needs it to do
     * traversions thro' its entries
     */
    GetSameTypeRootTreeItem(getter_AddRefs(root));
    NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);
    if (root.get() == NS_STATIC_CAST(nsIDocShellTreeItem *, this)) {
        mSessionHistory = aSessionHistory;
        nsCOMPtr<nsISHistoryInternal>
            shPrivate(do_QueryInterface(mSessionHistory));
        NS_ENSURE_TRUE(shPrivate, NS_ERROR_FAILURE);
        shPrivate->SetRootDocShell(this);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;

}

NS_IMETHODIMP
nsDocShell::GetSessionHistory(nsISHistory ** aSessionHistory)
{
    NS_ENSURE_ARG_POINTER(aSessionHistory);
    if (mSessionHistory) {
        *aSessionHistory = mSessionHistory;
        NS_IF_ADDREF(*aSessionHistory);
        return NS_OK;
    }

    return NS_ERROR_FAILURE;

}

//*****************************************************************************
// nsDocShell::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::InitWindow(nativeWindow parentNativeWindow,
                       nsIWidget * parentWidget, PRInt32 x, PRInt32 y,
                       PRInt32 cx, PRInt32 cy)
{
    NS_ENSURE_ARG(parentWidget);        // DocShells must get a widget for a parent

    SetParentWidget(parentWidget);
    SetPositionAndSize(x, y, cx, cy, PR_FALSE);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::Create()
{
    NS_ENSURE_STATE(!mContentViewer);
    mPrefs = do_GetService(NS_PREF_CONTRACTID);
    //GlobalHistory is now set in SetGlobalHistory
    //  mGlobalHistory = do_GetService(NS_GLOBALHISTORY_CONTRACTID);

    // i don't want to read this pref in every time we load a url
    // so read it in once here and be done with it...
    mPrefs->GetBoolPref("network.protocols.useSystemDefaults",
                        &mUseExternalProtocolHandler);
    mPrefs->GetBoolPref("browser.target_new_blocked", &mDisallowPopupWindows);
    mPrefs->GetBoolPref("browser.frames.enabled", &mAllowSubframes);

    // Check pref to see if we should prevent frameset spoofing
    mPrefs->GetBoolPref("browser.frame.validate_origin", &mValidateOrigin);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::Destroy()
{
    //Fire unload event before we blow anything away.
    (void) FireUnloadNotification();

    mIsBeingDestroyed = PR_TRUE;

    // Stop any URLs that are currently being loaded...
    Stop(nsIWebNavigation::STOP_ALL);
    if (mDocLoader) {
        mDocLoader->Destroy();
        mDocLoader->SetContainer(nsnull);
    }

    // Save the state of the current document, before destroying the window.
    // This is needed to capture the state of a frameset when the new document
    // causes the frameset to be destroyed...
    PersistLayoutHistoryState();

    // Remove this docshell from its parent's child list
    nsCOMPtr<nsIDocShellTreeNode>
        docShellParentAsNode(do_QueryInterface(mParent));
    if (docShellParentAsNode)
        docShellParentAsNode->RemoveChild(this);

    if (mContentViewer) {
        mContentViewer->Close();
        mContentViewer->Destroy();
        mContentViewer = nsnull;
    }

    DestroyChildren();

    mDocLoader = nsnull;
    mParentWidget = nsnull;
    mPrefs = nsnull;
    mCurrentURI = nsnull;

    if (mScriptGlobal) {
        mScriptGlobal->SetDocShell(nsnull);
        mScriptGlobal->SetGlobalObjectOwner(nsnull);
        mScriptGlobal = nsnull;
    }
    if (mScriptContext) {
        mScriptContext->SetOwner(nsnull);
        mScriptContext = nsnull;
    }

    mSessionHistory = nsnull;
    SetTreeOwner(nsnull);

    SetLoadCookie(nsnull);

    if (mInitInfo) {
        delete mInitInfo;
        mInitInfo = nsnull;
    }

    if (mContentListener) {
        mContentListener->DocShell(nsnull);
        NS_RELEASE(mContentListener);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetPosition(PRInt32 x, PRInt32 y)
{
    if (mContentViewer)
        NS_ENSURE_SUCCESS(mContentViewer->Move(x, y), NS_ERROR_FAILURE);
    else if (InitInfo()) {
        mInitInfo->x = x;
        mInitInfo->y = y;
    }
    else
        NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_FAILURE);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetPosition(PRInt32 * aX, PRInt32 * aY)
{
    PRInt32 dummyHolder;
    return GetPositionAndSize(aX, aY, &dummyHolder, &dummyHolder);
}

NS_IMETHODIMP
nsDocShell::SetSize(PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
    PRInt32 x = 0, y = 0;
    GetPosition(&x, &y);
    return SetPositionAndSize(x, y, aCX, aCY, aRepaint);
}

NS_IMETHODIMP
nsDocShell::GetSize(PRInt32 * aCX, PRInt32 * aCY)
{
    PRInt32 dummyHolder;
    return GetPositionAndSize(&dummyHolder, &dummyHolder, aCX, aCY);
}

NS_IMETHODIMP
nsDocShell::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
                               PRInt32 cy, PRBool fRepaint)
{
    if (mContentViewer) {
        //XXX Border figured in here or is that handled elsewhere?
        nsRect bounds(x, y, cx, cy);

        NS_ENSURE_SUCCESS(mContentViewer->SetBounds(bounds), NS_ERROR_FAILURE);
    }
    else if (InitInfo()) {
        mInitInfo->x = x;
        mInitInfo->y = y;
        mInitInfo->cx = cx;
        mInitInfo->cy = cy;
    }
    else
        NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_FAILURE);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetPositionAndSize(PRInt32 * x, PRInt32 * y, PRInt32 * cx,
                               PRInt32 * cy)
{
    if (mContentViewer) {
        nsRect bounds;

        NS_ENSURE_SUCCESS(mContentViewer->GetBounds(bounds), NS_ERROR_FAILURE);

        if (x)
            *x = bounds.x;
        if (y)
            *y = bounds.y;
        if (cx)
            *cx = bounds.width;
        if (cy)
            *cy = bounds.height;
    }
    else if (InitInfo()) {
        if (x)
            *x = mInitInfo->x;
        if (y)
            *y = mInitInfo->y;
        if (cx)
            *cx = mInitInfo->cx;
        if (cy)
            *cy = mInitInfo->cy;
    }
    else
        NS_ENSURE_TRUE(PR_FALSE, NS_ERROR_FAILURE);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::Repaint(PRBool aForce)
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

NS_IMETHODIMP
nsDocShell::GetParentWidget(nsIWidget ** parentWidget)
{
    NS_ENSURE_ARG_POINTER(parentWidget);

    *parentWidget = mParentWidget;
    NS_IF_ADDREF(*parentWidget);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetParentWidget(nsIWidget * aParentWidget)
{
    NS_ENSURE_STATE(!mContentViewer);

    mParentWidget = aParentWidget;

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetParentNativeWindow(nativeWindow * parentNativeWindow)
{
    NS_ENSURE_ARG_POINTER(parentNativeWindow);

    if (mParentWidget)
        *parentNativeWindow = mParentWidget->GetNativeData(NS_NATIVE_WIDGET);
    else
        *parentNativeWindow = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetParentNativeWindow(nativeWindow parentNativeWindow)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShell::GetVisibility(PRBool * aVisibility)
{
    NS_ENSURE_ARG_POINTER(aVisibility);
    if (!mContentViewer) {
        *aVisibility = PR_FALSE;
        return NS_OK;
    }

    // get the pres shell
    nsCOMPtr<nsIPresShell> presShell;
    NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)),
                      NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    // get the view manager
    nsCOMPtr<nsIViewManager> vm;
    NS_ENSURE_SUCCESS(presShell->GetViewManager(getter_AddRefs(vm)),
                      NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

    // get the root view
    nsIView *rootView = nsnull; // views are not ref counted
    NS_ENSURE_SUCCESS(vm->GetRootView(rootView), NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(rootView, NS_ERROR_FAILURE);

    // convert the view's visibility attribute to a bool
    nsViewVisibility vis;
    NS_ENSURE_SUCCESS(rootView->GetVisibility(vis), NS_ERROR_FAILURE);
    *aVisibility = nsViewVisibility_kHide == vis ? PR_FALSE : PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetVisibility(PRBool aVisibility)
{
    if (!mContentViewer)
        return NS_OK;
    if (aVisibility) {
        NS_ENSURE_SUCCESS(EnsureContentViewer(), NS_ERROR_FAILURE);
        mContentViewer->Show();
    }
    else if (mContentViewer)
        mContentViewer->Hide();

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetMainWidget(nsIWidget ** aMainWidget)
{
    // We don't create our own widget, so simply return the parent one. 
    return GetParentWidget(aMainWidget);
}

NS_IMETHODIMP
nsDocShell::SetFocus()
{
#ifdef DEBUG_DOCSHELL_FOCUS
  printf("nsDocShell::SetFocus %p\n", (nsIDocShell*)this);
#endif

  // Tell itself (and the DocShellFocusController) who has focus
  // this way focus gets removed from the currently focused DocShell

  SetHasFocus(PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetTitle(PRUnichar ** aTitle)
{
    NS_ENSURE_ARG_POINTER(aTitle);

    *aTitle = ToNewUnicode(mTitle);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetTitle(const PRUnichar * aTitle)
{
    // Store local title
    mTitle = aTitle;

    nsCOMPtr<nsIDocShellTreeItem> parent;
    GetSameTypeParent(getter_AddRefs(parent));

    // When title is set on the top object it should then be passed to the 
    // tree owner.
    if (!parent) {
        nsCOMPtr<nsIBaseWindow>
            treeOwnerAsWin(do_QueryInterface(mTreeOwner));
        if (treeOwnerAsWin)
            treeOwnerAsWin->SetTitle(aTitle);
    }

    if (mGlobalHistory && mCurrentURI) {
        nsXPIDLCString url;
        mCurrentURI->GetSpec(getter_Copies(url));
        nsCOMPtr<nsIBrowserHistory> browserHistory =
            do_QueryInterface(mGlobalHistory);
        browserHistory->SetPageTitle(url, aTitle);
    }


    // Update SessionHistory with the document's title. If the
    // page was loaded from history or the page bypassed history,
    // there is no need to update the title. There is no need to
    // go to mSessionHistory to update the title. Setting it in mOSHE 
    // would suffice. 
    if (mOSHE && (mLoadType != LOAD_BYPASS_HISTORY) && (mLoadType != LOAD_HISTORY)) {        
        mOSHE->SetTitle(mTitle.get());    
    }


    return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIScrollable
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::GetCurScrollPos(PRInt32 scrollOrientation, PRInt32 * curPos)
{
    NS_ENSURE_ARG_POINTER(curPos);

    nsCOMPtr<nsIScrollableView> scrollView;
    NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)),
                      NS_ERROR_FAILURE);
    if (!scrollView) {
        return NS_ERROR_FAILURE;
    }

    nscoord x, y;
    NS_ENSURE_SUCCESS(scrollView->GetScrollPosition(x, y), NS_ERROR_FAILURE);

    switch (scrollOrientation) {
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

NS_IMETHODIMP
nsDocShell::SetCurScrollPos(PRInt32 scrollOrientation, PRInt32 curPos)
{
    nsCOMPtr<nsIScrollableView> scrollView;
    NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)),
                      NS_ERROR_FAILURE);
    if (!scrollView) {
        return NS_ERROR_FAILURE;
    }

    PRInt32 other;
    PRInt32 x;
    PRInt32 y;

    GetCurScrollPos(scrollOrientation, &other);

    switch (scrollOrientation) {
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
        x = 0;
        y = 0;                  // fix compiler warning, not actually executed
    }

    NS_ENSURE_SUCCESS(scrollView->ScrollTo(x, y, NS_VMREFRESH_IMMEDIATE),
                      NS_ERROR_FAILURE);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetCurScrollPosEx(PRInt32 curHorizontalPos, PRInt32 curVerticalPos)
{
    nsCOMPtr<nsIScrollableView> scrollView;
    NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)),
                      NS_ERROR_FAILURE);
    if (!scrollView) {
        return NS_ERROR_FAILURE;
    }

    NS_ENSURE_SUCCESS(scrollView->ScrollTo(curHorizontalPos, curVerticalPos,
                                           NS_VMREFRESH_IMMEDIATE),
                      NS_ERROR_FAILURE);
    return NS_OK;
}

// XXX This is wrong
NS_IMETHODIMP
nsDocShell::GetScrollRange(PRInt32 scrollOrientation,
                           PRInt32 * minPos, PRInt32 * maxPos)
{
    NS_ENSURE_ARG_POINTER(minPos && maxPos);

    nsCOMPtr<nsIScrollableView> scrollView;
    NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)),
                      NS_ERROR_FAILURE);
    if (!scrollView) {
        return NS_ERROR_FAILURE;
    }

    PRInt32 cx;
    PRInt32 cy;

    NS_ENSURE_SUCCESS(scrollView->GetContainerSize(&cx, &cy), NS_ERROR_FAILURE);
    *minPos = 0;

    switch (scrollOrientation) {
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

NS_IMETHODIMP
nsDocShell::SetScrollRange(PRInt32 scrollOrientation,
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

NS_IMETHODIMP
nsDocShell::SetScrollRangeEx(PRInt32 minHorizontalPos,
                             PRInt32 maxHorizontalPos, PRInt32 minVerticalPos,
                             PRInt32 maxVerticalPos)
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
NS_IMETHODIMP
nsDocShell::GetCurrentScrollbarPreferences(PRInt32 scrollOrientation,
                                           PRInt32 * scrollbarPref)
{
    NS_ENSURE_ARG_POINTER(scrollbarPref);
    switch (scrollOrientation) {
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
NS_IMETHODIMP
nsDocShell::GetDefaultScrollbarPreferences(PRInt32 scrollOrientation,
                                           PRInt32 * scrollbarPref)
{
    NS_ENSURE_ARG_POINTER(scrollbarPref);
    switch (scrollOrientation) {
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
NS_IMETHODIMP
nsDocShell::SetCurrentScrollbarPreferences(PRInt32 scrollOrientation,
                                           PRInt32 scrollbarPref)
{
    switch (scrollOrientation) {
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
NS_IMETHODIMP
nsDocShell::SetDefaultScrollbarPreferences(PRInt32 scrollOrientation,
                                           PRInt32 scrollbarPref)
{
    switch (scrollOrientation) {
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
NS_IMETHODIMP
nsDocShell::ResetScrollbarPreferences()
{
    mCurrentScrollbarPref = mDefaultScrollbarPref;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetScrollbarVisibility(PRBool * verticalVisible,
                                   PRBool * horizontalVisible)
{
    nsCOMPtr<nsIScrollableView> scrollView;
    NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)),
                      NS_ERROR_FAILURE);
    if (!scrollView) {
        return NS_ERROR_FAILURE;
    }

    PRBool vertVisible;
    PRBool horizVisible;

    NS_ENSURE_SUCCESS(scrollView->GetScrollbarVisibility(&vertVisible,
                                                         &horizVisible),
                      NS_ERROR_FAILURE);

    if (verticalVisible)
        *verticalVisible = vertVisible;
    if (horizontalVisible)
        *horizontalVisible = horizVisible;

    return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsITextScroll
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::ScrollByLines(PRInt32 numLines)
{
    nsCOMPtr<nsIScrollableView> scrollView;

    NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)),
                      NS_ERROR_FAILURE);
    if (!scrollView) {
        return NS_ERROR_FAILURE;
    }

    NS_ENSURE_SUCCESS(scrollView->ScrollByLines(0, numLines), NS_ERROR_FAILURE);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::ScrollByPages(PRInt32 numPages)
{
    nsCOMPtr<nsIScrollableView> scrollView;

    NS_ENSURE_SUCCESS(GetRootScrollableView(getter_AddRefs(scrollView)),
                      NS_ERROR_FAILURE);
    if (!scrollView) {
        return NS_ERROR_FAILURE;
    }

    NS_ENSURE_SUCCESS(scrollView->ScrollByPages(numPages), NS_ERROR_FAILURE);

    return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIScriptGlobalObjectOwner
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::GetScriptGlobalObject(nsIScriptGlobalObject ** aGlobal)
{
    if (mIsBeingDestroyed) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    NS_ENSURE_ARG_POINTER(aGlobal);
    NS_ENSURE_SUCCESS(EnsureScriptEnvironment(), NS_ERROR_FAILURE);

    *aGlobal = mScriptGlobal;
    NS_IF_ADDREF(*aGlobal);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::ReportScriptError(nsIScriptError * errorObject)
{
    nsresult rv;

    if (errorObject == nsnull)
        return NS_ERROR_NULL_POINTER;

    // Get the console service, where we're going to register the error.
    nsCOMPtr<nsIConsoleService> consoleService
        (do_GetService("@mozilla.org/consoleservice;1"));

    if (consoleService != nsnull) {
        rv = consoleService->LogMessage(errorObject);
        if (NS_SUCCEEDED(rv)) {
            return NS_OK;
        }
        else {
            return rv;
        }
    }
    else {
        return NS_ERROR_NOT_AVAILABLE;
    }
}

//*****************************************************************************
// nsDocShell::nsIRefreshURI
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::RefreshURI(nsIURI * aURI, PRInt32 aDelay, PRBool aRepeat,
                       PRBool aMetaRefresh)
{
    NS_ENSURE_ARG(aURI);

    nsRefreshTimer *refreshTimer = new nsRefreshTimer();
    NS_ENSURE_TRUE(refreshTimer, NS_ERROR_OUT_OF_MEMORY);

    nsCOMPtr<nsISupports> dataRef = refreshTimer;    // Get the ref count to 1

    refreshTimer->mDocShell = this;
    refreshTimer->mURI = aURI;
    refreshTimer->mDelay = aDelay;
    refreshTimer->mRepeat = aRepeat;
    refreshTimer->mMetaRefresh = aMetaRefresh;

    if (!mRefreshURIList) {
        NS_ENSURE_SUCCESS(NS_NewISupportsArray(getter_AddRefs(mRefreshURIList)),
                          NS_ERROR_FAILURE);
    }

    nsCOMPtr<nsITimer> timer = do_CreateInstance("@mozilla.org/timer;1");
    NS_ENSURE_TRUE(timer, NS_ERROR_FAILURE);

    mRefreshURIList->AppendElement(timer);      // owning timer ref
    timer->Init(refreshTimer, aDelay);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::CancelRefreshURITimers()
{
    if (!mRefreshURIList)
        return NS_OK;

    PRUint32 n;
    mRefreshURIList->Count(&n);

    while (n) {
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

NS_IMETHODIMP
nsDocShell::Embed(nsIContentViewer * aContentViewer,
                  const char *aCommand, nsISupports * aExtraInfo)
{
    // Save the LayoutHistoryState of the previous document, before
    // setting up new document
    PersistLayoutHistoryState();

    nsresult rv = SetupNewViewer(aContentViewer);

    // XXX What if SetupNewViewer fails?
    if (mLSHE)
      mOSHE = mLSHE;

    PRBool updateHistory = PR_TRUE;

    // Determine if this type of load should update history   
    switch (mLoadType) {
    case LOAD_RELOAD_CHARSET_CHANGE:  //don't perserve history in charset reload
    case LOAD_NORMAL_REPLACE:
    case LOAD_RELOAD_BYPASS_CACHE:
    case LOAD_RELOAD_BYPASS_PROXY:
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
        updateHistory = PR_FALSE;
        break;
    default:
        break;
    }

    if (mOSHE && updateHistory) {
        nsCOMPtr<nsILayoutHistoryState> layoutState;

        rv = mOSHE->GetLayoutHistoryState(getter_AddRefs(layoutState));
        if (layoutState) {
            // This is a SH load. That's why there is a LayoutHistoryState in mOSHE
            nsCOMPtr<nsIPresShell> presShell;
            rv = GetPresShell(getter_AddRefs(presShell));
            if (NS_SUCCEEDED(rv) && presShell) {
                rv = presShell->SetHistoryState(layoutState);
            }
        }
    }
    return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIWebProgressListener
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::OnProgressChange(nsIWebProgress * aProgress,
                             nsIRequest * aRequest,
                             PRInt32 aCurSelfProgress,
                             PRInt32 aMaxSelfProgress,
                             PRInt32 aCurTotalProgress,
                             PRInt32 aMaxTotalProgress)
{
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::OnStateChange(nsIWebProgress * aProgress, nsIRequest * aRequest,
                          PRInt32 aStateFlags, nsresult aStatus)
{
    // Update the busy cursor
    if ((~aStateFlags & (STATE_START | STATE_IS_NETWORK)) == 0) {
        // Page has begun to load
        mBusyFlags = BUSY_FLAGS_BUSY | BUSY_FLAGS_BEFORE_PAGE_LOAD;
        nsCOMPtr<nsIWidget> mainWidget;
        GetMainWidget(getter_AddRefs(mainWidget));
        if (mainWidget) {
            mainWidget->SetCursor(eCursor_spinning);
        }
    }
    else if ((~aStateFlags & (STATE_TRANSFERRING | STATE_IS_DOCUMENT)) == 0) {
        // Page is loading
        mBusyFlags = BUSY_FLAGS_BUSY | BUSY_FLAGS_PAGE_LOADING;
    }
    else if ((aStateFlags & STATE_STOP) && (aStateFlags & STATE_IS_NETWORK)) {
        // Page has finished loading
        mBusyFlags = BUSY_FLAGS_NONE;
        nsCOMPtr<nsIWidget> mainWidget;
        GetMainWidget(getter_AddRefs(mainWidget));
        if (mainWidget) {
            mainWidget->SetCursor(eCursor_standard);
        }
    }

    if ((~aStateFlags & (STATE_IS_DOCUMENT | STATE_STOP)) == 0) {
        nsCOMPtr<nsIWebProgress> webProgress(do_QueryInterface(mLoadCookie));

        // Is the document stop notification for this document?
        if (aProgress == webProgress.get()) {
            nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));

            EndPageLoad(aProgress, channel, aStatus);
        }
    }
    else if ((~aStateFlags & (STATE_IS_DOCUMENT | STATE_REDIRECTING)) == 0) {
        // XXX Is it enough if I check just for the above 2 flags for redirection 
        nsCOMPtr<nsIWebProgress> webProgress(do_QueryInterface(mLoadCookie));

        // Is the document stop notification for this document?
        if (aProgress == webProgress.get()) {
            nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
            if (channel) {
                // Get the uri from the channel
                nsCOMPtr<nsIURI> uri;
                channel->GetURI(getter_AddRefs(uri));
                // Add the original url to global History so that
                // visited url color changes happen.
                if (uri) {
                    // Update Global history if necessary...
                    PRBool updateHistory = PR_FALSE;
                    ShouldAddToGlobalHistory(uri, &updateHistory);
                    if (updateHistory) {
                        AddToGlobalHistory(uri);
                    }
                }               // uri
            }                   // channel
        }                       // aProgress
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::OnLocationChange(nsIWebProgress * aProgress,
                             nsIRequest * aRequest, nsIURI * aURI)
{
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::OnStatusChange(nsIWebProgress * aWebProgress,
                           nsIRequest * aRequest,
                           nsresult aStatus, const PRUnichar * aMessage)
{
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::OnSecurityChange(nsIWebProgress * aWebProgress,
                             nsIRequest * aRequest, PRInt32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


nsresult
nsDocShell::EndPageLoad(nsIWebProgress * aProgress,
                        nsIChannel * aChannel, nsresult aStatus)
{
    if (mLSHE) {
        mLSHE->SetLoadType(nsIDocShellLoadInfo::loadHistory);

        // Clear the mLSHE reference to indicate document loading is done one
        // way or another.
        mLSHE = nsnull;
    }

    //
    // one of many safeguards that prevent death and destruction if
    // someone is so very very rude as to bring this window down
    // during this load handler.
    //
    nsCOMPtr<nsIDocShell> kungFuDeathGrip(this);
    //
    // Notify the ContentViewer that the Document has finished loading...
    //
    // This will cause any OnLoad(...) handlers to fire, if it is a HTML
    // document...
    //
    if (!mEODForCurrentDocument && mContentViewer) {
        mContentViewer->LoadComplete(aStatus);

        mEODForCurrentDocument = PR_TRUE;
    }
    return NS_OK;
}


//*****************************************************************************
// nsDocShell: Content Viewer Management
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::EnsureContentViewer()
{
    if (mContentViewer)
        return NS_OK;

    return CreateAboutBlankContentViewer();
}

NS_IMETHODIMP
nsDocShell::EnsureDeviceContext()
{
    if (mDeviceContext)
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

NS_IMETHODIMP
nsDocShell::CreateAboutBlankContentViewer()
{
  return NS_ERROR_NOT_IMPLEMENTED; // for now. not quite working yet.

  nsCOMPtr<nsIDocument> blankDoc;
  nsCOMPtr<nsIContentViewer> viewer;

  // one helper factory, please
  nsCOMPtr<nsIDocumentLoaderFactory> docFactory(do_CreateInstance(NS_DOCUMENT_LOADER_FACTORY_CONTRACTID_PREFIX "view;1?type=text/html"));
  if (!docFactory)
    return NS_ERROR_FAILURE;

  // generate (about:blank) document to load
  docFactory->CreateBlankDocument(getter_AddRefs(blankDoc));
  if (!blankDoc)
    return NS_ERROR_FAILURE;

  // create a content viewer for us and the new document
  docFactory->CreateInstanceForDocument(NS_ISUPPORTS_CAST(nsIDocShell *, this),
                blankDoc, "view", getter_AddRefs(viewer));

  // hook 'em up
  if (viewer) {
    viewer->SetContainer(NS_STATIC_CAST(nsIContentViewerContainer *,this));
    nsCOMPtr<nsIDOMDocument> domdoc(do_QueryInterface(blankDoc));
    Embed(viewer, "", 0);
    viewer->SetDOMDocument(domdoc);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::CreateContentViewer(const char *aContentType,
                                nsIRequest * request,
                                nsIStreamListener ** aContentHandler)
{
    // Can we check the content type of the current content viewer
    // and reuse it without destroying it and re-creating it?

    nsCOMPtr<nsILoadGroup> loadGroup(do_GetInterface(mLoadCookie));
    NS_ENSURE_TRUE(loadGroup, NS_ERROR_FAILURE);

    // Instantiate the content viewer object
    nsCOMPtr<nsIContentViewer> viewer;
    nsresult rv = NewContentViewerObj(aContentType, request, loadGroup,
                                      aContentHandler, getter_AddRefs(viewer));

    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    // Notify the current document that it is about to be unloaded!!
    //
    // It is important to fire the unload() notification *before* any state
    // is changed within the DocShell - otherwise, javascript will get the
    // wrong information :-(
    //
    (void) FireUnloadNotification();

    // Set mFiredUnloadEvent = PR_FALSE so that the unload handler for the
    // *new* document will fire.
    mFiredUnloadEvent = PR_FALSE;

    // we've created a new document so go ahead and call OnLoadingSite
    mURIResultedInDocument = PR_TRUE;

    nsCOMPtr<nsIChannel> aOpenedChannel = do_QueryInterface(request);

    OnLoadingSite(aOpenedChannel);

    // let's try resetting the load group if we need to...
    nsCOMPtr<nsILoadGroup> currentLoadGroup;
    NS_ENSURE_SUCCESS(aOpenedChannel->
                      GetLoadGroup(getter_AddRefs(currentLoadGroup)),
                      NS_ERROR_FAILURE);

    if (currentLoadGroup.get() != loadGroup.get()) {
        nsLoadFlags loadFlags = 0;

        //Cancel any URIs that are currently loading...
        /// XXX: Need to do this eventually      Stop();
        //
        // Retarget the document to this loadgroup...
        //
        /* First attach the channel to the right loadgroup
         * and then remove from the old loadgroup. This 
         * puts the notifications in the right order and
         * we don't null-out mLSHE in OnStateChange() for 
         * all redirected urls
         */
        aOpenedChannel->SetLoadGroup(loadGroup);

        // Mark the channel as being a document URI...
        aOpenedChannel->GetLoadFlags(&loadFlags);
        loadFlags |= nsIChannel::LOAD_DOCUMENT_URI;

        aOpenedChannel->SetLoadFlags(loadFlags);

        loadGroup->AddRequest(request, nsnull);
        if (currentLoadGroup)
            currentLoadGroup->RemoveRequest(request, nsnull, NS_OK);

        // Update the notification callbacks, so that progress and
        // status information are sent to the right docshell...
        aOpenedChannel->SetNotificationCallbacks(this);
    }

    NS_ENSURE_SUCCESS(Embed(viewer, "", (nsISupports *) nsnull),
                      NS_ERROR_FAILURE);

    mEODForCurrentDocument = PR_FALSE;

    return NS_OK;
}

nsresult
nsDocShell::NewContentViewerObj(const char *aContentType,
                                nsIRequest * request, nsILoadGroup * aLoadGroup,
                                nsIStreamListener ** aContentHandler,
                                nsIContentViewer ** aViewer)
{
    //XXX This should probably be some category thing....
    char id[256];
    PR_snprintf(id, sizeof(id),
                NS_DOCUMENT_LOADER_FACTORY_CONTRACTID_PREFIX "%s;1?type=%s",
                (const char *) "view", aContentType);

    // Note that we're always passing in "view" for the component id above
    // and to the docLoaderFactory->CreateInstance() at the end of this method. 
    // nsLayoutDLF makes the determination if it should be a "view-source"

    // Create an instance of the document-loader-factory
    nsCOMPtr<nsIDocumentLoaderFactory>
        docLoaderFactory(do_CreateInstance(id));
    if (!docLoaderFactory) {
        // try again after loading plugins
        nsresult err;
        nsCOMPtr<nsIPluginHost> pluginHost = 
                 do_GetService(kPluginManagerCID, &err);

        if (NS_FAILED(err))
            return NS_ERROR_FAILURE;

        pluginHost->LoadPlugins();

        docLoaderFactory = do_CreateInstance(id);

        if (!docLoaderFactory)
            return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIChannel> aOpenedChannel = do_QueryInterface(request);

    // Now create an instance of the content viewer
    NS_ENSURE_SUCCESS(docLoaderFactory->CreateInstance("view",
                                                       aOpenedChannel,
                                                       aLoadGroup, aContentType,
                                                       NS_STATIC_CAST
                                                       (nsIContentViewerContainer
                                                        *, this), nsnull,
                                                       aContentHandler,
                                                       aViewer),
                      NS_ERROR_FAILURE);

    (*aViewer)->SetContainer(NS_STATIC_CAST(nsIContentViewerContainer *, this));

    nsCOMPtr<nsIPluginViewer> pv(do_QueryInterface(*aViewer));
    if (pv)
      SetTitle(nsnull);  // clear title bar for full-page plugin

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetupNewViewer(nsIContentViewer * aNewViewer)
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

    if (mContentViewer || parent) {
        nsCOMPtr<nsIMarkupDocumentViewer> oldMUDV;
        if (mContentViewer) {
            // Get any interesting state from old content viewer
            // XXX: it would be far better to just reuse the document viewer ,
            //      since we know we're just displaying the same document as before
            oldMUDV = do_QueryInterface(mContentViewer);
        }
        else {
            // No old content viewer, so get state from parent's content viewer
            nsCOMPtr<nsIContentViewer> parentContentViewer;
            parent->GetContentViewer(getter_AddRefs(parentContentViewer));
            oldMUDV = do_QueryInterface(parentContentViewer);
        }

        nsXPIDLString defaultCharset;
        nsXPIDLString forceCharset;
        nsXPIDLString hintCharset;
        PRInt32 hintCharsetSource;

        nsCOMPtr<nsIMarkupDocumentViewer>
            newMUDV(do_QueryInterface(aNewViewer));
        if (oldMUDV && newMUDV) {
            NS_ENSURE_SUCCESS(oldMUDV->
                              GetDefaultCharacterSet(getter_Copies
                                                     (defaultCharset)),
                              NS_ERROR_FAILURE);
            NS_ENSURE_SUCCESS(oldMUDV->
                              GetForceCharacterSet(getter_Copies(forceCharset)),
                              NS_ERROR_FAILURE);
            NS_ENSURE_SUCCESS(oldMUDV->
                              GetHintCharacterSet(getter_Copies(hintCharset)),
                              NS_ERROR_FAILURE);
            NS_ENSURE_SUCCESS(oldMUDV->
                              GetHintCharacterSetSource(&hintCharsetSource),
                              NS_ERROR_FAILURE);

            // set the old state onto the new content viewer
            NS_ENSURE_SUCCESS(newMUDV->SetDefaultCharacterSet(defaultCharset),
                              NS_ERROR_FAILURE);
            NS_ENSURE_SUCCESS(newMUDV->SetForceCharacterSet(forceCharset),
                              NS_ERROR_FAILURE);
            NS_ENSURE_SUCCESS(newMUDV->SetHintCharacterSet(hintCharset),
                              NS_ERROR_FAILURE);
            NS_ENSURE_SUCCESS(newMUDV->
                              SetHintCharacterSetSource(hintCharsetSource),
                              NS_ERROR_FAILURE);
        }
    }

    // It is necessary to obtain the focus controller to utilize its ability
    // to suppress focus.  This is necessary to fix Win32-only bugs related to
    // a loss of focus when mContentViewer is set to null.  The internal window
    // is destroyed, and the OS focuses the parent window.  This call ends up
    // notifying the focus controller that the outer window should focus
    // and this hoses us on any link traversal.
    //
    // Please do not touch any of the focus controller code here without
    // testing bugs #28580 and 50509.  These are immensely important bugs,
    // so PLEASE take care not to regress them if you decide to alter this 
    // code later              -- hyatt
    nsCOMPtr<nsIFocusController> focusController;
    if (mScriptGlobal) {
        nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(mScriptGlobal);
        ourWindow->GetRootFocusController(getter_AddRefs(focusController));
        if (focusController) {
            // Suppress the command dispatcher.
            focusController->SetSuppressFocus(PR_TRUE,
                                              "Win32-Only Link Traversal Issue");
            // Remove focus from the element that has it
            nsCOMPtr<nsIDOMWindowInternal> focusedWindow;
            focusController->GetFocusedWindow(getter_AddRefs(focusedWindow));
            nsCOMPtr<nsIDOMWindowInternal> ourFocusedWindow(do_QueryInterface(ourWindow));

            // We want to null out the last focused element if the document containing
            // it is going away.  If the last focused element is in a descendent
            // window of our domwindow, its document will be destroyed when we
            // destroy our children.  So, check for this case and null out the
            // last focused element.  See bug 70484.

            PRBool isSubWindow = PR_FALSE;
            nsCOMPtr<nsIDOMWindow> curwin;
            if (focusedWindow)
              focusedWindow->GetParent(getter_AddRefs(curwin));
            while (curwin) {
              if (curwin == NS_STATIC_CAST(nsIDOMWindow*, ourFocusedWindow)) {
                isSubWindow = PR_TRUE;
                break;
              }
              nsIDOMWindow* temp;
              curwin->GetParent(&temp);
              if (curwin == temp) {
                NS_RELEASE(temp);
                break;
              }
              curwin = dont_AddRef(temp);
            }

            if (ourFocusedWindow == focusedWindow || isSubWindow)
              focusController->SetFocusedElement(nsnull);
        }
    }

    nscolor bgcolor = NS_RGBA(0, 0, 0, 0);
    PRBool bgSet = PR_FALSE;

    // Ensure that the content viewer is destroyed *after* the GC - bug 71515
    nsCOMPtr<nsIContentViewer> kungfuDeathGrip = mContentViewer;
    if (mContentViewer) {
        // Stop any activity that may be happening in the old document before
        // releasing it...
        mContentViewer->Stop();

        // Try to extract the default background color from the old
        // view manager, so we can use it for the next document.
        nsCOMPtr<nsIDocumentViewer> docviewer =
        do_QueryInterface(mContentViewer);

        if (docviewer) {
            nsCOMPtr<nsIPresShell> shell;
            docviewer->GetPresShell(*getter_AddRefs(shell));

            if (shell) {
                nsCOMPtr<nsIViewManager> vm;
                shell->GetViewManager(getter_AddRefs(vm));

                if (vm) {
                    vm->GetDefaultBackgroundColor(&bgcolor);
                    // If the background color is not known, don't propagate it.
                    bgSet = NS_GET_A(bgcolor) != 0;
                }
            }
        }

        mContentViewer->Close();
        aNewViewer->SetPreviousViewer(mContentViewer);
        mContentViewer = nsnull;
    }

    // End copying block (Don't mess with the old content/document viewer
    // beyond here!!)

    // See the book I wrote above regarding why the focus controller is 
    // being used here.  -- hyatt
    if (focusController)
        focusController->SetSuppressFocus(PR_FALSE,
                                          "Win32-Only Link Traversal Issue");

    mContentViewer = aNewViewer;

    nsCOMPtr<nsIWidget> widget;
    NS_ENSURE_SUCCESS(GetMainWidget(getter_AddRefs(widget)), NS_ERROR_FAILURE);
    if (!widget) {
        NS_ERROR("GetMainWidget coughed up a null widget");
        return NS_ERROR_FAILURE;
    }

    nsRect bounds(x, y, cx, cy);
    NS_ENSURE_SUCCESS(EnsureDeviceContext(), NS_ERROR_FAILURE);
    if (NS_FAILED(mContentViewer->Init(widget, mDeviceContext, bounds))) {
        mContentViewer = nsnull;
        NS_ERROR("ContentViewer Initialization failed");
        return NS_ERROR_FAILURE;
    }

    if (bgSet) {
        // Stuff the bgcolor from the last view manager into the new
        // view manager. This improves page load continuity.
        nsCOMPtr<nsIDocumentViewer> docviewer =
            do_QueryInterface(mContentViewer);

        if (docviewer) {
            nsCOMPtr<nsIPresShell> shell;
            docviewer->GetPresShell(*getter_AddRefs(shell));

            if (shell) {
                nsCOMPtr<nsIViewManager> vm;
                shell->GetViewManager(getter_AddRefs(vm));

                if (vm) {
                    vm->SetDefaultBackgroundColor(bgcolor);
                }
            }
        }
    }

// XXX: It looks like the LayoutState gets restored again in Embed()
//      right after the call to SetupNewViewer(...)

    // We don't show the mContentViewer yet, since we want to draw the old page
    // until we have enough of the new page to show.  Just return with the new
    // viewer still set to hidden.

    // Now that we have switched documents, forget all of our children
    DestroyChildren();

    return NS_OK;
}


//*****************************************************************************
// nsDocShell: Site Loading
//*****************************************************************************   
NS_IMETHODIMP
nsDocShell::InternalLoad(nsIURI * aURI,
                         nsIURI * aReferrer,
                         nsISupports * aOwner,
                         PRBool aInheritOwner,
                         const PRUnichar *aWindowTarget,
                         nsIInputStream * aPostData,
                         nsIInputStream * aHeadersData,
                         PRUint32 aLoadType,
                         nsISHEntry * aSHEntry)
{
    nsresult rv;

    nsCOMPtr<nsISupports> owner(aOwner);
    //
    // Get an owner from the current document if necessary
    //
    if (!owner && aInheritOwner)
        GetCurrentDocumentOwner(getter_AddRefs(owner));

    //
    // Resolve the window target before going any further...
    // If the load has been targeted to another DocShell, then transfer the
    // load to it...
    //
    if (aWindowTarget && *aWindowTarget) {
        PRBool bIsNewWindow;
        nsCOMPtr<nsIDocShell> targetDocShell;
        nsAutoString name(aWindowTarget);

        //
        // This is a hack for Shrimp :-(
        //
        // if the load cmd is a user click....and we are supposed to try using
        // external default protocol handlers....then try to see if we have one for
        // this protocol
        //
        // See bug #52182
        //
        if (mUseExternalProtocolHandler && aLoadType == LOAD_LINK) {
            PRBool bIsJavascript = PR_FALSE;

            aURI->SchemeIs("javascript", &bIsJavascript);
            // don't do it for javascript urls!
            if (!bIsJavascript &&
                (name.EqualsIgnoreCase("_content") || 
                 name.EqualsIgnoreCase("_blank"))) 
            {
                nsCOMPtr<nsIExternalProtocolService> extProtService;
                nsXPIDLCString urlScheme;

                extProtService = do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID);
                if (extProtService) {
                    PRBool haveHandler = PR_FALSE;
                    aURI->GetScheme(getter_Copies(urlScheme));

                    extProtService->ExternalProtocolHandlerExists(urlScheme,
                                                                  &haveHandler);
                    if (haveHandler)
                        return extProtService->LoadUrl(aURI);
                }
            }
        }

        //
        // This is a hack to prevent top-level windows from ever being
        // created.  It really doesn't belong here, but until there is a
        // way for embeddors to get involved in window targeting, this is
        // as good a place as any...
        //
        if (mDisallowPopupWindows) {
            PRBool bIsChromeOrResource = PR_FALSE;

            aURI->SchemeIs("chrome", &bIsChromeOrResource);
            if (!bIsChromeOrResource) {
                aURI->SchemeIs("resource", &bIsChromeOrResource);
            }
            if (!bIsChromeOrResource) {
                if (name.EqualsIgnoreCase("_blank") ||
                    name.EqualsIgnoreCase("_new")) {
                    name.AssignWithConversion("_top");
                }
                else {
                    nsCOMPtr<nsIDocShellTreeItem> targetTreeItem;
                    FindItemWithName(name.get(),
                                     NS_STATIC_CAST(nsIInterfaceRequestor *, this),
                                     getter_AddRefs(targetTreeItem));
                    if (targetTreeItem)
                        targetDocShell = do_QueryInterface(targetTreeItem);
                    else
                        name.AssignWithConversion("_top");
                }
            }
        }

        //
        // Locate the target DocShell.
        // This may involve creating a new toplevel window - if necessary.
        //
        if (!targetDocShell) {
            rv = FindTarget(name.get(), &bIsNewWindow,
                            getter_AddRefs(targetDocShell));
        }

        NS_ASSERTION(targetDocShell, "No Target docshell could be found!");
        //
        // Transfer the load to the target DocShell...  Pass nsnull as the
        // window target name from to prevent recursive retargeting!
        //
        if (targetDocShell) {
            rv = targetDocShell->InternalLoad(aURI,
                                              aReferrer,
                                              owner,
                                              aInheritOwner,
                                              nsnull,         // No window target
                                              aPostData,
                                              aHeadersData,
                                              aLoadType,
                                              aSHEntry);
            if (rv == NS_ERROR_NO_CONTENT) {
                if (bIsNewWindow) {
                    //
                    // At this point, a new window has been created, but the
                    // URI did not have any data associated with it...
                    //
                    // So, the best we can do, is to tear down the new window
                    // that was just created!
                    //
                    nsCOMPtr<nsIDocShellTreeItem> treeItem;
                    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;

                    treeItem = do_QueryInterface(targetDocShell);
                    treeItem->GetTreeOwner(getter_AddRefs(treeOwner));
                    if (treeOwner) {
                        nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;

                        treeOwnerAsWin = do_QueryInterface(treeOwner);
                        if (treeOwnerAsWin) {
                            treeOwnerAsWin->Destroy();
                        }
                    }
                }
                //
                // NS_ERROR_NO_CONTENT should not be returned to the
                // caller... This is an internal error code indicating that
                // the URI had no data associated with it - probably a 
                // helper-app style protocol (ie. mailto://)
                //
                rv = NS_OK;
            }
            else if (bIsNewWindow) {
                // XXX: Once new windows are created hidden, the new
                //      window will need to be made visible...  For now,
                //      do nothing.
            }
        }
        return rv;
    }

    //
    // Load is being targetted at this docshell so return an error if the
    // docshell is in the process of being destroyed.
    //
    if (mIsBeingDestroyed) {
        return NS_ERROR_FAILURE;
	}
    
    mURIResultedInDocument = PR_FALSE;  // reset the clock...
   
    //
    // First:
    // Check to see if the new URI is an anchor in the existing document.
    //
    if ((aLoadType == LOAD_NORMAL ||
         aLoadType == LOAD_NORMAL_REPLACE ||
         aLoadType == LOAD_HISTORY ||
         aLoadType == LOAD_LINK) && (aPostData == nsnull)) {
        PRBool wasAnchor = PR_FALSE;
        NS_ENSURE_SUCCESS(ScrollIfAnchor(aURI, &wasAnchor), NS_ERROR_FAILURE);
        if (wasAnchor) {
            mLoadType = aLoadType;
            mURIResultedInDocument = PR_TRUE;
            /* This is a anchor traversal with in the same page.
             * call OnNewURI() so that, this traversal will be 
             * recorded in session and global history.
             */             
            OnNewURI(aURI, nsnull, mLoadType);
            
            /* If this is a history load, OnNewURI() will not create
             * a mLSHE. In that case, we want to assign mOSHE to aSHEntry 
             * passed by SH, otherwise, mLSHE created by OnNewURI() should
             * be assigned to mOSHE.
             */
            if (aSHEntry && !mLSHE)
              mOSHE = aSHEntry;            
            else if (mLSHE)
              mOSHE = mLSHE;

            /* Clear out mLSHE so that further anchor visits get
             * recorded in SH and SH won't misbehave. 
             */
            mLSHE = nsnull;
            /* Set the title for the SH entry for this target url. so that
             * SH menus in go/back/forward buttons won't be empty for this.
             */
            if (mSessionHistory) {
                PRInt32 index = -1;
                mSessionHistory->GetIndex(&index);
                nsCOMPtr<nsIHistoryEntry> hEntry;
                mSessionHistory->GetEntryAtIndex(index, PR_FALSE,
                                                 getter_AddRefs(hEntry));
                NS_ENSURE_TRUE(hEntry, NS_ERROR_FAILURE);
                nsCOMPtr<nsISHEntry> shEntry(do_QueryInterface(hEntry));
                if (shEntry)
                    shEntry->SetTitle(mTitle.get());
            }

            return NS_OK;
        }
    }
    //
    // Stop any current network activity.  Do not stop the content until
    // data starts arriving from the new URI...
    //
    rv = Stop(nsIWebNavigation::STOP_NETWORK);
    if (NS_FAILED(rv)) return rv;
    

    mLoadType = aLoadType;
    // mLSHE should be assigned to aSHEntry, only after Stop() has
    // been called. 
    mLSHE = aSHEntry;

    rv = DoURILoad(aURI, aReferrer, owner, aPostData, aHeadersData);

    return rv;
}

NS_IMETHODIMP
nsDocShell::CreateFixupURI(const PRUnichar * aStringURI, nsIURI ** aURI)
{
    *aURI = nsnull;
    nsAutoString uriString(aStringURI);
    uriString.Trim(" ");        // Cleanup the empty spaces that might be on each end.

    // Eliminate embedded newlines, which single-line text fields now allow:
    uriString.StripChars("\r\n");

    // Create the fixup object if necessary
    if (!mURIFixup) {
        mURIFixup = do_GetService(NS_URIFIXUP_CONTRACTID);
        if (!mURIFixup) {
            // No fixup service so try and create a URI and see what happens
            return NS_NewURI(aURI, uriString, nsnull);
        }
    }

    // Call the fixup object
    return mURIFixup->CreateFixupURI(aStringURI,
        nsIURIFixup::FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP, aURI);
}

NS_IMETHODIMP
nsDocShell::GetCurrentDocumentOwner(nsISupports ** aOwner)
{
    nsresult rv;
    *aOwner = nsnull;
    nsCOMPtr<nsIDocument> document;
    //-- Get the current document
    if (mContentViewer) {
        nsCOMPtr<nsIDocumentViewer>
            docViewer(do_QueryInterface(mContentViewer));
        if (!docViewer)
            return NS_ERROR_FAILURE;
        rv = docViewer->GetDocument(*getter_AddRefs(document));
    }
    else //-- If there's no document loaded yet, look at the parent (frameset)
    {
        nsCOMPtr<nsIDocShellTreeItem> parentItem;
        rv = GetSameTypeParent(getter_AddRefs(parentItem));
        if (NS_FAILED(rv) || !parentItem)
            return rv;
        nsCOMPtr<nsIDOMWindowInternal>
            parentWindow(do_GetInterface(parentItem));
        if (!parentWindow)
            return NS_OK;
        nsCOMPtr<nsIDOMDocument> parentDomDoc;
        rv = parentWindow->GetDocument(getter_AddRefs(parentDomDoc));
        if (!parentDomDoc)
            return NS_OK;
        document = do_QueryInterface(parentDomDoc);
    }

    //-- Get the document's principal
    nsCOMPtr<nsIPrincipal> principal;
    rv = document->GetPrincipal(getter_AddRefs(principal));
    if (NS_FAILED(rv) || !principal)
        return NS_ERROR_FAILURE;
    rv = principal->QueryInterface(NS_GET_IID(nsISupports), (void **) aOwner);
    return rv;
}

nsresult nsDocShell::DoURILoad(nsIURI * aURI,
                               nsIURI * aReferrerURI,
                               nsISupports * aOwner,
                               nsIInputStream * aPostData,
                               nsIInputStream * aHeadersData)
{
    nsresult rv;
    nsCOMPtr<nsIURILoader> uriLoader;

    uriLoader = do_GetService(NS_URI_LOADER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    // we need to get the load group from our load cookie so we can pass it into open uri...
    nsCOMPtr<nsILoadGroup> loadGroup;
    rv = uriLoader->GetLoadGroupForContext(NS_STATIC_CAST
                                           (nsIDocShell *, this),
                                           getter_AddRefs(loadGroup));
    if (NS_FAILED(rv)) return rv;

    // open a channel for the url
    nsCOMPtr<nsIChannel> channel;

    rv = NS_OpenURI(getter_AddRefs(channel),
                    aURI,
                    nsnull,
                    loadGroup,
                    NS_STATIC_CAST(nsIInterfaceRequestor *, this));
    if (NS_FAILED(rv))
        return rv;

    channel->SetOriginalURI(aURI);

    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
    //
    // If this is a HTTP channel, then set up the HTTP specific information
    // (ie. POST data, referer, ...)
    //
    if (httpChannel) {
        nsCOMPtr<nsICachingChannel>  cacheChannel(do_QueryInterface(httpChannel));
        /* Get the cache Key from SH */
        nsCOMPtr<nsISupports> cacheKey;
        if (mLSHE) {
            mLSHE->GetCacheKey(getter_AddRefs(cacheKey));
        }
        else if (mOSHE)          // for reload cases
            mOSHE->GetCacheKey(getter_AddRefs(cacheKey));

        // figure out if we need to set the post data stream on the channel...
        // right now, this is only done for http channels.....
        if (aPostData) {
            // XXX it's a bit of a hack to rewind the postdata stream here but
            // it has to be done in case the post data is being reused multiple
            // times.
            nsCOMPtr<nsIRandomAccessStore>
                postDataRandomAccess(do_QueryInterface(aPostData));
            if (postDataRandomAccess) {
                postDataRandomAccess->Seek(PR_SEEK_SET, 0);
            }

            nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(httpChannel));
            NS_ASSERTION(uploadChannel, "http must support nsIUploadChannel");

            // we really need to have a content type associated with this stream!!
            uploadChannel->SetUploadStream(aPostData,nsnull, -1);
            /* If there is a valid postdata *and* it is a History Load,
             * set up the cache key on the channel, to retrieve the
             * data *only* from the cache. If it is a normal reload, the 
             * cache is free to go to the server for updated postdata. 
             */
            if (cacheChannel && cacheKey) {
                if (mLoadType == LOAD_HISTORY || mLoadType == LOAD_RELOAD_CHARSET_CHANGE) 
                    cacheChannel->SetCacheKey(cacheKey, PR_TRUE);
                else if (mLoadType == LOAD_RELOAD_NORMAL)
                    cacheChannel->SetCacheKey(cacheKey, PR_FALSE);
            }         

         
        }
        else {
            /* If there is no postdata, set the cache key on the channel
             * with the readFromCacheOnly set to false, so that cache will
             * be free to get it from net if it is not found in cache.
             * New cache may use it creatively on CGI pages with GET
             * method and even on those that say "no-cache"
             */
            if (mLoadType == LOAD_HISTORY || mLoadType == LOAD_RELOAD_NORMAL 
                || mLoadType == LOAD_RELOAD_CHARSET_CHANGE) {
                if (cacheChannel && cacheKey)
                    cacheChannel->SetCacheKey(cacheKey, PR_FALSE);
            }
        }
        if (aHeadersData) {
            rv = AddHeadersToChannel(aHeadersData, httpChannel);
        }
        // Set the referrer explicitly
        if (aReferrerURI)       // Referrer is currenly only set for link clicks here.
            httpChannel->SetReferrer(aReferrerURI,
                                     nsIHttpChannel::REFERRER_LINK_CLICK);
    }
    //
    // Set the owner of the channel - only for javascript and data channels.
    //
    // XXX: Is seems wrong that the owner is ignored - even if one is
    //      supplied) unless the URI is javascript or data.
    //
    //      (Currently chrome URIs set the owner when they are created!
    //      So setting a NULL owner would be bad!)
    //
    PRBool isJSOrData = PR_FALSE;
    aURI->SchemeIs("javascript", &isJSOrData);
    if (!isJSOrData) {
      aURI->SchemeIs("data", &isJSOrData);
    }
    if (isJSOrData) {
        channel->SetOwner(aOwner);
    }

    rv = DoChannelLoad(channel, uriLoader);

    return rv;
}

static NS_METHOD
AHTC_WriteFunc(nsIInputStream * in,
               void *closure,
               const char *fromRawSegment,
               PRUint32 toOffset, PRUint32 count, PRUint32 * writeCount)
{
    if (nsnull == writeCount || nsnull == closure ||
        nsnull == fromRawSegment || nsCRT::strlen(fromRawSegment) < 1) {
        return NS_BASE_STREAM_CLOSED;
    }

    *writeCount = 0;
    char *headersBuf = *((char **) closure);
    // pointer to where we should start copying bytes from rawSegment
    char *pHeadersBuf = nsnull;
    PRUint32 headersBufLen;
    PRUint32 rawSegmentLen = nsCRT::strlen(fromRawSegment);

    // if the buffer has no data yet
    if (!headersBuf) {
        headersBufLen = rawSegmentLen;
        pHeadersBuf = headersBuf = (char *) nsMemory::Alloc(headersBufLen + 1);
        if (!headersBuf) {
            return NS_BASE_STREAM_WOULD_BLOCK;
        }
        nsCRT::memset(headersBuf, nsnull, headersBufLen + 1);
    }
    else {
        // data has been read, reallocate
        // store a pointer to the old full buffer
        pHeadersBuf = headersBuf;

        // create a new buffer
        headersBufLen = nsCRT::strlen(headersBuf);
        headersBuf =
            (char *) nsMemory::Alloc(rawSegmentLen + headersBufLen + 1);
        if (!headersBuf) {
            headersBuf = pHeadersBuf;
            pHeadersBuf = nsnull;
            return NS_BASE_STREAM_WOULD_BLOCK;
        }
        memset(headersBuf, nsnull, rawSegmentLen + headersBufLen + 1);
        // copy the old buffer to the beginning of the new buffer
        nsCRT::memcpy(headersBuf, pHeadersBuf, headersBufLen);
        // free the old buffer
        nsCRT::free(pHeadersBuf);
        // make the buffer pointer point to the writeable part
        // of the new buffer
        pHeadersBuf = headersBuf + headersBufLen;
        // increment the length of the buffer
        headersBufLen += rawSegmentLen;
    }

    // at this point, pHeadersBuf points to where we should copy bits
    // from fromRawSegment.
    nsCRT::memcpy(pHeadersBuf, fromRawSegment, rawSegmentLen);
    // null termination
    headersBuf[headersBufLen] = nsnull;
    *((char **) closure) = headersBuf;
    *writeCount = rawSegmentLen;

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::AddHeadersToChannel(nsIInputStream * aHeadersData,
                                nsIChannel * aGenericChannel)
{
    if (nsnull == aHeadersData || nsnull == aGenericChannel) {
        return NS_ERROR_NULL_POINTER;
    }
    nsCOMPtr<nsIHttpChannel> aChannel = do_QueryInterface(aGenericChannel);
    if (!aChannel) {
        return NS_ERROR_NULL_POINTER;
    }

    // used during the manipulation of the InputStream
    nsresult rv = NS_ERROR_FAILURE;
    PRUint32 available = 0;
    PRUint32 bytesRead;
    char *headersBuf = nsnull;

    // used during the manipulation of the String from the InputStream
    nsCAutoString headersString;
    nsCAutoString oneHeader;
    nsCAutoString headerName;
    nsCAutoString headerValue;
    PRInt32 crlf = 0;
    PRInt32 colon = 0;

    //
    // Suck all the data out of the nsIInputStream into a char * buffer.
    //

    rv = aHeadersData->Available(&available);
    if (NS_FAILED(rv) || available < 1) {
        goto AHTC_CLEANUP;
    }

    do {
        aHeadersData->ReadSegments(AHTC_WriteFunc, &headersBuf, available,
                                   &bytesRead);
        rv = aHeadersData->Available(&available);
        if (NS_FAILED(rv)) {
            goto AHTC_CLEANUP;
        }
    } while (0 < available);

    //
    // Turn the char * buffer into an nsString.
    //
    headersString = (const char *) headersBuf;

    //
    // Iterate over the nsString: for each "\r\n" delimeted chunk,
    // add the value as a header to the nsIHttpChannel
    //

    while (PR_TRUE) {
        crlf = headersString.Find("\r\n", PR_TRUE);
        if (-1 == crlf) {
            rv = NS_OK;
            goto AHTC_CLEANUP;
        }
        headersString.Mid(oneHeader, 0, crlf);
        headersString.Cut(0, crlf + 2);
        oneHeader.StripWhitespace();
        colon = oneHeader.Find(":");
        if (-1 == colon) {
            rv = NS_ERROR_NULL_POINTER;
            goto AHTC_CLEANUP;
        }
        oneHeader.Left(headerName, colon);
        colon++;
        oneHeader.Mid(headerValue, colon, oneHeader.Length() - colon);

        //
        // FINALLY: we can set the header!
        // 

        rv = aChannel->SetRequestHeader(headerName.get(), headerValue.get());
        if (NS_FAILED(rv)) {
            rv = NS_ERROR_NULL_POINTER;
            goto AHTC_CLEANUP;
        }
    }

  AHTC_CLEANUP:
    nsMemory::Free((void *) headersBuf);
    headersBuf = nsnull;
    return rv;
}

nsresult nsDocShell::DoChannelLoad(nsIChannel * aChannel,
                                   nsIURILoader * aURILoader)
{
    nsresult rv;
    // Mark the channel as being a document URI...
    nsLoadFlags loadFlags = 0;
    (void) aChannel->GetLoadFlags(&loadFlags);
    loadFlags |= nsIChannel::LOAD_DOCUMENT_URI;

    // Load attributes depend on load type...
    switch (mLoadType) {
    case LOAD_HISTORY:
        loadFlags |= nsIRequest::VALIDATE_NEVER;
        break;

    case LOAD_RELOAD_CHARSET_CHANGE:
        loadFlags |= nsIRequest::LOAD_FROM_CACHE;
        break;
    

    case LOAD_RELOAD_NORMAL:
        loadFlags |= nsIRequest::VALIDATE_ALWAYS;
        break;

    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
    case LOAD_REFRESH:
        loadFlags |= nsIRequest::LOAD_BYPASS_CACHE;
        break;

    case LOAD_NORMAL:
    case LOAD_LINK:
        // Set cache checking flags
        if (mPrefs) {
            PRInt32 prefSetting;
            if (NS_SUCCEEDED
                (mPrefs->
                 GetIntPref("browser.cache.check_doc_frequency",
                            &prefSetting))) {
                switch (prefSetting) {
                case 0:
                    loadFlags |= nsIRequest::VALIDATE_ONCE_PER_SESSION;
                    break;
                case 1:
                    loadFlags |= nsIRequest::VALIDATE_ALWAYS;
                    break;
                case 2:
                    loadFlags |= nsIRequest::VALIDATE_NEVER;
                    break;
                }
            }
        }
        break;
    }

    (void) aChannel->SetLoadFlags(loadFlags);

    rv = aURILoader->OpenURI(aChannel,
                             (mLoadType == LOAD_LINK),
                             NS_STATIC_CAST(nsIDocShell *, this));
    
    if (rv == NS_ERROR_PORT_ACCESS_NOT_ALLOWED) {
        nsCOMPtr<nsIPrompt> prompter;
        nsCOMPtr<nsIStringBundle> stringBundle;

        GetInterface(NS_GET_IID(nsIPrompt), getter_AddRefs(prompter));
        if (!prompter) return rv;

        nsCOMPtr<nsIStringBundleService> sbs(do_GetService(NS_STRINGBUNDLE_CONTRACTID));
        if (!sbs) return rv;

        sbs->CreateBundle("chrome://necko/locale/necko.properties", 
                          getter_AddRefs(stringBundle));

        if (!stringBundle)
            return NS_ERROR_FAILURE;

        nsXPIDLString messageStr;
        stringBundle->GetStringFromName(NS_LITERAL_STRING("DeniedPortAccess").get(),
                                            getter_Copies(messageStr));

        prompter->Alert(nsnull, messageStr);

    }
    
    return rv;
}

NS_IMETHODIMP
nsDocShell::ScrollIfAnchor(nsIURI * aURI, PRBool * aWasAnchor)
{
    NS_ASSERTION(aURI, "null uri arg");
    NS_ASSERTION(aWasAnchor, "null anchor arg");

    if (aURI == nsnull || aWasAnchor == nsnull) {
        return NS_ERROR_FAILURE;
    }

    *aWasAnchor = PR_FALSE;

    if (!mCurrentURI) {
        return NS_OK;
    }

    nsresult rv;

    // NOTE: we assume URIs are absolute for comparison purposes

    nsXPIDLCString currentSpec;
    NS_ENSURE_SUCCESS(mCurrentURI->GetSpec(getter_Copies(currentSpec)),
                      NS_ERROR_FAILURE);

    nsXPIDLCString newSpec;
    NS_ENSURE_SUCCESS(aURI->GetSpec(getter_Copies(newSpec)), NS_ERROR_FAILURE);

    // Search for hash marks in the current URI and the new URI and
    // take a copy of everything to the left of the hash for
    // comparison.

    const char kHash = '#';

    // Split the new URI into a left and right part
    nsAutoString sNew;
    sNew.AssignWithConversion(newSpec);
    nsAutoString sNewLeft;
    nsAutoString sNewRef;
    PRInt32 hashNew = sNew.FindChar(kHash);
    if (hashNew == 0) {
        return NS_OK;           // Strange URI 
    }
    else if (hashNew > 0) {
        sNew.Left(sNewLeft, hashNew);
        sNew.Right(sNewRef, sNew.Length() - hashNew - 1);
    }
    else {
        sNewLeft = sNew;
    }

    // Split the current URI in a left and right part
    nsAutoString sCurrent;
    sCurrent.AssignWithConversion(currentSpec);
    nsAutoString sCurrentLeft;
    PRInt32 hashCurrent = sCurrent.FindChar(kHash);
    if (hashCurrent == 0) {
        return NS_OK;           // Strange URI 
    }
    else if (hashCurrent > 0) {
        sCurrent.Left(sCurrentLeft, hashCurrent);
    }
    else {
        sCurrentLeft = sCurrent;
    }

    // Exit when there are no anchors
    if (hashNew <= 0 && hashCurrent <= 0) {
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

    if (sCurrentLeft.CompareWithConversion(sNewLeft, PR_FALSE, -1) != 0) {
        return NS_OK;           // URIs not the same
    }

    // Both the new and current URIs refer to the same page. We can now
    // browse to the hash stored in the new URI.

    if (!sNewRef.IsEmpty()) {
        nsCOMPtr<nsIPresShell> shell = nsnull;
        rv = GetPresShell(getter_AddRefs(shell));
        if (NS_SUCCEEDED(rv) && shell) {
            *aWasAnchor = PR_TRUE;

            char *str = ToNewCString(sNewRef);

            // nsUnescape modifies the string that is passed into it.
            nsUnescape(str);

            // We assume that the bytes are in UTF-8, as it says in the spec:
            // http://www.w3.org/TR/html4/appendix/notes.html#h-B.2.1
            nsAutoString savedNewRef(sNewRef);
            sNewRef = NS_ConvertUTF8toUCS2(str);

            nsMemory::Free(str);

            // We try the UTF-8 string first, and then try the document's charset (see below).
            rv = shell->GoToAnchor(sNewRef);

            // Above will fail if the anchor name is not UTF-8.
            // Need to convert from document charset to unicode.
            if (NS_FAILED(rv)) {
                // Get a document charset
                NS_ENSURE_TRUE(mContentViewer, NS_ERROR_FAILURE);
                nsCOMPtr<nsIDocumentViewer>
                    docv(do_QueryInterface(mContentViewer));
                NS_ENSURE_TRUE(docv, NS_ERROR_FAILURE);
                nsCOMPtr<nsIDocument> doc;
                rv = docv->GetDocument(*getter_AddRefs(doc));
                NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
                nsAutoString aCharset;
                rv = doc->GetDocumentCharacterSet(aCharset);
                NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
                char *charsetStr = ToNewCString(aCharset);
                NS_ENSURE_TRUE(charsetStr, NS_ERROR_OUT_OF_MEMORY);

                // Use the saved string
                char *uriStr = ToNewCString(savedNewRef);
                if (!uriStr) {
                    nsMemory::Free(charsetStr);
                    return NS_ERROR_OUT_OF_MEMORY;
                }

                nsCOMPtr<nsITextToSubURI> textToSubURI =
                    do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
                if (NS_FAILED(rv)) {
                    nsMemory::Free(uriStr);
                    nsMemory::Free(charsetStr);
                    return NS_ERROR_FAILURE;
                }

                // Unescape and convert to unicode
                PRUnichar *uStr;
                rv = textToSubURI->UnEscapeAndConvert(charsetStr, uriStr,
                                                      &uStr);
                nsMemory::Free(uriStr);
                nsMemory::Free(charsetStr);
                NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

                sNewRef.Assign(uStr);
                nsMemory::Free(uStr);

                rv = shell->GoToAnchor(sNewRef);
            }
        }
    }
    else {
        // A bit of a hack - scroll to the top of the page.
        SetCurScrollPosEx(0, 0);
        *aWasAnchor = PR_TRUE;
    }

    return NS_OK;
}


NS_IMETHODIMP
nsDocShell::OnNewURI(nsIURI * aURI, nsIChannel * aChannel,
                     PRUint32 aLoadType)
{
    NS_ASSERTION(aURI, "uri is null");

    UpdateCurrentGlobalHistory();
    PRBool updateHistory = PR_TRUE;
    PRBool equalUri = PR_FALSE;
    PRBool shAvailable = PR_TRUE;  

    // Get the post data from the channel
    nsCOMPtr<nsIInputStream> inputStream;
    if (aChannel) {
        nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));

        if (httpChannel) {
            httpChannel->GetUploadStream(getter_AddRefs(inputStream));
        }
    }
    /* Create SH Entry (mLSHE) only if there is a  SessionHistory object (mSessionHistory) in
     * the current frame or in the root docshell
     */
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
                if (!rootSH)
                    shAvailable = PR_FALSE;
            }
        }
    }  // mSessionHistory


    // Determine if this type of load should update history.    
    if (aLoadType == LOAD_BYPASS_HISTORY ||
         aLoadType & LOAD_CMD_HISTORY ||
         aLoadType & LOAD_CMD_RELOAD)         
        updateHistory = PR_FALSE;


    /* If the url to be loaded is the same as the one already there,
     * and the original loadType is LOAD_NORMAL or LOAD_LINK,
     * set loadType to LOAD_NORMAL_REPLACE so that AddToSessionHistory()
     * won't mess with the current SHEntry and if this page has any frame 
     * children, it also will be handled properly. see bug 83684
     *
     * XXX Hopefully changing the loadType at this time will not hurt  
     *  anywhere. The other way to take care of sequentially repeating
     *  frameset pages is to add new methods to nsIDocShellTreeItem.
     * Hopefully I don't have to do that. 
     */
    if (NS_SUCCEEDED(aURI->Equals(mCurrentURI, &equalUri))
       && equalUri && !inputStream &&
       (mLoadType == LOAD_NORMAL || mLoadType == LOAD_LINK || mLoadType == LOAD_REFRESH))
        mLoadType = LOAD_NORMAL_REPLACE;

    /* If the user pressed shift-reload, cache will create a new cache key
     * for the page. Save the new cacheKey in Session History. 
     * see bug 90098
     */
    if (aChannel && aLoadType == LOAD_RELOAD_BYPASS_CACHE ||
        aLoadType == LOAD_RELOAD_BYPASS_PROXY ||
        aLoadType == LOAD_RELOAD_BYPASS_PROXY_AND_CACHE) {                 
        
        nsCOMPtr<nsICachingChannel> cacheChannel(do_QueryInterface(aChannel));
        nsCOMPtr<nsISupports>  cacheKey;
        // Get the Cache Key  and store it in SH.         
        if (cacheChannel) 
            cacheChannel->GetCacheKey(getter_AddRefs(cacheKey));
        if (mLSHE)
          mLSHE->SetCacheKey(cacheKey);
    }

    
    if (updateHistory && shAvailable) { 
        // Update session history if necessary...
        if (!mLSHE && (mItemType == typeContent) && mURIResultedInDocument) {
            /* This is  a fresh page getting loaded for the first time
             *.Create a Entry for it and add it to SH, if this is the
             * rootDocShell
             */
            (void) AddToSessionHistory(aURI, aChannel, getter_AddRefs(mLSHE));
        }

        // Update Global history if necessary...
        updateHistory = PR_FALSE;
        ShouldAddToGlobalHistory(aURI, &updateHistory);
        if (updateHistory) {
            AddToGlobalHistory(aURI);
        }
    }

    // If this was a history load, update the index in 
    // SH. 
    if (rootSH && (mLoadType & LOAD_CMD_HISTORY)) {
        nsCOMPtr<nsISHistoryInternal> shInternal(do_QueryInterface(rootSH));
        if (shInternal)
            shInternal->UpdateIndex();
    }
    SetCurrentURI(aURI);
    // if there's a refresh header in the channel, this method
    // will set it up for us. 
    SetupRefreshURI(aChannel);

    return NS_OK;
}

nsresult
nsDocShell::RefreshURIFromHeader(nsIURI * aBaseURI,
                                 const nsAReadableString & aHeader)
{
    // Refresh headers are parsed with the following format in mind
    // <META HTTP-EQUIV=REFRESH CONTENT="5; URL=http://uri">
    // By the time we are here, the following is true:
    // header = "REFRESH"
    // content = "5; URL=http://uri" // note the URL attribute is
    // optional, if it is absent, the currently loaded url is used.
    // Also note that the seconds and URL separator can be either
    // a ';' or a ','. The ',' separator should be illegal but CNN
    // is using it.
    // 
    // We need to handle the following strings, where
    //  - X is a set of digits
    //  - URI is either a relative or absolute URI
    //
    // Note that URI should start with "url=" but we allow omission
    //
    // "" || ";" || "," 
    //  empty string. use the currently loaded URI
    //  and refresh immediately.
    // "X" || "X;" || "X,"
    //  Refresh the currently loaded URI in X seconds.
    // "X; URI" || "X, URI"
    //  Refresh using URI as the destination in X seconds.
    // "URI" || "; URI" || ", URI"
    //  Refresh immediately using URI as the destination.
    // 
    // Currently, anything immediately following the URI, if
    // seperated by any char in the set "'\"\t\r\n " will be
    // ignored. So "10; url=go.html ; foo=bar" will work,
    // and so will "10; url='go.html'; foo=bar". However,
    // "10; url=go.html; foo=bar" will result in the uri
    // "go.html;" since ';' and ',' are valid uri characters.
    // 
    // Note that we need to remove any tokens wrapping the URI.
    // These tokens currently include spaces, double and single
    // quotes.

    // when done, seconds is 0 or the given number of seconds
    //            uriAttrib is empty or the URI specified
    nsAutoString uriAttrib;
    PRInt32 seconds = 0;

    nsReadingIterator < PRUnichar > iter, tokenStart, doneIterating;

    aHeader.BeginReading(iter);
    aHeader.EndReading(doneIterating);

    // skip leading whitespace
    while (iter != doneIterating && nsCRT::IsAsciiSpace(*iter))
        ++iter;

    tokenStart = iter;

    // skip leading + and -
    if (iter != doneIterating && (*iter == '-' || *iter == '+'))
        ++iter;

    // parse number
    while (iter != doneIterating && (*iter >= '0' && *iter <= '9')) {
        seconds = seconds * 10 + (*iter - '0');
        ++iter;
    }

    if (iter != doneIterating) {
        // if we started with a '-', number is negative
        if (*tokenStart == '-')
            seconds = -seconds;

        // skip to next ';' or ','
        while (iter != doneIterating && !(*iter == ';' || *iter == ','))
            ++iter;

        // skip ';' or ','
        if (iter != doneIterating && (*iter == ';' || *iter == ',')) {
            ++iter;

            // skip whitespace
            while (iter != doneIterating && nsCRT::IsAsciiSpace(*iter))
                ++iter;
        }
    }

    // possible start of URI
    tokenStart = iter;

    // skip "url = " to real start of URI
    if (iter != doneIterating && (*iter == 'u' || *iter == 'U')) {
        ++iter;
        if (iter != doneIterating && (*iter == 'r' || *iter == 'R')) {
            ++iter;
            if (iter != doneIterating && (*iter == 'l' || *iter == 'L')) {
                ++iter;

                // skip whitespace
                while (iter != doneIterating && nsCRT::IsAsciiSpace(*iter))
                    ++iter;

                if (iter != doneIterating && *iter == '=') {
                    ++iter;

                    // skip whitespace
                    while (iter != doneIterating && nsCRT::IsAsciiSpace(*iter))
                        ++iter;

                    // found real start of URI
                    tokenStart = iter;
                }
            }
        }
    }

    // skip a leading '"' or '\''
    if (tokenStart != doneIterating && (*tokenStart == '"' || *tokenStart == '\''))
        ++tokenStart;

    // set iter to start of URI
    iter = tokenStart;

    // tokenStart here points to the beginning of URI

    // skip anything which isn't whitespace
    while (iter != doneIterating && !nsCRT::IsAsciiSpace(*iter))
        ++iter;

    // move iter one back if the last character is a '"' or '\''
    if (iter != tokenStart) {
        --iter;
        if (!(*iter == '"' || *iter == '\''))
            ++iter;
    }

    // URI is whatever's contained from tokenStart to iter.
    // note: if tokenStart == doneIterating, so is iter.

    nsresult rv = NS_OK;

    nsCOMPtr<nsIURI> uri;
    if (tokenStart == iter) {
        uri = aBaseURI;
    }
    else {
        uriAttrib = Substring(tokenStart, iter);
        rv = NS_NewURI(getter_AddRefs(uri), uriAttrib, aBaseURI);
    }

    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIScriptSecurityManager>
            securityManager(do_GetService
                            (NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv));
        if (NS_SUCCEEDED(rv)) {
            rv = securityManager->CheckLoadURI(aBaseURI, uri,
                                               nsIScriptSecurityManager::
                                               DISALLOW_FROM_MAIL);
            if (NS_SUCCEEDED(rv)) {
                // since we can't travel back in time yet, just pretend it was meant figuratively
                if (seconds < 0)
                    seconds = 0;

                rv = RefreshURI(uri, seconds * 1000, PR_FALSE, PR_TRUE);
            }
        }
    }
    return rv;
}

NS_IMETHODIMP nsDocShell::SetupRefreshURI(nsIChannel * aChannel)
{
    nsresult
        rv;
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel, &rv));
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIURI> referrer;
        rv = httpChannel->GetReferrer(getter_AddRefs(referrer));

        if (NS_SUCCEEDED(rv)) {
            SetReferrerURI(referrer);

            nsXPIDLCString refreshHeader;
            rv = httpChannel->GetResponseHeader("refresh",
                                                getter_Copies(refreshHeader));

            if (refreshHeader)
                rv = RefreshURIFromHeader(mCurrentURI,
                                          NS_ConvertUTF8toUCS2(refreshHeader));
        }
    }
    return rv;
}

NS_IMETHODIMP
nsDocShell::OnLoadingSite(nsIChannel * aChannel)
{
    nsCOMPtr<nsIURI> uri;
    // If this a redirect, use the final url (uri)
    // else use the original url
    //
    // The better way would be to trust the OnRedirect() that necko gives us.
    // But this notification happen after the necko notification and hence
    // overrides it. Until OnRedirect() gets settles out, let us do this.
    nsLoadFlags loadFlags = 0;
    aChannel->GetLoadFlags(&loadFlags);
    if (loadFlags & nsIChannel::LOAD_REPLACE)
        aChannel->GetURI(getter_AddRefs(uri));
    else
        aChannel->GetOriginalURI(getter_AddRefs(uri));
    NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

    OnNewURI(uri, aChannel, mLoadType);

    return NS_OK;
}

void
nsDocShell::SetCurrentURI(nsIURI * aURI)
{
    mCurrentURI = aURI;         //This assignment addrefs
    PRBool isRoot = PR_FALSE;   // Is this the root docshell
    PRBool  isSubFrame=PR_FALSE;  // Is this a subframe navigation?

    if (!mLoadCookie)
      return; 

    nsCOMPtr<nsIDocumentLoader> loader(do_GetInterface(mLoadCookie)); 
    nsCOMPtr<nsIWebProgress> webProgress(do_QueryInterface(mLoadCookie));
    nsCOMPtr<nsIDocShellTreeItem> root;

    GetSameTypeRootTreeItem(getter_AddRefs(root));
    if (root.get() == NS_STATIC_CAST(nsIDocShellTreeItem *, this)) 
    {
        // This is the root docshell
        isRoot = PR_TRUE;
    }
    if (mLSHE) {
      nsCOMPtr<nsIHistoryEntry> historyEntry(do_QueryInterface(mLSHE));
      
      // Check if this is a subframe navigation
      if (historyEntry) {
        historyEntry->GetIsSubFrame(&isSubFrame);
      }
    }
 
    if (!isSubFrame && !isRoot) {
      /* 
       * We don't want to send OnLocationChange notifications when
       * a subframe is being loaded for the first time, while
       * visiting a frameset page
       */
      return;
    }
    

    NS_ASSERTION(loader, "No document loader");
    if (loader) {
        loader->FireOnLocationChange(webProgress, nsnull, aURI);
    }
}

void
nsDocShell::SetReferrerURI(nsIURI * aURI)
{
    mReferrerURI = aURI;        // This assigment addrefs
}

//*****************************************************************************
// nsDocShell: Session History
//*****************************************************************************   
PRBool
nsDocShell::ShouldAddToSessionHistory(nsIURI * aURI)
{
    // I believe none of the about: urls should go in the history. But then
    // that could just be me... If the intent is only deny about:blank then we
    // should just do a spec compare, rather than two gets of the scheme and
    // then the path.  -Gagan
    nsresult rv;
    nsXPIDLCString buffer;
    nsCAutoString schemeStr;

    rv = aURI->GetScheme(getter_Copies(buffer));
    if (NS_FAILED(rv))
        return PR_FALSE;

    schemeStr = buffer;
    if (schemeStr.Equals("about")) {
        rv = aURI->GetPath(getter_Copies(buffer));
        if (NS_FAILED(rv))
            return PR_FALSE;

        schemeStr = buffer;
        if (schemeStr.Equals("blank")) {
            return PR_FALSE;
        }
    }
    return PR_TRUE;
}

nsresult
nsDocShell::AddToSessionHistory(nsIURI * aURI,
                                nsIChannel * aChannel, nsISHEntry ** aNewEntry)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsISHEntry> entry;
    PRBool shouldPersist;

    shouldPersist = ShouldAddToSessionHistory(aURI);

    //
    // If the entry is being replaced in SH, then just use the
    // current entry...
    //
    if (LOAD_NORMAL_REPLACE == mLoadType) {
        // There is no need to go to mSessionHistory and get the entry at
        // current index. mOSHE works for subframes and top level docshells.
        entry = mOSHE;
        // If there are children for this entry destroy them, as they are
        // going out of scope. 
        if (entry) {
            nsCOMPtr<nsISHContainer> shContainer(do_QueryInterface(entry));
            if (shContainer) {
                PRInt32 childCount = 0;
                shContainer->GetChildCount(&childCount);
                // Remove all children of this entry 
                for (PRInt32 i = childCount - 1; i >= 0; i--) {
                    nsCOMPtr<nsISHEntry> child;
                    shContainer->GetChildAt(i, getter_AddRefs(child));
                    shContainer->RemoveChild(child);
                }
            }
        }
    }

    // Create a new entry if necessary.
    if (!entry) {
        entry = do_CreateInstance(NS_SHENTRY_CONTRACTID);

        if (!entry) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    // Get the post data & referrer
    nsCOMPtr<nsIInputStream> inputStream;
    nsCOMPtr<nsIURI> referrerURI;
    nsCOMPtr<nsISupports> cacheKey;
    nsCOMPtr<nsISupports> cacheToken;
    PRPackedBool expired = PR_FALSE;
    nsXPIDLCString val;
    if (aChannel) {
        nsCOMPtr<nsICachingChannel>
            cacheChannel(do_QueryInterface(aChannel));
        /* If there is a caching channel, get the Cache Key  and store it 
         * in SH.
         */
        if (cacheChannel) {
            cacheChannel->GetCacheKey(getter_AddRefs(cacheKey));
            cacheChannel->GetCacheToken(getter_AddRefs(cacheToken));
        }
        nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));

        if (httpChannel) {
            httpChannel->GetUploadStream(getter_AddRefs(inputStream));
            httpChannel->GetReferrer(getter_AddRefs(referrerURI));
            httpChannel->GetResponseHeader("Cache-Control", getter_Copies(val));
        }
    }

    //Title is set in nsDocShell::SetTitle()
    entry->Create(aURI,         // uri
                  nsnull,       // Title
                  nsnull,       // DOMDocument
                  inputStream,  // Post data stream
                  nsnull,       // LayoutHistory state
                  cacheKey);    // CacheKey
    entry->SetReferrerURI(referrerURI);
    /* If cache got a 'no-store', ask SH not to store
     * HistoryLayoutState. By default, SH will set this
     * flag to PR_TRUE and save HistoryLayoutState.
     */    
    if (val && (PL_strcasestr(val, "no-store") || PL_strcasestr(val, "no-cache"))) {
        entry->SetSaveLayoutStateFlag(PR_FALSE);
    }
    if (cacheToken) {
        // Check if the page has expired from cache 
        nsCOMPtr<nsICacheEntryDescriptor> cacheEntryDesc(do_QueryInterface(cacheToken));
        if (cacheEntryDesc) {        
            PRUint32 expTime;         
            cacheEntryDesc->GetExpirationTime(&expTime);         
            PRUint32 now = PRTimeToSeconds(PR_Now());                  
            if (expTime <=  now)            
                expired = PR_TRUE;         
         
        }
    }
    if (expired == PR_TRUE)
        entry->SetExpirationStatus(PR_TRUE);


    // If no Session History component is available in the parent DocShell
    // hierarchy, then AddChildSHEntry(...) will fail and the new entry
    // will be deleted when it loses scope...
    //
    if (mLoadType != LOAD_NORMAL_REPLACE) {
        if (mSessionHistory) {
            nsCOMPtr<nsISHistoryInternal>
                shPrivate(do_QueryInterface(mSessionHistory));
            NS_ENSURE_TRUE(shPrivate, NS_ERROR_FAILURE);
            rv = shPrivate->AddEntry(entry, shouldPersist);
        }
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


NS_IMETHODIMP
nsDocShell::LoadHistoryEntry(nsISHEntry * aEntry, PRUint32 aLoadType)
{
    nsresult rv;
    nsCOMPtr<nsIURI> uri;
    nsCOMPtr<nsIInputStream> postData;
    nsCOMPtr<nsIURI> referrerURI;

    NS_ENSURE_TRUE(aEntry, NS_ERROR_FAILURE);
    nsCOMPtr<nsIHistoryEntry> hEntry(do_QueryInterface(aEntry));
    NS_ENSURE_TRUE(hEntry, NS_ERROR_FAILURE);

    NS_ENSURE_SUCCESS(hEntry->GetURI(getter_AddRefs(uri)), NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(aEntry->GetReferrerURI(getter_AddRefs(referrerURI)),
                      NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(aEntry->GetPostData(getter_AddRefs(postData)),
                      NS_ERROR_FAILURE);

    /* If there is a valid postdata *and* the user pressed
     * reload or shift-reload, take user's permission before we  
     * repost the data to the server.
     */
    if ((aLoadType & LOAD_CMD_RELOAD) && postData) {

      nsCOMPtr<nsIPrompt> prompter;
      PRBool repost;
      nsCOMPtr<nsIStringBundle> stringBundle;
      GetPromptAndStringBundle(getter_AddRefs(prompter), 
                                getter_AddRefs(stringBundle));
 
      if (stringBundle && prompter) {
        nsXPIDLString messageStr;
        nsresult rv = stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2("repostConfirm").get(), 
                                                      getter_Copies(messageStr));
          
        if (NS_SUCCEEDED(rv) && messageStr) {
          prompter->Confirm(nsnull, messageStr, &repost);
          /* If the user pressed cancel in the dialog, 
           * return failure. 
           */
          if (!repost)
            return NS_ERROR_FAILURE;  
        }
      }
    }

    rv = InternalLoad(uri,
                      referrerURI,
                      nsnull,       // No owner
                      PR_FALSE,     // Do not inherit owner from document (security-critical!)
                      nsnull,       // No window target
                      postData,     // Post data stream
                      nsnull,       // No headers stream
                      aLoadType,    // Load type
                      aEntry);      // SHEntry
    return rv;
}


NS_IMETHODIMP nsDocShell::PersistLayoutHistoryState()
{
    nsresult  rv = NS_OK;
    

    if (mOSHE) {
        PRBool saveHistoryState = PR_TRUE;
        mOSHE->GetSaveLayoutStateFlag(&saveHistoryState);
        // Don't capture historystate and save it in history
        // if the page asked not to do so.
        if (!saveHistoryState)
          return NS_OK;
        nsCOMPtr<nsIPresShell> shell;

        rv = GetPresShell(getter_AddRefs(shell));
        if (NS_SUCCEEDED(rv) && shell) {
            nsCOMPtr<nsILayoutHistoryState> layoutState;
            rv = shell->CaptureHistoryState(getter_AddRefs(layoutState),
                                            PR_TRUE);
            if (NS_SUCCEEDED(rv) && layoutState) {
                rv = mOSHE->SetLayoutHistoryState(layoutState);
            }
        }

    }
    return rv;
}

NS_IMETHODIMP
nsDocShell::CloneAndReplace(nsISHEntry * src, PRUint32 aCloneID,
                            nsISHEntry * replaceEntry,
                            nsISHEntry ** resultEntry)
{
    nsresult result = NS_OK;
    NS_ENSURE_ARG_POINTER(resultEntry);
    nsISHEntry *dest = (nsISHEntry *) nsnull;
    PRUint32 srcID;
    src->GetID(&srcID);
    nsCOMPtr<nsIHistoryEntry> srcHE(do_QueryInterface(src));

    if (!src || !replaceEntry || !srcHE)
        return NS_ERROR_FAILURE;

    if (srcID == aCloneID) {
        dest = replaceEntry;
        dest->SetIsSubFrame(PR_TRUE);
        *resultEntry = dest;
        NS_IF_ADDREF(*resultEntry);
    }
    else {
        nsCOMPtr<nsIURI> uri;
        nsCOMPtr<nsIInputStream> postdata;
        nsCOMPtr<nsILayoutHistoryState> LHS;
        nsCOMPtr<nsIURI> referrerURI;
        PRUnichar *title = nsnull;
        nsCOMPtr<nsISHEntry> parent;
        PRBool expirationStatus;
        PRBool layoutStatus;
        nsCOMPtr<nsISupports> cacheKey;
        PRUint32 id;
        result = nsComponentManager::CreateInstance(NS_SHENTRY_CONTRACTID, NULL,
                                                    NS_GET_IID(nsISHEntry),
                                                    (void **) &dest);
        if (!NS_SUCCEEDED(result))
            return result;

        srcHE->GetURI(getter_AddRefs(uri));
        src->GetReferrerURI(getter_AddRefs(referrerURI));
        src->GetPostData(getter_AddRefs(postdata));
        srcHE->GetTitle(&title);
        src->GetLayoutHistoryState(getter_AddRefs(LHS));
        //XXX Is this correct? parent is a weak ref in nsISHEntry
        src->GetParent(getter_AddRefs(parent));
        src->GetID(&id);
        src->GetExpirationStatus(&expirationStatus);
        src->GetSaveLayoutStateFlag(&layoutStatus);
        src->GetCacheKey(getter_AddRefs(cacheKey));

        // XXX do we care much about valid values for these uri, title etc....
        dest->SetURI(uri);
        dest->SetReferrerURI(referrerURI);
        dest->SetPostData(postdata);
        dest->SetLayoutHistoryState(LHS);
        dest->SetTitle(title);
        dest->SetParent(parent);
        dest->SetID(id);
        dest->SetIsSubFrame(PR_TRUE);
        dest->SetExpirationStatus(expirationStatus);
        dest->SetSaveLayoutStateFlag(layoutStatus);
        dest->SetCacheKey(cacheKey);
        *resultEntry = dest;

        PRInt32 childCount = 0;

        nsCOMPtr<nsISHContainer> srcContainer(do_QueryInterface(src));
        if (!srcContainer)
            return NS_ERROR_FAILURE;
        nsCOMPtr<nsISHContainer> destContainer(do_QueryInterface(dest));
        if (!destContainer)
            return NS_ERROR_FAILURE;
        srcContainer->GetChildCount(&childCount);
        for (PRInt32 i = 0; i < childCount; i++) {
            nsCOMPtr<nsISHEntry> srcChild;
            srcContainer->GetChildAt(i, getter_AddRefs(srcChild));
            if (!srcChild)
                return NS_ERROR_FAILURE;
            nsCOMPtr<nsISHEntry> destChild;
            if (!NS_SUCCEEDED(result))
                return result;
            result =
                CloneAndReplace(srcChild, aCloneID, replaceEntry,
                                getter_AddRefs(destChild));
            if (!NS_SUCCEEDED(result))
                return result;
            result = destContainer->AddChild(destChild, i);
            if (!NS_SUCCEEDED(result))
                return result;
        }                       // for 
    }

    return result;

}

//*****************************************************************************
// nsDocShell: Global History
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::ShouldAddToGlobalHistory(nsIURI * aURI, PRBool * aShouldAdd)
{
    *aShouldAdd = PR_FALSE;
    if (!mGlobalHistory || !aURI || (typeContent != mItemType))
        return NS_OK;

    // The model is really if we don't know differently then add which basically
    // means we are suppose to try all the things we know not to allow in and
    // then if we don't bail go on and allow it in.  But here lets compare
    // against the most common case we know to allow in and go on and say yes
    // to it.
    PRBool isHTTP = PR_FALSE;
    PRBool isHTTPS = PR_FALSE;
    NS_ENSURE_SUCCESS(aURI->SchemeIs("http", &isHTTP), NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(aURI->SchemeIs("https", &isHTTPS), NS_ERROR_FAILURE);

    if (isHTTP || isHTTPS) {
        *aShouldAdd = PR_TRUE;
        return NS_OK;
    }

    PRBool isAbout = PR_FALSE;
    PRBool isImap = PR_FALSE;
    PRBool isNews = PR_FALSE;
    PRBool isMailbox = PR_FALSE;
    PRBool isViewSource = PR_FALSE;

    NS_ENSURE_SUCCESS(aURI->SchemeIs("about", &isAbout), NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(aURI->SchemeIs("imap", &isImap), NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(aURI->SchemeIs("news", &isNews), NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(aURI->SchemeIs("mailbox", &isMailbox), NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(aURI->SchemeIs("view-source", &isViewSource),
                      NS_ERROR_FAILURE);

    if (isAbout || isImap || isNews || isMailbox || isViewSource)
        return NS_OK;

    *aShouldAdd = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::AddToGlobalHistory(nsIURI * aURI)
{
    NS_ENSURE_STATE(mGlobalHistory);

    nsXPIDLCString spec;
    NS_ENSURE_SUCCESS(aURI->GetSpec(getter_Copies(spec)), NS_ERROR_FAILURE);

    NS_ENSURE_SUCCESS(mGlobalHistory->AddPage(spec), NS_ERROR_FAILURE);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::UpdateCurrentGlobalHistory()
{
    // XXX Add code here that needs to update the current history item
    return NS_OK;
}

//*****************************************************************************
// nsDocShell: Helper Routines
//*****************************************************************************   

nsresult
nsDocShell::SetLoadCookie(nsISupports * aCookie)
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


nsresult
nsDocShell::GetLoadCookie(nsISupports ** aResult)
{
    *aResult = mLoadCookie;
    NS_IF_ADDREF(*aResult);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetLoadType(PRUint32 aLoadType)
{
    mLoadType = aLoadType;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetLoadType(PRUint32 * aLoadType)
{
    *aLoadType = mLoadType;
    return NS_OK;
}

nsDocShellInitInfo *
nsDocShell::InitInfo()
{
    if (mInitInfo)
        return mInitInfo;
    return mInitInfo = new nsDocShellInitInfo();
}

#define DIALOG_STRING_URI "chrome://global/locale/appstrings.properties"

NS_IMETHODIMP
nsDocShell::GetPromptAndStringBundle(nsIPrompt ** aPrompt,
                                     nsIStringBundle ** aStringBundle)
{
    NS_ENSURE_SUCCESS(GetInterface(NS_GET_IID(nsIPrompt), (void **) aPrompt),
                      NS_ERROR_FAILURE);

    nsCOMPtr<nsIStringBundleService>
        stringBundleService(do_GetService(NS_STRINGBUNDLE_CONTRACTID));
    NS_ENSURE_TRUE(stringBundleService, NS_ERROR_FAILURE);

    NS_ENSURE_SUCCESS(stringBundleService->
                      CreateBundle(DIALOG_STRING_URI,
                                   aStringBundle),
                      NS_ERROR_FAILURE);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetChildOffset(nsIDOMNode * aChild, nsIDOMNode * aParent,
                           PRInt32 * aOffset)
{
    NS_ENSURE_ARG_POINTER(aChild || aParent);

    nsCOMPtr<nsIDOMNodeList> childNodes;
    NS_ENSURE_SUCCESS(aParent->GetChildNodes(getter_AddRefs(childNodes)),
                      NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(childNodes, NS_ERROR_FAILURE);

    PRInt32 i = 0;

    for (; PR_TRUE; i++) {
        nsCOMPtr<nsIDOMNode> childNode;
        NS_ENSURE_SUCCESS(childNodes->Item(i, getter_AddRefs(childNode)),
                          NS_ERROR_FAILURE);
        NS_ENSURE_TRUE(childNode, NS_ERROR_FAILURE);

        if (childNode.get() == aChild) {
            *aOffset = i;
            return NS_OK;
        }
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::GetRootScrollableView(nsIScrollableView ** aOutScrollView)
{
    NS_ENSURE_ARG_POINTER(aOutScrollView);

    nsCOMPtr<nsIPresShell> shell;
    NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(shell)), NS_ERROR_FAILURE);

    nsCOMPtr<nsIViewManager> viewManager;
    NS_ENSURE_SUCCESS(shell->GetViewManager(getter_AddRefs(viewManager)),
                      NS_ERROR_FAILURE);

    NS_ENSURE_SUCCESS(viewManager->GetRootScrollableView(aOutScrollView),
                      NS_ERROR_FAILURE);

    if (*aOutScrollView == nsnull) {
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::EnsureContentListener()
{
    nsresult rv = NS_OK;
    if (mContentListener)
        return NS_OK;

    mContentListener = new nsDSURIContentListener();
    NS_ENSURE_TRUE(mContentListener, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(mContentListener);

    rv = mContentListener->Init();
    if (NS_FAILED(rv))
        return rv;

    mContentListener->DocShell(this);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::EnsureScriptEnvironment()
{
    if (mScriptContext)
        return NS_OK;

    if (mIsBeingDestroyed) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    nsCOMPtr<nsIDOMScriptObjectFactory> factory =
        do_GetService(kDOMScriptObjectFactoryCID);
    NS_ENSURE_TRUE(factory, NS_ERROR_FAILURE);

    factory->NewScriptGlobalObject(getter_AddRefs(mScriptGlobal));
    NS_ENSURE_TRUE(mScriptGlobal, NS_ERROR_FAILURE);

    mScriptGlobal->SetDocShell(NS_STATIC_CAST(nsIDocShell *, this));
    mScriptGlobal->
        SetGlobalObjectOwner(NS_STATIC_CAST
                             (nsIScriptGlobalObjectOwner *, this));

    factory->NewScriptContext(mScriptGlobal, getter_AddRefs(mScriptContext));
    NS_ENSURE_TRUE(mScriptContext, NS_ERROR_FAILURE);

    return NS_OK;
}

NS_IMETHODIMP nsDocShell::EnsureFind()
{
    nsresult rv;
    if (!mFind)
    {
        mFind = do_CreateInstance("@mozilla.org/embedcomp/find;1", &rv);
        if (NS_FAILED(rv)) return rv;
    }
    
    // we promise that the nsIWebBrowserFind that we return has been set
    // up to point to the focussed, or content window, so we have to
    // set that up each time.
    nsCOMPtr<nsIScriptGlobalObject> scriptGO;
    rv = GetScriptGlobalObject(getter_AddRefs(scriptGO));
    if (NS_FAILED(rv)) return rv;

    // default to our window
    nsCOMPtr<nsIDOMWindow> rootWindow = do_QueryInterface(scriptGO);
    nsCOMPtr<nsIDOMWindow> windowToSearch = rootWindow;

    // if we can, search the focussed window
    nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(scriptGO);
    nsCOMPtr<nsIFocusController> focusController;
    if (ourWindow)
        ourWindow->GetRootFocusController(getter_AddRefs(focusController));
    if (focusController)
    {
        nsCOMPtr<nsIDOMWindowInternal> focussedWindow;
        focusController->GetFocusedWindow(getter_AddRefs(focussedWindow));
        if (focussedWindow)
            windowToSearch = focussedWindow;
    }

    nsCOMPtr<nsIWebBrowserFindInFrames> findInFrames = do_QueryInterface(mFind);
    if (!findInFrames) return NS_ERROR_NO_INTERFACE;
    
    rv = findInFrames->SetRootSearchFrame(rootWindow);
    if (NS_FAILED(rv)) return rv;
    rv = findInFrames->SetCurrentSearchFrame(windowToSearch);
    if (NS_FAILED(rv)) return rv;
    
    return NS_OK;
}

PRBool
nsDocShell::IsFrame()
{
    if (mParent) {
        PRInt32 parentType = ~mItemType;        // Not us
        mParent->GetItemType(&parentType);
        if (parentType == mItemType)    // This is a frame
            return PR_TRUE;
    }

    return PR_FALSE;
}

NS_IMETHODIMP
nsDocShell::GetHasFocus(PRBool *aHasFocus)
{
  *aHasFocus = mHasFocus;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetHasFocus(PRBool aHasFocus)
{
#ifdef DEBUG_DOCSHELL_FOCUS
    printf(">>>>>>>>>> nsDocShell::SetHasFocus: %p  %s\n", this, aHasFocus?"Yes":"No");
#endif

  mHasFocus = aHasFocus;

  nsDocShellFocusController* dsfc = nsDocShellFocusController::GetInstance();
  if (dsfc && aHasFocus) {
    dsfc->Focus(this);
  }

  if (!aHasFocus) {
      // We may be in a situation where the focus outline was shown
      // on this document because the user tabbed into it, but the focus
      // is now switching to another document via a click.  In this case,
      // we need to make sure the focus outline is removed from this document.
      SetCanvasHasFocus(PR_FALSE);
  }

  return NS_OK;
}

//-------------------------------------------------------
// Tells the HTMLFrame/CanvasFrame that is now has focus
NS_IMETHODIMP
nsDocShell::SetCanvasHasFocus(PRBool aCanvasHasFocus)
{
  nsCOMPtr<nsIPresShell> presShell;
  GetPresShell(getter_AddRefs(presShell));
  if (!presShell) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc;
  presShell->GetDocument(getter_AddRefs(doc));
  if (!doc) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> rootContent;
  doc->GetRootContent(getter_AddRefs(rootContent));
  if (!rootContent) return NS_ERROR_FAILURE;

  nsIFrame* frame;
  presShell->GetPrimaryFrameFor(rootContent, &frame);
  if (frame != nsnull) {
    frame->GetParent(&frame);
    if (frame != nsnull) {
      nsICanvasFrame* canvasFrame;
      if (NS_SUCCEEDED(frame->QueryInterface(NS_GET_IID(nsICanvasFrame), (void**)&canvasFrame))) {
        canvasFrame->SetHasFocus(aCanvasHasFocus);

        nsCOMPtr<nsIPresContext> presContext;
        GetPresContext(getter_AddRefs(presContext));
        
        nsIView* canvasView = nsnull;
        frame->GetView(presContext, &canvasView);

        nsCOMPtr<nsIViewManager> viewManager;
        canvasView->GetViewManager(*getter_AddRefs(viewManager));
        viewManager->UpdateView(canvasView, NS_VMREFRESH_NO_SYNC);

        return NS_OK;
      }
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::GetCanvasHasFocus(PRBool *aCanvasHasFocus)
{
  return NS_ERROR_FAILURE;
}

//*****************************************************************************
//***    nsRefreshTimer: Object Management
//*****************************************************************************

nsRefreshTimer::nsRefreshTimer():mRepeat(PR_FALSE), mDelay(0),
mMetaRefresh(PR_FALSE)
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
NS_IMETHODIMP_(void)
nsRefreshTimer::Notify(nsITimer * aTimer)
{
    NS_ASSERTION(mDocShell, "DocShell is somehow null");

    if (mDocShell && aTimer) {
        /* Check if Meta refresh/redirects are permitted. Some
         * embedded applications may not want to do this.
         */
        PRBool allowRedirects = PR_TRUE;
        mDocShell->GetAllowMetaRedirects(&allowRedirects);
        if (!allowRedirects)
          return;
        // Get the delay count
        PRUint32 delay;
        delay = aTimer->GetDelay();
        // Get the current uri from the docshell.
        nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
        nsCOMPtr<nsIURI> currURI;
        if (webNav) {
            webNav->GetCurrentURI(getter_AddRefs(currURI));
        }
        nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
        mDocShell->CreateLoadInfo(getter_AddRefs(loadInfo));
        /* Check if this META refresh causes a redirection
         * to another site. 
         */
        PRBool equalUri = PR_FALSE;
        nsresult rv = mURI->Equals(currURI, &equalUri);
        if (NS_SUCCEEDED(rv) && (!equalUri) && mMetaRefresh) {

            /* It is a META refresh based redirection. Now check if it happened within 
             * the threshold time we have in mind(15000 ms as defined by REFRESH_REDIRECT_TIMER).
             * If so, pass a REPLACE flag to LoadURI().
             */
            if (delay <= REFRESH_REDIRECT_TIMER) {
                loadInfo->SetLoadType(nsIDocShellLoadInfo::loadNormalReplace);
            }
            else
                loadInfo->SetLoadType(nsIDocShellLoadInfo::loadRefresh);
            /*
             * LoadURL(...) will cancel all refresh timers... This causes the Timer and
             * its refreshData instance to be released...
             */
            mDocShell->LoadURI(mURI, loadInfo,
                               nsIWebNavigation::LOAD_FLAGS_NONE);
            return;

        }
        else
            loadInfo->SetLoadType(nsIDocShellLoadInfo::loadRefresh);
        mDocShell->LoadURI(mURI, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE);
    }
}

//*****************************************************************************
//***    nsDocShellFocusController: Object Management
//*****************************************************************************
void 
nsDocShellFocusController::Focus(nsIDocShell* aDocShell)
{
#ifdef DEBUG_DOCSHELL_FOCUS
  printf("****** nsDocShellFocusController Focus To: %p  Blur To: %p\n", aDocShell, mFocusedDocShell);
#endif

  if (aDocShell != mFocusedDocShell) {
    if (mFocusedDocShell) {
      mFocusedDocShell->SetHasFocus(PR_FALSE);
    }
    mFocusedDocShell = aDocShell;
  }

}

//--------------------------------------------------
// This is need for when the document with focus goes away
void 
nsDocShellFocusController::ClosingDown(nsIDocShell* aDocShell)
{
  if (aDocShell == mFocusedDocShell) {
    mFocusedDocShell = nsnull;
  }
}
