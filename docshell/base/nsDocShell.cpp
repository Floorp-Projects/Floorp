/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: ft=cpp tw=78 sw=4 et ts=4 sts=4 cin
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Peter Annema <disttsc@bart.nl>
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

#ifdef MOZ_LOGGING
// so we can get logging even in release builds (but only for some things)
#define FORCE_PR_LOG 1
#endif

#include "nsIBrowserDOMWindow.h"
#include "nsIComponentManager.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMStorage.h"
#include "nsPIDOMStorage.h"
#include "nsIDocumentViewer.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsCURILoader.h"
#include "nsDocShellCID.h"
#include "nsLayoutCID.h"
#include "nsDOMCID.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsNetUtil.h"
#include "nsRect.h"
#include "prprf.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMChromeWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIWebBrowserChrome.h"
#include "nsPoint.h"
#include "nsGfxCIID.h"
#include "nsIObserverService.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPrompt2.h"
#include "nsTextFormatter.h"
#include "nsIChannelEventSink.h"
#include "nsIUploadChannel.h"
#include "nsISecurityEventSink.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSContextStack.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsDocumentCharsetInfoCID.h"
#include "nsICanvasFrame.h"
#include "nsIScrollableFrame.h"
#include "nsContentPolicyUtils.h" // NS_CheckContentLoadPolicy(...)
#include "nsICategoryManager.h"
#include "nsXPCOMCID.h"
#include "nsISeekableStream.h"
#include "nsAutoPtr.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranch2.h"
#include "nsIWritablePropertyBag2.h"
#include "nsIAppShell.h"
#include "nsWidgetsCID.h"
#include "nsDOMJSUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIScriptChannel.h"

// we want to explore making the document own the load group
// so we can associate the document URI with the load group.
// until this point, we have an evil hack:
#include "nsIHttpChannelInternal.h"  


// Local Includes
#include "nsDocShell.h"
#include "nsDocShellLoadInfo.h"
#include "nsCDefaultURIFixup.h"
#include "nsDocShellEnumerator.h"
#include "nsSHistory.h"

// Helper Classes
#include "nsDOMError.h"
#include "nsEscape.h"

// Interfaces Needed
#include "nsIUploadChannel.h"
#include "nsIProgressEventSink.h"
#include "nsIWebProgress.h"
#include "nsILayoutHistoryState.h"
#include "nsITimer.h"
#include "nsISHistoryInternal.h"
#include "nsIPrincipal.h"
#include "nsIHistoryEntry.h"
#include "nsISHistoryListener.h"
#include "nsIWindowWatcher.h"
#include "nsIPromptFactory.h"
#include "nsIObserver.h"
#include "nsINestedURI.h"
#include "nsITransportSecurityInfo.h"
#include "nsINSSErrorsService.h"

// Editor-related
#include "nsIEditingSession.h"

#include "nsPIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsICachingChannel.h"
#include "nsICacheVisitor.h"
#include "nsIMultiPartChannel.h"
#include "nsIWyciwygChannel.h"

// The following are for bug #13871: Prevent frameset spoofing
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
#include "prmem.h"

#include "nsISelectionDisplay.h"

#include "nsIGlobalHistory2.h"
#include "nsIGlobalHistory3.h"

#ifdef DEBUG_DOCSHELL_FOCUS
#include "nsIEventStateManager.h"
#endif

#include "nsIFrame.h"

// for embedding
#include "nsIWebBrowserChromeFocus.h"

#include "nsPluginError.h"

static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_CID(kDOMScriptObjectFactoryCID,
                     NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

#if defined(DEBUG_bryner) || defined(DEBUG_chb)
//#define DEBUG_DOCSHELL_FOCUS
#define DEBUG_PAGE_CACHE
#endif

#include "nsContentErrors.h"

// Number of documents currently loading
static PRInt32 gNumberOfDocumentsLoading = 0;

// Global count of existing docshells.
static PRInt32 gDocShellCount = 0;

// Global reference to the URI fixup service.
nsIURIFixup *nsDocShell::sURIFixup = 0;

// True means we validate window targets to prevent frameset
// spoofing. Initialize this to a non-bolean value so we know to check
// the pref on the creation of the first docshell.
static PRBool gValidateOrigin = (PRBool)0xffffffff;

// Hint for native dispatch of events on how long to delay after 
// all documents have loaded in milliseconds before favoring normal
// native event dispatch priorites over performance
#define NS_EVENT_STARVATION_DELAY_HINT 2000

// This is needed for displaying an error message 
// when navigation is attempted on a document when printing
// The value arbitrary as long as it doesn't conflict with
// any of the other values in the errors in DisplayLoadError
#define NS_ERROR_DOCUMENT_IS_PRINTMODE  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GENERAL,2001)

#ifdef PR_LOGGING
#ifdef DEBUG
static PRLogModuleInfo* gDocShellLog;
#endif
static PRLogModuleInfo* gDocShellLeakLog;
#endif

const char kBrandBundleURL[]      = "chrome://branding/locale/brand.properties";
const char kAppstringsBundleURL[] = "chrome://global/locale/appstrings.properties";

static void
FavorPerformanceHint(PRBool perfOverStarvation, PRUint32 starvationDelay)
{
    nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
    if (appShell)
        appShell->FavorPerformanceHint(perfOverStarvation, starvationDelay);
}

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
    nsDocLoader(),
    mAllowSubframes(PR_TRUE),
    mAllowPlugins(PR_TRUE),
    mAllowJavascript(PR_TRUE),
    mAllowMetaRedirects(PR_TRUE),
    mAllowImages(PR_TRUE),
    mFocusDocFirst(PR_FALSE),
    mHasFocus(PR_FALSE),
    mCreatingDocument(PR_FALSE),
    mUseErrorPages(PR_FALSE),
    mObserveErrorPages(PR_TRUE),
    mAllowAuth(PR_TRUE),
    mAllowKeywordFixup(PR_FALSE),
    mFiredUnloadEvent(PR_FALSE),
    mEODForCurrentDocument(PR_FALSE),
    mURIResultedInDocument(PR_FALSE),
    mIsBeingDestroyed(PR_FALSE),
    mIsExecutingOnLoadHandler(PR_FALSE),
    mIsPrintingOrPP(PR_FALSE),
    mSavingOldViewer(PR_FALSE),
    mAppType(nsIDocShell::APP_TYPE_UNKNOWN),
    mChildOffset(0),
    mBusyFlags(BUSY_FLAGS_NONE),
    mMarginWidth(0),
    mMarginHeight(0),
    mItemType(typeContent),
    mDefaultScrollbarPref(Scrollbar_Auto, Scrollbar_Auto),
    mPreviousTransIndex(-1),
    mLoadedTransIndex(-1),
    mEditorData(nsnull),
    mTreeOwner(nsnull),
    mChromeEventHandler(nsnull)
{
    if (gDocShellCount++ == 0) {
        NS_ASSERTION(sURIFixup == nsnull,
                     "Huh, sURIFixup not null in first nsDocShell ctor!");

        CallGetService(NS_URIFIXUP_CONTRACTID, &sURIFixup);
    }

#ifdef PR_LOGGING
#ifdef DEBUG
    if (! gDocShellLog)
        gDocShellLog = PR_NewLogModule("nsDocShell");
#endif
    if (nsnull == gDocShellLeakLog)
        gDocShellLeakLog = PR_NewLogModule("nsDocShellLeak");
    if (gDocShellLeakLog)
        PR_LOG(gDocShellLeakLog, PR_LOG_DEBUG, ("DOCSHELL %p created\n", this));
#endif
}

nsDocShell::~nsDocShell()
{
    nsDocShellFocusController* dsfc = nsDocShellFocusController::GetInstance();
    if (dsfc) {
      dsfc->ClosingDown(this);
    }
    Destroy();

    if (--gDocShellCount == 0) {
        NS_IF_RELEASE(sURIFixup);
    }

#ifdef PR_LOGGING
    if (gDocShellLeakLog)
        PR_LOG(gDocShellLeakLog, PR_LOG_DEBUG, ("DOCSHELL %p destroyed\n", this));
#endif
}

nsresult
nsDocShell::Init()
{
    nsresult rv = nsDocLoader::Init();
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ASSERTION(mLoadGroup, "Something went wrong!");

    mContentListener = new nsDSURIContentListener(this);
    NS_ENSURE_TRUE(mContentListener, NS_ERROR_OUT_OF_MEMORY);

    rv = mContentListener->Init();
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mStorages.Init())
        return NS_ERROR_OUT_OF_MEMORY;

    // We want to hold a strong ref to the loadgroup, so it better hold a weak
    // ref to us...  use an InterfaceRequestorProxy to do this.
    nsCOMPtr<InterfaceRequestorProxy> proxy =
        new InterfaceRequestorProxy(NS_STATIC_CAST(nsIInterfaceRequestor*,
                                                   this));
    NS_ENSURE_TRUE(proxy, NS_ERROR_OUT_OF_MEMORY);
    mLoadGroup->SetNotificationCallbacks(proxy);

    rv = nsDocLoader::AddDocLoaderAsChildOfRoot(this);
    NS_ENSURE_SUCCESS(rv, rv);
    
    // Add as |this| a progress listener to itself.  A little weird, but
    // simpler than reproducing all the listener-notification logic in
    // overrides of the various methods via which nsDocLoader can be
    // notified.   Note that this holds an nsWeakPtr to ourselves, so it's ok.
    return AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_DOCUMENT |
                                     nsIWebProgress::NOTIFY_STATE_NETWORK);
    
}

void
nsDocShell::DestroyChildren()
{
    nsCOMPtr<nsIDocShellTreeItem> shell;
    PRInt32 n = mChildList.Count();
    for (PRInt32 i = 0; i < n; i++) {
        shell = do_QueryInterface(ChildAt(i));
        NS_ASSERTION(shell, "docshell has null child");

        if (shell) {
            shell->SetTreeOwner(nsnull);
        }
    }

    nsDocLoader::DestroyChildren();
}

//*****************************************************************************
// nsDocShell::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF_INHERITED(nsDocShell, nsDocLoader)
NS_IMPL_RELEASE_INHERITED(nsDocShell, nsDocLoader)

NS_INTERFACE_MAP_BEGIN(nsDocShell)
    NS_INTERFACE_MAP_ENTRY(nsIDocShell)
    NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeItem)
    NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeNode)
    NS_INTERFACE_MAP_ENTRY(nsIDocShellHistory)
    NS_INTERFACE_MAP_ENTRY(nsIWebNavigation)
    NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
    NS_INTERFACE_MAP_ENTRY(nsIScrollable)
    NS_INTERFACE_MAP_ENTRY(nsITextScroll)
    NS_INTERFACE_MAP_ENTRY(nsIDocCharset)
    NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObjectOwner)
    NS_INTERFACE_MAP_ENTRY(nsIRefreshURI)
    NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
    NS_INTERFACE_MAP_ENTRY(nsIContentViewerContainer)
    NS_INTERFACE_MAP_ENTRY(nsIEditorDocShell)
    NS_INTERFACE_MAP_ENTRY(nsIWebPageDescriptor)
    NS_INTERFACE_MAP_ENTRY(nsIAuthPromptProvider)
    NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(nsDocLoader)

///*****************************************************************************
// nsDocShell::nsIInterfaceRequestor
//*****************************************************************************   
NS_IMETHODIMP nsDocShell::GetInterface(const nsIID & aIID, void **aSink)
{
    NS_PRECONDITION(aSink, "null out param");

    *aSink = nsnull;

    if (aIID.Equals(NS_GET_IID(nsIURIContentListener))) {
        *aSink = mContentListener;
    }
    else if (aIID.Equals(NS_GET_IID(nsIScriptGlobalObject)) &&
             NS_SUCCEEDED(EnsureScriptEnvironment())) {
        *aSink = mScriptGlobal;
    }
    else if ((aIID.Equals(NS_GET_IID(nsIDOMWindowInternal)) ||
              aIID.Equals(NS_GET_IID(nsPIDOMWindow)) ||
              aIID.Equals(NS_GET_IID(nsIDOMWindow))) &&
             NS_SUCCEEDED(EnsureScriptEnvironment())) {
        return mScriptGlobal->QueryInterface(aIID, aSink);
    }
    else if (aIID.Equals(NS_GET_IID(nsIDOMDocument)) &&
             NS_SUCCEEDED(EnsureContentViewer())) {
        mContentViewer->GetDOMDocument((nsIDOMDocument **) aSink);
        return *aSink ? NS_OK : NS_NOINTERFACE;
    }
    else if (aIID.Equals(NS_GET_IID(nsIPrompt)) &&
             NS_SUCCEEDED(EnsureScriptEnvironment())) {
        nsresult rv;
        nsCOMPtr<nsIWindowWatcher> wwatch =
            do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIDOMWindow> window(do_QueryInterface(mScriptGlobal));

        // Get the an auth prompter for our window so that the parenting
        // of the dialogs works as it should when using tabs.

        nsIPrompt *prompt;
        rv = wwatch->GetNewPrompter(window, &prompt);
        NS_ENSURE_SUCCESS(rv, rv);

        *aSink = prompt;
        return NS_OK;
    }
    else if (aIID.Equals(NS_GET_IID(nsIAuthPrompt)) ||
             aIID.Equals(NS_GET_IID(nsIAuthPrompt2))) {
        return NS_SUCCEEDED(
                GetAuthPrompt(PROMPT_NORMAL, aIID, aSink)) ?
                NS_OK : NS_NOINTERFACE;
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
    else if (aIID.Equals(NS_GET_IID(nsIEditingSession)) && NS_SUCCEEDED(EnsureEditorData())) {
      nsCOMPtr<nsIEditingSession> editingSession;
      mEditorData->GetEditingSession(getter_AddRefs(editingSession));
      if (editingSession)
      {
        *aSink = editingSession;
        NS_ADDREF((nsISupports *)*aSink);
        return NS_OK;
      }  

      return NS_NOINTERFACE;   
    }
    else if (aIID.Equals(NS_GET_IID(nsIClipboardDragDropHookList)) 
            && NS_SUCCEEDED(EnsureTransferableHookData())) {
        *aSink = mTransferableHookData;
        NS_ADDREF((nsISupports *)*aSink);
        return NS_OK;
    }
    else if (aIID.Equals(NS_GET_IID(nsISelectionDisplay))) {
      nsCOMPtr<nsIPresShell> shell;
      nsresult rv = GetPresShell(getter_AddRefs(shell));
      if (NS_SUCCEEDED(rv) && shell)
        return shell->QueryInterface(aIID,aSink);    
    }
    else if (aIID.Equals(NS_GET_IID(nsIDocShellTreeOwner))) {
      nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
      nsresult rv = GetTreeOwner(getter_AddRefs(treeOwner));
      if (NS_SUCCEEDED(rv) && treeOwner)
        return treeOwner->QueryInterface(aIID, aSink);
    }
    else {
        return nsDocLoader::GetInterface(aIID, aSink);
    }

    NS_IF_ADDREF(((nsISupports *) * aSink));
    return *aSink ? NS_OK : NS_NOINTERFACE;
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
    case nsIDocShellLoadInfo::loadNormalExternal:
        loadType = LOAD_NORMAL_EXTERNAL;
        break;
    case nsIDocShellLoadInfo::loadHistory:
        loadType = LOAD_HISTORY;
        break;
    case nsIDocShellLoadInfo::loadNormalBypassCache:
        loadType = LOAD_NORMAL_BYPASS_CACHE;
        break;
    case nsIDocShellLoadInfo::loadNormalBypassProxy:
        loadType = LOAD_NORMAL_BYPASS_PROXY;
        break;
    case nsIDocShellLoadInfo::loadNormalBypassProxyAndCache:
        loadType = LOAD_NORMAL_BYPASS_PROXY_AND_CACHE;
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
    case nsIDocShellLoadInfo::loadStopContent:
        loadType = LOAD_STOP_CONTENT;
        break;
    case nsIDocShellLoadInfo::loadStopContentAndReplace:
        loadType = LOAD_STOP_CONTENT_AND_REPLACE;
        break;
    default:
        NS_NOTREACHED("Unexpected nsDocShellInfoLoadType value");
    }

    return loadType;
}


nsDocShellInfoLoadType
nsDocShell::ConvertLoadTypeToDocShellLoadInfo(PRUint32 aLoadType)
{
    nsDocShellInfoLoadType docShellLoadType = nsIDocShellLoadInfo::loadNormal;
    switch (aLoadType) {
    case LOAD_NORMAL:
        docShellLoadType = nsIDocShellLoadInfo::loadNormal;
        break;
    case LOAD_NORMAL_REPLACE:
        docShellLoadType = nsIDocShellLoadInfo::loadNormalReplace;
        break;
    case LOAD_NORMAL_EXTERNAL:
        docShellLoadType = nsIDocShellLoadInfo::loadNormalExternal;
        break;
    case LOAD_NORMAL_BYPASS_CACHE:
        docShellLoadType = nsIDocShellLoadInfo::loadNormalBypassCache;
        break;
    case LOAD_NORMAL_BYPASS_PROXY:
        docShellLoadType = nsIDocShellLoadInfo::loadNormalBypassProxy;
        break;
    case LOAD_NORMAL_BYPASS_PROXY_AND_CACHE:
        docShellLoadType = nsIDocShellLoadInfo::loadNormalBypassProxyAndCache;
        break;
    case LOAD_HISTORY:
        docShellLoadType = nsIDocShellLoadInfo::loadHistory;
        break;
    case LOAD_RELOAD_NORMAL:
        docShellLoadType = nsIDocShellLoadInfo::loadReloadNormal;
        break;
    case LOAD_RELOAD_CHARSET_CHANGE:
        docShellLoadType = nsIDocShellLoadInfo::loadReloadCharsetChange;
        break;
    case LOAD_RELOAD_BYPASS_CACHE:
        docShellLoadType = nsIDocShellLoadInfo::loadReloadBypassCache;
        break;
    case LOAD_RELOAD_BYPASS_PROXY:
        docShellLoadType = nsIDocShellLoadInfo::loadReloadBypassProxy;
        break;
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
        docShellLoadType = nsIDocShellLoadInfo::loadReloadBypassProxyAndCache;
        break;
    case LOAD_LINK:
        docShellLoadType = nsIDocShellLoadInfo::loadLink;
        break;
    case LOAD_REFRESH:
        docShellLoadType = nsIDocShellLoadInfo::loadRefresh;
        break;
    case LOAD_BYPASS_HISTORY:
    case LOAD_ERROR_PAGE:
        docShellLoadType = nsIDocShellLoadInfo::loadBypassHistory;
        break;
    case LOAD_STOP_CONTENT:
        docShellLoadType = nsIDocShellLoadInfo::loadStopContent;
        break;
    case LOAD_STOP_CONTENT_AND_REPLACE:
        docShellLoadType = nsIDocShellLoadInfo::loadStopContentAndReplace;
        break;
    default:
        NS_NOTREACHED("Unexpected load type value");
    }

    return docShellLoadType;
}                                                                               

//*****************************************************************************
// nsDocShell::nsIDocShell
//*****************************************************************************   
NS_IMETHODIMP
nsDocShell::LoadURI(nsIURI * aURI,
                    nsIDocShellLoadInfo * aLoadInfo,
                    PRUint32 aLoadFlags,
                    PRBool aFirstParty)
{
    nsresult rv;
    nsCOMPtr<nsIURI> referrer;
    nsCOMPtr<nsIInputStream> postStream;
    nsCOMPtr<nsIInputStream> headersStream;
    nsCOMPtr<nsISupports> owner;
    PRBool inheritOwner = PR_FALSE;
    PRBool sendReferrer = PR_TRUE;
    nsCOMPtr<nsISHEntry> shEntry;
    nsXPIDLString target;
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
        aLoadInfo->GetHeadersStream(getter_AddRefs(headersStream));
        aLoadInfo->GetSendReferrer(&sendReferrer);
    }

#if defined(PR_LOGGING) && defined(DEBUG)
    if (PR_LOG_TEST(gDocShellLog, PR_LOG_DEBUG)) {
        nsCAutoString uristr;
        aURI->GetAsciiSpec(uristr);
        PR_LOG(gDocShellLog, PR_LOG_DEBUG,
               ("nsDocShell[%p]: loading %s with flags 0x%08x",
                this, uristr.get(), aLoadFlags));
    }
#endif

    if (!shEntry &&
        !LOAD_TYPE_HAS_FLAGS(loadType, LOAD_FLAGS_REPLACE_HISTORY)) {
        // First verify if this is a subframe.
        nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
        GetSameTypeParent(getter_AddRefs(parentAsItem));
        nsCOMPtr<nsIDocShell> parentDS(do_QueryInterface(parentAsItem));
        PRUint32 parentLoadType;

        if (parentDS && parentDS != NS_STATIC_CAST(nsIDocShell *, this)) {
            /* OK. It is a subframe. Checkout the 
             * parent's loadtype. If the parent was loaded thro' a history
             * mechanism, then get the SH entry for the child from the parent.
             * This is done to restore frameset navigation while going back/forward.
             * If the parent was loaded through any other loadType, set the
             * child's loadType too accordingly, so that session history does not
             * get confused. 
             */
            
            // Get the parent's load type
            parentDS->GetLoadType(&parentLoadType);            

            nsCOMPtr<nsIDocShellHistory> parent(do_QueryInterface(parentAsItem));
            if (parent) {
                // Get the ShEntry for the child from the parent
                parent->GetChildSHEntry(mChildOffset, getter_AddRefs(shEntry));
                // Make some decisions on the child frame's loadType based on the 
                // parent's loadType. 
                if (mCurrentURI == nsnull) {
                    // This is a newly created frame. Check for exception cases first. 
                    // By default the subframe will inherit the parent's loadType.
                    if (shEntry && (parentLoadType == LOAD_NORMAL ||
                                    parentLoadType == LOAD_LINK   ||
                                    parentLoadType == LOAD_NORMAL_EXTERNAL)) {
                        // The parent was loaded normally. In this case, this *brand new* child really shouldn't
                        // have a SHEntry. If it does, it could be because the parent is replacing an
                        // existing frame with a new frame, in the onLoadHandler. We don't want this
                        // url to get into session history. Clear off shEntry, and set laod type to
                        // LOAD_BYPASS_HISTORY. 
                        PRBool inOnLoadHandler=PR_FALSE;
                        parentDS->GetIsExecutingOnLoadHandler(&inOnLoadHandler);
                        if (inOnLoadHandler) {
                            loadType = LOAD_NORMAL_REPLACE;
                            shEntry = nsnull;
                        }
                    }   
                    else if (parentLoadType == LOAD_REFRESH) {
                        // Clear shEntry. For refresh loads, we have to load
                        // what comes thro' the pipe, not what's in history.
                        shEntry = nsnull;
                    }
                    else if ((parentLoadType == LOAD_BYPASS_HISTORY) ||
                             (parentLoadType == LOAD_ERROR_PAGE) ||
                              (shEntry && 
                               ((parentLoadType & LOAD_CMD_HISTORY) || 
                                (parentLoadType == LOAD_RELOAD_NORMAL) || 
                                (parentLoadType == LOAD_RELOAD_CHARSET_CHANGE)))) {
                        // If the parent url, bypassed history or was loaded from
                        // history, pass on the parent's loadType to the new child 
                        // frame too, so that the child frame will also
                        // avoid getting into history. 
                        loadType = parentLoadType;
                    }
                }
                else {
                    // This is a pre-existing subframe. If the load was not originally initiated
                    // by session history, (if (!shEntry) condition succeeded) and mCurrentURI is not null,
                    // it is possible that a parent's onLoadHandler or even self's onLoadHandler is loading 
                    // a new page in this child. Check parent's and self's busy flag  and if it is set,
                    // we don't want this onLoadHandler load to get in to session history.
                    PRUint32 parentBusy = BUSY_FLAGS_NONE;
                    PRUint32 selfBusy = BUSY_FLAGS_NONE;
                    parentDS->GetBusyFlags(&parentBusy);                    
                    GetBusyFlags(&selfBusy);
                    if (((parentBusy & BUSY_FLAGS_BUSY) ||
                         (selfBusy & BUSY_FLAGS_BUSY)) &&
                        shEntry) {
                        loadType = LOAD_NORMAL_REPLACE;
                        shEntry = nsnull; 
                    }
                }
            } // parent
        } //parentDS
        else {  
            // This is the root docshell. If we got here while  
            // executing an onLoad Handler,this load will not go 
            // into session history.
            PRBool inOnLoadHandler=PR_FALSE;
            GetIsExecutingOnLoadHandler(&inOnLoadHandler);
            if (inOnLoadHandler) {
                loadType = LOAD_NORMAL_REPLACE;
            }
        } 
    } // !shEntry

    if (shEntry) {
#ifdef DEBUG
        PR_LOG(gDocShellLog, PR_LOG_DEBUG,
              ("nsDocShell[%p]: loading from session history", this));
#endif

        rv = LoadHistoryEntry(shEntry, loadType);
    }
    // Perform the load...
    else {
        // We need an owner (a referring principal). 3 possibilities:
        // (1) If a principal was passed in, that's what we'll use.
        // (2) If the caller has allowed inheriting from the current document,
        //     or if we're being called from system code (eg chrome JS or pure
        //     C++) then inheritOwner should be true and InternalLoad will get
        //     an owner from the current document. If none of these things are
        //     true, then
        // (3) we pass a null owner into the channel, and an owner will be
        //     created later from the URL.
        //
        // NOTE: This all only works because the only thing the owner is used
        //       for in InternalLoad is data: and javascript: URIs.  For other
        //       URIs this would all be dead wrong!
        if (!owner && !inheritOwner) {
            // See if there's system or chrome JS code running
            nsCOMPtr<nsIScriptSecurityManager> secMan;

            secMan = do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
            if (NS_SUCCEEDED(rv)) {
                rv = secMan->SubjectPrincipalIsSystem(&inheritOwner);
                if (NS_FAILED(rv)) {
                    // Set it back to false
                    inheritOwner = PR_FALSE;
                }
            }
        }

        PRUint32 flags = 0;

        if (inheritOwner)
            flags |= INTERNAL_LOAD_FLAGS_INHERIT_OWNER;

        if (!sendReferrer)
            flags |= INTERNAL_LOAD_FLAGS_DONT_SEND_REFERRER;
            
        if (aLoadFlags & LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP)
            flags |= INTERNAL_LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;

        if (aLoadFlags & LOAD_FLAGS_FIRST_LOAD)
            flags |= INTERNAL_LOAD_FLAGS_FIRST_LOAD;

        rv = InternalLoad(aURI,
                          referrer,
                          owner,
                          flags,
                          target.get(),
                          nsnull,         // No type hint
                          postStream,
                          headersStream,
                          loadType,
                          nsnull,         // No SHEntry
                          aFirstParty,
                          nsnull,         // No nsIDocShell
                          nsnull);        // No nsIRequest
    }

    return rv;
}

NS_IMETHODIMP
nsDocShell::LoadStream(nsIInputStream *aStream, nsIURI * aURI,
                       const nsACString &aContentType,
                       const nsACString &aContentCharset,
                       nsIDocShellLoadInfo * aLoadInfo)
{
    NS_ENSURE_ARG(aStream);

    mAllowKeywordFixup = PR_FALSE;

    // if the caller doesn't pass in a URI we need to create a dummy URI. necko
    // currently requires a URI in various places during the load. Some consumers
    // do as well.
    nsCOMPtr<nsIURI> uri = aURI;
    if (!uri) {
        // HACK ALERT
        nsresult rv = NS_OK;
        uri = do_CreateInstance(NS_SIMPLEURI_CONTRACTID, &rv);
        if (NS_FAILED(rv))
            return rv;
        // Make sure that the URI spec "looks" like a protocol and path...
        // For now, just use a bogus protocol called "internal"
        rv = uri->SetSpec(NS_LITERAL_CSTRING("internal:load-stream"));
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
                      (getter_AddRefs(channel), uri, aStream,
                       aContentType, aContentCharset),
                      NS_ERROR_FAILURE);

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
nsDocShell::FirePageHideNotification(PRBool aIsUnload)
{
    if (mContentViewer && !mFiredUnloadEvent) {
        // Keep an explicit reference since calling PageHide could release
        // mContentViewer
        nsCOMPtr<nsIContentViewer> kungFuDeathGrip(mContentViewer);
        mFiredUnloadEvent = PR_TRUE;

        mContentViewer->PageHide(aIsUnload);

        PRInt32 i, n = mChildList.Count();
        for (i = 0; i < n; i++) {
            nsCOMPtr<nsIDocShell> shell(do_QueryInterface(ChildAt(i)));
            if (shell) {
                shell->FirePageHideNotification(aIsUnload);
            }
        }
    }
    return NS_OK;
}

//
// Bug 13871: Prevent frameset spoofing
//
// This routine answers: 'Is origin's document from same domain as
// target's document?'
//
/* static */
PRBool
nsDocShell::ValidateOrigin(nsIDocShellTreeItem* aOriginTreeItem,
                           nsIDocShellTreeItem* aTargetTreeItem)
{
    nsCOMPtr<nsIScriptSecurityManager> securityManager =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
    NS_ENSURE_TRUE(securityManager, PR_FALSE);

    nsCOMPtr<nsIPrincipal> subjectPrincipal;
    nsresult rv =
        securityManager->GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    if (subjectPrincipal) {
        // We're called from JS, check if UniversalBrowserWrite is
        // enabled.
        PRBool ubwEnabled = PR_FALSE;
        rv = securityManager->IsCapabilityEnabled("UniversalBrowserWrite",
                                                  &ubwEnabled);
        NS_ENSURE_SUCCESS(rv, PR_FALSE);

        if (ubwEnabled) {
            return PR_TRUE;
        }
    }

    // Get origin document principal
    nsCOMPtr<nsIDOMDocument> originDOMDocument =
        do_GetInterface(aOriginTreeItem);
    nsCOMPtr<nsIDocument> originDocument(do_QueryInterface(originDOMDocument));
    NS_ENSURE_TRUE(originDocument, PR_FALSE);
    
    // Get target principal
    nsCOMPtr<nsIDOMDocument> targetDOMDocument =
        do_GetInterface(aTargetTreeItem);
    nsCOMPtr<nsIDocument> targetDocument(do_QueryInterface(targetDOMDocument));
    NS_ENSURE_TRUE(targetDocument, PR_FALSE);

    return
        NS_SUCCEEDED(securityManager->
                     CheckSameOriginPrincipal(originDocument->NodePrincipal(),
                                              targetDocument->NodePrincipal()));
}

NS_IMETHODIMP
nsDocShell::GetEldestPresContext(nsPresContext** aPresContext)
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
                rv = docv->GetPresContext(aPresContext);
            break;
        }
    }

    return rv;
}

