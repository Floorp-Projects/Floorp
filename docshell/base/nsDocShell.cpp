/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: ft=cpp tw=78 sw=4 et ts=4 sts=4 cin
 *
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

#include "nsIComponentManager.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMElement.h"
#include "nsIDocumentViewer.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIPluginHost.h"
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
#include "nsIChromeEventHandler.h"
#include "nsIDOMWindowInternal.h"
#include "nsIWebBrowserChrome.h"
#include "nsPoint.h"
#include "nsGfxCIID.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsTextFormatter.h"
#include "nsIHttpEventSink.h"
#include "nsIUploadChannel.h"
#include "nsISecurityEventSink.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSContextStack.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsDocumentCharsetInfoCID.h"
#include "nsICanvasFrame.h"
#include "nsContentPolicyUtils.h" // NS_CheckContentLoadPolicy(...)
#include "nsICategoryManager.h"
#include "nsXPCOMCID.h"
#include "nsISeekableStream.h"
#include "nsAutoPtr.h"

// we want to explore making the document own the load group
// so we can associate the document URI with the load group.
// until this point, we have an evil hack:
#include "nsIHttpChannelInternal.h"  


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
#include "nsIDataChannel.h"
#include "nsIProgressEventSink.h"
#include "nsIWebProgress.h"
#include "nsILayoutHistoryState.h"
#include "nsITimer.h"
#include "nsISHistoryInternal.h"
#include "nsIPrincipal.h"
#include "nsIHistoryEntry.h"
#include "nsISHistoryListener.h"

// Pull in various NS_ERROR_* definitions
#include "nsIDNSService.h"
#include "nsISocketTransportService.h"
#include "nsISocketProvider.h"

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

#ifdef DEBUG_DOCSHELL_FOCUS
#include "nsIEventStateManager.h"
#endif

#include "nsIFrame.h"

// for embedding
#include "nsIWebBrowserChromeFocus.h"

#include "nsPluginError.h"

static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);
static NS_DEFINE_CID(kDocumentCharsetInfoCID, NS_DOCUMENTCHARSETINFO_CID);
static NS_DEFINE_CID(kPluginManagerCID, NS_PLUGINMANAGER_CID);
static NS_DEFINE_CID(kDOMScriptObjectFactoryCID,
                     NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

#if defined(DEBUG_bryner)
//#define DEBUG_DOCSHELL_FOCUS
#endif

#include "plevent.h"

// Number of documents currently loading
static PRInt32 gNumberOfDocumentsLoading = 0;

// Global count of existing docshells.
static PRInt32 gDocShellCount = 0;

// Global reference to the URI fixup service.
nsIURIFixup *nsDocShell::sURIFixup = 0;

// Hint for native dispatch of plevents on how long to delay after 
// all documents have loaded in milliseconds before favoring normal
// native event dispatch priorites over performance
#define NS_EVENT_STARVATION_DELAY_HINT 2000

// This is needed for displaying an error message 
// when navigation is attempted on a document when printing
// The value arbitrary as long as it doesn't conflict with
// any of the other values in the errors in DisplayLoadError
#define NS_ERROR_DOCUMENT_IS_PRINTMODE  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GENERAL,2001)
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
    mAllowSubframes(PR_TRUE),
    mAllowPlugins(PR_TRUE),
    mAllowJavascript(PR_TRUE),
    mAllowMetaRedirects(PR_TRUE),
    mAllowImages(PR_TRUE),
    mFocusDocFirst(PR_FALSE),
    mHasFocus(PR_FALSE),
    mCreatingDocument(PR_FALSE),
    mUseErrorPages(PR_FALSE),
    mAllowAuth(PR_TRUE),
    mFiredUnloadEvent(PR_FALSE),
    mEODForCurrentDocument(PR_FALSE),
    mURIResultedInDocument(PR_FALSE),
    mIsBeingDestroyed(PR_FALSE),
    mUseExternalProtocolHandler(PR_FALSE),
    mDisallowPopupWindows(PR_FALSE),
    mValidateOrigin(PR_TRUE), // validate frame origins by default
    mIsExecutingOnLoadHandler(PR_FALSE),
    mIsPrintingOrPP(PR_FALSE),
    mAppType(nsIDocShell::APP_TYPE_UNKNOWN),
    mChildOffset(0),
    mBusyFlags(BUSY_FLAGS_NONE),
    mMarginWidth(0),
    mMarginHeight(0),
    mItemType(typeContent),
    mContentListener(nsnull),
    mCurrentScrollbarPref(Scrollbar_Auto, Scrollbar_Auto),
    mDefaultScrollbarPref(Scrollbar_Auto, Scrollbar_Auto),
    mEditorData(nsnull),
    mParent(nsnull),
    mTreeOwner(nsnull),
    mChromeEventHandler(nsnull)
{
    if (gDocShellCount++ == 0) {
        NS_ASSERTION(sURIFixup == nsnull,
                     "Huh, sURIFixup not null in first nsDocShell ctor!");

        CallGetService(NS_URIFIXUP_CONTRACTID, &sURIFixup);
    }

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

    if (--gDocShellCount == 0) {
        NS_IF_RELEASE(sURIFixup);
    }
}

NS_IMETHODIMP
nsDocShell::DestroyChildren()
{
    PRInt32 i, n = mChildren.Count();
    nsCOMPtr<nsIDocShellTreeItem> shell;
    for (i = 0; i < n; i++) {
        shell = dont_AddRef((nsIDocShellTreeItem *) mChildren.ElementAt(i));
        NS_WARN_IF_FALSE(shell, "docshell has null child");

        if (shell) {
            shell->SetParent(nsnull);
            shell->SetTreeOwner(nsnull);
            // just clear out the array.  When the nsFrameFrame that holds
            // the subshell is destroyed, then the Destroy() method of
            // that subshell will actually get called.
        }
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
    NS_INTERFACE_MAP_ENTRY(nsIContentViewerContainer)
    NS_INTERFACE_MAP_ENTRY(nsIEditorDocShell)
    NS_INTERFACE_MAP_ENTRY(nsIWebPageDescriptor)
    NS_INTERFACE_MAP_ENTRY(nsIAuthPromptProvider)
NS_INTERFACE_MAP_END_THREADSAFE

///*****************************************************************************
// nsDocShell::nsIInterfaceRequestor
//*****************************************************************************   
NS_IMETHODIMP nsDocShell::GetInterface(const nsIID & aIID, void **aSink)
{
    NS_PRECONDITION(aSink, "null out param");

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
    else if (aIID.Equals(NS_GET_IID(nsPIDOMWindow)) &&
             NS_SUCCEEDED(EnsureScriptEnvironment())) {
        NS_ENSURE_SUCCESS(mScriptGlobal->
                          QueryInterface(NS_GET_IID(nsPIDOMWindow), aSink),
                          NS_ERROR_FAILURE);
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
        return NS_OK;
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
        // if auth is not allowed, bail out
        if (!mAllowAuth)
          return NS_NOINTERFACE;

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
                          GetDocumentLoaderForContext(this,
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
        docShellLoadType = nsIDocShellLoadInfo::loadBypassHistory;
        break;
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
                    PRBool firstParty)
{
    nsresult rv;
    nsCOMPtr<nsIURI> referrer;
    nsCOMPtr<nsIInputStream> postStream;
    nsCOMPtr<nsIInputStream> headersStream;
    nsCOMPtr<nsISupports> owner;
    PRBool inheritOwner = PR_FALSE;
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
    }

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gDocShellLog, PR_LOG_DEBUG)) {
        nsCAutoString uristr;
        aURI->GetAsciiSpec(uristr);
        PR_LOG(gDocShellLog, PR_LOG_DEBUG,
               ("nsDocShell[%p]: loading %s with flags 0x%08x",
                this, uristr.get(), aLoadFlags));
    }
#endif

    if (!shEntry && loadType != LOAD_NORMAL_REPLACE) {
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
                    if (shEntry && (parentLoadType == LOAD_NORMAL || parentLoadType == LOAD_LINK)) {
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
                    PRUint32 parentBusy=BUSY_FLAGS_NONE, selfBusy = BUSY_FLAGS_NONE;
                    parentDS->GetBusyFlags(&parentBusy);                    
                    GetBusyFlags(&selfBusy);
                    if (((parentBusy & BUSY_FLAGS_BUSY) || (selfBusy & BUSY_FLAGS_BUSY)) && shEntry) {
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

        rv = InternalLoad(aURI,
                          referrer,
                          owner,
                          inheritOwner,
                          target.get(),
                          nsnull,         // No type hint
                          postStream,
                          headersStream,
                          loadType,
                          nsnull,         // No SHEntry
                          firstParty,
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
nsDocShell::FireUnloadNotification()
{
    if (mContentViewer && !mFiredUnloadEvent) {
        mFiredUnloadEvent = PR_TRUE;

        mContentViewer->Unload();

        PRInt32 i, n = mChildren.Count();
        for (i = 0; i < n; i++) {
            nsIDocShellTreeItem* item = (nsIDocShellTreeItem*) mChildren.ElementAt(i);

            nsCOMPtr<nsIDocShell> shell(do_QueryInterface(item));
            if (shell) {
                shell->FireUnloadNotification();
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
  nsCAutoString targetScheme;
  nsresult rv = aTargetURI->GetScheme(targetScheme);
  NS_ENSURE_SUCCESS(rv, PR_TRUE);

  nsCAutoString originScheme;
  rv = aOriginURI->GetScheme(originScheme);
  NS_ENSURE_SUCCESS(rv, PR_TRUE);

  if (strcmp(targetScheme.get(), originScheme.get()))
    return PR_FALSE; // Different schemes - check fails

  if (! strcmp(targetScheme.get(), "file"))
    return PR_TRUE; // All file: urls are considered to have the same origin.

  if (! strcmp(targetScheme.get(), "imap") ||
      ! strcmp(targetScheme.get(), "mailbox") ||
      ! strcmp(targetScheme.get(), "news"))
  {

    // Each message is a distinct trust domain; use the whole spec for comparison
    nsCAutoString targetSpec;
    rv =aTargetURI->GetAsciiSpec(targetSpec);
    NS_ENSURE_SUCCESS(rv, PR_TRUE);

    nsCAutoString originSpec;
    rv = aOriginURI->GetAsciiSpec(originSpec);
    NS_ENSURE_SUCCESS(rv, PR_TRUE);

    return (! strcmp(targetSpec.get(), originSpec.get())); // True if full spec is same, false otherwise
  }

  // Compare ports.
  int targetPort, originPort;
  rv = aTargetURI->GetPort(&targetPort);
  NS_ENSURE_SUCCESS(rv, PR_TRUE);

  rv = aOriginURI->GetPort(&originPort);
  NS_ENSURE_SUCCESS(rv, PR_TRUE);

  if (targetPort != originPort)
    return PR_FALSE; // Different port - check fails

  // Need to check the hosts
  nsCAutoString targetHost;
  rv = aTargetURI->GetHost(targetHost);
  NS_ENSURE_SUCCESS(rv, PR_TRUE);

  nsCAutoString originHost;
  rv = aOriginURI->GetHost(originHost);
  NS_ENSURE_SUCCESS(rv, PR_TRUE);

  if (!strcmp(targetHost.get(), originHost.get()))
    return PR_TRUE; // Hosts are the same - check passed
  
  // If document.domain was set, do the relaxed check
  // Right align hostnames and compare - ensure preceeding char is . or /
  if (aDocumentDomainSet)
  {
    int targetHostLen = targetHost.Length();
    int originHostLen = originHost.Length();
    int prefixChar = originHostLen-targetHostLen-1;

    return ((originHostLen > targetHostLen) &&
            (! strcmp((originHost.get()+prefixChar+1), targetHost.get())) &&
            (originHost.CharAt(prefixChar) == '.' || originHost.CharAt(prefixChar) == '/'));
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
PRBool ValidateOrigin(nsIDocShellTreeItem* aOriginTreeItem,
                      nsIDocShellTreeItem* aTargetTreeItem)
{
  // Get origin document uri (ignoring document.domain)
  nsCOMPtr<nsIWebNavigation> originWebNav(do_QueryInterface(aOriginTreeItem));
  NS_ENSURE_TRUE(originWebNav, PR_TRUE);

  nsCOMPtr<nsIURI> originDocumentURI;
  nsresult rv = originWebNav->GetCurrentURI(getter_AddRefs(originDocumentURI));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && originDocumentURI, PR_TRUE);

  // Get target principal uri (including document.domain)
  nsCOMPtr<nsIDOMDocument> targetDOMDocument(do_GetInterface(aTargetTreeItem));
  nsCOMPtr<nsIDocument> targetDocument(do_QueryInterface(targetDOMDocument));
  NS_ENSURE_TRUE(targetDocument, PR_TRUE);

  nsIPrincipal *targetPrincipal = targetDocument->GetPrincipal();
  NS_ENSURE_TRUE(targetPrincipal, PR_TRUE);

  nsCOMPtr<nsIURI> targetPrincipalURI;
  rv = targetPrincipal->GetURI(getter_AddRefs(targetPrincipalURI));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && targetPrincipalURI, PR_TRUE);

  // Find out if document.domain was set for HTML documents
  PRBool documentDomainSet = PR_FALSE;
  nsCOMPtr<nsIHTMLDocument> targetHTMLDocument(do_QueryInterface(targetDocument));

  // If we don't have an HTML document, fall through with documentDomainSet false
  if (targetHTMLDocument) {
    documentDomainSet = targetHTMLDocument->WasDomainSet();
  }

  // Is origin same principal or a subdomain of target's document.domain
  // Compare actual URI of origin document, not origin principal's URI. (Per Nav 4.x)
  return SameOrSubdomainOfTarget(originDocumentURI, targetPrincipalURI,
                                 documentDomainSet);
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

    if(!name.Length() || name.LowerCaseEqualsLiteral("_self"))
    {
        *aResult = this;
    }
    else if (name.LowerCaseEqualsLiteral("_blank") || name.LowerCaseEqualsLiteral("_new"))
    {
        mustMakeNewWindow = PR_TRUE;
        name.Truncate();
    }
    else if(name.LowerCaseEqualsLiteral("_parent"))
    {
        GetSameTypeParent(getter_AddRefs(treeItem));
        if(!treeItem)
            *aResult = this;
    }
    else if(name.LowerCaseEqualsLiteral("_top"))
    {
        GetSameTypeRootTreeItem(getter_AddRefs(treeItem));
        if(!treeItem)
            *aResult = this;
    }
    // _main is an IE target which should be case-insensitive but isn't
    // see bug 217886 for details
    else if(name.LowerCaseEqualsLiteral("_content") || name.EqualsLiteral("_main"))
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
            nsCOMPtr<nsIDocShellTreeItem> tmp;
            treeItem->GetSameTypeRootTreeItem(getter_AddRefs(tmp));

            nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
            GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));

            if (sameTypeRoot != tmp && treeItem != tmp) {
                // The load was targeted at a frame and initiated in
                // another toplevel window. Assume we'll need to make
                // a new window until we find that the target, or one
                // of its ancestors, are from the same origin as the
                // loading docshell.
                mustMakeNewWindow = PR_TRUE;

                tmp = treeItem;

                do {
                    // Is origin frame from the same domain as target frame?
                    if (ValidateOrigin(this, tmp)) {
                        mustMakeNewWindow = PR_FALSE;

                        break;
                    }

                    nsCOMPtr<nsIDocShellTreeItem> t;
                    tmp->GetSameTypeParent(getter_AddRefs(t));
                    tmp.swap(t);
                } while (tmp);

                if (mustMakeNewWindow) {
                    // Origin mismatch, open the URL in a new blank
                    // window.
                    treeItem = nsnull;
                    name.Truncate();
                }
            }
        }
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

        rv = parentWindow->Open(EmptyString(),            // URL to load
                                name,                     // Window name
                                EmptyString(),            // Window features
                                getter_AddRefs(newWindow));
        if (NS_FAILED(rv)) return rv;

        // Get the DocShell from the new window...
        nsCOMPtr<nsIScriptGlobalObject> sgo;
        sgo = do_QueryInterface(newWindow, &rv);
        if (NS_FAILED(rv)) return rv;

        // This will AddRef() aResult...
        *aResult = sgo->GetDocShell();

        // If all went well, indicate that a new window has been created.
        if (*aResult) {
            NS_ADDREF(*aResult);

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
                nsCAutoString defaultCharset;
                nsCAutoString prevDocCharset;
                rv = muCV->GetDefaultCharacterSet(defaultCharset);
                if(NS_SUCCEEDED(rv)) {
                  target_muCV->SetDefaultCharacterSet(defaultCharset);
                }
                rv = muCV->GetPrevDocCharacterSet(prevDocCharset);
                if(NS_SUCCEEDED(rv)) {
                  target_muCV->SetPrevDocCharacterSet(prevDocCharset);
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
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(aPresContext);
    *aPresContext = nsnull;

    if (mContentViewer) {
        nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(mContentViewer));

        if (docv) {
            rv = docv->GetPresContext(aPresContext);
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
nsDocShell::SetChromeEventHandler(nsIChromeEventHandler * aChromeEventHandler)
{
    // Weak reference. Don't addref.
    mChromeEventHandler = aChromeEventHandler;

    NS_ASSERTION(!mScriptGlobal,
                 "SetChromeEventHandler() called after the script global "
                 "object was created! This means that the script global "
                 "object in this docshell won't get the right chrome event "
                 "handler. You really don't want to see this assert, FIX "
                 "YOUR CODE!");

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

/* [noscript] void setCurrentURI (in nsIURI uri); */
NS_IMETHODIMP
nsDocShell::SetCurrentURI(nsIURI *aURI)
{
    mCurrentURI = aURI;         //This assignment addrefs
    PRBool isRoot = PR_FALSE;   // Is this the root docshell
    PRBool isSubFrame = PR_FALSE;  // Is this a subframe navigation?

    if (!mLoadCookie)
      return NS_OK; 

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
      return NS_OK; 
    }
    
    NS_ASSERTION(loader, "No document loader");
    if (loader) {
        loader->FireOnLocationChange(webProgress, nsnull, aURI);
    }

    return NS_OK; 
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
    nsIViewManager* vm = presShell->GetViewManager();
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
        if (aForward)
            *aTookFocus = NS_SUCCEEDED(chromeFocus->FocusNextElement());
        else
            *aTookFocus = NS_SUCCEEDED(chromeFocus->FocusPrevElement());
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

    // If parent is another docshell, we inherit all their flags for
    // allowing plugins, scripting etc.
    nsCOMPtr<nsIDocShell> parentAsDocShell = do_QueryInterface(mParent);
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
    if (mName.Equals(aName) && ItemIsActive(this)) {
        *_retval = this;
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

    // This may fail, but comparing against null serves the same purpose
    nsCOMPtr<nsIDocShellTreeOwner>
        reqAsTreeOwner(do_GetInterface(aRequestor));

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
  nsCOMPtr<nsPresContext> presContext;
  parentAsDocShell->GetPresContext(getter_AddRefs(presContext));
  nsIDocument *doc = presShell->GetDocument();

  nsCOMPtr<nsIScriptGlobalObject> sgo;
  doc->GetScriptGlobalObject(getter_AddRefs(sgo));
  nsCOMPtr<nsIDOMWindowInternal> domwin(do_QueryInterface(sgo));

  nsCOMPtr<nsIWidget> widget;
  nsIViewManager* vm = presShell->GetViewManager();
  if (vm) {
    vm->GetWidget(getter_AddRefs(widget));
  }
  nsCOMPtr<nsIContent> rootContent;
  doc->GetRootContent(getter_AddRefs(rootContent));

  printf("DS %p  Ty %s  Doc %p DW %p EM %p CN %p\n",  
    parentAsDocShell.get(), 
    type==nsIDocShellTreeItem::typeChrome?"Chr":"Con", 
     doc.get(), domwin.get(),
     presContext->EventStateManager(), rootContent.get());

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
                webProgress->AddProgressListener(newListener,
                                                 nsIWebProgress::NOTIFY_ALL);
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

    NS_WARN_IF_FALSE(aIndex >=0 && aIndex < mChildren.Count(), "index of child element is out of range!");
    *aChild = (nsIDocShellTreeItem *) mChildren.SafeElementAt(aIndex);
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
        if (childNameEquals && ItemIsActive(child)) {
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
        /* Just pass this along */
        nsCOMPtr<nsIDocShellHistory> parent(do_QueryInterface(mParent, &rv));
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

    nsresult rv;
    nsCOMPtr<nsIDocShellHistory> parent(do_QueryInterface(mParent, &rv));
    if (parent) {
        rv = parent->AddChildSHEntry(mOSHE, aNewEntry, aChildOffset);
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
    // Create the fixup object if necessary
    if (!sURIFixup) {
        // No fixup service so try and create a URI and see what happens
        nsAutoString uriString(aURI);
        // Cleanup the empty spaces that might be on each end.
        uriString.Trim(" ");
        // Eliminate embedded newlines, which single-line text fields now allow:
        uriString.StripChars("\r\n");
        NS_ENSURE_TRUE(!uriString.IsEmpty(), NS_ERROR_FAILURE);

        rv = NS_NewURI(getter_AddRefs(uri), uriString);
    } else {
        // Call the fixup object
        rv = sURIFixup->CreateFixupURI(NS_ConvertUCS2toUTF8(aURI),
                                       nsIURIFixup::FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP,
                                       getter_AddRefs(uri));
    }

    if (NS_ERROR_MALFORMED_URI == rv) {
        DisplayLoadError(rv, uri, aURI);
    }

    if (NS_FAILED(rv) || !uri)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
    rv = CreateLoadInfo(getter_AddRefs(loadInfo));
    if (NS_FAILED(rv)) return rv;
    
    PRUint32 loadType = MAKE_LOAD_TYPE(LOAD_NORMAL, aLoadFlags); 
    loadInfo->SetLoadType(ConvertLoadTypeToDocShellLoadInfo(loadType));
    loadInfo->SetPostDataStream(aPostStream);
    loadInfo->SetReferrer(aReferringURI);
    loadInfo->SetHeadersStream(aHeaderStream);

    rv = LoadURI(uri, loadInfo, 0, PR_TRUE);
    
    return rv;
}

NS_IMETHODIMP
nsDocShell::DisplayLoadError(nsresult aError, nsIURI *aURI, const PRUnichar *aURL)
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

    // Turn the error code into a human readable error message.
    if (NS_ERROR_UNKNOWN_PROTOCOL == aError) {
        NS_ENSURE_ARG_POINTER(aURI);
        // extract the scheme
        nsCAutoString scheme;
        aURI->GetScheme(scheme);
        CopyASCIItoUCS2(scheme, formatStrs[0]);
        formatStrCount = 1;
        error.AssignLiteral("protocolNotFound");
    }
    else if (NS_ERROR_FILE_NOT_FOUND == aError) {
        NS_ENSURE_ARG_POINTER(aURI);
        nsCAutoString spec;
        aURI->GetPath(spec);
        nsCAutoString charset;
        // unescape and convert from origin charset
        aURI->GetOriginCharset(charset);
        nsCOMPtr<nsITextToSubURI> textToSubURI(
                do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv));
        if (NS_SUCCEEDED(rv))
          rv = textToSubURI->UnEscapeURIForUI(charset, spec, formatStrs[0]);
        if (NS_FAILED(rv)) {
          CopyASCIItoUCS2(spec, formatStrs[0]);
          rv = NS_OK;
        }
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
        }
    }

    // Test if the error should be displayed
    if (error.IsEmpty()) {
        return NS_OK;
    }

    // Test if the error needs to be formatted
    nsAutoString messageStr;
    if (formatStrCount > 0) {
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
    if (mUseErrorPages) {
        // Display an error page
        LoadErrorPage(aURI, aURL, error.get(), messageStr.get());
    } 
    else
    {
        // Display a message box
        prompter->Alert(nsnull, messageStr.get());
    }

    return NS_OK;
}


NS_IMETHODIMP
nsDocShell::LoadErrorPage(nsIURI *aURI, const PRUnichar *aURL, const PRUnichar *aErrorType, const PRUnichar *aDescription)
{
    nsAutoString url;
    if (aURI)
    {
        nsCAutoString uri;
        nsresult rv = aURI->GetSpec(uri);
        NS_ENSURE_SUCCESS(rv, rv);
        url.AssignWithConversion(uri.get());
    }
    else if (aURL)
    {
        url.Assign(aURL);
    }
    else
    {
        return NS_ERROR_INVALID_POINTER;
    }

    // Create a URL to pass all the error information through to the page.

    char *escapedUrl = nsEscape(NS_ConvertUCS2toUTF8(url.get()).get(), url_Path);
    char *escapedError = nsEscape(NS_ConvertUCS2toUTF8(aErrorType).get(), url_Path);
    char *escapedDescription = nsEscape(NS_ConvertUCS2toUTF8(aDescription).get(), url_Path);

    nsAutoString errorType(aErrorType);
    nsAutoString errorPageUrl;

    errorPageUrl.AssignLiteral("chrome://global/content/netError.xhtml?e=");
    errorPageUrl.AppendWithConversion(escapedError);
    errorPageUrl.AppendLiteral("&u=");
    errorPageUrl.AppendWithConversion(escapedUrl);
    errorPageUrl.AppendLiteral("&d=");
    errorPageUrl.AppendWithConversion(escapedDescription);

    PR_FREEIF(escapedDescription);
    PR_FREEIF(escapedError);
    PR_FREEIF(escapedUrl);
    
    return LoadURI(errorPageUrl.get(), // URI string
                   LOAD_FLAGS_BYPASS_HISTORY, 
                   nsnull,
                   nsnull,
                   nsnull);
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

    // XXXTAB Convert reload type to our type
    LoadType type = LOAD_RELOAD_NORMAL;
    if (aReloadFlags & LOAD_FLAGS_BYPASS_CACHE &&
        aReloadFlags & LOAD_FLAGS_BYPASS_PROXY)
        type = LOAD_RELOAD_BYPASS_PROXY_AND_CACHE;
    else if (aReloadFlags & LOAD_FLAGS_CHARSET_CHANGE)
        type = LOAD_RELOAD_CHARSET_CHANGE;
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
        rv = LoadHistoryEntry(mOSHE, type);
    }
    else if (mLSHE) { // In case a reload happened before the current load is done
        rv = LoadHistoryEntry(mLSHE, type);
    }
    else {
        nsAutoString contentTypeHint;
        nsCOMPtr<nsIDOMWindow> window(do_GetInterface((nsIDocShell*)this));
        if (window) {
            nsCOMPtr<nsIDOMDocument> document;
            window->GetDocument(getter_AddRefs(document));
            nsCOMPtr<nsIDOMNSDocument> doc(do_QueryInterface(document));
            if (doc) {
                doc->GetContentType(contentTypeHint);
            }
        }

        rv = InternalLoad(mCurrentURI,
                          mReferrerURI,
                          nsnull,         // No owner
                          PR_TRUE,        // Inherit owner from document
                          nsnull,         // No window target
                          NS_LossyConvertUCS2toASCII(contentTypeHint).get(),
                          nsnull,         // No post data
                          nsnull,         // No headers data
                          type,           // Load type
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
    NS_ENSURE_SUCCESS(EnsureContentViewer(), NS_ERROR_FAILURE);

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
    if (mSessionHistory) {
        *aSessionHistory = mSessionHistory;
        NS_IF_ADDREF(*aSessionHistory);
        return NS_OK;
    }

    return NS_ERROR_FAILURE;

}

//*****************************************************************************
// nsDocShell::nsIWebPageDescriptor
//*****************************************************************************   
NS_IMETHODIMP
nsDocShell::LoadPage(nsISupports *aPageDescriptor, PRUint32 aDisplayType)
{
    nsresult rv;
    nsCOMPtr<nsISHEntry> shEntry(do_QueryInterface(aPageDescriptor));

    // Currently, the opaque 'page descriptor' is an nsISHEntry...
    if (!shEntry) {
        return NS_ERROR_INVALID_POINTER;
    }

    //
    // load the page as view-source
    //
    if (nsIWebPageDescriptor::DISPLAY_AS_SOURCE == aDisplayType) {
        nsCOMPtr<nsIHistoryEntry> srcHE(do_QueryInterface(shEntry));
        nsCOMPtr<nsIURI> oldUri, newUri;
        nsCString spec, newSpec;

        // Create a new view-source URI and replace the original.
        rv = srcHE->GetURI(getter_AddRefs(oldUri));
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

        // NULL out inappropriate cloned attributes...
        shEntry->SetParent(nsnull);
        shEntry->SetIsSubFrame(PR_FALSE);
    }

    rv = LoadHistoryEntry(shEntry, LOAD_HISTORY);
    return rv;
}

NS_IMETHODIMP
nsDocShell::GetCurrentDescriptor(nsISupports **aPageDescriptor)
{
    nsresult rv;
    nsCOMPtr<nsISHEntry> src;

    if (!aPageDescriptor) {
        return NS_ERROR_NULL_POINTER;
    }
    *aPageDescriptor = nsnull;

    src = mOSHE ? mOSHE : mLSHE;
    if (src) {
        nsCOMPtr<nsISupports> sup;;
        nsCOMPtr<nsISHEntry> dest;

        rv = src->Clone(getter_AddRefs(dest));
        if (NS_FAILED(rv)) {
            return rv;
        }

        sup = do_QueryInterface(dest);
        *aPageDescriptor = sup;

        NS_ADDREF(*aPageDescriptor);
    }

    return (*aPageDescriptor) ? NS_OK : NS_ERROR_FAILURE;
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
    nsresult rv = NS_ERROR_FAILURE;
    mPrefs = do_GetService(NS_PREF_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool tmpbool;

    // i don't want to read this pref in every time we load a url
    // so read it in once here and be done with it...
    rv = mPrefs->GetBoolPref("network.protocols.useSystemDefaults",
                             &tmpbool);
    if (NS_SUCCEEDED(rv))
      mUseExternalProtocolHandler = tmpbool;

    rv = mPrefs->GetBoolPref("browser.block.target_new_window", &tmpbool);
    if (NS_SUCCEEDED(rv))
      mDisallowPopupWindows = tmpbool;

    rv = mPrefs->GetBoolPref("browser.frames.enabled", &tmpbool);
    if (NS_SUCCEEDED(rv))
      mAllowSubframes = tmpbool;

    // Check pref to see if we should prevent frameset spoofing
    rv = mPrefs->GetBoolPref("browser.frame.validate_origin", &tmpbool);
    if (NS_SUCCEEDED(rv))
      mValidateOrigin = tmpbool;

    // Should we use XUL error pages instead of alerts if possible?
    rv = mPrefs->GetBoolPref("browser.xul.error_pages.enabled", &tmpbool);
    if (NS_SUCCEEDED(rv))
      mUseErrorPages = tmpbool;

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

    delete mEditorData;
    mEditorData = 0;

    mTransferableHookData = nsnull;

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

    if (mContentListener) {
        mContentListener->DocShell(nsnull);
        mContentListener->SetParentContentListener(nsnull);
        NS_RELEASE(mContentListener);
    }

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

        nsIContent *shellContent =
            pPresShell->GetDocument()->FindContentForSubDocument(presShell->GetDocument());
        NS_ASSERTION(shellContent, "subshell not in the map");

        nsIFrame* frame;
        pPresShell->GetPrimaryFrameFor(shellContent, &frame);
        if (frame && !frame->AreAncestorViewsVisible()) {
            *aVisibility = PR_FALSE;
            return NS_OK;
        }

        treeItem = parentItem;
        treeItem->GetParent(getter_AddRefs(parentItem));
    }

    *aVisibility = PR_TRUE;
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
        mGlobalHistory->SetPageTitle(mCurrentURI, nsDependentString(aTitle));
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
    nsIScrollableView* scrollView;
    NS_ENSURE_SUCCESS(GetRootScrollableView(&scrollView),
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
    if (mIsBeingDestroyed) {
        return nsnull;
    }

    NS_ENSURE_SUCCESS(EnsureScriptEnvironment(), nsnull);

    return mScriptGlobal;
}

//*****************************************************************************
// nsDocShell::nsIRefreshURI
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::RefreshURI(nsIURI * aURI, PRInt32 aDelay, PRBool aRepeat, PRBool aMetaRefresh)
{
    NS_ENSURE_ARG(aURI);

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
            rv = securityManager->CheckLoadURI(aBaseURI, uri,
                                               nsIScriptSecurityManager::
                                               DISALLOW_FROM_MAIL);
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
    nsresult
        rv;
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel, &rv));
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIURI> referrer;
        rv = httpChannel->GetReferrer(getter_AddRefs(referrer));

        if (NS_SUCCEEDED(rv)) {
            SetReferrerURI(referrer);

            nsCAutoString refreshHeader;
            rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("refresh"),
                                                refreshHeader);

            if (!refreshHeader.IsEmpty())
                rv = SetupRefreshURIFromHeader(mCurrentURI, refreshHeader);
        }
    }
    return rv;
}

NS_IMETHODIMP
nsDocShell::CancelRefreshURITimers()
{
    if (!mRefreshURIList)
        return NS_OK;

    PRUint32 n=0;
    mRefreshURIList->Count(&n);

    while (n) {
        nsCOMPtr<nsISupports> element;
        mRefreshURIList->GetElementAt(--n, getter_AddRefs(element));
        nsCOMPtr<nsITimer> timer(do_QueryInterface(element));

        mRefreshURIList->RemoveElementAt(n);    // bye bye owning timer ref

        if (timer)
            timer->Cancel();        
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::RefreshURIFromQueue()
{
    if (!mRefreshURIList)
        return NS_OK;
    PRUint32 n = 0;
    mRefreshURIList->Count(&n);

    while (n) {
        nsCOMPtr<nsISupports> element;
        mRefreshURIList->GetElementAt(--n, getter_AddRefs(element));
        nsCOMPtr<nsRefreshTimer> refreshInfo(do_QueryInterface(element));

        if (refreshInfo) {   
            // This is the nsRefreshTimer object, waiting to be
            // setup in a timer object and fired.                         
            // Create the timer and  trigger it.
            PRUint32 delay = refreshInfo->GetDelay();
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
        nsCOMPtr<nsIWebProgress> webProgress(do_QueryInterface(mLoadCookie));

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
                SetCurrentURI(uri);
                // Save history state of the previous page
                rv = PersistLayoutHistoryState();
                if (mOSHE)
                    mOSHE = mLSHE;
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
                if (uri)
                    AddToGlobalHistory(uri, PR_TRUE);
            }                   // channel
        }                       // aProgress
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::OnLocationChange(nsIWebProgress * aProgress,
                             nsIRequest * aRequest, nsIURI * aURI)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
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
          PL_FavorPerformanceHint(PR_FALSE, NS_EVENT_STARVATION_DELAY_HINT);
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
        if (mLSHE && discardLayoutState && (mLoadType & LOAD_CMD_NORMAL) && (mLoadType != LOAD_BYPASS_HISTORY))
            mLSHE->SetSaveLayoutStateFlag(PR_FALSE);            
    }

    // Clear mLSHE after calling the onLoadHandlers. This way, if the
    // onLoadHandler tries to load something different in
    // itself or one of its children, we can deal with it appropriately.
    if (mLSHE) {
        mLSHE->SetLoadType(nsIDocShellLoadInfo::loadHistory);

        // Clear the mLSHE reference to indicate document loading is done one
        // way or another.
        mLSHE = nsnull;
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
    dev2twip = mDeviceContext->DevUnitsToTwips();
    mDeviceContext->SetDevUnitsToAppUnits(dev2twip);
    float twip2dev;
    twip2dev = mDeviceContext->TwipsToDevUnits();
    mDeviceContext->SetAppUnitsToDevUnits(twip2dev);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::CreateAboutBlankContentViewer()
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

    nsCOMPtr<nsILoadGroup> loadGroup(do_GetInterface(mLoadCookie));

    // generate (about:blank) document to load
    docFactory->CreateBlankDocument(loadGroup, getter_AddRefs(blankDoc));
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

        SetCurrentURI(blankDoc->GetDocumentURI());
        rv = NS_OK;
      }
    }
  }
  mCreatingDocument = PR_FALSE;
  return rv;
}

NS_IMETHODIMP
nsDocShell::CreateContentViewer(const char *aContentType,
                                nsIRequest * request,
                                nsIStreamListener ** aContentHandler)
{
    *aContentHandler = nsnull;

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

    // Give hint to native plevent dispatch mechanism. If a document
    // is loading the native plevent dispatch mechanism should favor
    // performance over normal native event dispatch priorities.
    if (++gNumberOfDocumentsLoading == 1) {
      // Hint to favor performance for the plevent notification mechanism.
      // We want the pages to load as fast as possible even if its means 
      // native messages might be starved.
      PL_FavorPerformanceHint(PR_TRUE, NS_EVENT_STARVATION_DELAY_HINT);
    }
  
    return NS_OK;
}

nsresult
nsDocShell::NewContentViewerObj(const char *aContentType,
                                nsIRequest * request, nsILoadGroup * aLoadGroup,
                                nsIStreamListener ** aContentHandler,
                                nsIContentViewer ** aViewer)
{
    nsCOMPtr<nsIPluginHost> pluginHost (do_GetService(kPluginManagerCID));
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
        // try again after loading plugins
        nsCOMPtr<nsIPluginManager> pluginManager = do_QueryInterface(pluginHost);
        if (!pluginManager)
            return NS_ERROR_FAILURE;

        // no need to do anything if plugins have not been changed
        // PR_FALSE will ensure that currently running plugins will not be shut down
        // but the plugin list will still be updated with newly installed plugins
        if (NS_ERROR_PLUGINS_PLUGINSNOTCHANGED == pluginManager->ReloadPlugins(PR_FALSE))
            return NS_ERROR_FAILURE;

        rv = catMan->GetCategoryEntry("Gecko-Content-Viewers", aContentType,
                                 getter_Copies(contractId));
        if (NS_FAILED(rv))
          return rv;

        docLoaderFactory = do_GetService(contractId.get());

        if (!docLoaderFactory)
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

        mContentViewer->Close();
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
    nsresult rv = NS_OK, sameOrigin = NS_OK;

    if (!mValidateOrigin || !IsFrame()) {
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
    nsIScriptGlobalObject *sgo;
    if (currentCX &&
        (sgo = currentCX->GetGlobalObject()) &&
        (callerTreeItem = do_QueryInterface(sgo->GetDocShell()))) {
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
                         PRBool aInheritOwner,
                         const PRUnichar *aWindowTarget,
                         const char* aTypeHint,
                         nsIInputStream * aPostData,
                         nsIInputStream * aHeadersData,
                         PRUint32 aLoadType,
                         nsISHEntry * aSHEntry,
                         PRBool firstParty,
                         nsIDocShell** aDocShell,
                         nsIRequest** aRequest)
{
    nsresult rv = NS_OK;
    
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
            // don't do it for javascript urls!
            // _main is an IE target which should be case-insensitive but isn't
            // see bug 217886 for details            
            if (!bIsJavascript &&
                (name.LowerCaseEqualsLiteral("_content") || name.EqualsLiteral("_main") ||
                 name.LowerCaseEqualsLiteral("_blank"))) 
            {
                nsCOMPtr<nsIExternalProtocolService> extProtService;
                nsCAutoString urlScheme;

                extProtService = do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID);
                if (extProtService) {
                    PRBool haveHandler = PR_FALSE;
                    aURI->GetScheme(urlScheme);

                    extProtService->ExternalProtocolHandlerExists(urlScheme.get(),
                                                                  &haveHandler);
                    if (haveHandler) {
                        return extProtService->LoadUrl(aURI);
                    }
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
            if (mCurrentURI)
                mCurrentURI->SchemeIs("chrome", &bIsChromeOrResource);
            if (!bIsChromeOrResource) {
                aURI->SchemeIs("chrome", &bIsChromeOrResource);
                if (!bIsChromeOrResource) {
                    aURI->SchemeIs("resource", &bIsChromeOrResource);
                }
            }
            if (!bIsChromeOrResource) {
                if (name.LowerCaseEqualsLiteral("_blank") ||
                    name.LowerCaseEqualsLiteral("_new")) {
                    name.AssignLiteral("_top");
                }
                // _main is an IE target which should be case-insensitive but isn't
                // see bug 217886 for details
                else if (!name.LowerCaseEqualsLiteral("_parent") &&
                         !name.LowerCaseEqualsLiteral("_self") &&
                         !name.LowerCaseEqualsLiteral("_content") &&
                         !name.EqualsLiteral("_main")) {
                    nsCOMPtr<nsIDocShellTreeItem> targetTreeItem;
                    FindItemWithName(name.get(),
                                     nsnull,
                                     getter_AddRefs(targetTreeItem));
                    if (targetTreeItem)
                        targetDocShell = do_QueryInterface(targetTreeItem);
                    else
                        name.AssignLiteral("_top");
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
                                              aTypeHint,
                                              aPostData,
                                              aHeadersData,
                                              aLoadType,
                                              aSHEntry,
                                              firstParty,
                                              aDocShell,
                                              aRequest);
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

    //
    // Load is being targetted at this docshell so return an error if the
    // docshell is in the process of being destroyed.
    //
    if (mIsBeingDestroyed) {
        return NS_ERROR_FAILURE;
    }

    rv = CheckLoadingPermissions();
    if (NS_FAILED(rv)) {
        return rv;
    }

    mURIResultedInDocument = PR_FALSE;  // reset the clock...
   
    //
    // First:
    // Check to see if the new URI is an anchor in the existing document.
    // Skip this check if we're doing some sort of abnormal load, if
    // the new load is a non-history load and has postdata, or if
    // we're doing a history load and there are postdata differences
    // between what we plan to load and what we have loaded currently.
    //
    PRBool samePostData = PR_TRUE;
    if (!aSHEntry) {
        samePostData = (aPostData == nsnull);
    } else if (mOSHE) {
        nsCOMPtr<nsIInputStream> currentPostData;
        mOSHE->GetPostData(getter_AddRefs(currentPostData));
        samePostData = (currentPostData == aPostData);
    }
    
    if ((aLoadType == LOAD_NORMAL ||
         aLoadType == LOAD_NORMAL_REPLACE ||
         aLoadType == LOAD_HISTORY ||
         aLoadType == LOAD_LINK) && samePostData) {
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
            mLSHE = aSHEntry;

            /* This is a anchor traversal with in the same page.
             * call OnNewURI() so that, this traversal will be 
             * recorded in session and global history.
             */
            OnNewURI(aURI, nsnull, mLoadType);
            nsCOMPtr<nsIInputStream> postData;
            
            if (mOSHE) {
                /* save current position of scroller(s) (bug 59774) */
                mOSHE->SetScrollPosition(cx, cy);
                // Get the postdata from the current page, if it was
                // loaded through normal means.
                if (aLoadType == LOAD_NORMAL || aLoadType == LOAD_LINK)
                    mOSHE->GetPostData(getter_AddRefs(postData));
            }
            
            /* Assign mOSHE to mLSHE. This will either be a new entry created
             * by OnNewURI() for normal loads or aSHEntry for history loads.
             */
            if (mLSHE) {
                mOSHE = mLSHE;
                // Save the postData obtained from the previous page
                // in to the session history entry created for the 
                // anchor page, so that any history load of the anchor
                // page will restore the appropriate postData.
                if (postData)
                    mOSHE->SetPostData(postData);
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

        if (zombieViewer) {
            rv = Stop(nsIWebNavigation::STOP_ALL);
        } else {
            rv = Stop(nsIWebNavigation::STOP_NETWORK);
        }

        if (NS_FAILED(rv)) 
            return rv;
    }

    mLoadType = aLoadType;
    // mLSHE should be assigned to aSHEntry, only after Stop() has
    // been called. 
    mLSHE = aSHEntry;

    rv = DoURILoad(aURI, aReferrer, owner, aTypeHint, aPostData, aHeadersData,
                   firstParty, aDocShell, aRequest);

    if (NS_FAILED(rv)) {
        DisplayLoadError(rv, aURI, nsnull);
    }
    
    return rv;
}

void
nsDocShell::GetCurrentDocumentOwner(nsISupports ** aOwner)
{
    *aOwner = nsnull;
    nsCOMPtr<nsIDocument> document;
    //-- Get the current document
    if (mContentViewer) {
        nsCOMPtr<nsIDocumentViewer>
            docViewer(do_QueryInterface(mContentViewer));
        if (!docViewer)
            return;
        docViewer->GetDocument(getter_AddRefs(document));
    }
    else //-- If there's no document loaded yet, look at the parent (frameset)
    {
        nsCOMPtr<nsIDocShellTreeItem> parentItem;
        GetSameTypeParent(getter_AddRefs(parentItem));
        if (!parentItem)
            return;
        nsCOMPtr<nsIDOMWindowInternal>
            parentWindow(do_GetInterface(parentItem));
        if (!parentWindow)
            return;
        nsCOMPtr<nsIDOMDocument> parentDomDoc;
        parentWindow->GetDocument(getter_AddRefs(parentDomDoc));
        if (!parentDomDoc)
            return;
        document = do_QueryInterface(parentDomDoc);
    }

    //-- Get the document's principal
    if (document) {
        *aOwner = document->GetPrincipal();
    }

    NS_IF_ADDREF(*aOwner);
}

nsresult
nsDocShell::DoURILoad(nsIURI * aURI,
                      nsIURI * aReferrerURI,
                      nsISupports * aOwner,
                      const char * aTypeHint,
                      nsIInputStream * aPostData,
                      nsIInputStream * aHeadersData,
                      PRBool firstParty,
                      nsIDocShell ** aDocShell,
                      nsIRequest ** aRequest)
{
    nsresult rv;
    nsCOMPtr<nsIURILoader> uriLoader;

    uriLoader = do_GetService(NS_URI_LOADER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    // we need to get the load group from our load cookie so we can pass it into open uri...
    nsCOMPtr<nsILoadGroup> loadGroup;
    rv = uriLoader->GetLoadGroupForContext(this,
                                           getter_AddRefs(loadGroup));
    if (NS_FAILED(rv)) return rv;

    nsLoadFlags loadFlags = nsIRequest::LOAD_NORMAL;
    if (firstParty) {
        // tag first party URL loads
        loadFlags |= nsIChannel::LOAD_INITIAL_DOCUMENT_URI;
    }

    // open a channel for the url
    nsCOMPtr<nsIChannel> channel;

    rv = NS_NewChannel(getter_AddRefs(channel),
                       aURI,
                       nsnull,
                       loadGroup,
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
      if (firstParty) {
        httpChannelInternal->SetDocumentURI(aURI);
      } else {
        httpChannelInternal->SetDocumentURI(aReferrerURI);
      }
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
        if (aReferrerURI)       // Referrer is currenly only set for link clicks here.
            httpChannel->SetReferrer(aReferrerURI);
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
    
    //
    // If the channel load failed, we failed and nsIWebProgress just ain't
    // gonna happen.
    //
    if (NS_SUCCEEDED(rv)) {
        if (aDocShell) {
          *aDocShell = this;
          NS_ADDREF(*aDocShell);
        }
        if (aRequest) {
          CallQueryInterface(channel, aRequest);
        }
    }

    return rv;
}

static NS_METHOD
AHTC_WriteFunc(nsIInputStream * in,
               void *closure,
               const char *fromRawSegment,
               PRUint32 toOffset, PRUint32 count, PRUint32 * writeCount)
{
    if (nsnull == writeCount || nsnull == closure ||
        nsnull == fromRawSegment || strlen(fromRawSegment) < 1) {
        return NS_BASE_STREAM_CLOSED;
    }

    *writeCount = 0;
    char *headersBuf = *((char **) closure);
    // pointer to where we should start copying bytes from rawSegment
    char *pHeadersBuf = nsnull;
    PRUint32 headersBufLen;
    PRUint32 rawSegmentLen = count;

    // if the buffer has no data yet
    if (!headersBuf) {
        headersBufLen = rawSegmentLen;
        pHeadersBuf = headersBuf = (char *) nsMemory::Alloc(headersBufLen + 1);
        if (!headersBuf) {
            return NS_BASE_STREAM_WOULD_BLOCK;
        }
        memset(headersBuf, nsnull, headersBufLen + 1);
    }
    else {
        // data has been read, reallocate
        // store a pointer to the old full buffer
        pHeadersBuf = headersBuf;

        // create a new buffer
        headersBufLen = strlen(headersBuf);
        headersBuf =
            (char *) nsMemory::Alloc(rawSegmentLen + headersBufLen + 1);
        if (!headersBuf) {
            headersBuf = pHeadersBuf;
            pHeadersBuf = nsnull;
            return NS_BASE_STREAM_WOULD_BLOCK;
        }
        memset(headersBuf, nsnull, rawSegmentLen + headersBufLen + 1);
        // copy the old buffer to the beginning of the new buffer
        memcpy(headersBuf, pHeadersBuf, headersBufLen);
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
    memcpy(pHeadersBuf, fromRawSegment, rawSegmentLen);
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
    nsXPIDLCString headersBuf;

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
    if (NS_FAILED(rv) || available < 1)
        return rv;

    do {
        aHeadersData->ReadSegments(AHTC_WriteFunc,
                                   getter_Copies(headersBuf),
                                   available,
                                   &bytesRead);
        rv = aHeadersData->Available(&available);
        if (NS_FAILED(rv))
            return rv;

    } while (0 < available);

    //
    // Turn nsXPIDLCString into an nsString.
    // (need to find the new string APIs so we don't do this
    //
    headersString = headersBuf.get();

    //
    // Iterate over the nsString: for each "\r\n" delimeted chunk,
    // add the value as a header to the nsIHttpChannel
    //

    const char *kWhitespace = "\b\t\r\n ";
    while (PR_TRUE) {
        crlf = headersString.Find("\r\n", PR_TRUE);
        if (-1 == crlf) {
            return NS_OK;
        }
        headersString.Mid(oneHeader, 0, crlf);
        headersString.Cut(0, crlf + 2);
        colon = oneHeader.Find(":");
        if (-1 == colon) {
            return NS_ERROR_NULL_POINTER;
        }
        oneHeader.Left(headerName, colon);
        colon++;
        oneHeader.Mid(headerValue, colon, oneHeader.Length() - colon);
        headerName.Trim(kWhitespace);
        headerValue.Trim(kWhitespace);

        //
        // FINALLY: we can set the header!
        // 

        rv = aChannel->SetRequestHeader(headerName, headerValue, PR_TRUE);
        if (NS_FAILED(rv)) {
            return NS_ERROR_NULL_POINTER;
        }
    }
    return NS_ERROR_FAILURE;
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
    else if (hashNew > 0) {
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
    else if (hashCurrent > 0) {
        currentLeftEnd = currentLeftStart;
        currentLeftEnd.advance(hashCurrent);
    }
    else {
        currentSpec.EndReading(currentLeftEnd);
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
        NS_ConvertUTF8toUCS2 uStr(str);
        if (!uStr.IsEmpty()) {
            rv = shell->GoToAnchor(NS_ConvertUTF8toUCS2(str), scroll);
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


NS_IMETHODIMP
nsDocShell::OnNewURI(nsIURI * aURI, nsIChannel * aChannel,
                     PRUint32 aLoadType)
{
    NS_ASSERTION(aURI, "uri is null");

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
    nsCOMPtr<nsISHistory> rootSH=mSessionHistory;
    if (!rootSH) {
        // Get the handle to SH from the root docshell          
        GetRootSessionHistory(getter_AddRefs(rootSH));
        if (!rootSH)
            shAvailable = PR_FALSE;
    }  // rootSH


    // Determine if this type of load should update history.
    if (aLoadType == LOAD_BYPASS_HISTORY ||
        aLoadType & LOAD_CMD_HISTORY ||
        aLoadType & LOAD_CMD_RELOAD)
        updateHistory = PR_FALSE;

    // Check if the url to be loaded is the same as the one already loaded. 
    if (mCurrentURI)
        aURI->Equals(mCurrentURI, &equalUri);


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
    if (equalUri &&
        (mLoadType == LOAD_NORMAL ||
         mLoadType == LOAD_LINK) &&
        !inputStream)
    {
        mLoadType = LOAD_NORMAL_REPLACE;
    }

    // If this is a refresh to the currently loaded url, we don't
    // have to update session or global history.
    if (mLoadType == LOAD_REFRESH && !inputStream && equalUri) {
        mLSHE = mOSHE;
    }


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

        // Update Global history
        AddToGlobalHistory(aURI, PR_FALSE);
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
    nsresult rv = NS_OK;
    nsCOMPtr<nsISHEntry> entry;
    PRBool shouldPersist;

    shouldPersist = ShouldAddToSessionHistory(aURI);

    // Get a handle to the root docshell 
    nsCOMPtr<nsIDocShellTreeItem> root;
    GetSameTypeRootTreeItem(getter_AddRefs(root));     
    /*
     * If this is a LOAD_NORMAL_REPLACE in a subframe, we use
     * the existing SH entry in the page and replace the url and
     * other vitalities.
     */
    if (LOAD_NORMAL_REPLACE == mLoadType && (root.get() != NS_STATIC_CAST(nsIDocShellTreeItem *, this))) {
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
    }

    //Title is set in nsDocShell::SetTitle()
    entry->Create(aURI,         // uri
                  nsnull,       // Title
                  nsnull,       // DOMDocument
                  inputStream,  // Post data stream
                  nsnull,       // LayoutHistory state
                  cacheKey,     // CacheKey
                  mContentTypeHint); // Content-type
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


    if (root.get() == NS_STATIC_CAST(nsIDocShellTreeItem *, this) && mSessionHistory) {
        // This is the root docshell
        if (mLoadType == LOAD_NORMAL_REPLACE) {            
            // Replace current entry in session history.
            PRInt32  index = 0;   
            nsCOMPtr<nsIHistoryEntry> hEntry;
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
            rv = shPrivate->AddEntry(entry, shouldPersist);
        }
    }
    else {  
        // This is a subframe.
        if ((mLoadType != LOAD_NORMAL_REPLACE) ||
            (mLoadType == LOAD_NORMAL_REPLACE && !mOSHE))
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
    nsresult rv;
    nsCOMPtr<nsIURI> uri;
    nsCOMPtr<nsIInputStream> postData;
    nsCOMPtr<nsIURI> referrerURI;
    nsCAutoString contentType;

    NS_ENSURE_TRUE(aEntry, NS_ERROR_FAILURE);
    nsCOMPtr<nsIHistoryEntry> hEntry(do_QueryInterface(aEntry));
    NS_ENSURE_TRUE(hEntry, NS_ERROR_FAILURE);

    NS_ENSURE_SUCCESS(hEntry->GetURI(getter_AddRefs(uri)), NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(aEntry->GetReferrerURI(getter_AddRefs(referrerURI)),
                      NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(aEntry->GetPostData(getter_AddRefs(postData)),
                      NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(aEntry->GetContentType(contentType), NS_ERROR_FAILURE);

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
        nsresult rv = stringBundle->GetStringFromName(NS_LITERAL_STRING("repostConfirm").get(), 
                                                      getter_Copies(messageStr));
          
        if (NS_SUCCEEDED(rv) && messageStr) {
          prompter->Confirm(nsnull, messageStr, &repost);
          /* If the user pressed cancel in the dialog, return.  We're
           * done here.
           */
          if (!repost)
            return NS_BINDING_ABORTED;  
        }
      }
    }

    rv = InternalLoad(uri,
                      referrerURI,
                      nsnull,            // No owner
                      PR_FALSE,          // Do not inherit owner from document (security-critical!)
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
        // Clone the SHEntry...
        result = src->Clone(&dest);
        if (NS_FAILED(result))
            return result;

        // This entry is for a frame...
        dest->SetIsSubFrame(PR_TRUE);

        // Transfer the owning reference to 'resultEntry'.  From this point on
        // 'dest' is *not* an owning reference...
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
            if (NS_FAILED(result))
                return result;
            result =
                CloneAndReplace(srcChild, aCloneID, replaceEntry,
                                getter_AddRefs(destChild));
            if (NS_FAILED(result))
                return result;
            result = destContainer->AddChild(destChild, i);
            if (NS_FAILED(result))
                return result;
        }                       // for 
    }

    return result;

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
nsDocShell::AddToGlobalHistory(nsIURI * aURI, PRBool aRedirect)
{
    if (mItemType != typeContent)
        return NS_OK;

    if (!mGlobalHistory)
        return NS_OK;

    return mGlobalHistory->AddURI(aURI, aRedirect, !IsFrame());
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
            webProgress->AddProgressListener(this,
                                     nsIWebProgress::NOTIFY_STATE_DOCUMENT |
                                     nsIWebProgress::NOTIFY_STATE_NETWORK);
        }

        nsCOMPtr<nsILoadGroup> loadGroup(do_GetInterface(mLoadCookie));
        NS_ENSURE_TRUE(loadGroup, NS_ERROR_FAILURE);
        if (loadGroup) {
            nsIInterfaceRequestor *ifPtr = NS_STATIC_CAST(nsIInterfaceRequestor *, this);
            nsCOMPtr<InterfaceRequestorProxy> ptr(new InterfaceRequestorProxy(ifPtr)); 
            if (ptr) {
                loadGroup->SetNotificationCallbacks(ptr);
            }
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
    NS_ENSURE_TRUE(shell, NS_ERROR_NULL_POINTER);

    NS_ENSURE_SUCCESS(shell->GetViewManager()->GetRootScrollableView(aOutScrollView),
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

    factory->NewScriptGlobalObject(mItemType == typeChrome,
                                   getter_AddRefs(mScriptGlobal));
    NS_ENSURE_TRUE(mScriptGlobal, NS_ERROR_FAILURE);

    mScriptGlobal->SetDocShell(NS_STATIC_CAST(nsIDocShell *, this));
    mScriptGlobal->
        SetGlobalObjectOwner(NS_STATIC_CAST
                             (nsIScriptGlobalObjectOwner *, this));

    factory->NewScriptContext(mScriptGlobal, getter_AddRefs(mScriptContext));
    NS_ENSURE_TRUE(mScriptContext, NS_ERROR_FAILURE);

    return NS_OK;
}


NS_IMETHODIMP
nsDocShell::EnsureEditorData()
{
  if (!mEditorData)
  {
    mEditorData = new nsDocShellEditorData(this);
    if (!mEditorData) return NS_ERROR_OUT_OF_MEMORY;
  }
  
  return mEditorData ? NS_OK : NS_ERROR_FAILURE;
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
    // up to point to the focussed, or content window, so we have to
    // set that up each time.

    nsIScriptGlobalObject* scriptGO = GetScriptGlobalObject();
    NS_ENSURE_TRUE(scriptGO, NS_ERROR_UNEXPECTED);

    // default to our window
    nsCOMPtr<nsIDOMWindow> rootWindow = do_QueryInterface(scriptGO);
    nsCOMPtr<nsIDOMWindow> windowToSearch = rootWindow;

    // if we can, search the focussed window
    nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(scriptGO);
    nsIFocusController *focusController = nsnull;
    if (ourWindow)
        focusController = ourWindow->GetRootFocusController();
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

  nsIDocument *doc = presShell->GetDocument();
  if (!doc) return NS_ERROR_FAILURE;

  nsIContent *rootContent = doc->GetRootContent();
  if (!rootContent) return NS_ERROR_FAILURE;

  nsIFrame* frame;
  presShell->GetPrimaryFrameFor(rootContent, &frame);
  if (frame) {
    frame = frame->GetParent();
    if (frame) {
      nsICanvasFrame* canvasFrame;
      if (NS_SUCCEEDED(frame->QueryInterface(NS_GET_IID(nsICanvasFrame), (void**)&canvasFrame))) {
        canvasFrame->SetHasFocus(aCanvasHasFocus);

        nsIView* canvasView = frame->GetViewExternal();

        canvasView->GetViewManager()->UpdateView(canvasView, NS_VMREFRESH_NO_SYNC);

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
//*****************************************************************************   
NS_IMETHODIMP
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
          return NS_OK;
        // Get the delay count
        PRUint32 delay = 0;
        aTimer->GetDelay(&delay);
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
                               nsIWebNavigation::LOAD_FLAGS_NONE, PR_TRUE);
            return NS_OK;

        }
        else
            loadInfo->SetLoadType(nsIDocShellLoadInfo::loadRefresh);
        mDocShell->LoadURI(mURI, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE, PR_TRUE);
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

nsresult
nsDocShell::GetAuthPrompt(PRUint32 aPromptReason, nsIAuthPrompt **aResult)
{
    // a priority prompt request will override a false mAllowAuth setting
    PRBool priorityPrompt = (aPromptReason == nsIAuthPromptProvider::PROMPT_PROXY);

    if (!mAllowAuth && !priorityPrompt)
        return NS_ERROR_NOT_AVAILABLE;

    // we're either allowing auth, or it's a proxy request
    nsCOMPtr<nsIAuthPrompt> authPrompter(do_GetInterface(mTreeOwner));
    if (!authPrompter)
        return NS_ERROR_NOT_AVAILABLE;

    *aResult = authPrompter;
    NS_ADDREF(*aResult);
    return NS_OK;
}