NS_IMETHODIMP
nsDocShell::GetPresContext(nsPresContext ** aPresContext)
{
    NS_ENSURE_ARG_POINTER(aPresContext);
    *aPresContext = nsnull;

    if (!mContentViewer)
      return NS_OK;

    nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(mContentViewer));
    NS_ENSURE_TRUE(docv, NS_ERROR_NO_INTERFACE);

    return docv->GetPresContext(aPresContext);
}

NS_IMETHODIMP
nsDocShell::GetPresShell(nsIPresShell ** aPresShell)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(aPresShell);
    *aPresShell = nsnull;

    nsCOMPtr<nsPresContext> presContext;
    (void) GetPresContext(getter_AddRefs(presContext));

    if (presContext) {
        NS_IF_ADDREF(*aPresShell = presContext->GetPresShell());
    }

    return rv;
}

NS_IMETHODIMP
nsDocShell::GetEldestPresShell(nsIPresShell** aPresShell)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(aPresShell);
    *aPresShell = nsnull;

    nsCOMPtr<nsPresContext> presContext;
    (void) GetEldestPresContext(getter_AddRefs(presContext));

    if (presContext) {
        NS_IF_ADDREF(*aPresShell = presContext->GetPresShell());
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
nsDocShell::SetChromeEventHandler(nsIDOMEventTarget* aChromeEventHandler)
{
    nsCOMPtr<nsPIDOMEventTarget> piTarget =
      do_QueryInterface(aChromeEventHandler);
    // Weak reference. Don't addref.
    mChromeEventHandler = piTarget;

    NS_ASSERTION(!mScriptGlobal,
                 "SetChromeEventHandler() called after the script global "
                 "object was created! This means that the script global "
                 "object in this docshell won't get the right chrome event "
                 "handler. You really don't want to see this assert, FIX "
                 "YOUR CODE!");

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetChromeEventHandler(nsIDOMEventTarget** aChromeEventHandler)
{
    NS_ENSURE_ARG_POINTER(aChromeEventHandler);
    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mChromeEventHandler);
    target.swap(*aChromeEventHandler);
    return NS_OK;
}

/* [noscript] void setCurrentURI (in nsIURI uri); */
NS_IMETHODIMP
nsDocShell::SetCurrentURI(nsIURI *aURI)
{
    SetCurrentURI(aURI, nsnull, PR_TRUE);
    return NS_OK;
}

PRBool
nsDocShell::SetCurrentURI(nsIURI *aURI, nsIRequest *aRequest,
                          PRBool aFireOnLocationChange)
{
#ifdef PR_LOGGING
    if (gDocShellLeakLog && PR_LOG_TEST(gDocShellLeakLog, PR_LOG_DEBUG)) {
        nsCAutoString spec;
        if (aURI)
            aURI->GetSpec(spec);
        PR_LogPrint("DOCSHELL %p SetCurrentURI %s\n", this, spec.get());
    }
#endif

    // We don't want to send a location change when we're displaying an error
    // page, and we don't want to change our idea of "current URI" either
    if (mLoadType == LOAD_ERROR_PAGE) {
        return PR_FALSE;
    }

    mCurrentURI = NS_TryToMakeImmutable(aURI);
    
    PRBool isRoot = PR_FALSE;   // Is this the root docshell
    PRBool isSubFrame = PR_FALSE;  // Is this a subframe navigation?

    nsCOMPtr<nsIDocShellTreeItem> root;

    GetSameTypeRootTreeItem(getter_AddRefs(root));
    if (root.get() == NS_STATIC_CAST(nsIDocShellTreeItem *, this)) 
    {
        // This is the root docshell
        isRoot = PR_TRUE;
    }
    if (mLSHE) {
        mLSHE->GetIsSubFrame(&isSubFrame);
    }

    if (!isSubFrame && !isRoot) {
      /* 
       * We don't want to send OnLocationChange notifications when
       * a subframe is being loaded for the first time, while
       * visiting a frameset page
       */
      return PR_FALSE; 
    }

    if (aFireOnLocationChange) {
        FireOnLocationChange(this, aRequest, aURI);
    }
    return !aFireOnLocationChange;
}

NS_IMETHODIMP
nsDocShell::GetCharset(char** aCharset)
{
    NS_ENSURE_ARG_POINTER(aCharset);
    *aCharset = nsnull; 

    nsCOMPtr<nsIPresShell> presShell;
    GetPresShell(getter_AddRefs(presShell));
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
    nsIDocument *doc = presShell->GetDocument();
    NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
    *aCharset = ToNewCString(doc->GetDocumentCharacterSet());
    if (!*aCharset) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetCharset(const char* aCharset)
{
    // set the default charset
    nsCOMPtr<nsIContentViewer> viewer;
    GetContentViewer(getter_AddRefs(viewer));
    if (viewer) {
      nsCOMPtr<nsIMarkupDocumentViewer> muDV(do_QueryInterface(viewer));
      if (muDV) {
        NS_ENSURE_SUCCESS(muDV->SetDefaultCharacterSet(nsDependentCString(aCharset)),
                          NS_ERROR_FAILURE);
      }
    }

    // set the charset override
    nsCOMPtr<nsIDocumentCharsetInfo> dcInfo;
    GetDocumentCharsetInfo(getter_AddRefs(dcInfo));
    if (dcInfo) {
      nsCOMPtr<nsIAtom> csAtom;
      csAtom = do_GetAtom(aCharset);
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
        mDocumentCharsetInfo = do_CreateInstance(NS_DOCUMENTCHARSETINFO_CONTRACTID);
        if (!mDocumentCharsetInfo)
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
    
    nsRefPtr<nsDocShellEnumerator> docShellEnum;
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

    rv = docShellEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)outEnum);

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
nsDocShell::GetAllowAuth(PRBool * aAllowAuth)
{
    *aAllowAuth = mAllowAuth;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowAuth(PRBool aAllowAuth)
{
    mAllowAuth = aAllowAuth;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetZoom(float *zoom)
{
    NS_ENSURE_ARG_POINTER(zoom);
    *zoom = 1.0f;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetZoom(float zoom)
{
    return NS_ERROR_NOT_IMPLEMENTED;
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
        if (aForward)
            *aTookFocus = NS_SUCCEEDED(chromeFocus->FocusNextElement());
        else
            *aTookFocus = NS_SUCCEEDED(chromeFocus->FocusPrevElement());
    } else
        *aTookFocus = PR_FALSE;
    
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSecurityUI(nsISecureBrowserUI **aSecurityUI)
{
    NS_IF_ADDREF(*aSecurityUI = mSecurityUI);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetSecurityUI(nsISecureBrowserUI *aSecurityUI)
{
    mSecurityUI = aSecurityUI;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetUseErrorPages(PRBool *aUseErrorPages)
{
    *aUseErrorPages = mUseErrorPages;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetUseErrorPages(PRBool aUseErrorPages)
{
    // If mUseErrorPages is set explicitly, stop observing the pref.
    if (mObserveErrorPages) {
        nsCOMPtr<nsIPrefBranch2> prefs(do_QueryInterface(mPrefs));
        if (prefs) {
            prefs->RemoveObserver("browser.xul.error_pages.enabled", this);
            mObserveErrorPages = PR_FALSE;
        }
    }
    mUseErrorPages = aUseErrorPages;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetPreviousTransIndex(PRInt32 *aPreviousTransIndex)
{
    *aPreviousTransIndex = mPreviousTransIndex;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetLoadedTransIndex(PRInt32 *aLoadedTransIndex)
{
    *aLoadedTransIndex = mLoadedTransIndex;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::HistoryPurged(PRInt32 aNumEntries)
{
    // These indices are used for fastback cache eviction, to determine
    // which session history entries are candidates for content viewer
    // eviction.  We need to adjust by the number of entries that we
    // just purged from history, so that we look at the right session history
    // entries during eviction.
    mPreviousTransIndex = PR_MAX(-1, mPreviousTransIndex - aNumEntries);
    mLoadedTransIndex = PR_MAX(0, mLoadedTransIndex - aNumEntries);

    PRInt32 count = mChildList.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIDocShell> shell = do_QueryInterface(ChildAt(i));
        if (shell) {
            shell->HistoryPurged(aNumEntries);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSessionStorageForURI(nsIURI* aURI,
                                    nsIDOMStorage** aStorage)
{
    NS_ENSURE_ARG_POINTER(aStorage);

    *aStorage = nsnull;

    nsCOMPtr<nsIDocShellTreeItem> topItem;
    nsresult rv = GetSameTypeRootTreeItem(getter_AddRefs(topItem));
    if (NS_FAILED(rv))
        return rv;

    if (!topItem)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDocShell> topDocShell = do_QueryInterface(topItem);
    if (topDocShell != this)
        return topDocShell->GetSessionStorageForURI(aURI, aStorage);

    nsCAutoString currentDomain;
    rv = aURI->GetAsciiHost(currentDomain);
    NS_ENSURE_SUCCESS(rv, rv);

    if (currentDomain.IsEmpty())
        return NS_OK;

    if (!mStorages.Get(currentDomain, aStorage)) {
        nsCOMPtr<nsIDOMStorage> newstorage =
            do_CreateInstance("@mozilla.org/dom/storage;1");
        if (!newstorage)
            return NS_ERROR_OUT_OF_MEMORY;

        nsCOMPtr<nsPIDOMStorage> pistorage = do_QueryInterface(newstorage);
        if (!pistorage)
            return NS_ERROR_FAILURE;
        pistorage->Init(aURI, NS_ConvertUTF8toUTF16(currentDomain), PR_FALSE);

        if (!mStorages.Put(currentDomain, newstorage))
            return NS_ERROR_OUT_OF_MEMORY;
		
        *aStorage = newstorage;
        NS_ADDREF(*aStorage);
    }

    return NS_OK;
}

nsresult
nsDocShell::AddSessionStorage(const nsACString& aDomain,
                              nsIDOMStorage* aStorage)
{
    NS_ENSURE_ARG_POINTER(aStorage);

    if (aDomain.IsEmpty())
        return NS_OK;

    nsCOMPtr<nsIDocShellTreeItem> topItem;
    nsresult rv = GetSameTypeRootTreeItem(getter_AddRefs(topItem));
    if (NS_FAILED(rv))
        return rv;

    if (topItem) {
        nsCOMPtr<nsIDocShell> topDocShell = do_QueryInterface(topItem);
        if (topDocShell == this) {
            if (!mStorages.Put(aDomain, aStorage))
                return NS_ERROR_OUT_OF_MEMORY;
        }
        else {
            return topDocShell->AddSessionStorage(aDomain, aStorage);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetCurrentDocumentChannel(nsIChannel** aResult)
{
    *aResult = nsnull;
    if (!mContentViewer)
        return NS_OK;

    nsCOMPtr<nsIDOMDocument> domDoc;
    nsresult rv = mContentViewer->GetDOMDocument(getter_AddRefs(domDoc));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
    if (doc) {
      *aResult = doc->GetChannel();
      NS_IF_ADDREF(*aResult);
    }
  
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

    // Only allow setting the type on root docshells.  Those would be the ones
    // that have the docloader service as mParent or have no mParent at all.
    nsCOMPtr<nsIDocumentLoader> docLoaderService =
        do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID);
    NS_ENSURE_TRUE(docLoaderService, NS_ERROR_UNEXPECTED);
    
    NS_ENSURE_STATE(!mParent || mParent == docLoaderService);

    mItemType = aItemType;

    // disable auth prompting for anything but content
    mAllowAuth = mItemType == typeContent; 

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetParent(nsIDocShellTreeItem ** aParent)
{
    if (!mParent) {
        *aParent = nsnull;
    } else {
        CallQueryInterface(mParent, aParent);
    }
    // Note that in the case when the parent is not an nsIDocShellTreeItem we
    // don't want to throw; we just want to return null.
    return NS_OK;
}

nsresult
nsDocShell::SetDocLoaderParent(nsDocLoader * aParent)
{
    nsDocLoader::SetDocLoaderParent(aParent);

    // Curse ambiguous nsISupports inheritance!
    nsISupports* parent = GetAsSupports(aParent);

    // If parent is another docshell, we inherit all their flags for
    // allowing plugins, scripting etc.
    nsCOMPtr<nsIDocShell> parentAsDocShell(do_QueryInterface(parent));
    if (parentAsDocShell)
    {
        PRBool value;
        if (NS_SUCCEEDED(parentAsDocShell->GetAllowPlugins(&value)))
        {
            SetAllowPlugins(value);
        }
        if (NS_SUCCEEDED(parentAsDocShell->GetAllowJavascript(&value)))
        {
            SetAllowJavascript(value);
        }
        if (NS_SUCCEEDED(parentAsDocShell->GetAllowMetaRedirects(&value)))
        {
            SetAllowMetaRedirects(value);
        }
        if (NS_SUCCEEDED(parentAsDocShell->GetAllowSubframes(&value)))
        {
            SetAllowSubframes(value);
        }
        if (NS_SUCCEEDED(parentAsDocShell->GetAllowImages(&value)))
        {
            SetAllowImages(value);
        }
    }

    nsCOMPtr<nsIURIContentListener> parentURIListener(do_GetInterface(parent));
    if (parentURIListener)
        mContentListener->SetParentContentListener(parentURIListener);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSameTypeParent(nsIDocShellTreeItem ** aParent)
{
    NS_ENSURE_ARG_POINTER(aParent);
    *aParent = nsnull;

    nsCOMPtr<nsIDocShellTreeItem> parent =
        do_QueryInterface(GetAsSupports(mParent));
    if (!parent)
        return NS_OK;

    PRInt32 parentType;
    NS_ENSURE_SUCCESS(parent->GetItemType(&parentType), NS_ERROR_FAILURE);

    if (parentType == mItemType) {
        parent.swap(*aParent);
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

/* static */
PRBool
nsDocShell::CanAccessItem(nsIDocShellTreeItem* aTargetItem,
                          nsIDocShellTreeItem* aAccessingItem,
                          PRBool aConsiderOpener)
{
    NS_PRECONDITION(aTargetItem, "Must have target item!");

    if (!gValidateOrigin || !aAccessingItem) {
        // Good to go
        return PR_TRUE;
    }

    // XXXbz should we care if aAccessingItem or the document therein is
    // chrome?  Should those get extra privileges?

    // Now do a security check
    // Bug 13871: Prevent frameset spoofing
    //     See BugSplat 336170, 338737 and XP_FindNamedContextInList in
    //     the classic codebase
    //     Nav's behaviour was:
    //         - pref controlled: "browser.frame.validate_origin" 
    //           (gValidateOrigin)
    //         - allow load if host of target or target's parent is same
    //           as host of origin
    //         - allow load if target is a top level window
    
    //     We are going to be a little more restrictive, with the
    //     following algorithm:
    //         - pref controlled in the same way
    //         - allow access if the two treeitems are in the same tree
    //         - allow access if the aTargetItem or one of its ancestors
    //           has the same origin as aAccessingItem
    //         - allow access if the target is a toplevel window and we can
    //           access its opener.  Note that we only allow one level of
    //           recursion there.

    nsCOMPtr<nsIDocShellTreeItem> targetRoot;
    aTargetItem->GetSameTypeRootTreeItem(getter_AddRefs(targetRoot));

    nsCOMPtr<nsIDocShellTreeItem> accessingRoot;
    aAccessingItem->GetSameTypeRootTreeItem(getter_AddRefs(accessingRoot));

    if (targetRoot == accessingRoot) {
        return PR_TRUE;
    }

    nsCOMPtr<nsIDocShellTreeItem> target = aTargetItem;
    do {
        if (ValidateOrigin(aAccessingItem, target)) {
            return PR_TRUE;
        }
            
        nsCOMPtr<nsIDocShellTreeItem> parent;
        target->GetSameTypeParent(getter_AddRefs(parent));
        parent.swap(target);
    } while (target);

    if (aTargetItem != targetRoot) {
        // target is a subframe, not in accessor's frame hierarchy, and all its
        // ancestors have origins different from that of the accessor. Don't
        // allow access.
        return PR_FALSE;
    }

    if (!aConsiderOpener) {
        // All done here
        return PR_FALSE;
    }

    nsCOMPtr<nsIDOMWindow> targetWindow(do_GetInterface(aTargetItem));
    nsCOMPtr<nsIDOMWindowInternal> targetInternal(do_QueryInterface(targetWindow));
    if (!targetInternal) {
        NS_ERROR("This should not happen, really");
        return PR_FALSE;
    }

    nsCOMPtr<nsIDOMWindowInternal> targetOpener;
    targetInternal->GetOpener(getter_AddRefs(targetOpener));
    nsCOMPtr<nsIWebNavigation> openerWebNav(do_GetInterface(targetOpener));
    nsCOMPtr<nsIDocShellTreeItem> openerItem(do_QueryInterface(openerWebNav));

    if (!openerItem) {
        return PR_FALSE;
    }

    return CanAccessItem(openerItem, aAccessingItem, PR_FALSE);    
}

static PRBool
ItemIsActive(nsIDocShellTreeItem *aItem)
{
    nsCOMPtr<nsIDOMWindow> tmp(do_GetInterface(aItem));
    nsCOMPtr<nsIDOMWindowInternal> window(do_QueryInterface(tmp));

    if (window) {
        PRBool isClosed;

        if (NS_SUCCEEDED(window->GetClosed(&isClosed)) && !isClosed) {
            return PR_TRUE;
        }
    }

    return PR_FALSE;
}

NS_IMETHODIMP
nsDocShell::FindItemWithName(const PRUnichar * aName,
                             nsISupports * aRequestor,
                             nsIDocShellTreeItem * aOriginalRequestor,
                             nsIDocShellTreeItem ** _retval)
{
    NS_ENSURE_ARG(aName);
    NS_ENSURE_ARG_POINTER(_retval);

    // If we don't find one, we return NS_OK and a null result
    *_retval = nsnull;

    if (!*aName)
        return NS_OK;

    if (!aRequestor)
    {
        nsCOMPtr<nsIDocShellTreeItem> foundItem;

        // This is the entry point into the target-finding algorithm.  Check
        // for special names.  This should only be done once, hence the check
        // for a null aRequestor.

        nsDependentString name(aName);
        if (name.LowerCaseEqualsLiteral("_self")) {
            foundItem = this;
        }
        else if (name.LowerCaseEqualsLiteral("_blank") ||
                 name.LowerCaseEqualsLiteral("_new"))
        {
            // Just return null.  Caller must handle creating a new window with
            // a blank name himself.
            return NS_OK;
        }
        else if (name.LowerCaseEqualsLiteral("_parent"))
        {
            GetSameTypeParent(getter_AddRefs(foundItem));
            if(!foundItem)
                foundItem = this;
        }
        else if (name.LowerCaseEqualsLiteral("_top"))
        {
            GetSameTypeRootTreeItem(getter_AddRefs(foundItem));
            NS_ASSERTION(foundItem, "Must have this; worst case it's us!");
        }
        // _main is an IE target which should be case-insensitive but isn't
        // see bug 217886 for details
        else if (name.LowerCaseEqualsLiteral("_content") ||
                 name.EqualsLiteral("_main"))
        {
            // Must pass our same type root as requestor to the
            // treeowner to make sure things work right.
            nsCOMPtr<nsIDocShellTreeItem> root;
            GetSameTypeRootTreeItem(getter_AddRefs(root));
            if (mTreeOwner) {
                NS_ASSERTION(root, "Must have this; worst case it's us!");
                mTreeOwner->FindItemWithName(aName, root, aOriginalRequestor,
                                             getter_AddRefs(foundItem));
            }
#ifdef DEBUG
            else {
                NS_ERROR("Someone isn't setting up the tree owner.  "
                         "You might like to try that.  "
                         "Things will.....you know, work.");
                // Note: _content should always exist.  If we don't have one
                // hanging off the treeowner, just create a named window....
                // so don't return here, in case we did that and can now find
                // it.                
                // XXXbz should we be using |root| instead of creating
                // a new window?
            }
#endif
        }

        if (foundItem && !CanAccessItem(foundItem, aOriginalRequestor)) {
            foundItem = nsnull;
        }

        if (foundItem) {
            // We return foundItem here even if it's not an active
            // item since all the names we've dealt with so far are
            // special cases that we won't bother looking for further.

            foundItem.swap(*_retval);
            return NS_OK;
        }
    }

    // Keep looking
        
    // First we check our name.
    if (mName.Equals(aName) && ItemIsActive(this) &&
        CanAccessItem(this, aOriginalRequestor)) {
        NS_ADDREF(*_retval = this);
        return NS_OK;
    }

    // This QI may fail, but the places where we want to compare, comparing
    // against nsnull serves the same purpose.
    nsCOMPtr<nsIDocShellTreeItem> reqAsTreeItem(do_QueryInterface(aRequestor));

    // Second we check our children making sure not to ask a child if
    // it is the aRequestor.
#ifdef DEBUG
    nsresult rv =
#endif
    FindChildWithName(aName, PR_TRUE, PR_TRUE, reqAsTreeItem,
                      aOriginalRequestor, _retval);
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "FindChildWithName should not be failing here.");
    if (*_retval)
        return NS_OK;
        
    // Third if we have a parent and it isn't the requestor then we
    // should ask it to do the search.  If it is the requestor we
    // should just stop here and let the parent do the rest.  If we
    // don't have a parent, then we should ask the
    // docShellTreeOwner to do the search.
    nsCOMPtr<nsIDocShellTreeItem> parentAsTreeItem =
        do_QueryInterface(GetAsSupports(mParent));
    if (parentAsTreeItem) {
        if (parentAsTreeItem == reqAsTreeItem)
            return NS_OK;

        PRInt32 parentType;
        parentAsTreeItem->GetItemType(&parentType);
        if (parentType == mItemType) {
            return parentAsTreeItem->
                FindItemWithName(aName,
                                 NS_STATIC_CAST(nsIDocShellTreeItem*,
                                                this),
                                 aOriginalRequestor,
                                 _retval);
        }
    }

    // If the parent is null or not of the same type fall through and ask tree
    // owner.

    // This may fail, but comparing against null serves the same purpose
    nsCOMPtr<nsIDocShellTreeOwner>
        reqAsTreeOwner(do_QueryInterface(aRequestor));

    if (mTreeOwner && mTreeOwner != reqAsTreeOwner) {
        return mTreeOwner->
            FindItemWithName(aName, this, aOriginalRequestor, _retval);
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
  nsCOMPtr<nsPresContext> presContext;
  parentAsDocShell->GetPresContext(getter_AddRefs(presContext));
  nsIDocument *doc = presShell->GetDocument();

  nsCOMPtr<nsIDOMWindowInternal> domwin(doc->GetWindow());

  nsCOMPtr<nsIWidget> widget;
  nsIViewManager* vm = presShell->GetViewManager();
  if (vm) {
    vm->GetWidget(getter_AddRefs(widget));
  }
  nsIContent* rootContent = doc->GetRootContent();

  printf("DS %p  Ty %s  Doc %p DW %p EM %p CN %p\n",  
    (void*)parentAsDocShell.get(), 
    type==nsIDocShellTreeItem::typeChrome?"Chr":"Con", 
     (void*)doc, (void*)domwin.get(),
     (void*)presContext->EventStateManager(), (void*)rootContent);

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
        nsCOMPtr<nsIWebProgress> webProgress =
            do_QueryInterface(GetAsSupports(this));

        if (webProgress) {
            nsCOMPtr<nsIWebProgressListener>
                oldListener(do_QueryInterface(mTreeOwner));
            nsCOMPtr<nsIWebProgressListener>
                newListener(do_QueryInterface(aTreeOwner));

            if (oldListener) {
                webProgress->RemoveProgressListener(oldListener);
            }

            if (newListener) {
                webProgress->AddProgressListener(newListener,
                                                 nsIWebProgress::NOTIFY_ALL);
            }
        }
    }

    mTreeOwner = aTreeOwner;    // Weak reference per API

    PRInt32 i, n = mChildList.Count();
    for (i = 0; i < n; i++) {
        nsCOMPtr<nsIDocShellTreeItem> child = do_QueryInterface(ChildAt(i));
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
    *aChildCount = mChildList.Count();
    return NS_OK;
}



NS_IMETHODIMP
nsDocShell::AddChild(nsIDocShellTreeItem * aChild)
{
    NS_ENSURE_ARG_POINTER(aChild);

    nsRefPtr<nsDocLoader> childAsDocLoader = GetAsDocLoader(aChild);
    NS_ENSURE_TRUE(childAsDocLoader, NS_ERROR_UNEXPECTED);

    // Make sure we're not creating a loop in the docshell tree
    nsDocLoader* ancestor = this;
    do {
        if (childAsDocLoader == ancestor) {
            return NS_ERROR_ILLEGAL_VALUE;
        }
        ancestor = ancestor->GetParent();
    } while (ancestor);
    
    // Make sure to remove the child from its current parent.
    nsDocLoader* childsParent = childAsDocLoader->GetParent();
    if (childsParent) {
        childsParent->RemoveChildLoader(childAsDocLoader);
    }

    // Make sure to clear the treeowner in case this child is a different type
    // from us.
    aChild->SetTreeOwner(nsnull);
    
    nsresult res = AddChildLoader(childAsDocLoader);
    NS_ENSURE_SUCCESS(res, res);

    // Set the child's index in the parent's children list 
    // XXX What if the parent had different types of children?
    // XXX in that case docshell hierarchy and SH hierarchy won't match.
    aChild->SetChildOffset(mChildList.Count() - 1);

    /* Set the child's global history if the parent has one */
    if (mGlobalHistory) {
        nsCOMPtr<nsIDocShellHistory>
            dsHistoryChild(do_QueryInterface(aChild));
        if (dsHistoryChild)
            dsHistoryChild->SetUseGlobalHistory(PR_TRUE);
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

    // charset, style-disabling, and zoom will be inherited in SetupNewViewer()

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
    res = docv->GetDocument(getter_AddRefs(doc));
    if (NS_FAILED(res) || (!doc))
        return NS_OK;
    const nsACString &parentCS = doc->GetDocumentCharacterSet();

    PRBool isWyciwyg = PR_FALSE;

    if (mCurrentURI) {
        // Check if the url is wyciwyg
        mCurrentURI->SchemeIs("wyciwyg", &isWyciwyg);      
    }

    if (!isWyciwyg) {
        // If this docshell is loaded from a wyciwyg: URI, don't
        // advertise our charset since it does not in any way reflect
        // the actual source charset, which is what we're trying to
        // expose here.

        // set the child's parentCharset
        nsCOMPtr<nsIAtom> parentCSAtom(do_GetAtom(parentCS));
        res = dcInfo->SetParentCharset(parentCSAtom);
        if (NS_FAILED(res))
            return NS_OK;

        PRInt32 charsetSource = doc->GetDocumentCharacterSetSource();

        // set the child's parentCharset
        res = dcInfo->SetParentCharsetSource(charsetSource);
        if (NS_FAILED(res))
            return NS_OK;
    }

    // printf("### 1 >>> Adding child. Parent CS = %s. ItemType = %d.\n", NS_LossyConvertUTF16toASCII(parentCS).get(), mItemType);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::RemoveChild(nsIDocShellTreeItem * aChild)
{
    NS_ENSURE_ARG_POINTER(aChild);

    nsRefPtr<nsDocLoader> childAsDocLoader = GetAsDocLoader(aChild);
    NS_ENSURE_TRUE(childAsDocLoader, NS_ERROR_UNEXPECTED);
    
    nsresult rv = RemoveChildLoader(childAsDocLoader);
    NS_ENSURE_SUCCESS(rv, rv);
    
    aChild->SetTreeOwner(nsnull);

    return nsDocLoader::AddDocLoaderAsChildOfRoot(childAsDocLoader);
}

NS_IMETHODIMP
nsDocShell::GetChildAt(PRInt32 aIndex, nsIDocShellTreeItem ** aChild)
{
    NS_ENSURE_ARG_POINTER(aChild);

#ifdef DEBUG
    if (aIndex < 0) {
      NS_WARNING("Negative index passed to GetChildAt");
    }
    else if (aIndex >= mChildList.Count()) {
      NS_WARNING("Too large an index passed to GetChildAt");
    }
#endif

    nsIDocumentLoader* child = SafeChildAt(aIndex);
    NS_ENSURE_TRUE(child, NS_ERROR_UNEXPECTED);
    
    return CallQueryInterface(child, aChild);
}

NS_IMETHODIMP
nsDocShell::FindChildWithName(const PRUnichar * aName,
                              PRBool aRecurse, PRBool aSameType,
                              nsIDocShellTreeItem * aRequestor,
                              nsIDocShellTreeItem * aOriginalRequestor,
                              nsIDocShellTreeItem ** _retval)
{
    NS_ENSURE_ARG(aName);
    NS_ENSURE_ARG_POINTER(_retval);

    *_retval = nsnull;          // if we don't find one, we return NS_OK and a null result 

    if (!*aName)
        return NS_OK;

    nsXPIDLString childName;
    PRInt32 i, n = mChildList.Count();
    for (i = 0; i < n; i++) {
        nsCOMPtr<nsIDocShellTreeItem> child = do_QueryInterface(ChildAt(i));
        NS_ENSURE_TRUE(child, NS_ERROR_FAILURE);
        PRInt32 childType;
        child->GetItemType(&childType);

        if (aSameType && (childType != mItemType))
            continue;

        PRBool childNameEquals = PR_FALSE;
        child->NameEquals(aName, &childNameEquals);
        if (childNameEquals && ItemIsActive(child) &&
            CanAccessItem(child, aOriginalRequestor)) {
            child.swap(*_retval);
            break;
        }

        if (childType != mItemType)     //Only ask it to check children if it is same type
            continue;

        if (aRecurse && (aRequestor != child))  // Only ask the child if it isn't the requestor
        {
            // See if child contains the shell with the given name
            nsCOMPtr<nsIDocShellTreeNode>
                childAsNode(do_QueryInterface(child));
            if (childAsNode) {
#ifdef DEBUG
                nsresult rv =
#endif
                childAsNode->FindChildWithName(aName, PR_TRUE,
                                               aSameType,
                                               NS_STATIC_CAST(nsIDocShellTreeItem*,
                                                              this),
                                               aOriginalRequestor,
                                               _retval);
                NS_ASSERTION(NS_SUCCEEDED(rv),
                             "FindChildWithName should not fail here");
                if (*_retval)           // found it
                    return NS_OK;
            }
        }
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
            loadType == nsIDocShellLoadInfo::loadReloadBypassProxyAndCache ||
            loadType == nsIDocShellLoadInfo::loadRefresh)
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
    else if (!aCloneRef) {
        /* This is an initial load in some subframe.  Just append it if we can */
        nsCOMPtr<nsISHContainer> container(do_QueryInterface(mOSHE, &rv));
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
            nsCOMPtr<nsISHEntry> nextEntry;
            aCloneRef->GetID(&cloneID);
            rv = CloneAndReplace(currentEntry, this, cloneID, aNewEntry,
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
        /* Just pass this along */
        nsCOMPtr<nsIDocShellHistory> parent =
            do_QueryInterface(GetAsSupports(mParent), &rv);
        if (parent) {
            rv = parent->AddChildSHEntry(aCloneRef, aNewEntry, aChildOffset);
        }          
    }
    return rv;
}

nsresult
nsDocShell::DoAddChildSHEntry(nsISHEntry* aNewEntry, PRInt32 aChildOffset)
{
    /* You will get here when you are in a subframe and
     * a new url has been loaded on you. 
     * The mOSHE in this subframe will be the previous url's
     * mOSHE. This mOSHE will be used as the identification
     * for this subframe in the  CloneAndReplace function.
     */

    // In this case, we will end up calling AddEntry, which increases the
    // current index by 1
    nsCOMPtr<nsISHistory> rootSH;
    GetRootSessionHistory(getter_AddRefs(rootSH));
    if (rootSH) {
        rootSH->GetIndex(&mPreviousTransIndex);
    }

    nsresult rv;
    nsCOMPtr<nsIDocShellHistory> parent =
        do_QueryInterface(GetAsSupports(mParent), &rv);
    if (parent) {
        rv = parent->AddChildSHEntry(mOSHE, aNewEntry, aChildOffset);
    }


    if (rootSH) {
        rootSH->GetIndex(&mLoadedTransIndex);
#ifdef DEBUG_PAGE_CACHE
        printf("Previous index: %d, Loaded index: %d\n\n", mPreviousTransIndex,
               mLoadedTransIndex);
#endif
    }

    return rv;
}

NS_IMETHODIMP
nsDocShell::SetUseGlobalHistory(PRBool aUseGlobalHistory)
{
    nsresult rv;

    if (!aUseGlobalHistory) {
        mGlobalHistory = nsnull;
        return NS_OK;
    }

    if (mGlobalHistory) {
        return NS_OK;
    }

    mGlobalHistory = do_GetService(NS_GLOBALHISTORY2_CONTRACTID, &rv);
    return rv;
}

NS_IMETHODIMP
nsDocShell::GetUseGlobalHistory(PRBool *aUseGlobalHistory)
{
    *aUseGlobalHistory = (mGlobalHistory != nsnull);
    return NS_OK;
}

//-------------------------------------
//-- Helper Method for Print discovery
//-------------------------------------
PRBool 
nsDocShell::IsPrintingOrPP(PRBool aDisplayErrorDialog)
{
  if (mIsPrintingOrPP && aDisplayErrorDialog) {
    DisplayLoadError(NS_ERROR_DOCUMENT_IS_PRINTMODE, nsnull, nsnull);
  }

  return mIsPrintingOrPP;
}

//*****************************************************************************
// nsDocShell::nsIWebNavigation
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::GetCanGoBack(PRBool * aCanGoBack)
{
    if (IsPrintingOrPP(PR_FALSE)) {
      *aCanGoBack = PR_FALSE;
      return NS_OK; // JS may not handle returning of an error code
    }
    nsresult rv;
    nsCOMPtr<nsISHistory> rootSH;
    rv = GetRootSessionHistory(getter_AddRefs(rootSH));
    nsCOMPtr<nsIWebNavigation> webnav(do_QueryInterface(rootSH));
    NS_ENSURE_TRUE(webnav, NS_ERROR_FAILURE);
    rv = webnav->GetCanGoBack(aCanGoBack);   
    return rv;

}

NS_IMETHODIMP
nsDocShell::GetCanGoForward(PRBool * aCanGoForward)
{
    if (IsPrintingOrPP(PR_FALSE)) {
      *aCanGoForward = PR_FALSE;
      return NS_OK; // JS may not handle returning of an error code
    }
    nsresult rv;
    nsCOMPtr<nsISHistory> rootSH;
    rv = GetRootSessionHistory(getter_AddRefs(rootSH)); 
    nsCOMPtr<nsIWebNavigation> webnav(do_QueryInterface(rootSH));
    NS_ENSURE_TRUE(webnav, NS_ERROR_FAILURE);
    rv = webnav->GetCanGoForward(aCanGoForward);
    return rv;

}

NS_IMETHODIMP
nsDocShell::GoBack()
{
    if (IsPrintingOrPP()) {
      return NS_OK; // JS may not handle returning of an error code
    }
    nsresult rv;
    nsCOMPtr<nsISHistory> rootSH;
    rv = GetRootSessionHistory(getter_AddRefs(rootSH));
    nsCOMPtr<nsIWebNavigation> webnav(do_QueryInterface(rootSH));
    NS_ENSURE_TRUE(webnav, NS_ERROR_FAILURE);
    rv = webnav->GoBack();
    return rv;

}

NS_IMETHODIMP
nsDocShell::GoForward()
{
    if (IsPrintingOrPP()) {
      return NS_OK; // JS may not handle returning of an error code
    }
    nsresult rv;
    nsCOMPtr<nsISHistory> rootSH;
    rv = GetRootSessionHistory(getter_AddRefs(rootSH));
    nsCOMPtr<nsIWebNavigation> webnav(do_QueryInterface(rootSH));
    NS_ENSURE_TRUE(webnav, NS_ERROR_FAILURE);
    rv = webnav->GoForward();
    return rv;

}

NS_IMETHODIMP nsDocShell::GotoIndex(PRInt32 aIndex)
{
    if (IsPrintingOrPP()) {
      return NS_OK; // JS may not handle returning of an error code
    }
    nsresult rv;
    nsCOMPtr<nsISHistory> rootSH;
    rv = GetRootSessionHistory(getter_AddRefs(rootSH));
    nsCOMPtr<nsIWebNavigation> webnav(do_QueryInterface(rootSH));
    NS_ENSURE_TRUE(webnav, NS_ERROR_FAILURE);
    rv = webnav->GotoIndex(aIndex);
    return rv;

}


NS_IMETHODIMP
nsDocShell::LoadURI(const PRUnichar * aURI,
                    PRUint32 aLoadFlags,
                    nsIURI * aReferringURI,
                    nsIInputStream * aPostStream,
                    nsIInputStream * aHeaderStream)
{
    if (IsPrintingOrPP()) {
      return NS_OK; // JS may not handle returning of an error code
    }
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_OK;

    // Create a URI from our string; if that succeeds, we want to
    // change aLoadFlags to not include the ALLOW_THIRD_PARTY_FIXUP
    // flag.

    NS_ConvertUTF16toUTF8 uriString(aURI);
    // Cleanup the empty spaces that might be on each end.
    uriString.Trim(" ");
    // Eliminate embedded newlines, which single-line text fields now allow:
    uriString.StripChars("\r\n");
    NS_ENSURE_TRUE(!uriString.IsEmpty(), NS_ERROR_FAILURE);

    rv = NS_NewURI(getter_AddRefs(uri), uriString);
    if (uri) {
        aLoadFlags &= ~LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
    }
    
    if (sURIFixup) {
        // Call the fixup object.  This will clobber the rv from NS_NewURI
        // above, but that's fine with us.  Note that we need to do this even
        // if NS_NewURI returned a URI, because fixup handles nested URIs, etc
        // (things like view-source:mozilla.org for example).
        PRUint32 fixupFlags = 0;
        if (aLoadFlags & LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP) {
          fixupFlags |= nsIURIFixup::FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
        }
        rv = sURIFixup->CreateFixupURI(uriString, fixupFlags,
                                       getter_AddRefs(uri));
    }
    // else no fixup service so just use the URI we created and see
    // what happens

    if (NS_ERROR_MALFORMED_URI == rv) {
        DisplayLoadError(rv, uri, aURI);
    }

    if (NS_FAILED(rv) || !uri)
        return NS_ERROR_FAILURE;

    // Don't pass the fixup flag to MAKE_LOAD_TYPE, since it isn't needed and
    // confuses ConvertLoadTypeToDocShellLoadInfo. We do need to ensure that
    // it is passed to LoadURI though, since it uses it to determine whether it
    // can do fixup.
    PRUint32 fixupFlag = (aLoadFlags & LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP);
    aLoadFlags &= ~LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;

    nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
    rv = CreateLoadInfo(getter_AddRefs(loadInfo));
    if (NS_FAILED(rv)) return rv;
    
    PRUint32 loadType = MAKE_LOAD_TYPE(LOAD_NORMAL, aLoadFlags);
    loadInfo->SetLoadType(ConvertLoadTypeToDocShellLoadInfo(loadType));
    loadInfo->SetPostDataStream(aPostStream);
    loadInfo->SetReferrer(aReferringURI);
    loadInfo->SetHeadersStream(aHeaderStream);

    rv = LoadURI(uri, loadInfo, fixupFlag, PR_TRUE);

    return rv;
}

NS_IMETHODIMP
nsDocShell::DisplayLoadError(nsresult aError, nsIURI *aURI,
                             const PRUnichar *aURL,
                             nsIChannel* aFailedChannel)
{
    // Get prompt and string bundle servcies
    nsCOMPtr<nsIPrompt> prompter;
    nsCOMPtr<nsIStringBundle> stringBundle;
    GetPromptAndStringBundle(getter_AddRefs(prompter),
                             getter_AddRefs(stringBundle));

    NS_ENSURE_TRUE(stringBundle, NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(prompter, NS_ERROR_FAILURE);

    nsAutoString error;
    const PRUint32 kMaxFormatStrArgs = 2;
    nsAutoString formatStrs[kMaxFormatStrArgs];
    PRUint32 formatStrCount = 0;
    nsresult rv = NS_OK;
    nsAutoString messageStr;

    // Turn the error code into a human readable error message.
    if (NS_ERROR_UNKNOWN_PROTOCOL == aError) {
        NS_ENSURE_ARG_POINTER(aURI);
        // extract the scheme
        nsCAutoString scheme;
        aURI->GetScheme(scheme);
        CopyASCIItoUTF16(scheme, formatStrs[0]);
        formatStrCount = 1;
        error.AssignLiteral("protocolNotFound");
    }
    else if (NS_ERROR_FILE_NOT_FOUND == aError) {
        NS_ENSURE_ARG_POINTER(aURI);
        nsCAutoString spec;
        // displaying "file://" is aesthetically unpleasing and could even be
        // confusing to the user
        PRBool isFileURI = PR_FALSE;
        rv = aURI->SchemeIs("file", &isFileURI);
        if (NS_FAILED(rv))
            return rv;
        if (isFileURI)
            aURI->GetPath(spec);
        else
            aURI->GetSpec(spec);
        nsCAutoString charset;
        // unescape and convert from origin charset
        aURI->GetOriginCharset(charset);
        nsCOMPtr<nsITextToSubURI> textToSubURI(
                do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv));
        if (NS_SUCCEEDED(rv))
            // UnEscapeURIForUI always succeeds 
            textToSubURI->UnEscapeURIForUI(charset, spec, formatStrs[0]);
        else 
            CopyUTF8toUTF16(spec, formatStrs[0]);
        rv = NS_OK;
        formatStrCount = 1;
        error.AssignLiteral("fileNotFound");
    }
    else if (NS_ERROR_UNKNOWN_HOST == aError) {
        NS_ENSURE_ARG_POINTER(aURI);
        // Get the host
        nsCAutoString host;
        aURI->GetHost(host);
        CopyUTF8toUTF16(host, formatStrs[0]);
        formatStrCount = 1;
        error.AssignLiteral("dnsNotFound");
    }
    else if(NS_ERROR_CONNECTION_REFUSED == aError) {
        NS_ENSURE_ARG_POINTER(aURI);
        // Build up the host:port string.
        nsCAutoString hostport;
        aURI->GetHostPort(hostport);
        CopyUTF8toUTF16(hostport, formatStrs[0]);
        formatStrCount = 1;
        error.AssignLiteral("connectionFailure");
    }
    else if(NS_ERROR_NET_INTERRUPT == aError) {
        NS_ENSURE_ARG_POINTER(aURI);
        // Build up the host:port string.
        nsCAutoString hostport;
        aURI->GetHostPort(hostport);
        CopyUTF8toUTF16(hostport, formatStrs[0]);
        formatStrCount = 1;
        error.AssignLiteral("netInterrupt");
    }
    else if (NS_ERROR_NET_TIMEOUT == aError) {
        NS_ENSURE_ARG_POINTER(aURI);
        // Get the host
        nsCAutoString host;
        aURI->GetHost(host);
        CopyUTF8toUTF16(host, formatStrs[0]);
        formatStrCount = 1;
        error.AssignLiteral("netTimeout");
    }
    else if (NS_ERROR_GET_MODULE(aError) == NS_ERROR_MODULE_SECURITY) {
        nsCOMPtr<nsISupports> securityInfo;
        nsCOMPtr<nsITransportSecurityInfo> tsi;
        if (aFailedChannel)
            aFailedChannel->GetSecurityInfo(getter_AddRefs(securityInfo));
        tsi = do_QueryInterface(securityInfo);
        if (tsi) {
            // Usually we should have aFailedChannel and get a detailed message
            tsi->GetErrorMessage(getter_Copies(messageStr));
        }
        else {
            // No channel, let's obtain the generic error message
            nsCOMPtr<nsINSSErrorsService> nsserr =
                do_GetService(NS_NSS_ERRORS_SERVICE_CONTRACTID);
            if (nsserr) {
                nsserr->GetErrorMessage(aError, messageStr);
            }
        }
        if (!messageStr.IsEmpty())
            error.AssignLiteral("nssFailure");
    }
    else {
        // Errors requiring simple formatting
        switch (aError) {
        case NS_ERROR_MALFORMED_URI:
            // URI is malformed
            error.AssignLiteral("malformedURI");
            break;
        case NS_ERROR_REDIRECT_LOOP:
            // Doc failed to load because the server generated too many redirects
            error.AssignLiteral("redirectLoop");
            break;
        case NS_ERROR_UNKNOWN_SOCKET_TYPE:
            // Doc failed to load because PSM is not installed
            error.AssignLiteral("unknownSocketType");
            break;
        case NS_ERROR_NET_RESET:
            // Doc failed to load because the server kept reseting the connection
            // before we could read any data from it
            error.AssignLiteral("netReset");
            break;
        case NS_ERROR_DOCUMENT_NOT_CACHED:
            // Doc falied to load because we are offline and the cache does not
            // contain a copy of the document.
            error.AssignLiteral("netOffline");
            break;
        case NS_ERROR_DOCUMENT_IS_PRINTMODE:
            // Doc navigation attempted while Printing or Print Preview
            error.AssignLiteral("isprinting");
            break;
        case NS_ERROR_PORT_ACCESS_NOT_ALLOWED:
            // Port blocked for security reasons
            error.AssignLiteral("deniedPortAccess");
            break;
        case NS_ERROR_UNKNOWN_PROXY_HOST:
            // Proxy hostname could not be resolved.
            error.AssignLiteral("proxyResolveFailure");
            break;
        case NS_ERROR_PROXY_CONNECTION_REFUSED:
            // Proxy connection was refused.
            error.AssignLiteral("proxyConnectFailure");
            break;
        case NS_ERROR_INVALID_CONTENT_ENCODING:
            // Bad Content Encoding.
            error.AssignLiteral("contentEncodingError");
            break;
        }
    }

    // Test if the error should be displayed
    if (error.IsEmpty()) {
        return NS_OK;
    }

    // Test if the error needs to be formatted
    if (!messageStr.IsEmpty()) {
        // already obtained message
    }
    else if (formatStrCount > 0) {
        const PRUnichar *strs[kMaxFormatStrArgs];
        for (PRUint32 i = 0; i < formatStrCount; i++) {
            strs[i] = formatStrs[i].get();
        }
        nsXPIDLString str;
        rv = stringBundle->FormatStringFromName(
            error.get(),
            strs, formatStrCount, getter_Copies(str));
        NS_ENSURE_SUCCESS(rv, rv);
        messageStr.Assign(str.get());
    }
    else
    {
        nsXPIDLString str;
        rv = stringBundle->GetStringFromName(
                error.get(),
                getter_Copies(str));
        NS_ENSURE_SUCCESS(rv, rv);
        messageStr.Assign(str.get());
    }

    // Display the error as a page or an alert prompt
    NS_ENSURE_FALSE(messageStr.IsEmpty(), NS_ERROR_FAILURE);
    // Note: For now, display an alert instead of an error page if we have no
    // URI object. Missing URI objects are handled badly by session history.
    if (mUseErrorPages && aURI && aFailedChannel) {
        // Display an error page
        LoadErrorPage(aURI, aURL, error.get(), messageStr.get(),
                      aFailedChannel);
    } 
    else
    {
        // The prompter reqires that our private window has a document (or it
        // asserts). Satisfy that assertion now since GetDocument will force
        // creation of one if it hasn't already been created.
        nsCOMPtr<nsPIDOMWindow> pwin(do_QueryInterface(mScriptGlobal));
        if (pwin) {
            nsCOMPtr<nsIDOMDocument> doc;
            pwin->GetDocument(getter_AddRefs(doc));
        }

        // Display a message box
        prompter->Alert(nsnull, messageStr.get());
    }

    return NS_OK;
}


NS_IMETHODIMP
nsDocShell::LoadErrorPage(nsIURI *aURI, const PRUnichar *aURL,
                          const PRUnichar *aErrorType,
                          const PRUnichar *aDescription,
                          nsIChannel* aFailedChannel)
{
#if defined(PR_LOGGING) && defined(DEBUG)
    if (PR_LOG_TEST(gDocShellLog, PR_LOG_DEBUG)) {
        nsCAutoString spec;
        aURI->GetSpec(spec);

        nsCAutoString chanName;
        if (aFailedChannel)
            aFailedChannel->GetName(chanName);
        else
            chanName.AssignLiteral("<no channel>");

        PR_LOG(gDocShellLog, PR_LOG_DEBUG,
               ("nsDocShell[%p]::LoadErrorPage(\"%s\", \"%s\", {...}, [%s])\n", this,
                spec.get(), NS_ConvertUTF16toUTF8(aURL).get(), chanName.get()));
    }
#endif
    // Create an shistory entry for the old load, if we have a channel
    if (aFailedChannel) {
        mURIResultedInDocument = PR_TRUE;
        OnLoadingSite(aFailedChannel, PR_TRUE, PR_FALSE);
    } else if (aURI) {
        mURIResultedInDocument = PR_TRUE;
        OnNewURI(aURI, nsnull, mLoadType, PR_TRUE, PR_FALSE);
    }
    // Be sure to have a correct mLSHE, it may have been cleared by
    // EndPageLoad. See bug 302115.
    if (mSessionHistory && !mLSHE) {
        PRInt32 idx;
        mSessionHistory->GetRequestedIndex(&idx);
        nsCOMPtr<nsIHistoryEntry> entry;
        mSessionHistory->GetEntryAtIndex(idx, PR_FALSE,
                                         getter_AddRefs(entry));
        mLSHE = do_QueryInterface(entry);

    }

    nsCAutoString url;
    nsCAutoString charset;
    if (aURI)
    {
        // Set our current URI
        SetCurrentURI(aURI);

        nsresult rv = aURI->GetSpec(url);
        rv |= aURI->GetOriginCharset(charset);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else if (aURL)
    {
        CopyUTF16toUTF8(aURL, url);
    }
    else
    {
        return NS_ERROR_INVALID_POINTER;
    }

    // Create a URL to pass all the error information through to the page.

    char *escapedUrl = nsEscape(url.get(), url_Path);
    char *escapedCharset = nsEscape(charset.get(), url_Path);
    char *escapedError = nsEscape(NS_ConvertUTF16toUTF8(aErrorType).get(), url_Path);
    char *escapedDescription = nsEscape(NS_ConvertUTF16toUTF8(aDescription).get(), url_Path);

    nsCString errorPageUrl("about:neterror?e=");

    errorPageUrl.AppendASCII(escapedError);
    errorPageUrl.AppendLiteral("&u=");
    errorPageUrl.AppendASCII(escapedUrl);
    errorPageUrl.AppendLiteral("&c=");
    errorPageUrl.AppendASCII(escapedCharset);
    errorPageUrl.AppendLiteral("&d=");
    errorPageUrl.AppendASCII(escapedDescription);

    nsMemory::Free(escapedDescription);
    nsMemory::Free(escapedError);
    nsMemory::Free(escapedUrl);
    nsMemory::Free(escapedCharset);

    nsCOMPtr<nsIURI> errorPageURI;
    nsresult rv = NS_NewURI(getter_AddRefs(errorPageURI), errorPageUrl);
    NS_ENSURE_SUCCESS(rv, rv);

    return InternalLoad(errorPageURI, nsnull, nsnull, PR_TRUE, nsnull, nsnull,
                        nsnull, nsnull, LOAD_ERROR_PAGE,
                        nsnull, PR_TRUE, nsnull, nsnull);
}


NS_IMETHODIMP
nsDocShell::Reload(PRUint32 aReloadFlags)
{
    if (IsPrintingOrPP()) {
      return NS_OK; // JS may not handle returning of an error code
    }
    nsresult rv;
    NS_ASSERTION(((aReloadFlags & 0xf) == 0),
                 "Reload command not updated to use load flags!");

    PRUint32 loadType = MAKE_LOAD_TYPE(LOAD_RELOAD_NORMAL, aReloadFlags);
    NS_ENSURE_TRUE(IsValidLoadType(loadType), NS_ERROR_INVALID_ARG);

    // Send notifications to the HistoryListener if any, about the impending reload
    nsCOMPtr<nsISHistory> rootSH;
    rv = GetRootSessionHistory(getter_AddRefs(rootSH));
    nsCOMPtr<nsISHistoryInternal> shistInt(do_QueryInterface(rootSH));
    PRBool canReload = PR_TRUE; 
    if (rootSH) {
      nsCOMPtr<nsISHistoryListener> listener;
      shistInt->GetListener(getter_AddRefs(listener));
      if (listener) {
        listener->OnHistoryReload(mCurrentURI, aReloadFlags, &canReload);
      }
    }

    if (!canReload)
      return NS_OK;
    
    /* If you change this part of code, make sure bug 45297 does not re-occur */
    if (mOSHE) {
        rv = LoadHistoryEntry(mOSHE, loadType);
    }
    else if (mLSHE) { // In case a reload happened before the current load is done
        rv = LoadHistoryEntry(mLSHE, loadType);
    }
    else {
        nsCOMPtr<nsIDOMDocument> domDoc(do_GetInterface(GetAsSupports(this)));
        nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));

        nsIPrincipal* principal = nsnull;
        nsAutoString contentTypeHint;
        if (doc) {
            principal = doc->NodePrincipal();
            doc->GetContentType(contentTypeHint);
        }

        rv = InternalLoad(mCurrentURI,
                          mReferrerURI,
                          principal,
                          INTERNAL_LOAD_FLAGS_NONE, // Do not inherit owner from document
                          nsnull,         // No window target
                          NS_LossyConvertUTF16toASCII(contentTypeHint).get(),
                          nsnull,         // No post data
                          nsnull,         // No headers data
                          loadType,       // Load type
                          nsnull,         // No SHEntry
                          PR_TRUE,
                          nsnull,         // No nsIDocShell
                          nsnull);        // No nsIRequest
    }
    
    return rv;
}

NS_IMETHODIMP
nsDocShell::Stop(PRUint32 aStopFlags)
{
    if (nsIWebNavigation::STOP_CONTENT & aStopFlags) {
        // Revoke any pending event related to content viewer restoration
        mRestorePresentationEvent.Revoke();

        // Stop the document loading
        if (mContentViewer)
            mContentViewer->Stop();
    }

    if (nsIWebNavigation::STOP_NETWORK & aStopFlags) {
        // Suspend any timers that were set for this loader.  We'll clear
        // them out for good in CreateContentViewer.
        if (mRefreshURIList) {
            SuspendRefreshURIs();
            mSavedRefreshURIList.swap(mRefreshURIList);
            mRefreshURIList = nsnull;
        }

        // XXXbz We could also pass |this| to nsIURILoader::Stop.  That will
        // just call Stop() on us as an nsIDocumentLoader... We need fewer
        // redundant apis!
        Stop();
    }

    PRInt32 n;
    PRInt32 count = mChildList.Count();
    for (n = 0; n < count; n++) {
        nsCOMPtr<nsIWebNavigation> shellAsNav(do_QueryInterface(ChildAt(n)));
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
    NS_ENSURE_SUCCESS(EnsureContentViewer(), NS_ERROR_FAILURE);

    return mContentViewer->GetDOMDocument(aDocument);
}

NS_IMETHODIMP
nsDocShell::GetCurrentURI(nsIURI ** aURI)
{
    NS_ENSURE_ARG_POINTER(aURI);

    if (mCurrentURI) {
        return NS_EnsureSafeToReturn(mCurrentURI, aURI);
    }

    *aURI = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetReferringURI(nsIURI ** aURI)
{
    NS_ENSURE_ARG_POINTER(aURI);

    *aURI = mReferrerURI;
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
    *aSessionHistory = mSessionHistory;
    NS_IF_ADDREF(*aSessionHistory);
    return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIWebPageDescriptor
//*****************************************************************************   
NS_IMETHODIMP
nsDocShell::LoadPage(nsISupports *aPageDescriptor, PRUint32 aDisplayType)
{
    nsCOMPtr<nsISHEntry> shEntryIn(do_QueryInterface(aPageDescriptor));

    // Currently, the opaque 'page descriptor' is an nsISHEntry...
    if (!shEntryIn) {
        return NS_ERROR_INVALID_POINTER;
    }

    // Now clone shEntryIn, since we might end up modifying it later on, and we
    // want a page descriptor to be reusable.
    nsCOMPtr<nsISHEntry> shEntry;
    nsresult rv = shEntryIn->Clone(getter_AddRefs(shEntry));
    NS_ENSURE_SUCCESS(rv, rv);
    
    //
    // load the page as view-source
    //
    if (nsIWebPageDescriptor::DISPLAY_AS_SOURCE == aDisplayType) {
        nsCOMPtr<nsIURI> oldUri, newUri;
        nsCString spec, newSpec;

        // Create a new view-source URI and replace the original.
        rv = shEntry->GetURI(getter_AddRefs(oldUri));
        if (NS_FAILED(rv))
              return rv;

        oldUri->GetSpec(spec);
        newSpec.AppendLiteral("view-source:");
        newSpec.Append(spec);

        rv = NS_NewURI(getter_AddRefs(newUri), newSpec);
        if (NS_FAILED(rv)) {
            return rv;
        }
        shEntry->SetURI(newUri);
    }

    rv = LoadHistoryEntry(shEntry, LOAD_HISTORY);
    return rv;
}

NS_IMETHODIMP
nsDocShell::GetCurrentDescriptor(nsISupports **aPageDescriptor)
{
    NS_PRECONDITION(aPageDescriptor, "Null out param?");

    *aPageDescriptor = nsnull;

    nsISHEntry* src = mOSHE ? mOSHE : mLSHE;
    if (src) {
        nsCOMPtr<nsISHEntry> dest;

        nsresult rv = src->Clone(getter_AddRefs(dest));
        if (NS_FAILED(rv)) {
            return rv;
        }

        // null out inappropriate cloned attributes...
        dest->SetParent(nsnull);
        dest->SetIsSubFrame(PR_FALSE);
        
        return CallQueryInterface(dest, aPageDescriptor);
    }

    return NS_ERROR_NOT_AVAILABLE;
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
    NS_ASSERTION(mItemType == typeContent || mItemType == typeChrome,
                 "Unexpected item type in docshell");

    nsresult rv = NS_ERROR_FAILURE;
    mPrefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool tmpbool;

    rv = mPrefs->GetBoolPref("browser.frames.enabled", &tmpbool);
    if (NS_SUCCEEDED(rv))
        mAllowSubframes = tmpbool;

    if (gValidateOrigin == (PRBool)0xffffffff) {
        // Check pref to see if we should prevent frameset spoofing
        rv = mPrefs->GetBoolPref("browser.frame.validate_origin", &tmpbool);
        if (NS_SUCCEEDED(rv)) {
            gValidateOrigin = tmpbool;
        } else {
            gValidateOrigin = PR_TRUE;
        }
    }

    // Should we use XUL error pages instead of alerts if possible?
    rv = mPrefs->GetBoolPref("browser.xul.error_pages.enabled", &tmpbool);
    if (NS_SUCCEEDED(rv))
        mUseErrorPages = tmpbool;

    nsCOMPtr<nsIPrefBranch2> prefs(do_QueryInterface(mPrefs, &rv));
    if (NS_SUCCEEDED(rv) && mObserveErrorPages) {
        prefs->AddObserver("browser.xul.error_pages.enabled", this, PR_FALSE);
    }

    nsCOMPtr<nsIObserverService> serv = do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    if (serv) {
        const char* msg = mItemType == typeContent ?
            NS_WEBNAVIGATION_CREATE : NS_CHROME_WEBNAVIGATION_CREATE;
        serv->NotifyObservers(GetAsSupports(this), msg, nsnull);
    }    

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::Destroy()
{
    NS_ASSERTION(mItemType == typeContent || mItemType == typeChrome,
                 "Unexpected item type in docshell");

    if (!mIsBeingDestroyed) {
        nsCOMPtr<nsIObserverService> serv =
            do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
        if (serv) {
            const char* msg = mItemType == typeContent ?
                NS_WEBNAVIGATION_DESTROY : NS_CHROME_WEBNAVIGATION_DESTROY;
            serv->NotifyObservers(GetAsSupports(this), msg, nsnull);
        }
    }
    
    mIsBeingDestroyed = PR_TRUE;

    // Remove our pref observers
    if (mObserveErrorPages) {
        nsCOMPtr<nsIPrefBranch2> prefs(do_QueryInterface(mPrefs));
        if (prefs) {
            prefs->RemoveObserver("browser.xul.error_pages.enabled", this);
            mObserveErrorPages = PR_FALSE;
        }
    }

    // Fire unload event before we blow anything away.
    (void) FirePageHideNotification(PR_TRUE);

    // Note: mContentListener can be null if Init() failed and we're being
    // called from the destructor.
    if (mContentListener) {
        mContentListener->DropDocShellreference();
        mContentListener->SetParentContentListener(nsnull);
        // Note that we do NOT set mContentListener to null here; that
        // way if someone tries to do a load in us after this point
        // the nsDSURIContentListener will block it.  All of which
        // means that we should do this before calling Stop(), of
        // course.
    }

    // Stop any URLs that are currently being loaded...
    Stop(nsIWebNavigation::STOP_ALL);

    delete mEditorData;
    mEditorData = 0;

    mTransferableHookData = nsnull;

    // Save the state of the current document, before destroying the window.
    // This is needed to capture the state of a frameset when the new document
    // causes the frameset to be destroyed...
    PersistLayoutHistoryState();

    // Remove this docshell from its parent's child list
    nsCOMPtr<nsIDocShellTreeNode> docShellParentAsNode =
        do_QueryInterface(GetAsSupports(mParent));
    if (docShellParentAsNode)
        docShellParentAsNode->RemoveChild(this);

    if (mContentViewer) {
        mContentViewer->Close(nsnull);
        mContentViewer->Destroy();
        mContentViewer = nsnull;
    }

    nsDocLoader::Destroy();
    
    mParentWidget = nsnull;
    mCurrentURI = nsnull;

    if (mScriptGlobal) {
        nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(mScriptGlobal));
        win->SetDocShell(nsnull);

        mScriptGlobal->SetGlobalObjectOwner(nsnull);
        mScriptGlobal = nsnull;
    }

    mSessionHistory = nsnull;
    SetTreeOwner(nsnull);

    // required to break ref cycle
    mSecurityUI = nsnull;

    // Cancel any timers that were set for this docshell; this is needed
    // to break the cycle between us and the timers.
    CancelRefreshURITimers();

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetPosition(PRInt32 x, PRInt32 y)
{
    mBounds.x = x;
    mBounds.y = y;

    if (mContentViewer)
        NS_ENSURE_SUCCESS(mContentViewer->Move(x, y), NS_ERROR_FAILURE);

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
    mBounds.x = x;
    mBounds.y = y;
    mBounds.width = cx;
    mBounds.height = cy;

    if (mContentViewer) {
        //XXX Border figured in here or is that handled elsewhere?
        NS_ENSURE_SUCCESS(mContentViewer->SetBounds(mBounds), NS_ERROR_FAILURE);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetPositionAndSize(PRInt32 * x, PRInt32 * y, PRInt32 * cx,
                               PRInt32 * cy)
{
    // We should really consider just getting this information from
    // our window instead of duplicating the storage and code...
    nsCOMPtr<nsIDOMDocument> document(do_GetInterface(GetAsSupports(mParent)));
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(document));
    if (doc) {
        doc->FlushPendingNotifications(Flush_Layout);
    }
    
    if (x)
        *x = mBounds.x;
    if (y)
        *y = mBounds.y;
    if (cx)
        *cx = mBounds.width;
    if (cy)
        *cy = mBounds.height;

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::Repaint(PRBool aForce)
{
    nsCOMPtr<nsPresContext> context;
    GetPresContext(getter_AddRefs(context));
    NS_ENSURE_TRUE(context, NS_ERROR_FAILURE);

    nsIViewManager* viewManager = context->GetViewManager();
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
    mParentWidget = aParentWidget;

    if (!mParentWidget) {
        // If the parent widget is set to null we don't want to hold
        // on to the current device context any more since it is
        // associated with the parent widget we no longer own. We'll
        // need to create a new device context if one is needed again.

        mDeviceContext = nsnull;
    }

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
    nsIViewManager* vm = presShell->GetViewManager();
    NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

    // get the root view
    nsIView *view = nsnull; // views are not ref counted
    NS_ENSURE_SUCCESS(vm->GetRootView(view), NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);

    // if our root view is hidden, we are not visible
    if (view->GetVisibility() == nsViewVisibility_kHide) {
        *aVisibility = PR_FALSE;
        return NS_OK;
    }

    // otherwise, we must walk up the document and view trees checking
    // for a hidden view.

    nsCOMPtr<nsIDocShellTreeItem> treeItem = this;
    nsCOMPtr<nsIDocShellTreeItem> parentItem;
    treeItem->GetParent(getter_AddRefs(parentItem));
    while (parentItem) {
        nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(treeItem));
        docShell->GetPresShell(getter_AddRefs(presShell));

        nsCOMPtr<nsIDocShell> parentDS = do_QueryInterface(parentItem);
        nsCOMPtr<nsIPresShell> pPresShell;
        parentDS->GetPresShell(getter_AddRefs(pPresShell));

        // Null-check for crash in bug 267804
        if (!pPresShell) {
            NS_NOTREACHED("docshell has null pres shell");
            *aVisibility = PR_FALSE;
            return NS_OK;
        }

        nsIContent *shellContent =
            pPresShell->GetDocument()->FindContentForSubDocument(presShell->GetDocument());
        NS_ASSERTION(shellContent, "subshell not in the map");

        nsIFrame* frame = pPresShell->GetPrimaryFrameFor(shellContent);
        if (frame && !frame->AreAncestorViewsVisible()) {
            *aVisibility = PR_FALSE;
            return NS_OK;
        }

        treeItem = parentItem;
        treeItem->GetParent(getter_AddRefs(parentItem));
    }

    nsCOMPtr<nsIBaseWindow>
        treeOwnerAsWin(do_QueryInterface(mTreeOwner));
    if (!treeOwnerAsWin) {
        *aVisibility = PR_TRUE;
        return NS_OK;
    }

    // Check with the tree owner as well to give embedders a chance to
    // expose visibility as well.
    return treeOwnerAsWin->GetVisibility(aVisibility);
}

NS_IMETHODIMP
nsDocShell::SetVisibility(PRBool aVisibility)
{
    if (!mContentViewer)
        return NS_OK;
    if (aVisibility) {
        mContentViewer->Show();
    }
    else {
        mContentViewer->Hide();
    }
    
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetEnabled(PRBool *aEnabled)
{
  NS_ENSURE_ARG_POINTER(aEnabled);
  *aEnabled = PR_TRUE;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShell::SetEnabled(PRBool aEnabled)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShell::GetBlurSuppression(PRBool *aBlurSuppression)
{
  NS_ENSURE_ARG_POINTER(aBlurSuppression);
  *aBlurSuppression = PR_FALSE;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShell::SetBlurSuppression(PRBool aBlurSuppression)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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
  printf("nsDocShell::SetFocus %p\n", (void*)this);
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

    if (mGlobalHistory && mCurrentURI && mLoadType != LOAD_ERROR_PAGE) {
        mGlobalHistory->SetPageTitle(mCurrentURI, nsDependentString(aTitle));
    }


    // Update SessionHistory with the document's title. If the
    // page was loaded from history or the page bypassed history,
    // there is no need to update the title. There is no need to
    // go to mSessionHistory to update the title. Setting it in mOSHE 
    // would suffice. 
    if (mOSHE && (mLoadType != LOAD_BYPASS_HISTORY) &&
        (mLoadType != LOAD_HISTORY) && (mLoadType != LOAD_ERROR_PAGE)) {
        mOSHE->SetTitle(mTitle);    
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

    nsIScrollableView* scrollView;
    NS_ENSURE_SUCCESS(GetRootScrollableView(&scrollView),
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
    nsIScrollableView* scrollView;
    NS_ENSURE_SUCCESS(GetRootScrollableView(&scrollView),
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
    nsIScrollableView* scrollView;
    NS_ENSURE_SUCCESS(GetRootScrollableView(&scrollView),
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

    nsIScrollableView* scrollView;
    NS_ENSURE_SUCCESS(GetRootScrollableView(&scrollView),
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

// Set scrolling preference for all documents in this shell
//
// There are three possible values stored in the shell:
//  1) nsIScrollable::Scrollbar_Never = no scrollbar
//  2) nsIScrollable::Scrollbar_Auto = scrollbar appears if the document
//     being displayed would normally have scrollbar
//  3) nsIScrollable::Scrollbar_Always = scrollbar always appears
//
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

NS_IMETHODIMP
nsDocShell::GetScrollbarVisibility(PRBool * verticalVisible,
                                   PRBool * horizontalVisible)
{
    nsIScrollableView* scrollView;
    NS_ENSURE_SUCCESS(GetRootScrollableView(&scrollView),
                      NS_ERROR_FAILURE);
    if (!scrollView)
        return NS_ERROR_FAILURE;

    // We should now call nsLayoutUtils::GetScrollableFrameFor,
    // but we can't because of stupid linkage!
    nsIFrame* scrollFrame =
        NS_STATIC_CAST(nsIFrame*, scrollView->View()->GetParent()->GetClientData());
    if (!scrollFrame)
        return NS_ERROR_FAILURE;
    nsIScrollableFrame* scrollable = nsnull;
    CallQueryInterface(scrollFrame, &scrollable);
    if (!scrollable)
        return NS_ERROR_FAILURE;

    nsMargin scrollbars = scrollable->GetActualScrollbarSizes();
    if (verticalVisible)
        *verticalVisible = scrollbars.left != 0 || scrollbars.right != 0;
    if (horizontalVisible)
        *horizontalVisible = scrollbars.top != 0 || scrollbars.bottom != 0;

    return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsITextScroll
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::ScrollByLines(PRInt32 numLines)
{
    nsIScrollableView* scrollView;

    NS_ENSURE_SUCCESS(GetRootScrollableView(&scrollView),
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
    nsIScrollableView* scrollView;

    NS_ENSURE_SUCCESS(GetRootScrollableView(&scrollView),
                      NS_ERROR_FAILURE);
    if (!scrollView) {
        return NS_ERROR_FAILURE;
    }

    NS_ENSURE_SUCCESS(scrollView->ScrollByPages(0, numPages), NS_ERROR_FAILURE);

    return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIScriptGlobalObjectOwner
//*****************************************************************************   

nsIScriptGlobalObject*
nsDocShell::GetScriptGlobalObject()
{
    NS_ENSURE_SUCCESS(EnsureScriptEnvironment(), nsnull);

    return mScriptGlobal;
}

//*****************************************************************************
// nsDocShell::nsIRefreshURI
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::RefreshURI(nsIURI * aURI, PRInt32 aDelay, PRBool aRepeat,
                       PRBool aMetaRefresh)
{
    NS_ENSURE_ARG(aURI);

    /* Check if Meta refresh/redirects are permitted. Some
     * embedded applications may not want to do this.
     * Must do this before sending out NOTIFY_REFRESH events
     * because listeners may have side effects (e.g. displaying a
     * button to manually trigger the refresh later).
     */
    PRBool allowRedirects = PR_TRUE;
    GetAllowMetaRedirects(&allowRedirects);
    if (!allowRedirects)
        return NS_OK;

    // If any web progress listeners are listening for NOTIFY_REFRESH events,
    // give them a chance to block this refresh.
    PRBool sameURI;
    nsresult rv = aURI->Equals(mCurrentURI, &sameURI);
    if (NS_FAILED(rv))
        sameURI = PR_FALSE;
    if (!RefreshAttempted(this, aURI, aDelay, sameURI))
        return NS_OK;

    nsRefreshTimer *refreshTimer = new nsRefreshTimer();
    NS_ENSURE_TRUE(refreshTimer, NS_ERROR_OUT_OF_MEMORY);
    PRUint32 busyFlags = 0;
    GetBusyFlags(&busyFlags);

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

    if (busyFlags & BUSY_FLAGS_BUSY) {
        // We are busy loading another page. Don't create the
        // timer right now. Instead queue up the request and trigger the
        // timer in EndPageLoad(). 
        mRefreshURIList->AppendElement(refreshTimer);
    }
    else {
        // There is no page loading going on right now.  Create the
        // timer and fire it right away.
        nsCOMPtr<nsITimer> timer = do_CreateInstance("@mozilla.org/timer;1");
        NS_ENSURE_TRUE(timer, NS_ERROR_FAILURE);

        mRefreshURIList->AppendElement(timer);      // owning timer ref
        timer->InitWithCallback(refreshTimer, aDelay, nsITimer::TYPE_ONE_SHOT);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::ForceRefreshURI(nsIURI * aURI,
                            PRInt32 aDelay, 
                            PRBool aMetaRefresh)
{
    NS_ENSURE_ARG(aURI);

    nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
    CreateLoadInfo(getter_AddRefs(loadInfo));
    NS_ENSURE_TRUE(loadInfo, NS_ERROR_OUT_OF_MEMORY);

    /* We do need to pass in a referrer, but we don't want it to
     * be sent to the server.
     */
    loadInfo->SetSendReferrer(PR_FALSE);

    /* for most refreshes the current URI is an appropriate
     * internal referrer
     */
    loadInfo->SetReferrer(mCurrentURI);

    /* Check if this META refresh causes a redirection
     * to another site. 
     */
    PRBool equalUri = PR_FALSE;
    nsresult rv = aURI->Equals(mCurrentURI, &equalUri);
    if (NS_SUCCEEDED(rv) && (!equalUri) && aMetaRefresh) {

        /* It is a META refresh based redirection. Now check if it happened
           within the threshold time we have in mind(15000 ms as defined by
           REFRESH_REDIRECT_TIMER). If so, pass a REPLACE flag to LoadURI().
         */
        if (aDelay <= REFRESH_REDIRECT_TIMER) {
            loadInfo->SetLoadType(nsIDocShellLoadInfo::loadNormalReplace);
            
            /* for redirects we mimic HTTP, which passes the
             *  original referrer
             */
            nsCOMPtr<nsIURI> internalReferrer;
            GetReferringURI(getter_AddRefs(internalReferrer));
            if (internalReferrer) {
                loadInfo->SetReferrer(internalReferrer);
            }
        }
        else
            loadInfo->SetLoadType(nsIDocShellLoadInfo::loadRefresh);
        /*
         * LoadURI(...) will cancel all refresh timers... This causes the
         * Timer and its refreshData instance to be released...
         */
        LoadURI(aURI, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE, PR_TRUE);
        return NS_OK;
    }
    else
        loadInfo->SetLoadType(nsIDocShellLoadInfo::loadRefresh);

    LoadURI(aURI, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE, PR_TRUE);

    return NS_OK;
}

nsresult
nsDocShell::SetupRefreshURIFromHeader(nsIURI * aBaseURI,
                                      const nsACString & aHeader)
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
    // separated by any char in the set "'\"\t\r\n " will be
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
    nsCAutoString uriAttrib;
    PRInt32 seconds = 0;
    PRBool specifiesSeconds = PR_FALSE;

    nsACString::const_iterator iter, tokenStart, doneIterating;

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
        specifiesSeconds = PR_TRUE;
        ++iter;
    }

    if (iter != doneIterating) {
        // if we started with a '-', number is negative
        if (*tokenStart == '-')
            seconds = -seconds;

        // skip to next ';' or ','
        nsACString::const_iterator iterAfterDigit = iter;
        while (iter != doneIterating && !(*iter == ';' || *iter == ','))
        {
            if (specifiesSeconds)
            {
                // Non-whitespace characters here mean that the string is
                // malformed but tolerate sites that specify a decimal point,
                // even though meta refresh only works on whole seconds.
                if (iter == iterAfterDigit &&
                    !nsCRT::IsAsciiSpace(*iter) && *iter != '.')
                {
                    // The characters between the seconds and the next
                    // section are just garbage!
                    //   e.g. content="2a0z+,URL=http://www.mozilla.org/"
                    // Just ignore this redirect.
                    return NS_ERROR_FAILURE;
                }
                else if (nsCRT::IsAsciiSpace(*iter))
                {
                    // We've had at least one whitespace so tolerate the mistake
                    // and drop through.
                    // e.g. content="10 foo"
                    ++iter;
                    break;
                }
            }
            ++iter;
        }

        // skip any remaining whitespace
        while (iter != doneIterating && nsCRT::IsAsciiSpace(*iter))
            ++iter;

        // skip ';' or ','
        if (iter != doneIterating && (*iter == ';' || *iter == ',')) {
            ++iter;
        }

        // skip whitespace
        while (iter != doneIterating && nsCRT::IsAsciiSpace(*iter))
            ++iter;
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

    // skip a leading '"' or '\''.

    PRBool isQuotedURI = PR_FALSE;
    if (tokenStart != doneIterating && (*tokenStart == '"' || *tokenStart == '\''))
    {
        isQuotedURI = PR_TRUE;
        ++tokenStart;
    }

    // set iter to start of URI
    iter = tokenStart;

    // tokenStart here points to the beginning of URI

    // grab the rest of the URI
    while (iter != doneIterating)
    {
        if (isQuotedURI && (*iter == '"' || *iter == '\''))
            break;
        ++iter;
    }

    // move iter one back if the last character is a '"' or '\''
    if (iter != tokenStart && isQuotedURI) {
        --iter;
        if (!(*iter == '"' || *iter == '\''))
            ++iter;
    }

    // URI is whatever's contained from tokenStart to iter.
    // note: if tokenStart == doneIterating, so is iter.

    nsresult rv = NS_OK;

    nsCOMPtr<nsIURI> uri;
    PRBool specifiesURI = PR_FALSE;
    if (tokenStart == iter) {
        uri = aBaseURI;
    }
    else {
        uriAttrib = Substring(tokenStart, iter);
        // NS_NewURI takes care of any whitespace surrounding the URL
        rv = NS_NewURI(getter_AddRefs(uri), uriAttrib, nsnull, aBaseURI);
        specifiesURI = PR_TRUE;
    }

    // No URI or seconds were specified
    if (!specifiesSeconds && !specifiesURI)
    {
        // Do nothing because the alternative is to spin around in a refresh
        // loop forever!
        return NS_ERROR_FAILURE;
    }

    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIScriptSecurityManager>
            securityManager(do_GetService
                            (NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv));
        if (NS_SUCCEEDED(rv)) {
            rv = securityManager->
                CheckLoadURI(aBaseURI, uri,
                             nsIScriptSecurityManager::
                             LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT);
            if (NS_SUCCEEDED(rv)) {
                // Since we can't travel back in time yet, just pretend
                // negative numbers do nothing at all.
                if (seconds < 0)
                    return NS_ERROR_FAILURE;

                rv = RefreshURI(uri, seconds * 1000, PR_FALSE, PR_TRUE);
            }
        }
    }
    return rv;
}

NS_IMETHODIMP nsDocShell::SetupRefreshURI(nsIChannel * aChannel)
{
    nsresult rv;
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel, &rv));
    if (NS_SUCCEEDED(rv)) {
        nsCAutoString refreshHeader;
        rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("refresh"),
                                            refreshHeader);

        if (!refreshHeader.IsEmpty()) {
            SetupReferrerFromChannel(aChannel);
            rv = SetupRefreshURIFromHeader(mCurrentURI, refreshHeader);
            if (NS_SUCCEEDED(rv)) {
                return NS_REFRESHURI_HEADER_FOUND;
            }
        }
    }
    return rv;
}

static void
DoCancelRefreshURITimers(nsISupportsArray* aTimerList)
{
    if (!aTimerList)
        return;

    PRUint32 n=0;
    aTimerList->Count(&n);

    while (n) {
        nsCOMPtr<nsITimer> timer(do_QueryElementAt(aTimerList, --n));

        aTimerList->RemoveElementAt(n);    // bye bye owning timer ref

        if (timer)
            timer->Cancel();        
    }
}

NS_IMETHODIMP
nsDocShell::CancelRefreshURITimers()
{
    DoCancelRefreshURITimers(mRefreshURIList);
    DoCancelRefreshURITimers(mSavedRefreshURIList);
    mRefreshURIList = nsnull;
    mSavedRefreshURIList = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetRefreshPending(PRBool* _retval)
{
    if (!mRefreshURIList) {
        *_retval = PR_FALSE;
        return NS_OK;
    }

    PRUint32 count;
    nsresult rv = mRefreshURIList->Count(&count);
    if (NS_SUCCEEDED(rv))
        *_retval = (count != 0);
    return rv;
}

NS_IMETHODIMP
nsDocShell::SuspendRefreshURIs()
{
    if (mRefreshURIList) {
        PRUint32 n = 0;
        mRefreshURIList->Count(&n);

        for (PRUint32 i = 0;  i < n; ++i) {
            nsCOMPtr<nsITimer> timer = do_QueryElementAt(mRefreshURIList, i);
            if (!timer)
                continue;  // this must be a nsRefreshURI already

            // Replace this timer object with a nsRefreshTimer object.
            nsCOMPtr<nsITimerCallback> callback;
            timer->GetCallback(getter_AddRefs(callback));

            timer->Cancel();

            nsCOMPtr<nsITimerCallback> rt = do_QueryInterface(callback);
            NS_ASSERTION(rt, "RefreshURIList timer callbacks should only be RefreshTimer objects");

            mRefreshURIList->ReplaceElementAt(rt, i);
        }
    }

    // Suspend refresh URIs for our child shells as well.
    PRInt32 n = mChildList.Count();

    for (PRInt32 i = 0; i < n; ++i) {
        nsCOMPtr<nsIDocShell> shell = do_QueryInterface(ChildAt(i));
        if (shell)
            shell->SuspendRefreshURIs();
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::ResumeRefreshURIs()
{
    RefreshURIFromQueue();

    // Resume refresh URIs for our child shells as well.
    PRInt32 n = mChildList.Count();

    for (PRInt32 i = 0; i < n; ++i) {
        nsCOMPtr<nsIDocShell> shell = do_QueryInterface(ChildAt(i));
        if (shell)
            shell->ResumeRefreshURIs();
    }

    return NS_OK;
}

nsresult
nsDocShell::RefreshURIFromQueue()
{
    if (!mRefreshURIList)
        return NS_OK;
    PRUint32 n = 0;
    mRefreshURIList->Count(&n);

    while (n) {
        nsCOMPtr<nsISupports> element;
        mRefreshURIList->GetElementAt(--n, getter_AddRefs(element));
        nsCOMPtr<nsITimerCallback> refreshInfo(do_QueryInterface(element));

        if (refreshInfo) {   
            // This is the nsRefreshTimer object, waiting to be
            // setup in a timer object and fired.                         
            // Create the timer and  trigger it.
            PRUint32 delay = NS_STATIC_CAST(nsRefreshTimer*, NS_STATIC_CAST(nsITimerCallback*, refreshInfo))->GetDelay();
            nsCOMPtr<nsITimer> timer = do_CreateInstance("@mozilla.org/timer;1");
            if (timer) {    
                // Replace the nsRefreshTimer element in the queue with
                // its corresponding timer object, so that in case another
                // load comes through before the timer can go off, the timer will
                // get cancelled in CancelRefreshURITimer()
                mRefreshURIList->ReplaceElementAt(timer, n);
                timer->InitWithCallback(refreshInfo, delay, nsITimer::TYPE_ONE_SHOT);
            }           
        }        
    }  // while
 
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

    // If we are loading a wyciwyg url from history, change the base URI for 
    // the document to the original http url that created the document.write().
    // This makes sure that all relative urls in a document.written page loaded
    // via history work properly.
    if (mCurrentURI &&
       (mLoadType & LOAD_CMD_HISTORY ||
        mLoadType == LOAD_RELOAD_NORMAL ||
        mLoadType == LOAD_RELOAD_CHARSET_CHANGE)){
        PRBool isWyciwyg = PR_FALSE;
        // Check if the url is wyciwyg
        rv = mCurrentURI->SchemeIs("wyciwyg", &isWyciwyg);      
        if (isWyciwyg && NS_SUCCEEDED(rv))
            SetBaseUrlForWyciwyg(aContentViewer);
    }
    // XXX What if SetupNewViewer fails?
    if (mLSHE)
        SetHistoryEntry(&mOSHE, mLSHE);

    PRBool updateHistory = PR_TRUE;

    // Determine if this type of load should update history
    switch (mLoadType) {
    case LOAD_RELOAD_CHARSET_CHANGE: // don't preserve history in charset reload
    case LOAD_NORMAL_REPLACE:
    case LOAD_STOP_CONTENT_AND_REPLACE:
    case LOAD_RELOAD_BYPASS_CACHE:
    case LOAD_RELOAD_BYPASS_PROXY:
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
        updateHistory = PR_FALSE;
        break;
    default:
        break;
    }

    if (!updateHistory)
        SetLayoutHistoryState(nsnull);

    return NS_OK;
}

/* void setIsPrinting (in boolean aIsPrinting); */
NS_IMETHODIMP 
nsDocShell::SetIsPrinting(PRBool aIsPrinting)
{
    mIsPrintingOrPP = aIsPrinting;
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
                          PRUint32 aStateFlags, nsresult aStatus)
{
    nsresult rv;

    // Update the busy cursor
    if ((~aStateFlags & (STATE_START | STATE_IS_NETWORK)) == 0) {
        nsCOMPtr<nsIWyciwygChannel>  wcwgChannel(do_QueryInterface(aRequest));
        nsCOMPtr<nsIWebProgress> webProgress =
            do_QueryInterface(GetAsSupports(this));

        // Was the wyciwyg document loaded on this docshell?
        if (wcwgChannel && !mLSHE && (mItemType == typeContent) && aProgress == webProgress.get()) {
            nsCOMPtr<nsIURI> uri;
            wcwgChannel->GetURI(getter_AddRefs(uri));
        
            PRBool equalUri = PR_TRUE;
            // Store the wyciwyg url in session history, only if it is
            // being loaded fresh for the first time. We don't want 
            // multiple entries for successive loads
            if (mCurrentURI &&
                NS_SUCCEEDED(uri->Equals(mCurrentURI, &equalUri)) &&
                !equalUri) {
                // This is a document.write(). Get the made-up url
                // from the channel and store it in session history.
                rv = AddToSessionHistory(uri, wcwgChannel, getter_AddRefs(mLSHE));
                SetCurrentURI(uri, aRequest, PR_TRUE);
                // Save history state of the previous page
                rv = PersistLayoutHistoryState();
                if (mOSHE)
                    SetHistoryEntry(&mOSHE, mLSHE);
            }
        
        }
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
        nsCOMPtr<nsIWebProgress> webProgress =
            do_QueryInterface(GetAsSupports(this));
        // Is the document stop notification for this document?
        if (aProgress == webProgress.get()) {
            nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
            EndPageLoad(aProgress, channel, aStatus);
        }
    }
    // note that redirect state changes will go through here as well, but it
    // is better to handle those in OnRedirectStateChange where more
    // information is available.
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::OnLocationChange(nsIWebProgress * aProgress,
                             nsIRequest * aRequest, nsIURI * aURI)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

void
nsDocShell::OnRedirectStateChange(nsIChannel* aOldChannel,
                                  nsIChannel* aNewChannel,
                                  PRUint32 aRedirectFlags,
                                  PRUint32 aStateFlags)
{
    NS_ASSERTION(aStateFlags & STATE_REDIRECTING,
                 "Calling OnRedirectStateChange when there is no redirect");
    if (!(aStateFlags & STATE_IS_DOCUMENT))
        return; // not a toplevel document

    nsCOMPtr<nsIGlobalHistory3> history3(do_QueryInterface(mGlobalHistory));
    nsresult result = NS_ERROR_NOT_IMPLEMENTED;
    if (history3) {
        // notify global history of this redirect
        result = history3->AddDocumentRedirect(aOldChannel, aNewChannel,
                                               aRedirectFlags, !IsFrame());
    }

    if (result == NS_ERROR_NOT_IMPLEMENTED) {
        // when there is no GlobalHistory3, or it doesn't implement
        // AddToplevelRedirect, we fall back to GlobalHistory2.  Just notify
        // that the redirecting page was a redirect so it will be link colored
        // but not visible.
        nsCOMPtr<nsIURI> oldURI;
        aOldChannel->GetURI(getter_AddRefs(oldURI));
        if (! oldURI)
            return; // nothing to tell anybody about
        AddToGlobalHistory(oldURI, PR_TRUE, aOldChannel);
    }
}

NS_IMETHODIMP
nsDocShell::OnStatusChange(nsIWebProgress * aWebProgress,
                           nsIRequest * aRequest,
                           nsresult aStatus, const PRUnichar * aMessage)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::OnSecurityChange(nsIWebProgress * aWebProgress,
                             nsIRequest * aRequest, PRUint32 state)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}


nsresult
nsDocShell::EndPageLoad(nsIWebProgress * aProgress,
                        nsIChannel * aChannel, nsresult aStatus)
{
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
        mIsExecutingOnLoadHandler = PR_TRUE;
        mContentViewer->LoadComplete(aStatus);
        mIsExecutingOnLoadHandler = PR_FALSE;

        mEODForCurrentDocument = PR_TRUE;

        // If all documents have completed their loading
        // favor native event dispatch priorities
        // over performance
        if (--gNumberOfDocumentsLoading == 0) {
          // Hint to use normal native event dispatch priorities 
          FavorPerformanceHint(PR_FALSE, NS_EVENT_STARVATION_DELAY_HINT);
        }
    }
    /* Check if the httpChannel has any cache-control related response headers,
     * like no-store, no-cache. If so, update SHEntry so that 
     * when a user goes back/forward to this page, we appropriately do 
     * form value restoration or load from server.
     */
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));
    if (!httpChannel) // HttpChannel could be hiding underneath a Multipart channel.    
        GetHttpChannel(aChannel, getter_AddRefs(httpChannel));

    if (httpChannel) {
        // figure out if SH should be saving layout state.
        PRBool discardLayoutState = ShouldDiscardLayoutState(httpChannel);       
        if (mLSHE && discardLayoutState && (mLoadType & LOAD_CMD_NORMAL) &&
            (mLoadType != LOAD_BYPASS_HISTORY) && (mLoadType != LOAD_ERROR_PAGE))
            mLSHE->SetSaveLayoutStateFlag(PR_FALSE);            
    }

    // Clear mLSHE after calling the onLoadHandlers. This way, if the
    // onLoadHandler tries to load something different in
    // itself or one of its children, we can deal with it appropriately.
    if (mLSHE) {
        mLSHE->SetLoadType(nsIDocShellLoadInfo::loadHistory);

        // Clear the mLSHE reference to indicate document loading is done one
        // way or another.
        SetHistoryEntry(&mLSHE, nsnull);
    }
    // if there's a refresh header in the channel, this method
    // will set it up for us. 
    RefreshURIFromQueue();

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
    if (mIsBeingDestroyed)
        return NS_ERROR_FAILURE;

    nsIPrincipal* principal = nsnull;

    nsCOMPtr<nsPIDOMWindow> piDOMWindow(do_QueryInterface(mScriptGlobal));
    if (piDOMWindow) {
        principal = piDOMWindow->GetOpenerScriptPrincipal();
    }

    if (!principal) {
        principal = GetInheritedPrincipal(PR_FALSE);
    }

    nsresult rv = CreateAboutBlankContentViewer(principal);

    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIDOMDocument> domDoc;
        mContentViewer->GetDOMDocument(getter_AddRefs(domDoc));
        nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
        NS_ASSERTION(doc,
                     "Should have doc if CreateAboutBlankContentViewer "
                     "succeeded!");

        doc->SetIsInitialDocument(PR_TRUE);
    }

    return rv;
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

    return NS_OK;
}

nsresult
nsDocShell::CreateAboutBlankContentViewer(nsIPrincipal* aPrincipal)
{
  nsCOMPtr<nsIDocument> blankDoc;
  nsCOMPtr<nsIContentViewer> viewer;
  nsresult rv = NS_ERROR_FAILURE;

  /* mCreatingDocument should never be true at this point. However, it's
     a theoretical possibility. We want to know about it and make it stop,
     and this sounds like a job for an assertion. */
  NS_ASSERTION(!mCreatingDocument, "infinite(?) loop creating document averted");
  if (mCreatingDocument)
    return NS_ERROR_FAILURE;

  mCreatingDocument = PR_TRUE;

  // mContentViewer->PermitUnload may release |this| docshell.
  nsCOMPtr<nsIDocShell> kungFuDeathGrip(this);
  
  if (mContentViewer) {
    // We've got a content viewer already. Make sure the user
    // permits us to discard the current document and replace it
    // with about:blank. And also ensure we fire the unload events
    // in the current document.

    PRBool okToUnload;
    rv = mContentViewer->PermitUnload(&okToUnload);

    if (NS_SUCCEEDED(rv) && !okToUnload) {
      // The user chose not to unload the page, interrupt the load.
      return NS_ERROR_FAILURE;
    }

    mSavingOldViewer = CanSavePresentation(LOAD_NORMAL, nsnull, nsnull);

    // Notify the current document that it is about to be unloaded!!
    //
    // It is important to fire the unload() notification *before* any state
    // is changed within the DocShell - otherwise, javascript will get the
    // wrong information :-(
    //
    (void) FirePageHideNotification(!mSavingOldViewer);
  }

  // one helper factory, please
  nsCOMPtr<nsICategoryManager> catMan(do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
  if (!catMan)
    return NS_ERROR_FAILURE;

  nsXPIDLCString contractId;
  rv = catMan->GetCategoryEntry("Gecko-Content-Viewers", "text/html", getter_Copies(contractId));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDocumentLoaderFactory> docFactory(do_GetService(contractId));
  if (docFactory) {
    // generate (about:blank) document to load
    docFactory->CreateBlankDocument(mLoadGroup, aPrincipal,
                                    getter_AddRefs(blankDoc));
    if (blankDoc) {
      blankDoc->SetContainer(NS_STATIC_CAST(nsIDocShell *, this));

      // create a content viewer for us and the new document
      docFactory->CreateInstanceForDocument(NS_ISUPPORTS_CAST(nsIDocShell *, this),
                    blankDoc, "view", getter_AddRefs(viewer));

      // hook 'em up
      if (viewer) {
        viewer->SetContainer(NS_STATIC_CAST(nsIContentViewerContainer *,this));
        nsCOMPtr<nsIDOMDocument> domdoc(do_QueryInterface(blankDoc));
        Embed(viewer, "", 0);
        viewer->SetDOMDocument(domdoc);

        SetCurrentURI(blankDoc->GetDocumentURI(), nsnull, PR_TRUE);
        rv = NS_OK;
      }
    }
  }
  mCreatingDocument = PR_FALSE;

  // The transient about:blank viewer doesn't have a session history entry.
  SetHistoryEntry(&mOSHE, nsnull);

  return rv;
}

PRBool
nsDocShell::CanSavePresentation(PRUint32 aLoadType,
                                nsIRequest *aNewRequest,
                                nsIDocument *aNewDocument)
{
    if (!mOSHE)
        return PR_FALSE; // no entry to save into

    // Only save presentation for "normal" loads and link loads.  Anything else
    // probably wants to refetch the page, so caching the old presentation
    // would be incorrect.
    if (aLoadType != LOAD_NORMAL &&
        aLoadType != LOAD_HISTORY &&
        aLoadType != LOAD_LINK &&
        aLoadType != LOAD_STOP_CONTENT &&
        aLoadType != LOAD_STOP_CONTENT_AND_REPLACE &&
        aLoadType != LOAD_ERROR_PAGE)
        return PR_FALSE;

    // If the session history entry has the saveLayoutState flag set to false,
    // then we should not cache the presentation.
    PRBool canSaveState;
    mOSHE->GetSaveLayoutStateFlag(&canSaveState);
    if (canSaveState == PR_FALSE)
        return PR_FALSE;

    // If the document is not done loading, don't cache it.
    nsCOMPtr<nsPIDOMWindow> pWin = do_QueryInterface(mScriptGlobal);
    if (!pWin || pWin->IsLoading())
        return PR_FALSE;

    if (pWin->WouldReuseInnerWindow(aNewDocument))
        return PR_FALSE;

    // Avoid doing the work of saving the presentation state in the case where
    // the content viewer cache is disabled.
    if (nsSHistory::GetMaxTotalViewers() == 0)
        return PR_FALSE;

    // Don't cache the content viewer if we're in a subframe and the subframe
    // pref is disabled.
    PRBool cacheFrames = PR_FALSE;
    mPrefs->GetBoolPref("browser.sessionhistory.cache_subframes",
                        &cacheFrames);
    if (!cacheFrames) {
        nsCOMPtr<nsIDocShellTreeItem> root;
        GetSameTypeParent(getter_AddRefs(root));
        if (root && root != this) {
            return PR_FALSE;  // this is a subframe load
        }
    }

    // If the document does not want its presentation cached, then don't.
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(pWin->GetExtantDocument());
    if (!doc || !doc->CanSavePresentation(aNewRequest))
        return PR_FALSE;

    return PR_TRUE;
}

nsresult
nsDocShell::CaptureState()
{
    if (!mOSHE || mOSHE == mLSHE) {
        // No entry to save into, or we're replacing the existing entry.
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsPIDOMWindow> privWin = do_QueryInterface(mScriptGlobal);
    if (!privWin)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsISupports> windowState;
    nsresult rv = privWin->SaveWindowState(getter_AddRefs(windowState));
    NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_PAGE_CACHE
    nsCOMPtr<nsIURI> uri;
    mOSHE->GetURI(getter_AddRefs(uri));
    nsCAutoString spec;
    if (uri)
        uri->GetSpec(spec);
    printf("Saving presentation into session history\n");
    printf("  SH URI: %s\n", spec.get());
#endif

    rv = mOSHE->SetWindowState(windowState);
    NS_ENSURE_SUCCESS(rv, rv);

    // Suspend refresh URIs and save off the timer queue
    rv = mOSHE->SetRefreshURIList(mSavedRefreshURIList);
    NS_ENSURE_SUCCESS(rv, rv);

    // Capture the current content viewer bounds.
    nsCOMPtr<nsIPresShell> shell;
    nsDocShell::GetPresShell(getter_AddRefs(shell));
    if (shell) {
        nsIViewManager *vm = shell->GetViewManager();
        if (vm) {
            nsIView *rootView = nsnull;
            vm->GetRootView(rootView);
            if (rootView) {
                nsIWidget *widget = rootView->GetWidget();
                if (widget) {
                    nsRect bounds(0, 0, 0, 0);
                    widget->GetBounds(bounds);
                    rv = mOSHE->SetViewerBounds(bounds);
                }
            }
        }
    }

    // Capture the docshell hierarchy.
    mOSHE->ClearChildShells();

    PRInt32 childCount = mChildList.Count();
    for (PRInt32 i = 0; i < childCount; ++i) {
        nsCOMPtr<nsIDocShellTreeItem> childShell = do_QueryInterface(ChildAt(i));
        NS_ASSERTION(childShell, "null child shell");

        mOSHE->AddChildShell(childShell);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::RestorePresentationEvent::Run()
{
    if (mDocShell && NS_FAILED(mDocShell->RestoreFromHistory()))
        NS_WARNING("RestoreFromHistory failed");
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::BeginRestore(nsIContentViewer *aContentViewer, PRBool aTop)
{
    nsresult rv;
    if (!aContentViewer) {
        rv = EnsureContentViewer();
        NS_ENSURE_SUCCESS(rv, rv);

        aContentViewer = mContentViewer;
    }

    // Dispatch events for restoring the presentation.  We try to simulate
    // the progress notifications loading the document would cause, so we add
    // the document's channel to the loadgroup to initiate stateChange
    // notifications.

    nsCOMPtr<nsIDOMDocument> domDoc;
    aContentViewer->GetDOMDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
    if (doc) {
        nsIChannel *channel = doc->GetChannel();
        if (channel) {
            mEODForCurrentDocument = PR_FALSE;
            mIsRestoringDocument = PR_TRUE;
            mLoadGroup->AddRequest(channel, nsnull);
            mIsRestoringDocument = PR_FALSE;
        }
    }

    if (!aTop) {
        // For non-top frames, there is no notion of making sure that the
        // previous document is in the domwindow when STATE_START notifications
        // happen.  We can just call BeginRestore for all of the child shells
        // now.
        rv = BeginRestoreChildren();
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

nsresult
nsDocShell::BeginRestoreChildren()
{
    PRInt32 n = mChildList.Count();
    for (PRInt32 i = 0; i < n; ++i) {
        nsCOMPtr<nsIDocShell> child = do_QueryInterface(ChildAt(i));
        if (child) {
            nsresult rv = child->BeginRestore(nsnull, PR_FALSE);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::FinishRestore()
{
    // First we call finishRestore() on our children.  In the simulated load,
    // all of the child frames finish loading before the main document.

    PRInt32 n = mChildList.Count();
    for (PRInt32 i = 0; i < n; ++i) {
        nsCOMPtr<nsIDocShell> child = do_QueryInterface(ChildAt(i));
        if (child) {
            child->FinishRestore();
        }
    }

    if (mContentViewer) {
        nsCOMPtr<nsIDOMDocument> domDoc;
        mContentViewer->GetDOMDocument(getter_AddRefs(domDoc));

        nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
        if (doc) {
            // Finally, we remove the request from the loadgroup.  This will
            // cause onStateChange(STATE_STOP) to fire, which will fire the
            // pageshow event to the chrome.

            nsIChannel *channel = doc->GetChannel();
            if (channel) {
                mIsRestoringDocument = PR_TRUE;
                mLoadGroup->RemoveRequest(channel, nsnull, NS_OK);
                mIsRestoringDocument = PR_FALSE;
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetRestoringDocument(PRBool *aRestoring)
{
    *aRestoring = mIsRestoringDocument;
    return NS_OK;
}

nsresult
nsDocShell::RestorePresentation(nsISHEntry *aSHEntry, PRBool *aRestoring)
{
    NS_ASSERTION(mLoadType & LOAD_CMD_HISTORY,
                 "RestorePresentation should only be called for history loads");

    nsCOMPtr<nsIContentViewer> viewer;
    aSHEntry->GetContentViewer(getter_AddRefs(viewer));

#ifdef DEBUG_PAGE_CACHE
    nsCOMPtr<nsIURI> uri;
    aSHEntry->GetURI(getter_AddRefs(uri));

    nsCAutoString spec;
    if (uri)
        uri->GetSpec(spec);
#endif

    *aRestoring = PR_FALSE;

    if (!viewer) {
#ifdef DEBUG_PAGE_CACHE
        printf("no saved presentation for uri: %s\n", spec.get());
#endif
        return NS_OK;
    }

    // We need to make sure the content viewer's container is this docshell.
    // In subframe navigation, it's possible for the docshell that the
    // content viewer was originally loaded into to be replaced with a
    // different one.  We don't currently support restoring the presentation
    // in that case.

    nsCOMPtr<nsISupports> container;
    viewer->GetContainer(getter_AddRefs(container));
    if (!::SameCOMIdentity(container, GetAsSupports(this))) {
#ifdef DEBUG_PAGE_CACHE
        printf("No valid container, clearing presentation\n");
#endif
        aSHEntry->SetContentViewer(nsnull);
        return NS_ERROR_FAILURE;
    }

    NS_ASSERTION(mContentViewer != viewer, "Restoring existing presentation");

#ifdef DEBUG_PAGE_CACHE
    printf("restoring presentation from session history: %s\n", spec.get());
#endif

    SetHistoryEntry(&mLSHE, aSHEntry);

    // Add the request to our load group.  We do this before swapping out
    // the content viewers so that consumers of STATE_START can access
    // the old document.  We only deal with the toplevel load at this time --
    // to be consistent with normal document loading, subframes cannot start
    // loading until after data arrives, which is after STATE_START completes.

    BeginRestore(viewer, PR_TRUE);

    // Post an event that will remove the request after we've returned
    // to the event loop.  This mimics the way it is called by nsIChannel
    // implementations.

    // Revoke any pending restore (just in case)
    NS_ASSERTION(!mRestorePresentationEvent.IsPending(),
        "should only have one RestorePresentationEvent");
    mRestorePresentationEvent.Revoke();

    nsRefPtr<RestorePresentationEvent> evt = new RestorePresentationEvent(this);
    nsresult rv = NS_DispatchToCurrentThread(evt);
    if (NS_SUCCEEDED(rv)) {
        mRestorePresentationEvent = evt.get();
        // The rest of the restore processing will happen on our event
        // callback.
        *aRestoring = PR_TRUE;
    }

    return rv;
}

nsresult
nsDocShell::RestoreFromHistory()
{
    mRestorePresentationEvent.Forget();

    // This section of code follows the same ordering as CreateContentViewer.
    if (!mLSHE)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIContentViewer> viewer;
    mLSHE->GetContentViewer(getter_AddRefs(viewer));
    if (!viewer)
        return NS_ERROR_FAILURE;

    if (mSavingOldViewer) {
        // We determined that it was safe to cache the document presentation
        // at the time we initiated the new load.  We need to check whether
        // it's still safe to do so, since there may have been DOM mutations
        // or new requests initiated.
        nsCOMPtr<nsIDOMDocument> domDoc;
        viewer->GetDOMDocument(getter_AddRefs(domDoc));
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
        nsIRequest *request = nsnull;
        if (doc)
            request = doc->GetChannel();
        mSavingOldViewer = CanSavePresentation(mLoadType, request, doc);
    }

    nsCOMPtr<nsIMarkupDocumentViewer> oldMUDV(do_QueryInterface(mContentViewer));
    nsCOMPtr<nsIMarkupDocumentViewer> newMUDV(do_QueryInterface(viewer));
    float zoom = 1.0;
    if (oldMUDV && newMUDV)
        oldMUDV->GetTextZoom(&zoom);

    // Protect against mLSHE going away via a load triggered from
    // pagehide or unload.
    nsCOMPtr<nsISHEntry> origLSHE = mLSHE;

    // Notify the old content viewer that it's being hidden.
    FirePageHideNotification(!mSavingOldViewer);

    // If mLSHE was changed as a result of the pagehide event, then
    // something else was loaded.  Don't finish restoring.
    if (mLSHE != origLSHE)
      return NS_OK;

    // Set mFiredUnloadEvent = PR_FALSE so that the unload handler for the
    // *new* document will fire.
    mFiredUnloadEvent = PR_FALSE;

    mURIResultedInDocument = PR_TRUE;
    nsCOMPtr<nsISHistory> rootSH;
    GetRootSessionHistory(getter_AddRefs(rootSH));
    if (rootSH) {
        nsCOMPtr<nsISHistoryInternal> hist = do_QueryInterface(rootSH);
        rootSH->GetIndex(&mPreviousTransIndex);
        hist->UpdateIndex();
        rootSH->GetIndex(&mLoadedTransIndex);
#ifdef DEBUG_PAGE_CACHE
        printf("Previous index: %d, Loaded index: %d\n\n", mPreviousTransIndex,
                   mLoadedTransIndex);
#endif
    }

    // Rather than call Embed(), we will retrieve the viewer from the session
    // history entry and swap it in.
    // XXX can we refactor this so that we can just call Embed()?
    PersistLayoutHistoryState();
    nsresult rv;
    if (mContentViewer) {
        if (mSavingOldViewer && NS_FAILED(CaptureState())) {
            if (mOSHE) {
                mOSHE->SyncPresentationState();
            }
            mSavingOldViewer = PR_FALSE;
        }
    }

    mSavedRefreshURIList = nsnull;

    // In cases where we use a transient about:blank viewer between loads,
    // we never show the transient viewer, so _its_ previous viewer is never
    // unhooked from the view hierarchy.  Destroy any such previous viewer now,
    // before we grab the root view sibling, so that we don't grab a view
    // that's about to go away.

    if (mContentViewer) {
        nsCOMPtr<nsIContentViewer> previousViewer;
        mContentViewer->GetPreviousViewer(getter_AddRefs(previousViewer));
        if (previousViewer) {
            mContentViewer->SetPreviousViewer(nsnull);
            previousViewer->Destroy();
        }
    }

    // Save off the root view's parent and sibling so that we can insert the
    // new content viewer's root view at the same position.  Also save the
    // bounds of the root view's widget.

    nsIView *rootViewSibling = nsnull, *rootViewParent = nsnull;
    nsRect newBounds(0, 0, 0, 0);

    nsCOMPtr<nsIPresShell> oldPresShell;
    nsDocShell::GetPresShell(getter_AddRefs(oldPresShell));
    if (oldPresShell) {
        nsIViewManager *vm = oldPresShell->GetViewManager();
        if (vm) {
            nsIView *oldRootView = nsnull;
            vm->GetRootView(oldRootView);

            if (oldRootView) {
                rootViewSibling = oldRootView->GetNextSibling();
                rootViewParent = oldRootView->GetParent();

                nsIWidget *widget = oldRootView->GetWidget();
                if (widget) {
                    widget->GetBounds(newBounds);
                }
            }
        }
    }

    // Transfer ownership to mContentViewer.  By ensuring that either the
    // docshell or the session history, but not both, have references to the
    // content viewer, we prevent the viewer from being torn down after
    // Destroy() is called.

    if (mContentViewer) {
        mContentViewer->Close(mSavingOldViewer ? mOSHE.get() : nsnull);
        viewer->SetPreviousViewer(mContentViewer);
    }

    mContentViewer.swap(viewer);
    viewer = nsnull; // force a release to complete ownership transfer

    // Grab all of the related presentation from the SHEntry now.
    // Clearing the viewer from the SHEntry will clear all of this state.
    nsCOMPtr<nsISupports> windowState;
    mLSHE->GetWindowState(getter_AddRefs(windowState));
    mLSHE->SetWindowState(nsnull);

    PRBool sticky;
    mLSHE->GetSticky(&sticky);

    nsCOMPtr<nsIDOMDocument> domDoc;
    mContentViewer->GetDOMDocument(getter_AddRefs(domDoc));

    nsCOMArray<nsIDocShellTreeItem> childShells;
    PRInt32 i = 0;
    nsCOMPtr<nsIDocShellTreeItem> child;
    while (NS_SUCCEEDED(mLSHE->ChildShellAt(i++, getter_AddRefs(child))) &&
           child) {
        childShells.AppendObject(child);
    }

    // get the previous content viewer size
    nsRect oldBounds(0, 0, 0, 0);
    mLSHE->GetViewerBounds(oldBounds);

    // Restore the refresh URI list.  The refresh timers will be restarted
    // when EndPageLoad() is called.
    nsCOMPtr<nsISupportsArray> refreshURIList;
    mLSHE->GetRefreshURIList(getter_AddRefs(refreshURIList));

    // Reattach to the window object.
    rv = mContentViewer->Open(windowState, mLSHE);

    // Now remove it from the cached presentation.
    mLSHE->SetContentViewer(nsnull);
    mEODForCurrentDocument = PR_FALSE;

#ifdef DEBUG
 {
     nsCOMPtr<nsISupportsArray> refreshURIs;
     mLSHE->GetRefreshURIList(getter_AddRefs(refreshURIs));
     nsCOMPtr<nsIDocShellTreeItem> childShell;
     mLSHE->ChildShellAt(0, getter_AddRefs(childShell));
     NS_ASSERTION(!refreshURIs && !childShell,
                  "SHEntry should have cleared presentation state");
 }
#endif

    // Restore the sticky state of the viewer.  The viewer has set this state
    // on the history entry in Destroy() just before marking itself non-sticky,
    // to avoid teardown of the presentation.
    mContentViewer->SetSticky(sticky);

    // Now that we have switched documents, forget all of our children.
    DestroyChildren();
    NS_ENSURE_SUCCESS(rv, rv);

    // mLSHE is now our currently-loaded document.
    SetHistoryEntry(&mOSHE, mLSHE);
    
    // XXX special wyciwyg handling in Embed()?

    // We aren't going to restore any items from the LayoutHistoryState,
    // but we don't want them to stay around in case the page is reloaded.
    SetLayoutHistoryState(nsnull);

    // This is the end of our Embed() replacement

    mSavingOldViewer = PR_FALSE;
    mEODForCurrentDocument = PR_FALSE;

    // Tell the event loop to favor plevents over user events, see comments
    // in CreateContentViewer.
    if (++gNumberOfDocumentsLoading == 1)
        FavorPerformanceHint(PR_TRUE, NS_EVENT_STARVATION_DELAY_HINT);


    if (oldMUDV && newMUDV)
        newMUDV->SetTextZoom(zoom);

    nsCOMPtr<nsIDocument> document = do_QueryInterface(domDoc);
    if (document) {
        // Use the uri from the mLSHE we had when we entered this function
        // (which need not match the document's URI if anchors are involved),
        // since that's the history entry we're loading.  Note that if we use
        // origLSHE we don't have to worry about whether the entry in question
        // is still mLSHE or whether it's now mOSHE.
        nsCOMPtr<nsIURI> uri;
        origLSHE->GetURI(getter_AddRefs(uri));
        SetCurrentURI(uri, document->GetChannel(), PR_TRUE);
    }

    // This is the end of our CreateContentViewer() replacement.
    // Now we simulate a load.  First, we restore the state of the javascript
    // window object.
    nsCOMPtr<nsPIDOMWindow> privWin =
        do_GetInterface(NS_STATIC_CAST(nsIInterfaceRequestor*, this));
    NS_ASSERTION(privWin, "could not get nsPIDOMWindow interface");

    rv = privWin->RestoreWindowState(windowState);
    NS_ENSURE_SUCCESS(rv, rv);

    // Now, dispatch a title change event which would happed as the
    // <head> is parsed.
    nsCOMPtr<nsIDOMNSDocument> nsDoc = do_QueryInterface(document);
    if (nsDoc) {
        const nsAFlatString &title = document->GetDocumentTitle();
        nsDoc->SetTitle(title);
    }

    // Now we simulate appending child docshells for subframes.
    for (i = 0; i < childShells.Count(); ++i) {
        nsIDocShellTreeItem *childItem = childShells.ObjectAt(i);
        AddChild(childItem);

        nsCOMPtr<nsIDocShell> childShell = do_QueryInterface(childItem);
        rv = childShell->BeginRestore(nsnull, PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    nsCOMPtr<nsIPresShell> shell;
    nsDocShell::GetPresShell(getter_AddRefs(shell));

    nsIViewManager *newVM = shell ? shell->GetViewManager() : nsnull;
    nsIView *newRootView = nsnull;
    if (newVM)
        newVM->GetRootView(newRootView);

    // Insert the new root view at the correct location in the view tree.
    if (rootViewParent) {
        nsIViewManager *parentVM = rootViewParent->GetViewManager();

        if (parentVM && newRootView) {
            // InsertChild(parent, child, sib, PR_TRUE) inserts the child after
            // sib in content order, which is before sib in view order. BUT
            // when sib is null it inserts at the end of the the document
            // order, i.e., first in view order.  But when oldRootSibling is
            // null, the old root as at the end of the view list --- last in
            // content order --- and we want to call InsertChild(parent, child,
            // nsnull, PR_FALSE) in that case.
            parentVM->InsertChild(rootViewParent, newRootView,
                                  rootViewSibling,
                                  rootViewSibling ? PR_TRUE : PR_FALSE);

            NS_ASSERTION(newRootView->GetNextSibling() == rootViewSibling,
                         "error in InsertChild");
        }
    }

    // Now that all of the child docshells have been put into place, we can
    // restart the timers for the window and all of the child frames.
    privWin->ResumeTimeouts();

    // Restore the refresh URI list.  The refresh timers will be restarted
    // when EndPageLoad() is called.
    mRefreshURIList = refreshURIList;

    // Meta-refresh timers have been restarted for this shell, but not
    // for our children.  Walk the child shells and restart their timers.
    PRInt32 n = mChildList.Count();
    for (i = 0; i < n; ++i) {
        nsCOMPtr<nsIDocShell> child = do_QueryInterface(ChildAt(i));
        if (child)
            child->ResumeRefreshURIs();
    }

    // Make sure this presentation is the same size as the previous
    // presentation.  If this is not the same size we showed it at last time,
    // then we need to resize the widget.

    // XXXbryner   This interacts poorly with Firefox's infobar.  If the old
    // presentation had the infobar visible, then we will resize the new
    // presentation to that smaller size.  However, firing the locationchanged
    // event will hide the infobar, which will immediately resize the window
    // back to the larger size.  A future optimization might be to restore
    // the presentation at the "wrong" size, then fire the locationchanged
    // event and check whether the docshell's new size is the same as the
    // cached viewer size (skipping the resize if they are equal).

    if (newRootView) {
        nsIWidget *widget = newRootView->GetWidget();
        if (widget && !newBounds.IsEmpty() && newBounds != oldBounds) {
#ifdef DEBUG_PAGE_CACHE
            printf("resize widget(%d, %d, %d, %d)\n", newBounds.x,
                   newBounds.y, newBounds.width, newBounds.height);
#endif

            widget->Resize(newBounds.x, newBounds.y, newBounds.width,
                           newBounds.height, PR_FALSE);
        }
    }

    // Simulate the completion of the load.
    nsDocShell::FinishRestore();

    // Restart plugins, and paint the content.
    if (shell)
        shell->Thaw();

    return privWin->FireDelayedDOMEvents();
}

NS_IMETHODIMP
nsDocShell::CreateContentViewer(const char *aContentType,
                                nsIRequest * request,
                                nsIStreamListener ** aContentHandler)
{
    *aContentHandler = nsnull;

    // Can we check the content type of the current content viewer
    // and reuse it without destroying it and re-creating it?

    NS_ASSERTION(mLoadGroup, "Someone ignored return from Init()?");

    // Instantiate the content viewer object
    nsCOMPtr<nsIContentViewer> viewer;
    nsresult rv = NewContentViewerObj(aContentType, request, mLoadGroup,
                                      aContentHandler, getter_AddRefs(viewer));

    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    // Notify the current document that it is about to be unloaded!!
    //
    // It is important to fire the unload() notification *before* any state
    // is changed within the DocShell - otherwise, javascript will get the
    // wrong information :-(
    //

    if (mSavingOldViewer) {
        // We determined that it was safe to cache the document presentation
        // at the time we initiated the new load.  We need to check whether
        // it's still safe to do so, since there may have been DOM mutations
        // or new requests initiated.
        nsCOMPtr<nsIDOMDocument> domDoc;
        viewer->GetDOMDocument(getter_AddRefs(domDoc));
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
        mSavingOldViewer = CanSavePresentation(mLoadType, request, doc);
    }

    FirePageHideNotification(!mSavingOldViewer);

    // Set mFiredUnloadEvent = PR_FALSE so that the unload handler for the
    // *new* document will fire.
    mFiredUnloadEvent = PR_FALSE;

    // we've created a new document so go ahead and call
    // OnLoadingSite(), but don't fire OnLocationChange()
    // notifications before we've called Embed(). See bug 284993.
    mURIResultedInDocument = PR_TRUE;

    nsCOMPtr<nsIChannel> aOpenedChannel = do_QueryInterface(request);

    PRBool onLocationChangeNeeded = OnLoadingSite(aOpenedChannel, PR_FALSE);

    // let's try resetting the load group if we need to...
    nsCOMPtr<nsILoadGroup> currentLoadGroup;
    NS_ENSURE_SUCCESS(aOpenedChannel->
                      GetLoadGroup(getter_AddRefs(currentLoadGroup)),
                      NS_ERROR_FAILURE);

    if (currentLoadGroup != mLoadGroup) {
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
        aOpenedChannel->SetLoadGroup(mLoadGroup);

        // Mark the channel as being a document URI...
        aOpenedChannel->GetLoadFlags(&loadFlags);
        loadFlags |= nsIChannel::LOAD_DOCUMENT_URI;

        aOpenedChannel->SetLoadFlags(loadFlags);

        mLoadGroup->AddRequest(request, nsnull);
        if (currentLoadGroup)
            currentLoadGroup->RemoveRequest(request, nsnull,
                                            NS_BINDING_RETARGETED);

        // Update the notification callbacks, so that progress and
        // status information are sent to the right docshell...
        aOpenedChannel->SetNotificationCallbacks(this);
    }

    NS_ENSURE_SUCCESS(Embed(viewer, "", (nsISupports *) nsnull),
                      NS_ERROR_FAILURE);

    mSavedRefreshURIList = nsnull;
    mSavingOldViewer = PR_FALSE;
    mEODForCurrentDocument = PR_FALSE;

    // if this document is part of a multipart document,
    // the ID can be used to distinguish it from the other parts.
    nsCOMPtr<nsIMultiPartChannel> multiPartChannel(do_QueryInterface(request));
    if (multiPartChannel) {
      nsCOMPtr<nsIPresShell> shell;
      rv = GetPresShell(getter_AddRefs(shell));
      if (NS_SUCCEEDED(rv) && shell) {
        nsIDocument *doc = shell->GetDocument();
        if (doc) {
          PRUint32 partID;
          multiPartChannel->GetPartID(&partID);
          doc->SetPartID(partID);
        }
      }
    }

    // Give hint to native plevent dispatch mechanism. If a document
    // is loading the native plevent dispatch mechanism should favor
    // performance over normal native event dispatch priorities.
    if (++gNumberOfDocumentsLoading == 1) {
      // Hint to favor performance for the plevent notification mechanism.
      // We want the pages to load as fast as possible even if its means 
      // native messages might be starved.
      FavorPerformanceHint(PR_TRUE, NS_EVENT_STARVATION_DELAY_HINT);
    }

    if (onLocationChangeNeeded) {
      FireOnLocationChange(this, request, mCurrentURI);
    }
  
    return NS_OK;
}

nsresult
nsDocShell::NewContentViewerObj(const char *aContentType,
                                nsIRequest * request, nsILoadGroup * aLoadGroup,
                                nsIStreamListener ** aContentHandler,
                                nsIContentViewer ** aViewer)
{
    nsCOMPtr<nsIChannel> aOpenedChannel = do_QueryInterface(request);

    nsresult rv;
    nsCOMPtr<nsICategoryManager> catMan(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
    if (NS_FAILED(rv))
      return rv;
    
    nsXPIDLCString contractId;
    rv = catMan->GetCategoryEntry("Gecko-Content-Viewers", aContentType, getter_Copies(contractId));

    // Create an instance of the document-loader-factory
    nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory;
    if (NS_SUCCEEDED(rv))
        docLoaderFactory = do_GetService(contractId.get());

    if (!docLoaderFactory) {
        return NS_ERROR_FAILURE;
    }

    // Now create an instance of the content viewer
    // nsLayoutDLF makes the determination if it should be a "view-source" instead of "view"
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

    nsCAutoString defaultCharset;
    nsCAutoString forceCharset;
    nsCAutoString hintCharset;
    PRInt32 hintCharsetSource;
    nsCAutoString prevDocCharset;
    float textZoom;
    PRBool styleDisabled;
    // |newMUDV| also serves as a flag to set the data from the above vars
    nsCOMPtr<nsIMarkupDocumentViewer> newMUDV;

    if (mContentViewer || parent) {
        nsCOMPtr<nsIMarkupDocumentViewer> oldMUDV;
        if (mContentViewer) {
            // Get any interesting state from old content viewer
            // XXX: it would be far better to just reuse the document viewer ,
            //      since we know we're just displaying the same document as before
            oldMUDV = do_QueryInterface(mContentViewer);

            // Tell the old content viewer to hibernate in session history when
            // it is destroyed.

            if (mSavingOldViewer && NS_FAILED(CaptureState())) {
                if (mOSHE) {
                    mOSHE->SyncPresentationState();
                }
                mSavingOldViewer = PR_FALSE;
            }
        }
        else {
            // No old content viewer, so get state from parent's content viewer
            nsCOMPtr<nsIContentViewer> parentContentViewer;
            parent->GetContentViewer(getter_AddRefs(parentContentViewer));
            oldMUDV = do_QueryInterface(parentContentViewer);
        }

        if (oldMUDV) {
            nsresult rv;

            newMUDV = do_QueryInterface(aNewViewer,&rv);
            if (newMUDV) {
                NS_ENSURE_SUCCESS(oldMUDV->
                                  GetDefaultCharacterSet(defaultCharset),
                                  NS_ERROR_FAILURE);
                NS_ENSURE_SUCCESS(oldMUDV->
                                  GetForceCharacterSet(forceCharset),
                                  NS_ERROR_FAILURE);
                NS_ENSURE_SUCCESS(oldMUDV->
                                  GetHintCharacterSet(hintCharset),
                                  NS_ERROR_FAILURE);
                NS_ENSURE_SUCCESS(oldMUDV->
                                  GetHintCharacterSetSource(&hintCharsetSource),
                                  NS_ERROR_FAILURE);
                NS_ENSURE_SUCCESS(oldMUDV->
                                  GetTextZoom(&textZoom),
                                  NS_ERROR_FAILURE);
                NS_ENSURE_SUCCESS(oldMUDV->
                                  GetAuthorStyleDisabled(&styleDisabled),
                                  NS_ERROR_FAILURE);
                NS_ENSURE_SUCCESS(oldMUDV->
                                  GetPrevDocCharacterSet(prevDocCharset),
                                  NS_ERROR_FAILURE);
            }
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
    nsIFocusController *focusController = nsnull;
    if (mScriptGlobal) {
        nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(mScriptGlobal);
        focusController = ourWindow->GetRootFocusController();
        if (focusController) {
            // Suppress the command dispatcher.
            focusController->SetSuppressFocus(PR_TRUE,
                                              "Win32-Only Link Traversal Issue");
            // Remove focus from the element that has it
            nsCOMPtr<nsIDOMWindowInternal> focusedWindow;
            focusController->GetFocusedWindow(getter_AddRefs(focusedWindow));

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
              if (curwin == ourWindow) {
                isSubWindow = PR_TRUE;
                break;
              }

              // don't use nsCOMPtr here to avoid extra addref
              // when assigning to curwin
              nsIDOMWindow* temp;
              curwin->GetParent(&temp);
              if (curwin == temp) {
                NS_RELEASE(temp);
                break;
              }
              curwin = dont_AddRef(temp);
            }

            if (ourWindow == focusedWindow || isSubWindow)
              focusController->ResetElementFocus();
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
            docviewer->GetPresShell(getter_AddRefs(shell));

            if (shell) {
                nsIViewManager* vm = shell->GetViewManager();

                if (vm) {
                    vm->GetDefaultBackgroundColor(&bgcolor);
                    // If the background color is not known, don't propagate it.
                    bgSet = NS_GET_A(bgcolor) != 0;
                }
            }
        }

        mContentViewer->Close(mSavingOldViewer ? mOSHE.get() : nsnull);
        aNewViewer->SetPreviousViewer(mContentViewer);

        mContentViewer = nsnull;
    }

    mContentViewer = aNewViewer;

    nsCOMPtr<nsIWidget> widget;
    NS_ENSURE_SUCCESS(GetMainWidget(getter_AddRefs(widget)), NS_ERROR_FAILURE);

    if (widget) {
        NS_ENSURE_SUCCESS(EnsureDeviceContext(), NS_ERROR_FAILURE);
    }

    nsRect bounds(x, y, cx, cy);

    if (NS_FAILED(mContentViewer->Init(widget, mDeviceContext, bounds))) {
        mContentViewer = nsnull;
        NS_ERROR("ContentViewer Initialization failed");
        return NS_ERROR_FAILURE;
    }

    // If we have old state to copy, set the old state onto the new content
    // viewer
    if (newMUDV) {
        NS_ENSURE_SUCCESS(newMUDV->SetDefaultCharacterSet(defaultCharset),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(newMUDV->SetForceCharacterSet(forceCharset),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(newMUDV->SetHintCharacterSet(hintCharset),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(newMUDV->
                          SetHintCharacterSetSource(hintCharsetSource),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(newMUDV->SetPrevDocCharacterSet(prevDocCharset),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(newMUDV->SetTextZoom(textZoom),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(newMUDV->SetAuthorStyleDisabled(styleDisabled),
                          NS_ERROR_FAILURE);
    }

    // End copying block (Don't mess with the old content/document viewer
    // beyond here!!)

    // See the book I wrote above regarding why the focus controller is 
    // being used here.  -- hyatt

    /* Note it's important that focus suppression be turned off no earlier
       because in cases where the docshell is lazily creating an about:blank
       document, mContentViewer->Init finally puts a reference to that
       document into the DOM window, which prevents an infinite recursion
       attempting to lazily create the document as focus is unsuppressed
       (bug 110856). */
    if (focusController)
        focusController->SetSuppressFocus(PR_FALSE,
                                          "Win32-Only Link Traversal Issue");

    if (bgSet && widget) {
        // Stuff the bgcolor from the last view manager into the new
        // view manager. This improves page load continuity.
        nsCOMPtr<nsIDocumentViewer> docviewer =
            do_QueryInterface(mContentViewer);

        if (docviewer) {
            nsCOMPtr<nsIPresShell> shell;
            docviewer->GetPresShell(getter_AddRefs(shell));

            if (shell) {
                nsIViewManager* vm = shell->GetViewManager();

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


nsresult
nsDocShell::CheckLoadingPermissions()
{
    // This method checks whether the caller may load content into
    // this docshell. Even though we've done our best to hide windows
    // from code that doesn't have the right to access them, it's
    // still possible for an evil site to open a window and access
    // frames in the new window through window.frames[] (which is
    // allAccess for historic reasons), so we still need to do this
    // check on load.
    nsresult rv = NS_OK, sameOrigin = NS_OK;

    if (!gValidateOrigin || !IsFrame()) {
        // Origin validation was turned off, or we're not a frame.
        // Permit all loads.

        return rv;
    }

    // We're a frame. Check that the caller has write permission to
    // the parent before allowing it to load anything into this
    // docshell.

    nsCOMPtr<nsIScriptSecurityManager> securityManager =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool ubwEnabled = PR_FALSE;
    rv = securityManager->IsCapabilityEnabled("UniversalBrowserWrite",
                                              &ubwEnabled);
    if (NS_FAILED(rv) || ubwEnabled) {
        return rv;
    }

    nsCOMPtr<nsIPrincipal> subjPrincipal;
    rv = securityManager->GetSubjectPrincipal(getter_AddRefs(subjPrincipal));
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && subjPrincipal, rv);

    // Check if the caller is from the same origin as this docshell,
    // or any of it's ancestors.
    nsCOMPtr<nsIDocShellTreeItem> item(this);
    do {
        nsCOMPtr<nsIScriptGlobalObject> sgo(do_GetInterface(item));
        nsCOMPtr<nsIScriptObjectPrincipal> sop(do_QueryInterface(sgo));

        nsIPrincipal *p;
        if (!sop || !(p = sop->GetPrincipal())) {
            return NS_ERROR_UNEXPECTED;
        }

        // Compare origins
        sameOrigin =
            securityManager->CheckSameOriginPrincipal(subjPrincipal, p);
        if (NS_SUCCEEDED(sameOrigin)) {
            // Same origin, permit load

            return sameOrigin;
        }

        nsCOMPtr<nsIDocShellTreeItem> tmp;
        item->GetSameTypeParent(getter_AddRefs(tmp));
        item.swap(tmp);
    } while (item);

    // The caller is not from the same origin as this item, or any if
    // this items ancestors. Only permit loading content if both are
    // part of the same window, assuming we can find the window of the
    // caller.

    nsCOMPtr<nsIJSContextStack> stack =
        do_GetService("@mozilla.org/js/xpc/ContextStack;1");
    if (!stack) {
        // No context stack available. Should never happen, but in
        // case it does, return the sameOrigin error from the security
        // check above.

        return sameOrigin;
    }

    JSContext *cx = nsnull;
    stack->Peek(&cx);

    if (!cx) {
        // No caller docshell reachable, return the sameOrigin error
        // from the security check above.

        return sameOrigin;
    }

    nsIScriptContext *currentCX = GetScriptContextFromJSContext(cx);
    nsCOMPtr<nsIDocShellTreeItem> callerTreeItem;
    nsCOMPtr<nsPIDOMWindow> win;
    if (currentCX &&
        (win = do_QueryInterface(currentCX->GetGlobalObject())) &&
        (callerTreeItem = do_QueryInterface(win->GetDocShell()))) {
        nsCOMPtr<nsIDocShellTreeItem> callerRoot;
        callerTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(callerRoot));

        nsCOMPtr<nsIDocShellTreeItem> ourRoot;
        GetSameTypeRootTreeItem(getter_AddRefs(ourRoot));

        if (ourRoot == callerRoot) {
            // The running JS is in the same window as the target
            // frame, permit load.
            sameOrigin = NS_OK;
        }
    }

    return sameOrigin;
}

//*****************************************************************************
// nsDocShell: Site Loading
//*****************************************************************************   
NS_IMETHODIMP
nsDocShell::InternalLoad(nsIURI * aURI,
                         nsIURI * aReferrer,
                         nsISupports * aOwner,
                         PRUint32 aFlags,
                         const PRUnichar *aWindowTarget,
                         const char* aTypeHint,
                         nsIInputStream * aPostData,
                         nsIInputStream * aHeadersData,
                         PRUint32 aLoadType,
                         nsISHEntry * aSHEntry,
                         PRBool aFirstParty,
                         nsIDocShell** aDocShell,
                         nsIRequest** aRequest)
{
    nsresult rv = NS_OK;

#ifdef PR_LOGGING
    if (gDocShellLeakLog && PR_LOG_TEST(gDocShellLeakLog, PR_LOG_DEBUG)) {
        nsCAutoString spec;
        if (aURI)
            aURI->GetSpec(spec);
        PR_LogPrint("DOCSHELL %p InternalLoad %s\n", this, spec.get());
    }
#endif
    
    // Initialize aDocShell/aRequest
    if (aDocShell) {
        *aDocShell = nsnull;
    }
    if (aRequest) {
        *aRequest = nsnull;
    }

    if (!aURI) {
        return NS_ERROR_NULL_POINTER;
    }

    NS_ENSURE_TRUE(IsValidLoadType(aLoadType), NS_ERROR_INVALID_ARG);

    NS_ENSURE_TRUE(!mIsBeingDestroyed, NS_ERROR_NOT_AVAILABLE);

    // wyciwyg urls can only be loaded through history. Any normal load of
    // wyciwyg through docshell is  illegal. Disallow such loads.
    if (aLoadType & LOAD_CMD_NORMAL) {
        PRBool isWyciwyg = PR_FALSE;
        rv = aURI->SchemeIs("wyciwyg", &isWyciwyg);   
        if ((isWyciwyg && NS_SUCCEEDED(rv)) || NS_FAILED(rv)) 
            return NS_ERROR_FAILURE;
    }

    PRBool bIsJavascript = PR_FALSE;
    if (NS_FAILED(aURI->SchemeIs("javascript", &bIsJavascript))) {
        bIsJavascript = PR_FALSE;
    }

    //
    // First, notify any nsIContentPolicy listeners about the document load.
    // Only abort the load if a content policy listener explicitly vetos it!
    //
    nsCOMPtr<nsIDOMElement> requestingElement;
    // Use nsPIDOMWindow since we _want_ to cross the chrome boundary if needed
    nsCOMPtr<nsPIDOMWindow> privateWin(do_QueryInterface(mScriptGlobal));
    if (privateWin)
        requestingElement = privateWin->GetFrameElementInternal();

    PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
    PRUint32 contentType;
    if (IsFrame()) {
        NS_ASSERTION(requestingElement, "A frame but no DOM element!?");
        contentType = nsIContentPolicy::TYPE_SUBDOCUMENT;
    } else {
        contentType = nsIContentPolicy::TYPE_DOCUMENT;
    }

    nsISupports* context = requestingElement;
    if (!context) {
        context =  mScriptGlobal;
    }
    rv = NS_CheckContentLoadPolicy(contentType,
                                   aURI,
                                   aReferrer,
                                   context,
                                   EmptyCString(), //mime guess
                                   nsnull,         //extra
                                   &shouldLoad);

    if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
        if (NS_SUCCEEDED(rv) && shouldLoad == nsIContentPolicy::REJECT_TYPE) {
            return NS_ERROR_CONTENT_BLOCKED_SHOW_ALT;
        }

        return NS_ERROR_CONTENT_BLOCKED;
    }

    nsCOMPtr<nsISupports> owner(aOwner);
    //
    // Get an owner from the current document if necessary.  Note that we only
    // do this for URIs that inherit a security context; in particular we do
    // NOT do this for about:blank.  This way, random about:blank loads that
    // have no owner (which basically means they were done by someone from
    // chrome manually messing with our nsIWebNavigation or by C++ setting
    // document.location) don't get a funky principal.  If callers want
    // something interesting to happen with the about:blank principal in this
    // case, they should pass an owner in.
    //
    {
        PRBool inherits;
        if (!owner && (aFlags & INTERNAL_LOAD_FLAGS_INHERIT_OWNER) &&
            NS_SUCCEEDED(URIInheritsSecurityContext(aURI, &inherits)) &&
            inherits) {
            owner = GetInheritedPrincipal(PR_TRUE);
        }
    }

    //
    // Resolve the window target before going any further...
    // If the load has been targeted to another DocShell, then transfer the
    // load to it...
    //
    if (aWindowTarget && *aWindowTarget) {
        // We've already done our owner-inheriting.  Mask out that bit, so we
        // don't try inheriting an owner from the target window if we came up
        // with a null owner above.
        aFlags = aFlags & ~INTERNAL_LOAD_FLAGS_INHERIT_OWNER;
        
        // Locate the target DocShell.
        // This may involve creating a new toplevel window - if necessary.
        //
        nsCOMPtr<nsIDocShellTreeItem> targetItem;
        FindItemWithName(aWindowTarget, nsnull, this,
                         getter_AddRefs(targetItem));

        nsCOMPtr<nsIDocShell> targetDocShell = do_QueryInterface(targetItem);
        
        PRBool isNewWindow = PR_FALSE;
        if (!targetDocShell) {
            nsCOMPtr<nsIDOMWindowInternal> win =
                do_GetInterface(GetAsSupports(this));
            NS_ENSURE_TRUE(win, NS_ERROR_NOT_AVAILABLE);

            nsDependentString name(aWindowTarget);
            nsCOMPtr<nsIDOMWindow> newWin;
            rv = win->Open(EmptyString(), // URL to load
                           name,          // window name
                           EmptyString(), // Features
                           getter_AddRefs(newWin));

            // In some cases the Open call doesn't actually result in a new
            // window being opened.  We can detect these cases by examining the
            // document in |newWin|, if any.
            nsCOMPtr<nsPIDOMWindow> piNewWin = do_QueryInterface(newWin);
            if (piNewWin) {
                nsCOMPtr<nsIDocument> newDoc =
                    do_QueryInterface(piNewWin->GetExtantDocument());
                if (!newDoc || newDoc->IsInitialDocument()) {
                    isNewWindow = PR_TRUE;
                    aFlags |= INTERNAL_LOAD_FLAGS_FIRST_LOAD;
                }
            }

            nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(newWin);
            targetDocShell = do_QueryInterface(webNav);

            nsCOMPtr<nsIScriptObjectPrincipal> sop =
                do_QueryInterface(mScriptGlobal);
            nsCOMPtr<nsIURI> currentCodebase;

            if (sop) {
                nsIPrincipal *principal = sop->GetPrincipal();

                if (principal) {
                    principal->GetURI(getter_AddRefs(currentCodebase));
                }
            }

            // We opened a new window for the target, clone the
            // session storage if the current URI's domain matches
            // that of the loading URI.
            if (targetDocShell && currentCodebase && aURI) {
                nsCAutoString thisDomain, newDomain;
                nsresult gethostrv = currentCodebase->GetAsciiHost(thisDomain);
                gethostrv |= aURI->GetAsciiHost(newDomain);
                if (NS_SUCCEEDED(gethostrv) && thisDomain.Equals(newDomain)) {
                    nsCOMPtr<nsIDOMStorage> storage;
                    GetSessionStorageForURI(currentCodebase,
                                            getter_AddRefs(storage));
                    nsCOMPtr<nsPIDOMStorage> piStorage =
                        do_QueryInterface(storage);
                    if (piStorage) {
                        nsCOMPtr<nsIDOMStorage> newstorage =
                            piStorage->Clone(currentCodebase);
                        targetDocShell->AddSessionStorage(thisDomain,
                                                          newstorage);
                    }
                }
            }
        }
        
        //
        // Transfer the load to the target DocShell...  Pass nsnull as the
        // window target name from to prevent recursive retargeting!
        //
        if (NS_SUCCEEDED(rv) && targetDocShell) {
            rv = targetDocShell->InternalLoad(aURI,
                                              aReferrer,
                                              owner,
                                              aFlags,
                                              nsnull,         // No window target
                                              aTypeHint,
                                              aPostData,
                                              aHeadersData,
                                              aLoadType,
                                              aSHEntry,
                                              aFirstParty,
                                              aDocShell,
                                              aRequest);
            if (rv == NS_ERROR_NO_CONTENT) {
                // XXXbz except we never reach this code!
                if (isNewWindow) {
                    //
                    // At this point, a new window has been created, but the
                    // URI did not have any data associated with it...
                    //
                    // So, the best we can do, is to tear down the new window
                    // that was just created!
                    //
                    nsCOMPtr<nsIDOMWindowInternal> domWin =
                        do_GetInterface(targetDocShell);
                    if (domWin) {
                        domWin->Close();
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
            else if (isNewWindow) {
                // XXX: Once new windows are created hidden, the new
                //      window will need to be made visible...  For now,
                //      do nothing.
            }
        }

        // Else we ran out of memory, or were a popup and got blocked,
        // or something.
        
        return rv;
    }

    //
    // Load is being targetted at this docshell so return an error if the
    // docshell is in the process of being destroyed.
    //
    if (mIsBeingDestroyed) {
        return NS_ERROR_FAILURE;
    }

    // Before going any further vet loads initiated by external programs.
    if (aLoadType == LOAD_NORMAL_EXTERNAL) {
        // Disallow external chrome: loads targetted at content windows
        PRBool isChrome = PR_FALSE;
        if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) && isChrome) {
            NS_WARNING("blocked external chrome: url -- use '-chrome' option");
            return NS_ERROR_FAILURE;
        }

        // clear the decks to prevent context bleed-through (bug 298255)
        rv = CreateAboutBlankContentViewer(nsnull);
        if (NS_FAILED(rv))
            return NS_ERROR_FAILURE;

        // reset loadType so we don't have to add lots of tests for
        // LOAD_NORMAL_EXTERNAL after this point
        aLoadType = LOAD_NORMAL;
    }

    rv = CheckLoadingPermissions();
    if (NS_FAILED(rv)) {
        return rv;
    }

    mAllowKeywordFixup =
      (aFlags & INTERNAL_LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP) != 0;
    mURIResultedInDocument = PR_FALSE;  // reset the clock...
   
    //
    // First:
    // Check to see if the new URI is an anchor in the existing document.
    // Skip this check if we're doing some sort of abnormal load, if the
    // new load is a non-history load and has postdata, or if we're doing
    // a history load and the page identifiers of mOSHE and aSHEntry
    // don't match.
    //
    PRBool allowScroll = PR_TRUE;
    if (!aSHEntry) {
        allowScroll = (aPostData == nsnull);
    } else if (mOSHE) {
        PRUint32 ourPageIdent;
        mOSHE->GetPageIdentifier(&ourPageIdent);
        PRUint32 otherPageIdent;
        aSHEntry->GetPageIdentifier(&otherPageIdent);
        allowScroll = (ourPageIdent == otherPageIdent);
#ifdef DEBUG
        if (allowScroll) {
            nsCOMPtr<nsIInputStream> currentPostData;
            mOSHE->GetPostData(getter_AddRefs(currentPostData));
            NS_ASSERTION(currentPostData == aPostData,
                         "Different POST data for entries for the same page?");
        }
#endif
    }
    
    if ((aLoadType == LOAD_NORMAL ||
         aLoadType == LOAD_STOP_CONTENT ||
         LOAD_TYPE_HAS_FLAGS(aLoadType, LOAD_FLAGS_REPLACE_HISTORY) ||
         aLoadType == LOAD_HISTORY ||
         aLoadType == LOAD_LINK) && allowScroll) {
        PRBool wasAnchor = PR_FALSE;
        nscoord cx, cy;
        NS_ENSURE_SUCCESS(ScrollIfAnchor(aURI, &wasAnchor, aLoadType, &cx, &cy), NS_ERROR_FAILURE);
        if (wasAnchor) {
            mLoadType = aLoadType;
            mURIResultedInDocument = PR_TRUE;

            /* we need to assign mLSHE to aSHEntry right here, so that on History loads, 
             * SetCurrentURI() called from OnNewURI() will send proper 
             * onLocationChange() notifications to the browser to update
             * back/forward buttons. 
             */
            SetHistoryEntry(&mLSHE, aSHEntry);

            /* This is a anchor traversal with in the same page.
             * call OnNewURI() so that, this traversal will be 
             * recorded in session and global history.
             */
            OnNewURI(aURI, nsnull, mLoadType, PR_TRUE);
            nsCOMPtr<nsIInputStream> postData;
            PRUint32 pageIdent = PR_UINT32_MAX;
            
            if (mOSHE) {
                /* save current position of scroller(s) (bug 59774) */
                mOSHE->SetScrollPosition(cx, cy);
                // Get the postdata and page ident from the current page, if
                // the new load is being done via normal means.  Note that
                // "normal means" can be checked for just by checking for
                // LOAD_CMD_NORMAL, given the loadType and allowScroll check
                // above -- it filters out some LOAD_CMD_NORMAL cases that we
                // wouldn't want here.
                if (aLoadType & LOAD_CMD_NORMAL) {
                    mOSHE->GetPostData(getter_AddRefs(postData));
                    mOSHE->GetPageIdentifier(&pageIdent);
                }
            }
            
            /* Assign mOSHE to mLSHE. This will either be a new entry created
             * by OnNewURI() for normal loads or aSHEntry for history loads.
             */
            if (mLSHE) {
                SetHistoryEntry(&mOSHE, mLSHE);
                // Save the postData obtained from the previous page
                // in to the session history entry created for the 
                // anchor page, so that any history load of the anchor
                // page will restore the appropriate postData.
                if (postData)
                    mOSHE->SetPostData(postData);
                
                // Propagate our page ident to the new mOSHE so that
                // we'll know it just differed by a scroll on the page.
                if (pageIdent != PR_UINT32_MAX)
                    mOSHE->SetPageIdentifier(pageIdent);
            }

            /* restore previous position of scroller(s), if we're moving
             * back in history (bug 59774)
             */
            if (mOSHE && (aLoadType == LOAD_HISTORY || aLoadType == LOAD_RELOAD_NORMAL))
            {
                nscoord bx, by;
                mOSHE->GetScrollPosition(&bx, &by);
                SetCurScrollPosEx(bx, by);
            }

            /* Clear out mLSHE so that further anchor visits get
             * recorded in SH and SH won't misbehave. 
             */
            SetHistoryEntry(&mLSHE, nsnull);
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
                    shEntry->SetTitle(mTitle);
            }

            return NS_OK;
        }
    }
    
    // mContentViewer->PermitUnload can destroy |this| docShell, which
    // causes the next call of CanSavePresentation to crash. 
    // Hold onto |this| until we return, to prevent a crash from happening. 
    // (bug#331040)
    nsCOMPtr<nsIDocShell> kungFuDeathGrip(this);

    // Check if the page doesn't want to be unloaded. The javascript:
    // protocol handler deals with this for javascript: URLs.
    if (!bIsJavascript && mContentViewer) {
        PRBool okToUnload;
        rv = mContentViewer->PermitUnload(&okToUnload);

        if (NS_SUCCEEDED(rv) && !okToUnload) {
            // The user chose not to unload the page, interrupt the
            // load.
            return NS_OK;
        }
    }

    // Check for saving the presentation here, before calling Stop().
    // This is necessary so that we can catch any pending requests.
    // Since the new request has not been created yet, we pass null for the
    // new request parameter.
    // Also pass nsnull for the document, since it doesn't affect the return
    // value for our purposes here.
    PRBool savePresentation = CanSavePresentation(aLoadType, nsnull, nsnull);

    // Don't stop current network activity for javascript: URL's since
    // they might not result in any data, and thus nothing should be
    // stopped in those cases. In the case where they do result in
    // data, the javascript: URL channel takes care of stopping
    // current network activity.
    if (!bIsJavascript) {
        // Stop any current network activity.
        // Also stop content if this is a zombie doc. otherwise 
        // the onload will be delayed by other loads initiated in the 
        // background by the first document that
        // didn't fully load before the next load was initiated.
        // If not a zombie, don't stop content until data 
        // starts arriving from the new URI...

        nsCOMPtr<nsIContentViewer> zombieViewer;
        if (mContentViewer) {
            mContentViewer->GetPreviousViewer(getter_AddRefs(zombieViewer));
        }

        if (zombieViewer ||
            LOAD_TYPE_HAS_FLAGS(aLoadType, LOAD_FLAGS_STOP_CONTENT)) {
            rv = Stop(nsIWebNavigation::STOP_ALL);
        } else {
            rv = Stop(nsIWebNavigation::STOP_NETWORK);
        }

        if (NS_FAILED(rv)) 
            return rv;
    }

    mLoadType = aLoadType;
    
    // mLSHE should be assigned to aSHEntry, only after Stop() has
    // been called. But when loading an error page, do not clear the
    // mLSHE for the real page.
    if (mLoadType != LOAD_ERROR_PAGE)
        SetHistoryEntry(&mLSHE, aSHEntry);

    mSavingOldViewer = savePresentation;

    // If we have a saved content viewer in history, restore and show it now.
    if (aSHEntry && (mLoadType & LOAD_CMD_HISTORY)) {
        nsCOMPtr<nsISHEntry> oldEntry = mOSHE;
        PRBool restoring;
        rv = RestorePresentation(aSHEntry, &restoring);
        if (restoring)
            return rv;

        // We failed to restore the presentation, so clean up.
        // Both the old and new history entries could potentially be in
        // an inconsistent state.
        if (NS_FAILED(rv)) {
            if (oldEntry)
                oldEntry->SyncPresentationState();

            aSHEntry->SyncPresentationState();
        }
    }

    nsCOMPtr<nsIRequest> req;
    rv = DoURILoad(aURI, aReferrer,
                   !(aFlags & INTERNAL_LOAD_FLAGS_DONT_SEND_REFERRER),
                   owner, aTypeHint, aPostData, aHeadersData, aFirstParty,
                   aDocShell, getter_AddRefs(req),
                   (aFlags & INTERNAL_LOAD_FLAGS_FIRST_LOAD) != 0);
    if (req && aRequest)
        NS_ADDREF(*aRequest = req);

    if (NS_FAILED(rv)) {
        nsCOMPtr<nsIChannel> chan(do_QueryInterface(req));
        DisplayLoadError(rv, aURI, nsnull, chan);
    }
    
    return rv;
}

nsIPrincipal*
nsDocShell::GetInheritedPrincipal(PRBool aConsiderCurrentDocument)
{
    nsCOMPtr<nsIDocument> document;

    if (aConsiderCurrentDocument && mContentViewer) {
        nsCOMPtr<nsIDocumentViewer>
            docViewer(do_QueryInterface(mContentViewer));
        if (!docViewer)
            return nsnull;
        docViewer->GetDocument(getter_AddRefs(document));
    }

    if (!document) {
        nsCOMPtr<nsIDocShellTreeItem> parentItem;
        GetSameTypeParent(getter_AddRefs(parentItem));
        if (parentItem) {
            nsCOMPtr<nsIDOMDocument> parentDomDoc(do_GetInterface(parentItem));
            document = do_QueryInterface(parentDomDoc);
        }
    }

    if (!document) {
        if (!aConsiderCurrentDocument) {
            return nsnull;
        }

        // Make sure we end up with _something_ as the principal no matter
        // what.
        EnsureContentViewer();  // If this fails, we'll just get a null
                                // docViewer and bail.

        nsCOMPtr<nsIDocumentViewer>
            docViewer(do_QueryInterface(mContentViewer));
        if (!docViewer)
            return nsnull;
        docViewer->GetDocument(getter_AddRefs(document));
    }

    //-- Get the document's principal
    if (document) {
        return document->NodePrincipal();
    }

    return nsnull;
}

nsresult
nsDocShell::DoURILoad(nsIURI * aURI,
                      nsIURI * aReferrerURI,
                      PRBool aSendReferrer,
                      nsISupports * aOwner,
                      const char * aTypeHint,
                      nsIInputStream * aPostData,
                      nsIInputStream * aHeadersData,
                      PRBool aFirstParty,
                      nsIDocShell ** aDocShell,
                      nsIRequest ** aRequest,
                      PRBool aIsNewWindowTarget)
{
    nsresult rv;
    nsCOMPtr<nsIURILoader> uriLoader;

    uriLoader = do_GetService(NS_URI_LOADER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsLoadFlags loadFlags = nsIRequest::LOAD_NORMAL;
    if (aFirstParty) {
        // tag first party URL loads
        loadFlags |= nsIChannel::LOAD_INITIAL_DOCUMENT_URI;
    }

    if (mLoadType == LOAD_ERROR_PAGE) {
        // Error pages are LOAD_BACKGROUND
        loadFlags |= nsIChannel::LOAD_BACKGROUND;
    }

    // open a channel for the url
    nsCOMPtr<nsIChannel> channel;

    rv = NS_NewChannel(getter_AddRefs(channel),
                       aURI,
                       nsnull,
                       nsnull,
                       NS_STATIC_CAST(nsIInterfaceRequestor *, this),
                       loadFlags);
    if (NS_FAILED(rv)) {
        if (rv == NS_ERROR_UNKNOWN_PROTOCOL) {
            // This is a uri with a protocol scheme we don't know how
            // to handle.  Embedders might still be interested in
            // handling the load, though, so we fire a notification
            // before throwing the load away.
            PRBool abort = PR_FALSE;
            nsresult rv2 = mContentListener->OnStartURIOpen(aURI, &abort);
            if (NS_SUCCEEDED(rv2) && abort) {
                // Hey, they're handling the load for us!  How convenient!
                return NS_OK;
            }
        }
            
        return rv;
    }

    // Make sure to give the caller a channel if we managed to create one
    // This is important for correct error page/session history interaction
    if (aRequest)
        NS_ADDREF(*aRequest = channel);

    channel->SetOriginalURI(aURI);
    if (aTypeHint && *aTypeHint) {
        channel->SetContentType(nsDependentCString(aTypeHint));
        mContentTypeHint = aTypeHint;
    }
    else {
        mContentTypeHint.Truncate();
    }
    
    //hack
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
    nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal(do_QueryInterface(channel));
    if (httpChannelInternal) {
      if (aFirstParty) {
        httpChannelInternal->SetDocumentURI(aURI);
      } else {
        httpChannelInternal->SetDocumentURI(aReferrerURI);
      }
    }

    nsCOMPtr<nsIWritablePropertyBag2> props(do_QueryInterface(channel));
    if (props)
    {
      // save true referrer for those who need it (e.g. xpinstall whitelisting)
      // Currently only http and ftp channels support this.
      props->SetPropertyAsInterface(NS_LITERAL_STRING("docshell.internalReferrer"),
                                    aReferrerURI);
    }

    //
    // If this is a HTTP channel, then set up the HTTP specific information
    // (ie. POST data, referrer, ...)
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
            nsCOMPtr<nsISeekableStream>
                postDataSeekable(do_QueryInterface(aPostData));
            if (postDataSeekable) {
                rv = postDataSeekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
                NS_ENSURE_SUCCESS(rv, rv);
            }

            nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(httpChannel));
            NS_ASSERTION(uploadChannel, "http must support nsIUploadChannel");

            // we really need to have a content type associated with this stream!!
            uploadChannel->SetUploadStream(aPostData, EmptyCString(), -1);
            /* If there is a valid postdata *and* it is a History Load,
             * set up the cache key on the channel, to retrieve the
             * data *only* from the cache. If it is a normal reload, the 
             * cache is free to go to the server for updated postdata. 
             */
            if (cacheChannel && cacheKey) {
                if (mLoadType == LOAD_HISTORY || mLoadType == LOAD_RELOAD_CHARSET_CHANGE) {
                    cacheChannel->SetCacheKey(cacheKey);
                    PRUint32 loadFlags;
                    if (NS_SUCCEEDED(channel->GetLoadFlags(&loadFlags)))
                        channel->SetLoadFlags(loadFlags | nsICachingChannel::LOAD_ONLY_FROM_CACHE);
                }
                else if (mLoadType == LOAD_RELOAD_NORMAL)
                    cacheChannel->SetCacheKey(cacheKey);
            }         
        }
        else {
            /* If there is no postdata, set the cache key on the channel, and
             * do not set the LOAD_ONLY_FROM_CACHE flag, so that the channel
             * will be free to get it from net if it is not found in cache.
             * New cache may use it creatively on CGI pages with GET
             * method and even on those that say "no-cache"
             */
            if (mLoadType == LOAD_HISTORY || mLoadType == LOAD_RELOAD_NORMAL 
                || mLoadType == LOAD_RELOAD_CHARSET_CHANGE) {
                if (cacheChannel && cacheKey)
                    cacheChannel->SetCacheKey(cacheKey);
            }
        }
        if (aHeadersData) {
            rv = AddHeadersToChannel(aHeadersData, httpChannel);
        }
        // Set the referrer explicitly
        if (aReferrerURI && aSendReferrer) {
            // Referrer is currenly only set for link clicks here.
            httpChannel->SetReferrer(aReferrerURI);
        }
    }
    //
    // Set the owner of the channel, but only for channels that can't
    // provide their own security context.
    //
    // XXX: Is seems wrong that the owner is ignored - even if one is
    //      supplied) unless the URI is javascript or data or about:blank.
    // XXX: If this is ever changed, check all callers for what owners they're
    //      passing in.  In particular, see the code and comments in LoadURI
    //      where we fall back on inheriting the owner if called
    //      from chrome.  That would be very wrong if this code changed
    //      anything but channels that can't provide their own security context!
    //
    //      (Currently chrome URIs set the owner when they are created!
    //      So setting a NULL owner would be bad!)
    //

    PRBool inherit;
    // We expect URIInheritsSecurityContext to return success for an
    // about:blank URI, so don't call IsAboutBlank() if this call fails.
    rv = URIInheritsSecurityContext(aURI, &inherit);
    if (NS_SUCCEEDED(rv) && (inherit || IsAboutBlank(aURI))) {
        channel->SetOwner(aOwner);
        nsCOMPtr<nsIScriptChannel> scriptChannel = do_QueryInterface(channel);
        if (scriptChannel) {
            // Allow execution against our context if the principals match
            scriptChannel->
                SetExecutionPolicy(nsIScriptChannel::EXECUTE_NORMAL);
        }
    }

    if (aIsNewWindowTarget) {
        nsCOMPtr<nsIWritablePropertyBag2> props = do_QueryInterface(channel);
        if (props) {
            props->SetPropertyAsBool(
                NS_LITERAL_STRING("docshell.newWindowTarget"),
                PR_TRUE);
        }
    }

    rv = DoChannelLoad(channel, uriLoader);
    
    //
    // If the channel load failed, we failed and nsIWebProgress just ain't
    // gonna happen.
    //
    if (NS_SUCCEEDED(rv)) {
        if (aDocShell) {
          *aDocShell = this;
          NS_ADDREF(*aDocShell);
        }
    }

    return rv;
}

static NS_METHOD
AppendSegmentToString(nsIInputStream *in,
                      void *closure,
                      const char *fromRawSegment,
                      PRUint32 toOffset,
                      PRUint32 count,
                      PRUint32 *writeCount)
{
    // aFromSegment now contains aCount bytes of data.

    nsCAutoString *buf = NS_STATIC_CAST(nsCAutoString *, closure);
    buf->Append(fromRawSegment, count);

    // Indicate that we have consumed all of aFromSegment
    *writeCount = count;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::AddHeadersToChannel(nsIInputStream *aHeadersData,
                                nsIChannel *aGenericChannel)
{
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aGenericChannel);
    NS_ENSURE_STATE(httpChannel);

    PRUint32 numRead;
    nsCAutoString headersString;
    nsresult rv = aHeadersData->ReadSegments(AppendSegmentToString,
                                             &headersString,
                                             PR_UINT32_MAX,
                                             &numRead);
    NS_ENSURE_SUCCESS(rv, rv);

    // used during the manipulation of the String from the InputStream
    nsCAutoString headerName;
    nsCAutoString headerValue;
    PRInt32 crlf;
    PRInt32 colon;

    //
    // Iterate over the headersString: for each "\r\n" delimited chunk,
    // add the value as a header to the nsIHttpChannel
    //

    static const char kWhitespace[] = "\b\t\r\n ";
    while (PR_TRUE) {
        crlf = headersString.Find("\r\n");
        if (crlf == kNotFound)
            return NS_OK;

        const nsCSubstring &oneHeader = StringHead(headersString, crlf);

        colon = oneHeader.FindChar(':');
        if (colon == kNotFound)
            return NS_ERROR_UNEXPECTED;

        headerName = StringHead(oneHeader, colon);
        headerValue = Substring(oneHeader, colon + 1);

        headerName.Trim(kWhitespace);
        headerValue.Trim(kWhitespace);

        headersString.Cut(0, crlf + 2);

        //
        // FINALLY: we can set the header!
        // 

        rv = httpChannel->SetRequestHeader(headerName, headerValue, PR_TRUE);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    NS_NOTREACHED("oops");
    return NS_ERROR_UNEXPECTED;
}

nsresult nsDocShell::DoChannelLoad(nsIChannel * aChannel,
                                   nsIURILoader * aURILoader)
{
    nsresult rv;
    // Mark the channel as being a document URI and allow content sniffing...
    nsLoadFlags loadFlags = 0;
    (void) aChannel->GetLoadFlags(&loadFlags);
    loadFlags |= nsIChannel::LOAD_DOCUMENT_URI |
                 nsIChannel::LOAD_CALL_CONTENT_SNIFFERS;

    // Load attributes depend on load type...
    switch (mLoadType) {
    case LOAD_HISTORY:
        loadFlags |= nsIRequest::VALIDATE_NEVER;
        break;

    case LOAD_RELOAD_CHARSET_CHANGE:
        loadFlags |= nsIRequest::LOAD_FROM_CACHE;
        break;
    
    case LOAD_RELOAD_NORMAL:
    case LOAD_REFRESH:
        loadFlags |= nsIRequest::VALIDATE_ALWAYS;
        break;

    case LOAD_NORMAL_BYPASS_CACHE:
    case LOAD_NORMAL_BYPASS_PROXY:
    case LOAD_NORMAL_BYPASS_PROXY_AND_CACHE:
    case LOAD_RELOAD_BYPASS_CACHE:
    case LOAD_RELOAD_BYPASS_PROXY:
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
        loadFlags |= nsIRequest::LOAD_BYPASS_CACHE;
        break;

    case LOAD_NORMAL:
    case LOAD_LINK:
        // Set cache checking flags
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
        break;
    }

    (void) aChannel->SetLoadFlags(loadFlags);

    rv = aURILoader->OpenURI(aChannel,
                             (mLoadType == LOAD_LINK),
                             this);
    
    return rv;
}

NS_IMETHODIMP
nsDocShell::ScrollIfAnchor(nsIURI * aURI, PRBool * aWasAnchor,
                           PRUint32 aLoadType, nscoord *cx, nscoord *cy)
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

    nsCOMPtr<nsIPresShell> shell;
    nsresult rv = GetPresShell(getter_AddRefs(shell));
    if (NS_FAILED(rv) || !shell) {
        // If we failed to get the shell, or if there is no shell,
        // nothing left to do here.
        
        return rv;
    }

    // NOTE: we assume URIs are absolute for comparison purposes

    nsCAutoString currentSpec;
    NS_ENSURE_SUCCESS(mCurrentURI->GetSpec(currentSpec),
                      NS_ERROR_FAILURE);

    nsCAutoString newSpec;
    NS_ENSURE_SUCCESS(aURI->GetSpec(newSpec), NS_ERROR_FAILURE);

    // Search for hash marks in the current URI and the new URI and
    // take a copy of everything to the left of the hash for
    // comparison.

    const char kHash = '#';

    // Split the new URI into a left and right part
    // (assume we're parsing it out right
    nsACString::const_iterator urlStart, urlEnd, refStart, refEnd;
    newSpec.BeginReading(urlStart);
    newSpec.EndReading(refEnd);
    
    PRInt32 hashNew = newSpec.FindChar(kHash);
    if (hashNew == 0) {
        return NS_OK;           // Strange URI
    }

    if (hashNew > 0) {
        // found it
        urlEnd = urlStart;
        urlEnd.advance(hashNew);
        
        refStart = urlEnd;
        ++refStart;             // advanced past '#'
        
    }
    else {
        // no hash at all
        urlEnd = refStart = refEnd;
    }
    const nsACString& sNewLeft = Substring(urlStart, urlEnd);
    const nsACString& sNewRef =  Substring(refStart, refEnd);
                                          
    // Split the current URI in a left and right part
    nsACString::const_iterator currentLeftStart, currentLeftEnd;
    currentSpec.BeginReading(currentLeftStart);

    PRInt32 hashCurrent = currentSpec.FindChar(kHash);
    if (hashCurrent == 0) {
        return NS_OK;           // Strange URI 
    }

    if (hashCurrent > 0) {
        currentLeftEnd = currentLeftStart;
        currentLeftEnd.advance(hashCurrent);
    }
    else {
        currentSpec.EndReading(currentLeftEnd);
    }

    // If we have no new anchor, we do not want to scroll, unless there is a
    // current anchor and we are doing a history load.  So return if we have no
    // new anchor, and there is no current anchor or the load is not a history
    // load.
    NS_ASSERTION(hashNew != 0 && hashCurrent != 0,
                 "What happened to the early returns above?");
    if (hashNew == kNotFound &&
        (hashCurrent == kNotFound || aLoadType != LOAD_HISTORY)) {
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

    if (!Substring(currentLeftStart, currentLeftEnd).Equals(sNewLeft)) {
        return NS_OK;           // URIs not the same
    }

    // Now we know we are dealing with an anchor
    *aWasAnchor = PR_TRUE;

    // Both the new and current URIs refer to the same page. We can now
    // browse to the hash stored in the new URI.
    //
    // But first let's capture positions of scroller(s) that can
    // (and usually will) be modified by GoToAnchor() call.

    GetCurScrollPos(ScrollOrientation_X, cx);
    GetCurScrollPos(ScrollOrientation_Y, cy);

    if (!sNewRef.IsEmpty()) {
        // anchor is there, but if it's a load from history,
        // we don't have any anchor jumping to do
        PRBool scroll = aLoadType != LOAD_HISTORY &&
                        aLoadType != LOAD_RELOAD_NORMAL;

        char *str = ToNewCString(sNewRef);
        if (!str) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        // nsUnescape modifies the string that is passed into it.
        nsUnescape(str);

        // We assume that the bytes are in UTF-8, as it says in the
        // spec:
        // http://www.w3.org/TR/html4/appendix/notes.html#h-B.2.1

        // We try the UTF-8 string first, and then try the document's
        // charset (see below).  If the string is not UTF-8,
        // conversion will fail and give us an empty Unicode string.
        // In that case, we should just fall through to using the
        // page's charset.
        rv = NS_ERROR_FAILURE;
        NS_ConvertUTF8toUTF16 uStr(str);
        if (!uStr.IsEmpty()) {
            rv = shell->GoToAnchor(NS_ConvertUTF8toUTF16(str), scroll);
        }
        nsMemory::Free(str);

        // Above will fail if the anchor name is not UTF-8.  Need to
        // convert from document charset to unicode.
        if (NS_FAILED(rv)) {
                
            // Get a document charset
            NS_ENSURE_TRUE(mContentViewer, NS_ERROR_FAILURE);
            nsCOMPtr<nsIDocumentViewer>
                docv(do_QueryInterface(mContentViewer));
            NS_ENSURE_TRUE(docv, NS_ERROR_FAILURE);
            nsCOMPtr<nsIDocument> doc;
            rv = docv->GetDocument(getter_AddRefs(doc));
            NS_ENSURE_SUCCESS(rv, rv);
            const nsACString &aCharset = doc->GetDocumentCharacterSet();

            nsCOMPtr<nsITextToSubURI> textToSubURI =
                do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
            NS_ENSURE_SUCCESS(rv, rv);

            // Unescape and convert to unicode
            nsXPIDLString uStr;

            rv = textToSubURI->UnEscapeAndConvert(PromiseFlatCString(aCharset).get(),
                                                  PromiseFlatCString(sNewRef).get(),
                                                  getter_Copies(uStr));
            NS_ENSURE_SUCCESS(rv, rv);

            // Ignore return value of GoToAnchor, since it will return an error
            // if there is no such anchor in the document, which is actually a
            // success condition for us (we want to update the session history
            // with the new URI no matter whether we actually scrolled
            // somewhere).
            shell->GoToAnchor(uStr, scroll);
        }
    }
    else {

        // Tell the shell it's at an anchor, without scrolling.
        shell->GoToAnchor(EmptyString(), PR_FALSE);
        
        // An empty anchor was found, but if it's a load from history,
        // we don't have to jump to the top of the page. Scrollbar 
        // position will be restored by the caller, based on positions
        // stored in session history.
        if (aLoadType == LOAD_HISTORY || aLoadType == LOAD_RELOAD_NORMAL)
            return rv;
        //An empty anchor. Scroll to the top of the page.
        rv = SetCurScrollPosEx(0, 0);
    }

    return rv;
}

void
nsDocShell::SetupReferrerFromChannel(nsIChannel * aChannel)
{
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));
    if (httpChannel) {
        nsCOMPtr<nsIURI> referrer;
        nsresult rv = httpChannel->GetReferrer(getter_AddRefs(referrer));
        if (NS_SUCCEEDED(rv)) {
            SetReferrerURI(referrer);
        }
    }
}

PRBool
nsDocShell::OnNewURI(nsIURI * aURI, nsIChannel * aChannel,
                     PRUint32 aLoadType, PRBool aFireOnLocationChange,
                     PRBool aAddToGlobalHistory)
{
    NS_ASSERTION(aURI, "uri is null");
#if defined(PR_LOGGING) && defined(DEBUG)
    if (PR_LOG_TEST(gDocShellLog, PR_LOG_DEBUG)) {
        nsCAutoString spec;
        aURI->GetSpec(spec);

        nsCAutoString chanName;
        if (aChannel)
            aChannel->GetName(chanName);
        else
            chanName.AssignLiteral("<no channel>");

        PR_LOG(gDocShellLog, PR_LOG_DEBUG,
               ("nsDocShell[%p]::OnNewURI(\"%s\", [%s], 0x%x)\n", this, spec.get(),
                chanName.get(), aLoadType));
    }
#endif

    PRBool updateHistory = PR_TRUE;
    PRBool equalUri = PR_FALSE;
    PRBool shAvailable = PR_TRUE;  

    // Get the post data from the channel
    nsCOMPtr<nsIInputStream> inputStream;
    if (aChannel) {
        nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));
        
        // Check if the HTTPChannel is hiding under a multiPartChannel
        if (!httpChannel)  {
            GetHttpChannel(aChannel, getter_AddRefs(httpChannel));
        }

        if (httpChannel) {
            nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(httpChannel));
            if (uploadChannel) {
                uploadChannel->GetUploadStream(getter_AddRefs(inputStream));
            }
        }
    }
    /* Create SH Entry (mLSHE) only if there is a  SessionHistory object (mSessionHistory) in
     * the current frame or in the root docshell
     */
    nsCOMPtr<nsISHistory> rootSH = mSessionHistory;
    if (!rootSH) {
        // Get the handle to SH from the root docshell          
        GetRootSessionHistory(getter_AddRefs(rootSH));
        if (!rootSH)
            shAvailable = PR_FALSE;
    }  // rootSH


    // Determine if this type of load should update history.
    if (aLoadType == LOAD_BYPASS_HISTORY ||
        aLoadType == LOAD_ERROR_PAGE ||
        aLoadType & LOAD_CMD_HISTORY ||
        aLoadType & LOAD_CMD_RELOAD)
        updateHistory = PR_FALSE;

    // Check if the url to be loaded is the same as the one already loaded. 
    if (mCurrentURI)
        aURI->Equals(mCurrentURI, &equalUri);

#ifdef DEBUG
    PR_LOG(gDocShellLog, PR_LOG_DEBUG,
           ("  shAvailable=%i updateHistory=%i equalURI=%i\n",
            shAvailable, updateHistory, equalUri));
#endif

    /* If the url to be loaded is the same as the one already there,
     * and the original loadType is LOAD_NORMAL, LOAD_LINK, or
     * LOAD_STOP_CONTENT, set loadType to LOAD_NORMAL_REPLACE so that
     * AddToSessionHistory() won't mess with the current SHEntry and
     * if this page has any frame children, it also will be handled
     * properly. see bug 83684
     *
     * XXX Hopefully changing the loadType at this time will not hurt  
     *  anywhere. The other way to take care of sequentially repeating
     *  frameset pages is to add new methods to nsIDocShellTreeItem.
     * Hopefully I don't have to do that. 
     */
    if (equalUri &&
        (mLoadType == LOAD_NORMAL ||
         mLoadType == LOAD_LINK ||
         mLoadType == LOAD_STOP_CONTENT) &&
        !inputStream)
    {
        mLoadType = LOAD_NORMAL_REPLACE;
    }

    // If this is a refresh to the currently loaded url, we don't
    // have to update session or global history.
    if (mLoadType == LOAD_REFRESH && !inputStream && equalUri) {
        SetHistoryEntry(&mLSHE, mOSHE);
    }


    /* If the user pressed shift-reload, cache will create a new cache key
     * for the page. Save the new cacheKey in Session History. 
     * see bug 90098
     */
    if (aChannel &&
        (aLoadType == LOAD_RELOAD_BYPASS_CACHE ||
         aLoadType == LOAD_RELOAD_BYPASS_PROXY ||
         aLoadType == LOAD_RELOAD_BYPASS_PROXY_AND_CACHE)) {
        NS_ASSERTION(!updateHistory,
                     "We shouldn't be updating history for forced reloads!");
        
        nsCOMPtr<nsICachingChannel> cacheChannel(do_QueryInterface(aChannel));
        nsCOMPtr<nsISupports>  cacheKey;
        // Get the Cache Key  and store it in SH.         
        if (cacheChannel) 
            cacheChannel->GetCacheKey(getter_AddRefs(cacheKey));
        // If we already have a loading history entry, store the new cache key
        // in it.  Otherwise, since we're doing a reload and won't be updating
        // our history entry, store the cache key in our current history entry.
        if (mLSHE)
            mLSHE->SetCacheKey(cacheKey);
        else if (mOSHE)
            mOSHE->SetCacheKey(cacheKey);
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

        // Update Global history
        if (aAddToGlobalHistory) {
            // Get the referrer uri from the channel
            AddToGlobalHistory(aURI, PR_FALSE, aChannel);
        }
    }

    // If this was a history load, update the index in 
    // SH. 
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
    PRBool onLocationChangeNeeded = SetCurrentURI(aURI, aChannel,
                                                  aFireOnLocationChange);
    // Make sure to store the referrer from the channel, if any
    SetupReferrerFromChannel(aChannel);
    return onLocationChangeNeeded;
}

PRBool
nsDocShell::OnLoadingSite(nsIChannel * aChannel, PRBool aFireOnLocationChange,
                          PRBool aAddToGlobalHistory)
{
    nsCOMPtr<nsIURI> uri;
    // If this a redirect, use the final url (uri)
    // else use the original url
    //
    // Note that this should match what documents do (see nsDocument::Reset).
    nsLoadFlags loadFlags = 0;
    aChannel->GetLoadFlags(&loadFlags);
    if (loadFlags & nsIChannel::LOAD_REPLACE)
        aChannel->GetURI(getter_AddRefs(uri));
    else
        aChannel->GetOriginalURI(getter_AddRefs(uri));
    NS_ENSURE_TRUE(uri, PR_FALSE);

    return OnNewURI(uri, aChannel, mLoadType, aFireOnLocationChange,
                    aAddToGlobalHistory);

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
    nsCAutoString buf;

    rv = aURI->GetScheme(buf);
    if (NS_FAILED(rv))
        return PR_FALSE;

    if (buf.Equals("about")) {
        rv = aURI->GetPath(buf);
        if (NS_FAILED(rv))
            return PR_FALSE;

        if (buf.Equals("blank")) {
            return PR_FALSE;
        }
    }
    return PR_TRUE;
}

nsresult
nsDocShell::AddToSessionHistory(nsIURI * aURI,
                                nsIChannel * aChannel, nsISHEntry ** aNewEntry)
{
#if defined(PR_LOGGING) && defined(DEBUG)
    if (PR_LOG_TEST(gDocShellLog, PR_LOG_DEBUG)) {
        nsCAutoString spec;
        aURI->GetSpec(spec);

        nsCAutoString chanName;
        if (aChannel)
            aChannel->GetName(chanName);
        else
            chanName.AssignLiteral("<no channel>");

        PR_LOG(gDocShellLog, PR_LOG_DEBUG,
               ("nsDocShell[%p]::AddToSessionHistory(\"%s\", [%s])\n", this, spec.get(),
                chanName.get()));
    }
#endif

    nsresult rv = NS_OK;
    nsCOMPtr<nsISHEntry> entry;
    PRBool shouldPersist;

    shouldPersist = ShouldAddToSessionHistory(aURI);

    // Get a handle to the root docshell 
    nsCOMPtr<nsIDocShellTreeItem> root;
    GetSameTypeRootTreeItem(getter_AddRefs(root));     
    /*
     * If this is a LOAD_FLAGS_REPLACE_HISTORY in a subframe, we use
     * the existing SH entry in the page and replace the url and
     * other vitalities.
     */
    if (LOAD_TYPE_HAS_FLAGS(mLoadType, LOAD_FLAGS_REPLACE_HISTORY) &&
        root != NS_STATIC_CAST(nsIDocShellTreeItem *, this)) {
        // This is a subframe 
        entry = mOSHE;
        nsCOMPtr<nsISHContainer> shContainer(do_QueryInterface(entry));
        if (shContainer) {
            PRInt32 childCount = 0;
            shContainer->GetChildCount(&childCount);
            // Remove all children of this entry 
            for (PRInt32 i = childCount - 1; i >= 0; i--) {
                nsCOMPtr<nsISHEntry> child;
                shContainer->GetChildAt(i, getter_AddRefs(child));
                shContainer->RemoveChild(child);
            }  // for
        }  // shContainer
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
    nsCOMPtr<nsISupports> owner;
    PRBool expired = PR_FALSE;
    PRBool discardLayoutState = PR_FALSE;
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
        
        // Check if the httpChannel is hiding under a multipartChannel
        if (!httpChannel) {
            GetHttpChannel(aChannel, getter_AddRefs(httpChannel));
        }
        if (httpChannel) {
            nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(httpChannel));
            if (uploadChannel) {
                uploadChannel->GetUploadStream(getter_AddRefs(inputStream));
            }
            httpChannel->GetReferrer(getter_AddRefs(referrerURI));

            discardLayoutState = ShouldDiscardLayoutState(httpChannel);
        }
        aChannel->GetOwner(getter_AddRefs(owner));
    }

    //Title is set in nsDocShell::SetTitle()
    entry->Create(aURI,              // uri
                  EmptyString(),     // Title
                  inputStream,       // Post data stream
                  nsnull,            // LayoutHistory state
                  cacheKey,          // CacheKey
                  mContentTypeHint,  // Content-type
                  owner);            // Channel owner
    entry->SetReferrerURI(referrerURI);
    /* If cache got a 'no-store', ask SH not to store
     * HistoryLayoutState. By default, SH will set this
     * flag to PR_TRUE and save HistoryLayoutState.
     */    
    if (discardLayoutState) {
        entry->SetSaveLayoutStateFlag(PR_FALSE);
    }
    if (cacheToken) {
        // Check if the page has expired from cache 
        nsCOMPtr<nsICacheEntryInfo> cacheEntryInfo(do_QueryInterface(cacheToken));
        if (cacheEntryInfo) {        
            PRUint32 expTime;         
            cacheEntryInfo->GetExpirationTime(&expTime);         
            PRUint32 now = PRTimeToSeconds(PR_Now());                  
            if (expTime <=  now)            
                expired = PR_TRUE;         
         
        }
    }
    if (expired == PR_TRUE)
        entry->SetExpirationStatus(PR_TRUE);


    if (root == NS_STATIC_CAST(nsIDocShellTreeItem *, this) && mSessionHistory) {
        // This is the root docshell
        if (LOAD_TYPE_HAS_FLAGS(mLoadType, LOAD_FLAGS_REPLACE_HISTORY)) {            
            // Replace current entry in session history.
            PRInt32  index = 0;   
            mSessionHistory->GetIndex(&index);
            nsCOMPtr<nsISHistoryInternal>   shPrivate(do_QueryInterface(mSessionHistory));
            // Replace the current entry with the new entry
            if (shPrivate)
                rv = shPrivate->ReplaceEntry(index, entry);          
        }
        else {
            // Add to session history
            nsCOMPtr<nsISHistoryInternal>
                shPrivate(do_QueryInterface(mSessionHistory));
            NS_ENSURE_TRUE(shPrivate, NS_ERROR_FAILURE);
            mSessionHistory->GetIndex(&mPreviousTransIndex);
            rv = shPrivate->AddEntry(entry, shouldPersist);
            mSessionHistory->GetIndex(&mLoadedTransIndex);
#ifdef DEBUG_PAGE_CACHE
            printf("Previous index: %d, Loaded index: %d\n\n",
                   mPreviousTransIndex, mLoadedTransIndex);
#endif
        }
    }
    else {  
        // This is a subframe.
        if (!mOSHE || !LOAD_TYPE_HAS_FLAGS(mLoadType,
                                           LOAD_FLAGS_REPLACE_HISTORY))
            rv = DoAddChildSHEntry(entry, mChildOffset);
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
    nsCOMPtr<nsIURI> uri;
    nsCOMPtr<nsIInputStream> postData;
    nsCOMPtr<nsIURI> referrerURI;
    nsCAutoString contentType;
    nsCOMPtr<nsISupports> owner;

    NS_ENSURE_TRUE(aEntry, NS_ERROR_FAILURE);

    NS_ENSURE_SUCCESS(aEntry->GetURI(getter_AddRefs(uri)), NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(aEntry->GetReferrerURI(getter_AddRefs(referrerURI)),
                      NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(aEntry->GetPostData(getter_AddRefs(postData)),
                      NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(aEntry->GetContentType(contentType), NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(aEntry->GetOwner(getter_AddRefs(owner)),
                      NS_ERROR_FAILURE);

    // Calling CreateAboutBlankContentViewer can set mOSHE to null, and if
    // that's the only thing holding a ref to aEntry that will cause aEntry to
    // die while we're loading it.  So hold a strong ref to aEntry here, just
    // in case.
    nsCOMPtr<nsISHEntry> kungFuDeathGrip(aEntry);
    PRBool isJS;
    nsresult rv = uri->SchemeIs("javascript", &isJS);
    if (NS_FAILED(rv) || isJS) {
        // We're loading a URL that will execute script from inside asyncOpen.
        // Replace the current document with about:blank now to prevent
        // anything from the current document from leaking into any JavaScript
        // code in the URL.
        rv = CreateAboutBlankContentViewer(nsnull);

        if (NS_FAILED(rv)) {
            // The creation of the intermittent about:blank content
            // viewer failed for some reason (potentially because the
            // user prevented it). Interrupt the history load.
            return NS_OK;
        }

        if (!owner) {
            // Ensure that we have an owner.  Otherwise javascript: URIs will
            // pick it up from the about:blank page we just loaded, and we
            // don't really want even that in this case.
            owner = do_CreateInstance("@mozilla.org/nullprincipal;1");
            NS_ENSURE_TRUE(owner, NS_ERROR_OUT_OF_MEMORY);
        }
    }

    /* If there is a valid postdata *and* the user pressed
     * reload or shift-reload, take user's permission before we  
     * repost the data to the server.
     */
    if ((aLoadType & LOAD_CMD_RELOAD) && postData) {
      PRBool repost;
      rv = ConfirmRepost(&repost);
      if (NS_FAILED(rv)) return rv;

      // If the user pressed cancel in the dialog, return.  We're done here.
      if (!repost)
        return NS_BINDING_ABORTED;
    }

    rv = InternalLoad(uri,
                      referrerURI,
                      owner,
                      INTERNAL_LOAD_FLAGS_NONE, // Do not inherit owner from document (security-critical!)
                      nsnull,            // No window target
                      contentType.get(), // Type hint
                      postData,          // Post data stream
                      nsnull,            // No headers stream
                      aLoadType,         // Load type
                      aEntry,            // SHEntry
                      PR_TRUE,
                      nsnull,            // No nsIDocShell
                      nsnull);           // No nsIRequest
    return rv;
}

NS_IMETHODIMP nsDocShell::GetShouldSaveLayoutState(PRBool* aShould)
{
    *aShould = PR_FALSE;
    if (mOSHE) {
        // Don't capture historystate and save it in history
        // if the page asked not to do so.
        mOSHE->GetSaveLayoutStateFlag(aShould);
    }

    return NS_OK;
}

NS_IMETHODIMP nsDocShell::PersistLayoutHistoryState()
{
    nsresult  rv = NS_OK;
    
    if (mOSHE) {
        PRBool shouldSave;
        GetShouldSaveLayoutState(&shouldSave);
        if (!shouldSave)
            return NS_OK;

        nsCOMPtr<nsIPresShell> shell;
        rv = GetPresShell(getter_AddRefs(shell));
        if (NS_SUCCEEDED(rv) && shell) {
            nsCOMPtr<nsILayoutHistoryState> layoutState;
            rv = shell->CaptureHistoryState(getter_AddRefs(layoutState),
                                            PR_TRUE);
        }
    }

    return rv;
}

/* static */ nsresult
nsDocShell::WalkHistoryEntries(nsISHEntry *aRootEntry,
                               nsDocShell *aRootShell,
                               WalkHistoryEntriesFunc aCallback,
                               void *aData)
{
    NS_ENSURE_TRUE(aRootEntry, NS_ERROR_FAILURE);

    nsCOMPtr<nsISHContainer> container(do_QueryInterface(aRootEntry));
    if (!container)
        return NS_ERROR_FAILURE;

    PRInt32 childCount;
    container->GetChildCount(&childCount);
    for (PRInt32 i = 0; i < childCount; i++) {
        nsCOMPtr<nsISHEntry> childEntry;
        container->GetChildAt(i, getter_AddRefs(childEntry));
        if (!childEntry) {
            // childEntry can be null for valid reasons, for example if the
            // docshell at index i never loaded anything useful.
            continue;
        }

        nsDocShell *childShell = nsnull;
        if (aRootShell) {
            // Walk the children of aRootShell and see if one of them
            // has srcChild as a SHEntry.

            PRInt32 childCount = aRootShell->mChildList.Count();
            for (PRInt32 j = 0; j < childCount; ++j) {
                nsDocShell *child =
                    NS_STATIC_CAST(nsDocShell*, aRootShell->ChildAt(j));

                if (child->HasHistoryEntry(childEntry)) {
                    childShell = child;
                    break;
                }
            }
        }
        nsresult rv = aCallback(childEntry, childShell, i, aData);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

// callback data for WalkHistoryEntries
struct CloneAndReplaceData
{
    CloneAndReplaceData(PRUint32 aCloneID, nsISHEntry *aReplaceEntry,
                        nsISHEntry *aDestTreeParent)
        : cloneID(aCloneID),
          replaceEntry(aReplaceEntry),
          destTreeParent(aDestTreeParent) { }

    PRUint32              cloneID;
    nsISHEntry           *replaceEntry;
    nsISHEntry           *destTreeParent;
    nsCOMPtr<nsISHEntry>  resultEntry;
};

/* static */ nsresult
nsDocShell::CloneAndReplaceChild(nsISHEntry *aEntry, nsDocShell *aShell,
                                 PRInt32 aEntryIndex, void *aData)
{
    nsresult result = NS_OK;
    nsCOMPtr<nsISHEntry> dest;

    CloneAndReplaceData *data = NS_STATIC_CAST(CloneAndReplaceData*, aData);
    PRUint32 cloneID = data->cloneID;
    nsISHEntry *replaceEntry = data->replaceEntry;

    PRUint32 srcID;
    aEntry->GetID(&srcID);

    if (srcID == cloneID) {
        // Just replace the entry, and don't walk the children.
        dest = replaceEntry;
        dest->SetIsSubFrame(PR_TRUE);
    } else {
        // Clone the SHEntry...
        result = aEntry->Clone(getter_AddRefs(dest));
        if (NS_FAILED(result))
            return result;

        // This entry is for a subframe navigation
        dest->SetIsSubFrame(PR_TRUE);

        // Walk the children
        CloneAndReplaceData childData(cloneID, replaceEntry, dest);
        result = WalkHistoryEntries(aEntry, aShell,
                                    CloneAndReplaceChild, &childData);
        if (NS_FAILED(result))
            return result;

        if (aShell)
            aShell->SwapHistoryEntries(aEntry, dest);
    }

    nsCOMPtr<nsISHContainer> container =
        do_QueryInterface(data->destTreeParent);
    if (container)
        container->AddChild(dest, aEntryIndex);

    data->resultEntry = dest;
    return result;
}

/* static */ nsresult
nsDocShell::CloneAndReplace(nsISHEntry *aSrcEntry,
                                   nsDocShell *aSrcShell,
                                   PRUint32 aCloneID,
                                   nsISHEntry *aReplaceEntry,
                                   nsISHEntry **aResultEntry)
{
    NS_ENSURE_ARG_POINTER(aResultEntry);
    NS_ENSURE_TRUE(aReplaceEntry, NS_ERROR_FAILURE);

    CloneAndReplaceData data(aCloneID, aReplaceEntry, nsnull);
    nsresult rv = CloneAndReplaceChild(aSrcEntry, aSrcShell, 0, &data);

    data.resultEntry.swap(*aResultEntry);
    return rv;
}


void
nsDocShell::SwapHistoryEntries(nsISHEntry *aOldEntry, nsISHEntry *aNewEntry)
{
    if (aOldEntry == mOSHE)
        mOSHE = aNewEntry;

    if (aOldEntry == mLSHE)
        mLSHE = aNewEntry;
}


struct SwapEntriesData
{
    nsDocShell *ignoreShell;     // constant; the shell to ignore
    nsISHEntry *destTreeRoot;    // constant; the root of the dest tree
    nsISHEntry *destTreeParent;  // constant; the node under destTreeRoot
                                 // whose children will correspond to aEntry
};


nsresult
nsDocShell::SetChildHistoryEntry(nsISHEntry *aEntry, nsDocShell *aShell,
                                 PRInt32 aEntryIndex, void *aData)
{
    SwapEntriesData *data = NS_STATIC_CAST(SwapEntriesData*, aData);
    nsDocShell *ignoreShell = data->ignoreShell;

    if (!aShell || aShell == ignoreShell)
        return NS_OK;

    nsISHEntry *destTreeRoot = data->destTreeRoot;

    nsCOMPtr<nsISHEntry> destEntry;
    nsCOMPtr<nsISHContainer> container =
        do_QueryInterface(data->destTreeParent);

    if (container) {
        // aEntry is a clone of some child of destTreeParent, but since the
        // trees aren't necessarily in sync, we'll have to locate it.
        // Note that we could set aShell's entry to null if we don't find a
        // corresponding entry under destTreeParent.

        PRUint32 targetID, id;
        aEntry->GetID(&targetID);

        // First look at the given index, since this is the common case.
        nsCOMPtr<nsISHEntry> entry;
        container->GetChildAt(aEntryIndex, getter_AddRefs(entry));
        if (entry && NS_SUCCEEDED(entry->GetID(&id)) && id == targetID) {
            destEntry.swap(entry);
        } else {
            PRInt32 childCount;
            container->GetChildCount(&childCount);
            for (PRInt32 i = 0; i < childCount; ++i) {
                container->GetChildAt(i, getter_AddRefs(entry));
                if (!entry)
                    continue;

                entry->GetID(&id);
                if (id == targetID) {
                    destEntry.swap(entry);
                    break;
                }
            }
        }
    } else {
        destEntry = destTreeRoot;
    }

    aShell->SwapHistoryEntries(aEntry, destEntry);

    // Now handle the children of aEntry.
    SwapEntriesData childData = { ignoreShell, destTreeRoot, destEntry };
    return WalkHistoryEntries(aEntry, aShell,
                              SetChildHistoryEntry, &childData);
}


static nsISHEntry*
GetRootSHEntry(nsISHEntry *aEntry)
{
    nsCOMPtr<nsISHEntry> rootEntry = aEntry;
    nsISHEntry *result = nsnull;
    while (rootEntry) {
        result = rootEntry;
        result->GetParent(getter_AddRefs(rootEntry));
    }

    return result;
}


void
nsDocShell::SetHistoryEntry(nsCOMPtr<nsISHEntry> *aPtr, nsISHEntry *aEntry)
{
    // We need to sync up the docshell and session history trees for
    // subframe navigation.  If the load was in a subframe, we forward up to
    // the root docshell, which will then recursively sync up all docshells
    // to their corresponding entries in the new session history tree.
    // If we don't do this, then we can cache a content viewer on the wrong
    // cloned entry, and subsequently restore it at the wrong time.

    nsISHEntry *newRootEntry = GetRootSHEntry(aEntry);
    if (newRootEntry) {
        // newRootEntry is now the new root entry.
        // Find the old root entry as well.

        // Need a strong ref. on |oldRootEntry| so it isn't destroyed when
        // SetChildHistoryEntry() does SwapHistoryEntries() (bug 304639).
        nsCOMPtr<nsISHEntry> oldRootEntry = GetRootSHEntry(*aPtr);
        if (oldRootEntry) {
            nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
            GetSameTypeParent(getter_AddRefs(parentAsItem));
            nsCOMPtr<nsIDocShell> rootShell = do_QueryInterface(parentAsItem);
            if (rootShell) { // if we're the root just set it, nothing to swap
                SwapEntriesData data = { this, newRootEntry };
                nsIDocShell *rootIDocShell =
                    NS_STATIC_CAST(nsIDocShell*, rootShell);
                nsDocShell *rootDocShell = NS_STATIC_CAST(nsDocShell*,
                                                          rootIDocShell);

#ifdef NS_DEBUG
                nsresult rv =
#endif
                SetChildHistoryEntry(oldRootEntry, rootDocShell,
                                                   0, &data);
                NS_ASSERTION(NS_SUCCEEDED(rv), "SetChildHistoryEntry failed");
            }
        }
    }

    *aPtr = aEntry;
}


nsresult
nsDocShell::GetRootSessionHistory(nsISHistory ** aReturn)
{
    nsresult rv;

    nsCOMPtr<nsIDocShellTreeItem> root;
    //Get the root docshell
    rv = GetSameTypeRootTreeItem(getter_AddRefs(root));
    // QI to nsIWebNavigation
    nsCOMPtr<nsIWebNavigation> rootAsWebnav(do_QueryInterface(root));
    if (rootAsWebnav) {
        // Get the handle to SH from the root docshell
        rv = rootAsWebnav->GetSessionHistory(aReturn);
    }
    return rv;
}

nsresult
nsDocShell::GetHttpChannel(nsIChannel * aChannel, nsIHttpChannel ** aReturn)
{
    NS_ENSURE_ARG_POINTER(aReturn);
    if (!aChannel)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIMultiPartChannel>  multiPartChannel(do_QueryInterface(aChannel));
    if (multiPartChannel) {
        nsCOMPtr<nsIChannel> baseChannel;
        multiPartChannel->GetBaseChannel(getter_AddRefs(baseChannel));
        nsCOMPtr<nsIHttpChannel>  httpChannel(do_QueryInterface(baseChannel));
        *aReturn = httpChannel;
        NS_IF_ADDREF(*aReturn);
    }
    return NS_OK;
}

PRBool 
nsDocShell::ShouldDiscardLayoutState(nsIHttpChannel * aChannel)
{    
    // By default layout State will be saved. 
    if (!aChannel)
        return PR_FALSE;

    // figure out if SH should be saving layout state 
    nsCOMPtr<nsISupports> securityInfo;
    PRBool noStore = PR_FALSE, noCache = PR_FALSE;
    aChannel->GetSecurityInfo(getter_AddRefs(securityInfo));
    aChannel->IsNoStoreResponse(&noStore);
    aChannel->IsNoCacheResponse(&noCache);

    return (noStore || (noCache && securityInfo));
}

//*****************************************************************************
// nsDocShell: nsIEditorDocShell
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::GetEditor(nsIEditor * *aEditor)
{
  NS_ENSURE_ARG_POINTER(aEditor);
  nsresult rv = EnsureEditorData();
  if (NS_FAILED(rv)) return rv;
  
  return mEditorData->GetEditor(aEditor);
}

NS_IMETHODIMP nsDocShell::SetEditor(nsIEditor * aEditor)
{
  nsresult rv = EnsureEditorData();
  if (NS_FAILED(rv)) return rv;

  return mEditorData->SetEditor(aEditor);
}


NS_IMETHODIMP nsDocShell::GetEditable(PRBool *aEditable)
{
  NS_ENSURE_ARG_POINTER(aEditable);
  *aEditable = mEditorData && mEditorData->GetEditable();
  return NS_OK;
}


NS_IMETHODIMP nsDocShell::GetHasEditingSession(PRBool *aHasEditingSession)
{
  NS_ENSURE_ARG_POINTER(aHasEditingSession);
  
  if (mEditorData)
  {
    nsCOMPtr<nsIEditingSession> editingSession;
    mEditorData->GetEditingSession(getter_AddRefs(editingSession));
    *aHasEditingSession = (editingSession.get() != nsnull);
  }
  else
  {
    *aHasEditingSession = PR_FALSE;
  }

  return NS_OK;
}

NS_IMETHODIMP nsDocShell::MakeEditable(PRBool inWaitForUriLoad)
{
  nsresult rv = EnsureEditorData();
  if (NS_FAILED(rv)) return rv;

  return mEditorData->MakeEditable(inWaitForUriLoad);
}

nsresult
nsDocShell::AddToGlobalHistory(nsIURI * aURI, PRBool aRedirect,
                               nsIChannel * aChannel)
{
    if (mItemType != typeContent || !mGlobalHistory)
        return NS_OK;

    PRBool visited;
    nsresult rv = mGlobalHistory->IsVisited(aURI, &visited);
    if (NS_FAILED(rv))
        return rv;

    // Get referrer from the channel. We have to check for a property on a
    // property bag because the referrer may be empty for security reasons (for
    // example, when loading a http page with a https referrer).
    nsCOMPtr<nsIURI> referrer;
    nsCOMPtr<nsIPropertyBag2> props(do_QueryInterface(aChannel));
    if (props) {
        props->GetPropertyAsInterface(NS_LITERAL_STRING("docshell.internalReferrer"),
                                      NS_GET_IID(nsIURI),
                                      getter_AddRefs(referrer));
    }

    rv = mGlobalHistory->AddURI(aURI, aRedirect, !IsFrame(), referrer);
    if (NS_FAILED(rv))
        return rv;

    if (!visited) {
        nsCOMPtr<nsIObserverService> obsService =
            do_GetService("@mozilla.org/observer-service;1");
        if (obsService) {
            obsService->NotifyObservers(aURI, NS_LINK_VISITED_EVENT_TOPIC, nsnull);
        }
    }

    return NS_OK;
}

//*****************************************************************************
// nsDocShell: Helper Routines
//*****************************************************************************

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

nsresult
nsDocShell::ConfirmRepost(PRBool * aRepost)
{
  nsresult rv;
  nsCOMPtr<nsIPrompt> prompter;
  CallGetInterface(this, NS_STATIC_CAST(nsIPrompt**, getter_AddRefs(prompter)));

  nsCOMPtr<nsIStringBundleService> 
      stringBundleService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundle> appBundle;
  rv = stringBundleService->CreateBundle(kAppstringsBundleURL,
                                         getter_AddRefs(appBundle));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundle> brandBundle;
  rv = stringBundleService->CreateBundle(kBrandBundleURL, getter_AddRefs(brandBundle));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(prompter && brandBundle && appBundle,
               "Unable to set up repost prompter.");

  nsXPIDLString brandName;
  rv = brandBundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                                      getter_Copies(brandName));
  if (NS_FAILED(rv)) return rv;
  const PRUnichar *formatStrings[] = { brandName.get() };

  nsXPIDLString msgString, button0Title;
  rv = appBundle->FormatStringFromName(NS_LITERAL_STRING("confirmRepost").get(),
                                        formatStrings, NS_ARRAY_LENGTH(formatStrings),
                                        getter_Copies(msgString));
  if (NS_FAILED(rv)) return rv;

  rv = appBundle->GetStringFromName(NS_LITERAL_STRING("resendButton.label").get(),
                                    getter_Copies(button0Title));
  if (NS_FAILED(rv)) return rv;

  PRInt32 buttonPressed;
  rv = prompter->
         ConfirmEx(nsnull, msgString.get(),
                   (nsIPrompt::BUTTON_POS_0 * nsIPrompt::BUTTON_TITLE_IS_STRING) +
                   (nsIPrompt::BUTTON_POS_1 * nsIPrompt::BUTTON_TITLE_CANCEL),
                   button0Title.get(), nsnull, nsnull, nsnull, nsnull, &buttonPressed);
  if (NS_FAILED(rv)) return rv;

  *aRepost = (buttonPressed == 0);
  return NS_OK;
}

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
                      CreateBundle(kAppstringsBundleURL,
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
    NS_ENSURE_TRUE(shell, NS_ERROR_NULL_POINTER);

    NS_ENSURE_SUCCESS(shell->GetViewManager()->GetRootScrollableView(aOutScrollView),
                      NS_ERROR_FAILURE);

    if (*aOutScrollView == nsnull) {
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::EnsureScriptEnvironment()
{
    if (mScriptGlobal)
        return NS_OK;

    if (mIsBeingDestroyed) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    nsCOMPtr<nsIDOMScriptObjectFactory> factory =
        do_GetService(kDOMScriptObjectFactoryCID);
    NS_ENSURE_TRUE(factory, NS_ERROR_FAILURE);

    factory->NewScriptGlobalObject(mItemType == typeChrome,
                                   getter_AddRefs(mScriptGlobal));
    NS_ENSURE_TRUE(mScriptGlobal, NS_ERROR_FAILURE);

    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(mScriptGlobal));
    win->SetDocShell(NS_STATIC_CAST(nsIDocShell *, this));
    mScriptGlobal->
        SetGlobalObjectOwner(NS_STATIC_CAST
                             (nsIScriptGlobalObjectOwner *, this));

    // Ensure the script object is set to run javascript - other languages
    // setup on demand.
    // XXXmarkh - should this be setup to run the default language for this doc?
    nsresult rv;
    rv = mScriptGlobal->EnsureScriptEnvironment(nsIProgrammingLanguage::JAVASCRIPT);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
}


NS_IMETHODIMP
nsDocShell::EnsureEditorData()
{
    if (!mEditorData && !mIsBeingDestroyed)
    {
        mEditorData = new nsDocShellEditorData(this);
        if (!mEditorData) return NS_ERROR_OUT_OF_MEMORY;
    }

    return mEditorData ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

nsresult
nsDocShell::EnsureTransferableHookData()
{
    if (!mTransferableHookData) {
        mTransferableHookData = new nsTransferableHookData();
        if (!mTransferableHookData) return NS_ERROR_OUT_OF_MEMORY;
    }

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
    // up to point to the focused, or content window, so we have to
    // set that up each time.

    nsIScriptGlobalObject* scriptGO = GetScriptGlobalObject();
    NS_ENSURE_TRUE(scriptGO, NS_ERROR_UNEXPECTED);

    // default to our window
    nsCOMPtr<nsIDOMWindow> rootWindow = do_QueryInterface(scriptGO);
    nsCOMPtr<nsIDOMWindow> windowToSearch = rootWindow;

    // if we can, search the focused window
    nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(scriptGO);
    nsIFocusController *focusController = nsnull;
    if (ourWindow)
        focusController = ourWindow->GetRootFocusController();
    if (focusController)
    {
        nsCOMPtr<nsIDOMWindowInternal> focusedWindow;
        focusController->GetFocusedWindow(getter_AddRefs(focusedWindow));
        if (focusedWindow)
            windowToSearch = focusedWindow;
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
    nsCOMPtr<nsIDocShellTreeItem> parent =
        do_QueryInterface(GetAsSupports(mParent));
    if (parent) {
        PRInt32 parentType = ~mItemType;        // Not us
        parent->GetItemType(&parentType);
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
    printf(">>>>>>>>>> nsDocShell::SetHasFocus: %p  %s\n", (void*)this,
           aHasFocus?"Yes":"No");
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

// Find an nsICanvasFrame under aFrame.  Only search the principal
// child lists.  aFrame must be non-null.
static nsICanvasFrame* FindCanvasFrame(nsIFrame* aFrame)
{
    nsICanvasFrame* canvasFrame;
    if (NS_SUCCEEDED(CallQueryInterface(aFrame, &canvasFrame))) {
        return canvasFrame;
    }

    nsIFrame* kid = aFrame->GetFirstChild(nsnull);
    while (kid) {
        canvasFrame = FindCanvasFrame(kid);
        if (canvasFrame) {
            return canvasFrame;
        }
        kid = kid->GetNextSibling();
    }

    return nsnull;
}

//-------------------------------------------------------
// Tells the HTMLFrame/CanvasFrame that is now has focus
NS_IMETHODIMP
nsDocShell::SetCanvasHasFocus(PRBool aCanvasHasFocus)
{
  if (mEditorData && mEditorData->GetEditable())
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsIPresShell> presShell;
  GetPresShell(getter_AddRefs(presShell));
  if (!presShell) return NS_ERROR_FAILURE;

  nsIDocument *doc = presShell->GetDocument();
  if (!doc) return NS_ERROR_FAILURE;

  nsIContent *rootContent = doc->GetRootContent();
  if (rootContent) {
      nsIFrame* frame = presShell->GetPrimaryFrameFor(rootContent);
      if (frame) {
          frame = frame->GetParent();
          if (frame) {
              nsICanvasFrame* canvasFrame;
              if (NS_SUCCEEDED(CallQueryInterface(frame, &canvasFrame))) {
                  return canvasFrame->SetHasFocus(aCanvasHasFocus);
              }
          }
      }
  } else {
      // Look for the frame the hard way
      nsIFrame* frame = presShell->GetRootFrame();
      if (frame) {
          nsICanvasFrame* canvasFrame = FindCanvasFrame(frame);
          if (canvasFrame) {
              return canvasFrame->SetHasFocus(aCanvasHasFocus);
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

/* boolean IsBeingDestroyed (); */
NS_IMETHODIMP 
nsDocShell::IsBeingDestroyed(PRBool *aDoomed)
{
  NS_ENSURE_ARG(aDoomed);
  *aDoomed = mIsBeingDestroyed;
  return NS_OK;
}


NS_IMETHODIMP 
nsDocShell::GetIsExecutingOnLoadHandler(PRBool *aResult)
{
  NS_ENSURE_ARG(aResult);
  *aResult = mIsExecutingOnLoadHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetLayoutHistoryState(nsILayoutHistoryState **aLayoutHistoryState)
{
  if (mOSHE)
    mOSHE->GetLayoutHistoryState(aLayoutHistoryState);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetLayoutHistoryState(nsILayoutHistoryState *aLayoutHistoryState)
{
  if (mOSHE)
    mOSHE->SetLayoutHistoryState(aLayoutHistoryState);
  return NS_OK;
}

//*****************************************************************************
//***    nsRefreshTimer: Object Management
//*****************************************************************************

nsRefreshTimer::nsRefreshTimer()
    : mDelay(0), mRepeat(PR_FALSE), mMetaRefresh(PR_FALSE)
{
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
//******************************************************************************
NS_IMETHODIMP
nsRefreshTimer::Notify(nsITimer * aTimer)
{
    NS_ASSERTION(mDocShell, "DocShell is somehow null");

    if (mDocShell && aTimer) {
        // Get the delay count to determine load type
        PRUint32 delay = 0;
        aTimer->GetDelay(&delay);
        nsCOMPtr<nsIRefreshURI> refreshURI = do_QueryInterface(mDocShell);
        if (refreshURI)
            refreshURI->ForceRefreshURI(mURI, delay, mMetaRefresh);
    }
    return NS_OK;
}

//*****************************************************************************
//***    nsDocShellFocusController: Object Management
//*****************************************************************************
void 
nsDocShellFocusController::Focus(nsIDocShell* aDocShell)
{
#ifdef DEBUG_DOCSHELL_FOCUS
  printf("****** nsDocShellFocusController Focus To: %p  Blur To: %p\n",
         (void*)aDocShell, (void*)mFocusedDocShell);
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

//*****************************************************************************
// nsDocShell::InterfaceRequestorProxy
//*****************************************************************************
nsDocShell::InterfaceRequestorProxy::InterfaceRequestorProxy(nsIInterfaceRequestor* p)
{
    if (p) {
        mWeakPtr = do_GetWeakReference(p);
    }
}
 
nsDocShell::InterfaceRequestorProxy::~InterfaceRequestorProxy()
{
    mWeakPtr = nsnull;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDocShell::InterfaceRequestorProxy, nsIInterfaceRequestor) 
  
NS_IMETHODIMP 
nsDocShell::InterfaceRequestorProxy::GetInterface(const nsIID & aIID, void **aSink)
{
    NS_ENSURE_ARG_POINTER(aSink);
    nsCOMPtr<nsIInterfaceRequestor> ifReq = do_QueryReferent(mWeakPtr);
    if (ifReq) {
        return ifReq->GetInterface(aIID, aSink);
    }
    *aSink = nsnull;
    return NS_NOINTERFACE;
}

nsresult
nsDocShell::SetBaseUrlForWyciwyg(nsIContentViewer * aContentViewer)
{
    if (!aContentViewer)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIURI> baseURI;
    nsCOMPtr<nsIDocument> document;
    nsresult rv = NS_ERROR_NOT_AVAILABLE;

    if (sURIFixup)
        rv = sURIFixup->CreateExposableURI(mCurrentURI,
                                           getter_AddRefs(baseURI));

    // Get the current document and set the base uri
    if (baseURI) {
        nsCOMPtr<nsIDocumentViewer> docViewer(do_QueryInterface(aContentViewer));
        if (docViewer) {
            rv = docViewer->GetDocument(getter_AddRefs(document));
            if (document)
                rv = document->SetBaseURI(baseURI);
        }
    }
    return rv;
}

//*****************************************************************************
// nsDocShell::nsIAuthPromptProvider
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::GetAuthPrompt(PRUint32 aPromptReason, const nsIID& iid,
                          void** aResult)
{
    // a priority prompt request will override a false mAllowAuth setting
    PRBool priorityPrompt = (aPromptReason == PROMPT_PROXY);

    if (!mAllowAuth && !priorityPrompt)
        return NS_ERROR_NOT_AVAILABLE;

    // we're either allowing auth, or it's a proxy request
    nsresult rv;
    nsCOMPtr<nsIPromptFactory> wwatch =
      do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = EnsureScriptEnvironment();
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMWindow> window(do_QueryInterface(mScriptGlobal));

    // Get the an auth prompter for our window so that the parenting
    // of the dialogs works as it should when using tabs.

    return wwatch->GetPrompt(window, iid,
                             NS_REINTERPRET_CAST(void**, aResult));
}

//*****************************************************************************
// nsDocShell::nsIObserver
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::Observe(nsISupports *aSubject, const char *aTopic,
                    const PRUnichar *aData)
{
    nsresult rv = NS_OK;
    if (mObserveErrorPages &&
        !nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) &&
        !nsCRT::strcmp(aData,
          NS_LITERAL_STRING("browser.xul.error_pages.enabled").get())) {

        nsCOMPtr<nsIPrefBranch> prefs(do_QueryInterface(aSubject, &rv));
        NS_ENSURE_SUCCESS(rv, rv);

        PRBool tmpbool;
        rv = prefs->GetBoolPref("browser.xul.error_pages.enabled", &tmpbool);
        if (NS_SUCCEEDED(rv))
            mUseErrorPages = tmpbool;

    } else {
        rv = NS_ERROR_UNEXPECTED;
    }
    return rv;
}

/* static */
nsresult
nsDocShell::URIInheritsSecurityContext(nsIURI* aURI, PRBool* aResult)
{
    // Note: about:blank URIs do NOT inherit the security context from the
    // current document, which is what this function tests for...
    return NS_URIChainHasFlags(aURI,
                               nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
                               aResult);
}

/* static */
PRBool
nsDocShell::IsAboutBlank(nsIURI* aURI)
{
    NS_PRECONDITION(aURI, "Must have URI");
    
    // GetSpec can be expensive for some URIs, so check the scheme first.
    PRBool isAbout = PR_FALSE;
    if (NS_FAILED(aURI->SchemeIs("about", &isAbout)) || !isAbout) {
        return PR_FALSE;
    }
    
    nsCAutoString str;
    aURI->GetSpec(str);
    return str.EqualsLiteral("about:blank");
}
                                     
