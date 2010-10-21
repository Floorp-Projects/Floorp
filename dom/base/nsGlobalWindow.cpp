/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=78: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 *   Brendan Eich <brendan@mozilla.org>
 *   David Hyatt (hyatt@netscape.com)
 *   Dan Rosen <dr@netscape.com>
 *   Vidur Apparao <vidur@netscape.com>
 *   Johnny Stenback <jst@netscape.com>
 *   Mark Hammond <mhammond@skippinet.com.au>
 *   Ryan Jones <sciguyryan@gmail.com>
 *   Jeff Walden <jwalden+code@mit.edu>
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

#ifdef MOZ_IPC
#include "base/basictypes.h"
#endif

// Local Includes
#include "nsGlobalWindow.h"
#include "nsScreen.h"
#include "nsHistory.h"
#include "nsBarProps.h"
#include "nsDOMStorage.h"
#include "nsDOMOfflineResourceList.h"
#include "nsDOMError.h"

// Helper Classes
#include "nsXPIDLString.h"
#include "nsJSUtils.h"
#include "prmem.h"
#include "jsapi.h"              // for JSAutoRequest
#include "jsdbgapi.h"           // for JS_ClearWatchPointsForObject
#include "nsReadableUtils.h"
#include "nsDOMClassInfo.h"
#include "nsContentUtils.h"

// Other Classes
#include "nsIEventListenerManager.h"
#include "nsEscape.h"
#include "nsStyleCoord.h"
#include "nsMimeTypeArray.h"
#include "nsNetUtil.h"
#include "nsICachingChannel.h"
#include "nsPluginArray.h"
#include "nsIPluginHost.h"
#include "nsGeolocation.h"
#include "nsDesktopNotification.h"
#include "nsContentCID.h"
#include "nsLayoutStatics.h"
#include "nsCycleCollector.h"
#include "nsCCUncollectableMarker.h"
#include "nsDOMThreadService.h"
#include "nsAutoJSValHolder.h"

// Interfaces Needed
#include "nsIFrame.h"
#include "nsCanvasFrame.h"
#include "nsIWidget.h"
#include "nsIBaseWindow.h"
#include "nsAccelerometer.h"
#include "nsIContent.h"
#include "nsIContentViewerEdit.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIEditorDocShell.h"
#include "nsIDocCharset.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#ifndef MOZ_DISABLE_DOMCRYPTO
#include "nsIDOMCrypto.h"
#endif
#include "nsIDOMDocument.h"
#include "nsIDOM3Document.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEvent.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMessageEvent.h"
#include "nsIDOMPopupBlockedEvent.h"
#include "nsIDOMPopStateEvent.h"
#include "nsIDOMOfflineResourceList.h"
#include "nsIDOMGeoGeolocation.h"
#include "nsIDOMDesktopNotification.h"
#include "nsPIDOMStorage.h"
#include "nsDOMString.h"
#include "nsIEmbeddingSiteWindow2.h"
#include "nsThreadUtils.h"
#include "nsIEventStateManager.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIJSContextStack.h"
#include "nsIJSRuntimeService.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIPrefBranch.h"
#include "nsIPresShell.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIProgrammingLanguage.h"
#include "nsIServiceManager.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScrollableFrame.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsISelectionController.h"
#include "nsISelection.h"
#include "nsIPrompt.h"
#include "nsIPromptService.h"
#include "nsIWebNavigation.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserFind.h"  // For window.find()
#include "nsIWebContentHandlerRegistrar.h"
#include "nsIWindowMediator.h"  // For window.find()
#include "nsComputedDOMStyle.h"
#include "nsIEntropyCollector.h"
#include "nsDOMCID.h"
#include "nsDOMError.h"
#include "nsDOMWindowUtils.h"
#include "nsIWindowWatcher.h"
#include "nsPIWindowWatcher.h"
#include "nsIContentViewer.h"
#include "nsDOMClassInfo.h"
#include "nsIJSNativeInitializer.h"
#include "nsIScriptError.h"
#include "nsIScriptEventManager.h" // For GetInterface()
#include "nsIConsoleService.h"
#include "nsIControllers.h"
#include "nsIControllerContext.h"
#include "nsGlobalWindowCommands.h"
#include "nsAutoPtr.h"
#include "nsContentUtils.h"
#include "nsCSSProps.h"
#include "nsFileDataProtocolHandler.h"
#include "nsIDOMFile.h"
#include "nsIURIFixup.h"
#include "mozilla/FunctionTimer.h"
#include "nsCDefaultURIFixup.h"
#include "nsEventDispatcher.h"
#include "nsIObserverService.h"
#include "nsIXULAppInfo.h"
#include "nsNetUtil.h"
#include "nsFocusManager.h"
#include "nsIJSON.h"
#include "nsIXULWindow.h"
#ifdef MOZ_XUL
#include "nsXULPopupManager.h"
#include "nsIDOMXULControlElement.h"
#include "nsIFrame.h"
#endif

#include "plbase64.h"

#ifdef NS_PRINTING
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsService.h"
#include "nsIWebBrowserPrint.h"
#endif

#include "nsWindowRoot.h"
#include "nsNetCID.h"
#include "nsIArray.h"
#include "nsIScriptRuntime.h"

// XXX An unfortunate dependency exists here (two XUL files).
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULCommandDispatcher.h"

#include "nsBindingManager.h"
#include "nsIXBLService.h"

// used for popup blocking, needs to be converted to something
// belonging to the back-end like nsIContentPolicy
#include "nsIPopupWindowManager.h"

#include "nsIDragService.h"
#include "mozilla/dom/Element.h"
#include "nsFrameLoader.h"
#include "nsISupportsPrimitives.h"
#include "nsXPCOMCID.h"

#include "mozilla/FunctionTimer.h"

#ifdef MOZ_LOGGING
// so we can get logging even in release builds
#define FORCE_PR_LOG 1
#endif
#include "prlog.h"

#include "mozilla/dom/indexedDB/IDBFactory.h"

#include "nsRefreshDriver.h"

#ifdef PR_LOGGING
static PRLogModuleInfo* gDOMLeakPRLog;
#endif

using namespace mozilla::dom;
using mozilla::TimeStamp;
using mozilla::TimeDuration;

nsIDOMStorageList *nsGlobalWindow::sGlobalStorageList  = nsnull;

static nsIEntropyCollector *gEntropyCollector          = nsnull;
static PRInt32              gRefCnt                    = 0;
static PRInt32              gOpenPopupSpamCount        = 0;
static PopupControlState    gPopupControlState         = openAbused;
static PRInt32              gRunningTimeoutDepth       = 0;
static PRPackedBool         gMouseDown                 = PR_FALSE;
static PRPackedBool         gDragServiceDisabled       = PR_FALSE;
static FILE                *gDumpFile                  = nsnull;
static PRUint64             gNextWindowID              = 0;
static PRUint32             gSerialCounter             = 0;

#ifdef DEBUG_jst
PRInt32 gTimeoutCnt                                    = 0;
#endif

#if !(defined(NS_DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
static PRBool               gDOMWindowDumpEnabled      = PR_FALSE;
#endif

#if defined(DEBUG_bryner) || defined(DEBUG_chb)
#define DEBUG_PAGE_CACHE
#endif

// The shortest interval/timeout we permit
#define DOM_MIN_TIMEOUT_VALUE 10 // 10ms

// The number of nested timeouts before we start clamping. HTML5 says 1, WebKit
// uses 5.
#define DOM_CLAMP_TIMEOUT_NESTING_LEVEL 5

// The longest interval (as PRIntervalTime) we permit, or that our
// timer code can handle, really. See DELAY_INTERVAL_LIMIT in
// nsTimerImpl.h for details.
#define DOM_MAX_TIMEOUT_VALUE    PR_BIT(8 * sizeof(PRIntervalTime) - 1)

#define FORWARD_TO_OUTER(method, args, err_rval)                              \
  PR_BEGIN_MACRO                                                              \
  if (IsInnerWindow()) {                                                      \
    nsGlobalWindow *outer = GetOuterWindowInternal();                         \
    if (!outer) {                                                             \
      NS_WARNING("No outer window available!");                               \
      return err_rval;                                                        \
    }                                                                         \
    return outer->method args;                                                \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_OUTER_VOID(method, args)                                   \
  PR_BEGIN_MACRO                                                              \
  if (IsInnerWindow()) {                                                      \
    nsGlobalWindow *outer = GetOuterWindowInternal();                         \
    if (!outer) {                                                             \
      NS_WARNING("No outer window available!");                               \
      return;                                                                 \
    }                                                                         \
    outer->method args;                                                       \
    return;                                                                   \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_OUTER_CHROME(method, args, err_rval)                       \
  PR_BEGIN_MACRO                                                              \
  if (IsInnerWindow()) {                                                      \
    nsGlobalWindow *outer = GetOuterWindowInternal();                         \
    if (!outer) {                                                             \
      NS_WARNING("No outer window available!");                               \
      return err_rval;                                                        \
    }                                                                         \
    return ((nsGlobalChromeWindow *)outer)->method args;                      \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_INNER_CHROME(method, args, err_rval)                       \
  PR_BEGIN_MACRO                                                              \
  if (IsOuterWindow()) {                                                      \
    if (!mInnerWindow) {                                                      \
      NS_WARNING("No inner window available!");                               \
      return err_rval;                                                        \
    }                                                                         \
    return ((nsGlobalChromeWindow *)mInnerWindow)->method args;               \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_OUTER_MODAL_CONTENT_WINDOW(method, args, err_rval)         \
  PR_BEGIN_MACRO                                                              \
  if (IsInnerWindow()) {                                                      \
    nsGlobalWindow *outer = GetOuterWindowInternal();                         \
    if (!outer) {                                                             \
      NS_WARNING("No outer window available!");                               \
      return err_rval;                                                        \
    }                                                                         \
    return ((nsGlobalModalWindow *)outer)->method args;                       \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_INNER(method, args, err_rval)                              \
  PR_BEGIN_MACRO                                                              \
  if (IsOuterWindow()) {                                                      \
    if (!mInnerWindow) {                                                      \
      NS_WARNING("No inner window available!");                               \
      return err_rval;                                                        \
    }                                                                         \
    return GetCurrentInnerWindowInternal()->method args;                      \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_INNER_MODAL_CONTENT_WINDOW(method, args, err_rval)         \
  PR_BEGIN_MACRO                                                              \
  if (IsOuterWindow()) {                                                      \
    if (!mInnerWindow) {                                                      \
      NS_WARNING("No inner window available!");                               \
      return err_rval;                                                        \
    }                                                                         \
    return ((nsGlobalModalWindow*)GetCurrentInnerWindowInternal())->method args; \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_INNER_VOID(method, args)                                   \
  PR_BEGIN_MACRO                                                              \
  if (IsOuterWindow()) {                                                      \
    if (!mInnerWindow) {                                                      \
      NS_WARNING("No inner window available!");                               \
      return;                                                                 \
    }                                                                         \
    GetCurrentInnerWindowInternal()->method args;                             \
    return;                                                                   \
  }                                                                           \
  PR_END_MACRO

// Same as FORWARD_TO_INNER, but this will create a fresh inner if an
// inner doesn't already exists.
#define FORWARD_TO_INNER_CREATE(method, args, err_rval)                       \
  PR_BEGIN_MACRO                                                              \
  if (IsOuterWindow()) {                                                      \
    if (!mInnerWindow) {                                                      \
      if (mIsClosed) {                                                        \
        return err_rval;                                                      \
      }                                                                       \
      nsCOMPtr<nsIDOMDocument> doc;                                           \
      nsresult fwdic_nr = GetDocument(getter_AddRefs(doc));                   \
      NS_ENSURE_SUCCESS(fwdic_nr, err_rval);                                  \
      if (!mInnerWindow) {                                                    \
        return err_rval;                                                      \
      }                                                                       \
    }                                                                         \
    return GetCurrentInnerWindowInternal()->method args;                      \
  }                                                                           \
  PR_END_MACRO

// CIDs
static NS_DEFINE_CID(kXULControllersCID, NS_XULCONTROLLERS_CID);

static const char sJSStackContractID[] = "@mozilla.org/js/xpc/ContextStack;1";
#ifndef MOZ_DISABLE_DOMCRYPTO
static const char kCryptoContractID[] = NS_CRYPTO_CONTRACTID;
static const char kPkcs11ContractID[] = NS_PKCS11_CONTRACTID;
#endif
static const char sPopStatePrefStr[] = "browser.history.allowPopState";

static PRBool
IsAboutBlank(nsIURI* aURI)
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

class nsDummyJavaPluginOwner : public nsIPluginInstanceOwner
{
public:
  nsDummyJavaPluginOwner(nsIDocument *aDocument)
    : mDocument(aDocument)
  {
  }

  void Destroy();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIPLUGININSTANCEOWNER

  NS_IMETHOD GetURL(const char *aURL, const char *aTarget,
                    nsIInputStream *aPostStream,
                    void *aHeadersData, PRUint32 aHeadersDataLen);
  NS_IMETHOD ShowStatus(const PRUnichar *aStatusMsg);
  NPError ShowNativeContextMenu(NPMenu* menu, void* event);
  NPBool ConvertPoint(double sourceX, double sourceY, NPCoordinateSpace sourceSpace,
                      double *destX, double *destY, NPCoordinateSpace destSpace);
  void SendIdleEvent();

  NS_DECL_CYCLE_COLLECTION_CLASS(nsDummyJavaPluginOwner)

private:
  nsCOMPtr<nsIPluginInstance> mInstance;
  nsCOMPtr<nsIDocument> mDocument;
};

NS_IMPL_CYCLE_COLLECTION_2(nsDummyJavaPluginOwner, mDocument, mInstance)

// QueryInterface implementation for nsDummyJavaPluginOwner
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDummyJavaPluginOwner)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIPluginInstanceOwner)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDummyJavaPluginOwner)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDummyJavaPluginOwner)


void
nsDummyJavaPluginOwner::Destroy()
{
  // If we have a plugin instance, stop it and destroy it now.
  if (mInstance) {
    mInstance->Stop();
    mInstance->InvalidateOwner();
    mInstance = nsnull;
  }

  mDocument = nsnull;
}

NS_IMETHODIMP
nsDummyJavaPluginOwner::SetInstance(nsIPluginInstance *aInstance)
{
  mInstance = aInstance;

  return NS_OK;
}

NS_IMETHODIMP
nsDummyJavaPluginOwner::GetInstance(nsIPluginInstance *&aInstance)
{
  NS_IF_ADDREF(aInstance = mInstance);

  return NS_OK;
}

NS_IMETHODIMP
nsDummyJavaPluginOwner::GetWindow(NPWindow *&aWindow)
{
  aWindow = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDummyJavaPluginOwner::GetMode(PRInt32 *aMode)
{
  // This is wrong, but there's no better alternative.
  *aMode = NP_EMBED;

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDummyJavaPluginOwner::CreateWidget(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDummyJavaPluginOwner::GetURL(const char *aURL, const char *aTarget,
                               nsIInputStream *aPostStream,
                               void *aHeadersData, PRUint32 aHeadersDataLen)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDummyJavaPluginOwner::ShowStatus(const char *aStatusMsg)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDummyJavaPluginOwner::ShowStatus(const PRUnichar *aStatusMsg)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NPError
nsDummyJavaPluginOwner::ShowNativeContextMenu(NPMenu* menu, void* event)
{
  return NPERR_GENERIC_ERROR;
}

NPBool
nsDummyJavaPluginOwner::ConvertPoint(double sourceX, double sourceY, NPCoordinateSpace sourceSpace,
                                     double *destX, double *destY, NPCoordinateSpace destSpace)
{
  return PR_FALSE;
}

NS_IMETHODIMP
nsDummyJavaPluginOwner::GetDocument(nsIDocument **aDocument)
{
  NS_IF_ADDREF(*aDocument = mDocument);

  return NS_OK;
}

NS_IMETHODIMP
nsDummyJavaPluginOwner::InvalidateRect(NPRect *invalidRect)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDummyJavaPluginOwner::InvalidateRegion(NPRegion invalidRegion)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDummyJavaPluginOwner::ForceRedraw()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDummyJavaPluginOwner::GetNetscapeWindow(void *value)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDummyJavaPluginOwner::SetEventModel(PRInt32 eventModel)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
nsDummyJavaPluginOwner::SendIdleEvent()
{
}

/**
 * An indirect observer object that means we don't have to implement nsIObserver
 * on nsGlobalWindow, where any script could see it.
 */
class nsGlobalWindowObserver : public nsIObserver {
public:
  nsGlobalWindowObserver(nsGlobalWindow* aWindow) : mWindow(aWindow) {}
  NS_DECL_ISUPPORTS
  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aData)
  {
    if (!mWindow)
      return NS_OK;
    return mWindow->Observe(aSubject, aTopic, aData);
  }
  void Forget() { mWindow = nsnull; }
private:
  nsGlobalWindow* mWindow;
};

NS_IMPL_ISUPPORTS1(nsGlobalWindowObserver, nsIObserver)

nsTimeout::nsTimeout()
{
#ifdef DEBUG_jst
  {
    extern int gTimeoutCnt;

    ++gTimeoutCnt;
  }
#endif

  memset(this, 0, sizeof(*this));

  MOZ_COUNT_CTOR(nsTimeout);
}

nsTimeout::~nsTimeout()
{
#ifdef DEBUG_jst
  {
    extern int gTimeoutCnt;

    --gTimeoutCnt;
  }
#endif

  MOZ_COUNT_DTOR(nsTimeout);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsTimeout)
NS_IMPL_CYCLE_COLLECTION_UNLINK_NATIVE_0(nsTimeout)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_BEGIN(nsTimeout)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mWindow,
                                                       nsIScriptGlobalObject)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mScriptHandler)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsTimeout, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsTimeout, Release)

nsPIDOMWindow::nsPIDOMWindow(nsPIDOMWindow *aOuterWindow)
: mFrameElement(nsnull), mDocShell(nsnull), mModalStateDepth(0),
  mRunningTimeout(nsnull), mMutationBits(0), mIsDocumentLoaded(PR_FALSE),
  mIsHandlingResizeEvent(PR_FALSE), mIsInnerWindow(aOuterWindow != nsnull),
  mMayHavePaintEventListener(PR_FALSE), mMayHaveTouchEventListener(PR_FALSE),
  mMayHaveAudioAvailableEventListener(PR_FALSE), mIsModalContentWindow(PR_FALSE),
  mIsActive(PR_FALSE), mInnerWindow(nsnull), mOuterWindow(aOuterWindow),
  // Make sure no actual window ends up with mWindowID == 0
  mWindowID(++gNextWindowID)
 {}

nsPIDOMWindow::~nsPIDOMWindow() {}

//*****************************************************************************
//***    nsGlobalWindow: Object Management
//*****************************************************************************

nsGlobalWindow::nsGlobalWindow(nsGlobalWindow *aOuterWindow)
  : nsPIDOMWindow(aOuterWindow),
    mIsFrozen(PR_FALSE),
    mDidInitJavaProperties(PR_FALSE),
    mFullScreen(PR_FALSE),
    mIsClosed(PR_FALSE), 
    mInClose(PR_FALSE), 
    mHavePendingClose(PR_FALSE),
    mHadOriginalOpener(PR_FALSE),
    mIsPopupSpam(PR_FALSE),
    mBlockScriptedClosingFlag(PR_FALSE),
    mFireOfflineStatusChangeEventOnThaw(PR_FALSE),
    mCreatingInnerWindow(PR_FALSE),
    mIsChrome(PR_FALSE),
    mNeedsFocus(PR_TRUE),
    mHasFocus(PR_FALSE),
#if defined(XP_MAC) || defined(XP_MACOSX)
    mShowAccelerators(PR_FALSE),
    mShowFocusRings(PR_FALSE),
#else
    mShowAccelerators(PR_TRUE),
    mShowFocusRings(PR_TRUE),
#endif
    mShowFocusRingForContent(PR_FALSE),
    mFocusByKeyOccurred(PR_FALSE),
    mHasAcceleration(PR_FALSE),
    mNotifiedIDDestroyed(PR_FALSE),
    mTimeoutInsertionPoint(nsnull),
    mTimeoutPublicIdCounter(1),
    mTimeoutFiringDepth(0),
    mJSObject(nsnull),
    mPendingStorageEventsObsolete(nsnull),
    mTimeoutsSuspendDepth(0),
    mFocusMethod(0),
    mSerial(0),
#ifdef DEBUG
    mSetOpenerWindowCalled(PR_FALSE),
#endif
    mCleanedUp(PR_FALSE),
    mCallCleanUpAfterModalDialogCloses(PR_FALSE),
    mDialogAbuseCount(0),
    mDialogDisabled(PR_FALSE)
{
  nsLayoutStatics::AddRef();

  // Initialize the PRCList (this).
  PR_INIT_CLIST(this);

  // Initialize timeout storage
  PR_INIT_CLIST(&mTimeouts);

  if (aOuterWindow) {
    // |this| is an inner window, add this inner window to the outer
    // window list of inners.
    PR_INSERT_AFTER(this, aOuterWindow);

    mObserver = new nsGlobalWindowObserver(this);
    if (mObserver) {
      NS_ADDREF(mObserver);
      nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
      if (os) {
        // Watch for online/offline status changes so we can fire events. Use
        // a strong reference.
        os->AddObserver(mObserver, NS_IOSERVICE_OFFLINE_STATUS_TOPIC,
                        PR_FALSE);

        // Watch for dom-storage-changed so we can fire storage
        // events. Use a strong reference.
        os->AddObserver(mObserver, "dom-storage2-changed", PR_FALSE);
        os->AddObserver(mObserver, "dom-storage-changed", PR_FALSE);
      }
    }
  } else {
    // |this| is an outer window. Outer windows start out frozen and
    // remain frozen until they get an inner window, so freeze this
    // outer window here.
    Freeze();

    mObserver = nsnull;
  }

  // We could have failed the first time through trying
  // to create the entropy collector, so we should
  // try to get one until we succeed.

  gRefCnt++;

#if !(defined(NS_DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
  if (gRefCnt == 1) {
    static const char* prefName = "browser.dom.window.dump.enabled";
    nsContentUtils::AddBoolPrefVarCache(prefName, &gDOMWindowDumpEnabled);
    gDOMWindowDumpEnabled = nsContentUtils::GetBoolPref(prefName);
  }
#endif

  if (gDumpFile == nsnull) {
    const nsAdoptingCString& fname = 
      nsContentUtils::GetCharPref("browser.dom.window.dump.file");
    if (!fname.IsEmpty()) {
      // if this fails to open, Dump() knows to just go to stdout
      // on null.
      gDumpFile = fopen(fname, "wb+");
    } else {
      gDumpFile = stdout;
    }
  }

  if (!gEntropyCollector) {
    CallGetService(NS_ENTROPYCOLLECTOR_CONTRACTID, &gEntropyCollector);
  }

  mSerial = ++gSerialCounter;

#ifdef DEBUG
  printf("++DOMWINDOW == %d (%p) [serial = %d] [outer = %p]\n", gRefCnt,
         static_cast<void*>(static_cast<nsIScriptGlobalObject*>(this)),
         gSerialCounter, static_cast<void*>(aOuterWindow));
#endif

#ifdef PR_LOGGING
  if (!gDOMLeakPRLog)
    gDOMLeakPRLog = PR_NewLogModule("DOMLeak");

  if (gDOMLeakPRLog)
    PR_LOG(gDOMLeakPRLog, PR_LOG_DEBUG,
           ("DOMWINDOW %p created outer=%p", this, aOuterWindow));
#endif
}

nsGlobalWindow::~nsGlobalWindow()
{
  if (!--gRefCnt) {
    NS_IF_RELEASE(gEntropyCollector);
  }
#ifdef DEBUG
  nsCAutoString url;
  if (mLastOpenedURI) {
    mLastOpenedURI->GetSpec(url);
  }

  printf("--DOMWINDOW == %d (%p) [serial = %d] [outer = %p] [url = %s]\n",
         gRefCnt, static_cast<void*>(static_cast<nsIScriptGlobalObject*>(this)),
         mSerial, static_cast<void*>(mOuterWindow), url.get());
#endif

#ifdef PR_LOGGING
  if (gDOMLeakPRLog)
    PR_LOG(gDOMLeakPRLog, PR_LOG_DEBUG,
           ("DOMWINDOW %p destroyed", this));
#endif

  if (mObserver) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->RemoveObserver(mObserver, NS_IOSERVICE_OFFLINE_STATUS_TOPIC);
      os->RemoveObserver(mObserver, "dom-storage2-changed");
      os->RemoveObserver(mObserver, "dom-storage-changed");
    }

    // Drop its reference to this dying window, in case for some bogus reason
    // the object stays around.
    mObserver->Forget();
    NS_RELEASE(mObserver);
  }

  if (IsOuterWindow()) {
    // An outer window is destroyed with inner windows still possibly
    // alive, iterate through the inner windows and null out their
    // back pointer to this outer, and pull them out of the list of
    // inner windows.

    nsGlobalWindow *w;
    while ((w = (nsGlobalWindow *)PR_LIST_HEAD(this)) != this) {
      NS_ASSERTION(w->mOuterWindow == this, "Uh, bad outer window pointer?");

      w->mOuterWindow = nsnull;

      PR_REMOVE_AND_INIT_LINK(w);
    }
  } else {
    if (mListenerManager) {
      mListenerManager->Disconnect();
      mListenerManager = nsnull;
    }

    // An inner window is destroyed, pull it out of the outer window's
    // list if inner windows.

    PR_REMOVE_LINK(this);

    // If our outer window's inner window is this window, null out the
    // outer window's reference to this window that's being deleted.
    nsGlobalWindow *outer = GetOuterWindowInternal();
    if (outer && outer->mInnerWindow == this) {
      outer->mInnerWindow = nsnull;
    }
  }

  mDocument = nsnull;           // Forces Release
  mDoc = nsnull;

  NS_ASSERTION(!mArguments, "mArguments wasn't cleaned up properly!");

  CleanUp(PR_TRUE);

#ifdef DEBUG
  nsCycleCollector_DEBUG_wasFreed(static_cast<nsIScriptGlobalObject*>(this));
#endif

  delete mPendingStorageEventsObsolete;

  nsLayoutStatics::Release();
}

// static
void
nsGlobalWindow::ShutDown()
{
  NS_IF_RELEASE(sGlobalStorageList);

  if (gDumpFile && gDumpFile != stdout) {
    fclose(gDumpFile);
  }
  gDumpFile = nsnull;
}

// static
void
nsGlobalWindow::CleanupCachedXBLHandlers(nsGlobalWindow* aWindow)
{
  if (aWindow->mCachedXBLPrototypeHandlers.IsInitialized() &&
      aWindow->mCachedXBLPrototypeHandlers.Count() > 0) {
    aWindow->mCachedXBLPrototypeHandlers.Clear();

    nsCOMPtr<nsISupports> supports;
    aWindow->QueryInterface(NS_GET_IID(nsCycleCollectionISupports),
                            getter_AddRefs(supports));
    NS_ASSERTION(supports, "Failed to QI to nsCycleCollectionISupports?!");

    nsContentUtils::DropJSObjects(supports);
  }
}

void
nsGlobalWindow::MaybeForgiveSpamCount()
{
  if (IsOuterWindow() &&
      IsPopupSpamWindow())
  {
    SetPopupSpamWindow(PR_FALSE);
    --gOpenPopupSpamCount;
    NS_ASSERTION(gOpenPopupSpamCount >= 0,
                 "Unbalanced decrement of gOpenPopupSpamCount");
  }
}

void
nsGlobalWindow::CleanUp(PRBool aIgnoreModalDialog)
{
  if (IsOuterWindow() && !aIgnoreModalDialog) {
    nsGlobalWindow* inner = GetCurrentInnerWindowInternal();
    nsCOMPtr<nsIDOMModalContentWindow>
      dlg(do_QueryInterface(static_cast<nsPIDOMWindow*>(inner)));
    if (dlg) {
      // The window we're trying to clean up is the outer window of a
      // modal dialog.  Defer cleanup until the window closes, and let
      // ShowModalDialog take care of calling CleanUp.
      mCallCleanUpAfterModalDialogCloses = PR_TRUE;
      return;
    }
  }

  // Guarantee idempotence.
  if (mCleanedUp)
    return;
  mCleanedUp = PR_TRUE;

  mNavigator = nsnull;
  mScreen = nsnull;
  mHistory = nsnull;
  mMenubar = nsnull;
  mToolbar = nsnull;
  mLocationbar = nsnull;
  mPersonalbar = nsnull;
  mStatusbar = nsnull;
  mScrollbars = nsnull;
  mLocation = nsnull;
  mFrames = nsnull;
  mApplicationCache = nsnull;
  mIndexedDB = nsnull;

  ClearControllers();

  mOpener = nsnull;             // Forces Release
  if (mContext) {
#ifdef DEBUG
    nsCycleCollector_DEBUG_shouldBeFreed(mContext);
#endif
    mContext = nsnull;            // Forces Release
  }
  mChromeEventHandler = nsnull; // Forces Release
  mParentTarget = nsnull;

  nsGlobalWindow *inner = GetCurrentInnerWindowInternal();

  if (inner) {
    inner->CleanUp(aIgnoreModalDialog);
  }

  if (mHasAcceleration) {
    nsCOMPtr<nsIAccelerometer> ac = do_GetService(NS_ACCELEROMETER_CONTRACTID);
    if (ac)
      ac->RemoveWindowListener(this);
  }

  if (mIsChrome && static_cast<nsGlobalChromeWindow*>(this)->mMessageManager) {
    static_cast<nsFrameMessageManager*>(
       static_cast<nsGlobalChromeWindow*>(
         this)->mMessageManager.get())->Disconnect();
  }

  mInnerWindowHolder = nsnull;
  mArguments = nsnull;
  mArgumentsLast = nsnull;
  mArgumentsOrigin = nsnull;

  CleanupCachedXBLHandlers(this);

#ifdef DEBUG
  nsCycleCollector_DEBUG_shouldBeFreed(static_cast<nsIScriptGlobalObject*>(this));
#endif
}

void
nsGlobalWindow::ClearControllers()
{
  if (mControllers) {
    PRUint32 count;
    mControllers->GetControllerCount(&count);

    while (count--) {
      nsCOMPtr<nsIController> controller;
      mControllers->GetControllerAt(count, getter_AddRefs(controller));

      nsCOMPtr<nsIControllerContext> context = do_QueryInterface(controller);
      if (context)
        context->SetCommandContext(nsnull);
    }

    mControllers = nsnull;
  }
}

class ClearScopeEvent : public nsRunnable
{
public:
  ClearScopeEvent(nsGlobalWindow *innerWindow)
    : mInnerWindow(innerWindow) {
  }

  NS_IMETHOD Run()
  {
    mInnerWindow->ReallyClearScope(this);
    return NS_OK;
  }

private:
  nsRefPtr<nsGlobalWindow> mInnerWindow;
};

void
nsGlobalWindow::ReallyClearScope(nsRunnable *aRunnable)
{
  NS_ASSERTION(IsInnerWindow(), "Must be an inner window");

  nsIScriptContext *jsscx = GetContextInternal();
  if (jsscx && jsscx->GetExecutingScript()) {
    if (!aRunnable) {
      aRunnable = new ClearScopeEvent(this);
      if (!aRunnable) {
        // The only reason that we clear scope here is to try to prevent
        // leaks. Failing to clear scope might mean that we'll leak more
        // but if we don't have enough memory to allocate a ClearScopeEvent
        // we probably don't have to worry about this anyway.
        return;
      }
    }

    NS_DispatchToMainThread(aRunnable);
    return;
  }

  NotifyWindowIDDestroyed("inner-window-destroyed");
  nsIScriptContext *scx = GetContextInternal();
  if (scx) {
    scx->ClearScope(mJSObject, PR_TRUE);
  }
}

void
nsGlobalWindow::FreeInnerObjects(PRBool aClearScope)
{
  NS_ASSERTION(IsInnerWindow(), "Don't free inner objects on an outer window");

  // Kill all of the workers for this window.
  nsDOMThreadService* dts = nsDOMThreadService::get();
  if (dts) {
    nsIScriptContext *scx = GetContextInternal();

    JSContext *cx = scx ? (JSContext *)scx->GetNativeContext() : nsnull;

    // Have to suspend this request here because CancelWorkersForGlobal will
    // lock until the worker has died and that could cause a deadlock.
    JSAutoSuspendRequest asr(cx);

    dts->CancelWorkersForGlobal(static_cast<nsIScriptGlobalObject*>(this));
  }

  ClearAllTimeouts();

  mChromeEventHandler = nsnull;

  if (mListenerManager) {
    mListenerManager->Disconnect();
    mListenerManager = nsnull;
  }

  mLocation = nsnull;

  if (mDocument) {
    NS_ASSERTION(mDoc, "Why is mDoc null?");

    // Remember the document's principal.
    mDocumentPrincipal = mDoc->NodePrincipal();
  }

#ifdef DEBUG
  if (mDocument)
    nsCycleCollector_DEBUG_shouldBeFreed(nsCOMPtr<nsISupports>(do_QueryInterface(mDocument)));
#endif

  // Make sure that this is called before we null out the document.
  NotifyDOMWindowDestroyed(this);

  // Remove our reference to the document and the document principal.
  mDocument = nsnull;
  mDoc = nsnull;

  if (mApplicationCache) {
    static_cast<nsDOMOfflineResourceList*>(mApplicationCache.get())->Disconnect();
    mApplicationCache = nsnull;
  }

  mIndexedDB = nsnull;

  if (aClearScope) {
    // NB: This might not clear our scope, but fire an event to do so
    // instead.
    ReallyClearScope(nsnull);
  }

  if (mDummyJavaPluginOwner) {
    // Tear down the dummy java plugin.

    // XXXjst: On a general note, should windows with java stuff in
    // them ever even make it into the fast-back cache?

    mDummyJavaPluginOwner->Destroy();

    mDummyJavaPluginOwner = nsnull;
  }

  CleanupCachedXBLHandlers(this);

#ifdef DEBUG
  nsCycleCollector_DEBUG_shouldBeFreed(static_cast<nsIScriptGlobalObject*>(this));
#endif
}

//*****************************************************************************
// nsGlobalWindow::nsISupports
//*****************************************************************************

#define WINDOW_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(_class)                      \
  if (aIID.Equals(NS_GET_IID(nsIClassInfo)) ||                                \
      aIID.Equals(NS_GET_IID(nsXPCClassInfo))) {                              \
    foundInterface = NS_GetDOMClassInfoInstance(IsInnerWindow()               \
                                                ? eDOMClassInfo_Inner##_class##_id \
                                                : eDOMClassInfo_##_class##_id);\
    if (!foundInterface) {                                                    \
      *aInstancePtr = nsnull;                                                 \
      return NS_ERROR_OUT_OF_MEMORY;                                          \
    }                                                                         \
  } else

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGlobalWindow)

DOMCI_DATA(Window, nsGlobalWindow)
DOMCI_DATA(InnerWindow, nsGlobalWindow)

// QueryInterface implementation for nsGlobalWindow
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsGlobalWindow)
  // Make sure this matches the cast in nsGlobalWindow::FromWrapper()
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindowInternal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindow2)
  NS_INTERFACE_MAP_ENTRY(nsIDOMJSWindow)
  NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsPIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3EventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsPIDOMWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMViewCSS)
  NS_INTERFACE_MAP_ENTRY(nsIDOMAbstractView)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorageWindow)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  WINDOW_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Window)
NS_INTERFACE_MAP_END


NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsGlobalWindow, nsIScriptGlobalObject)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsGlobalWindow,
                                           nsIScriptGlobalObject)


NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsGlobalWindow)
  if (tmp->mDoc && nsCCUncollectableMarker::InGeneration(
                     cb, tmp->mDoc->GetMarkedCCGeneration())) {
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mContext)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mControllers)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mArguments)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mArgumentsLast)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mInnerWindowHolder)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOpenerScriptPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mListenerManager)

  for (nsTimeout* timeout = tmp->FirstTimeout();
       tmp->IsTimeout(timeout);
       timeout = timeout->Next()) {
    cb.NoteNativeChild(timeout, &NS_CYCLE_COLLECTION_NAME(nsTimeout));
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSessionStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mApplicationCache)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDocumentPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDoc)

  // Traverse stuff from nsPIDOMWindow
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mChromeEventHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mParentTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFrameElement)

  // Traverse mDummyJavaPluginOwner
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDummyJavaPluginOwner)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFocusedNode)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsGlobalWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mContext)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mControllers)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mArguments)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mArgumentsLast)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mInnerWindowHolder)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOpenerScriptPrincipal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mListenerManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSessionStorage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mApplicationCache)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDocumentPrincipal)

  // Unlink stuff from nsPIDOMWindow
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mChromeEventHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mParentTarget)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFrameElement)

  // Unlink mDummyJavaPluginOwner
  if (tmp->mDummyJavaPluginOwner) {
    tmp->mDummyJavaPluginOwner->Destroy();
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDummyJavaPluginOwner)
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFocusedNode)

NS_IMPL_CYCLE_COLLECTION_UNLINK_END

struct TraceData
{
  TraceData(TraceCallback& aCallback, void* aClosure) :
    callback(aCallback), closure(aClosure) {}

  TraceCallback& callback;
  void* closure;
};

static PLDHashOperator
TraceXBLHandlers(const void* aKey, void* aData, void* aClosure)
{
  TraceData* data = static_cast<TraceData*>(aClosure);
  data->callback(nsIProgrammingLanguage::JAVASCRIPT, aData, data->closure);
  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsGlobalWindow)
  if (tmp->mCachedXBLPrototypeHandlers.IsInitialized()) {
    TraceData data(aCallback, aClosure);
    tmp->mCachedXBLPrototypeHandlers.EnumerateRead(TraceXBLHandlers, &data);
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_BEGIN(nsGlobalWindow)
  nsGlobalWindow::CleanupCachedXBLHandlers(tmp);
NS_IMPL_CYCLE_COLLECTION_ROOT_END

//*****************************************************************************
// nsGlobalWindow::nsIScriptGlobalObject
//*****************************************************************************

nsresult
nsGlobalWindow::SetScriptContext(PRUint32 lang_id, nsIScriptContext *aScriptContext)
{
  NS_ASSERTION(lang_id == nsIProgrammingLanguage::JAVASCRIPT,
               "We don't support this language ID");
  NS_ASSERTION(IsOuterWindow(), "Uh, SetScriptContext() called on inner window!");

  NS_ASSERTION(!aScriptContext || !mContext, "Bad call to SetContext()!");

  if (aScriptContext) {
    // should probably assert the context is clean???
    aScriptContext->WillInitializeContext();

    nsresult rv = aScriptContext->InitContext();
    NS_ENSURE_SUCCESS(rv, rv);

    if (IsFrame()) {
      // This window is a [i]frame, don't bother GC'ing when the
      // frame's context is destroyed since a GC will happen when the
      // frameset or host document is destroyed anyway.

      aScriptContext->SetGCOnDestruction(PR_FALSE);
    }
  }

  mContext = aScriptContext;
  return NS_OK;
}

nsresult
nsGlobalWindow::EnsureScriptEnvironment(PRUint32 aLangID)
{
  NS_ASSERTION(aLangID == nsIProgrammingLanguage::JAVASCRIPT,
               "We don't support this language ID");
  FORWARD_TO_OUTER(EnsureScriptEnvironment, (aLangID), NS_ERROR_NOT_INITIALIZED);

  if (mJSObject)
      return NS_OK;

  NS_ASSERTION(!GetCurrentInnerWindowInternal(),
               "mJSObject is null, but we have an inner window?");

  nsCOMPtr<nsIScriptRuntime> scriptRuntime;
  nsresult rv = NS_GetScriptRuntimeByID(aLangID, getter_AddRefs(scriptRuntime));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIScriptContext> context;
  rv = scriptRuntime->CreateContext(getter_AddRefs(context));
  NS_ENSURE_SUCCESS(rv, rv);

  return SetScriptContext(aLangID, context);
}

nsIScriptContext *
nsGlobalWindow::GetScriptContext(PRUint32 lang)
{
  NS_ASSERTION(lang == nsIProgrammingLanguage::JAVASCRIPT,
               "We don't support this language ID");

  FORWARD_TO_OUTER(GetScriptContext, (lang), nsnull);
  return mContext;
}

void *
nsGlobalWindow::GetScriptGlobal(PRUint32 lang)
{
  NS_ASSERTION(lang == nsIProgrammingLanguage::JAVASCRIPT,
               "We don't support this language ID");
  return mJSObject;
}

nsIScriptContext *
nsGlobalWindow::GetContext()
{
  FORWARD_TO_OUTER(GetContext, (), nsnull);

  // check GetContext is indeed identical to GetScriptContext()
  NS_ASSERTION(mContext == GetScriptContext(nsIProgrammingLanguage::JAVASCRIPT),
               "GetContext confused?");
  return mContext;
}

JSObject *
nsGlobalWindow::GetGlobalJSObject()
{
  NS_ASSERTION(mJSObject == GetScriptGlobal(nsIProgrammingLanguage::JAVASCRIPT),
               "GetGlobalJSObject confused?");
  return FastGetGlobalJSObject();
}

PRBool
nsGlobalWindow::WouldReuseInnerWindow(nsIDocument *aNewDocument)
{
  // We reuse the inner window when:
  // a. We are currently at our original document.
  // b. At least one of the following conditions are true:
  // -- We are not currently a content window (i.e., we're currently a chrome
  //    window).
  // -- The new document is the same as the old document. This means that we're
  //    getting called from document.open().
  // -- The new document has the same origin as what we have loaded right now.

  if (!mDoc || !aNewDocument) {
    return PR_FALSE;
  }

  if (!mDoc->IsInitialDocument()) {
    return PR_FALSE;
  }
  
  NS_ASSERTION(IsAboutBlank(mDoc->GetDocumentURI()),
               "How'd this happen?");
  
  // Great, we're the original document, check for one of the other
  // conditions.
  if (mDoc == aNewDocument) {
    // aClearScopeHint is false.
    return PR_TRUE;
  }

  PRBool equal;
  if (NS_SUCCEEDED(mDoc->NodePrincipal()->Equals(aNewDocument->NodePrincipal(),
                                                 &equal)) &&
      equal) {
    // The origin is the same.
    return PR_TRUE;
  }

  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(mDocShell));

  if (treeItem) {
    PRInt32 itemType = nsIDocShellTreeItem::typeContent;
    treeItem->GetItemType(&itemType);

    // If we're a chrome window, then we want to reuse the inner window.
    return itemType == nsIDocShellTreeItem::typeChrome;
  }

  // No treeItem: don't reuse the current inner window.
  return PR_FALSE;
}

void
nsGlobalWindow::SetOpenerScriptPrincipal(nsIPrincipal* aPrincipal)
{
  FORWARD_TO_OUTER_VOID(SetOpenerScriptPrincipal, (aPrincipal));

  if (mDoc) {
    if (!mDoc->IsInitialDocument()) {
      // We have a document already, and it's not the original one.  Bail out.
      // Do NOT set mOpenerScriptPrincipal in this case, just to be safe.
      return;
    }
    
#ifdef DEBUG
    // We better have an about:blank document loaded at this point.  Otherwise,
    // something is really weird.
    nsCOMPtr<nsIURI> uri;
    mDoc->NodePrincipal()->GetURI(getter_AddRefs(uri));
    NS_ASSERTION(uri && IsAboutBlank(uri) &&
                 IsAboutBlank(mDoc->GetDocumentURI()),
                 "Unexpected original document");
#endif
    
    // Set the opener principal on our document; given the above check, this
    // is safe.
    mDoc->SetPrincipal(aPrincipal);
  }
    
  mOpenerScriptPrincipal = aPrincipal;
}

nsIPrincipal*
nsGlobalWindow::GetOpenerScriptPrincipal()
{
  FORWARD_TO_OUTER(GetOpenerScriptPrincipal, (), nsnull);

  return mOpenerScriptPrincipal;
}

PopupControlState
PushPopupControlState(PopupControlState aState, PRBool aForce)
{
  PopupControlState oldState = gPopupControlState;

  if (aState < gPopupControlState || aForce) {
    gPopupControlState = aState;
  }

  return oldState;
}

void
PopPopupControlState(PopupControlState aState)
{
  gPopupControlState = aState;
}

PopupControlState
nsGlobalWindow::PushPopupControlState(PopupControlState aState,
                                      PRBool aForce) const
{
  return ::PushPopupControlState(aState, aForce);
}

void
nsGlobalWindow::PopPopupControlState(PopupControlState aState) const
{
  ::PopPopupControlState(aState);
}

PopupControlState
nsGlobalWindow::GetPopupControlState() const
{
  return gPopupControlState;
}

#define WINDOWSTATEHOLDER_IID \
{0x0b917c3e, 0xbd50, 0x4683, {0xaf, 0xc9, 0xc7, 0x81, 0x07, 0xae, 0x33, 0x26}}

class WindowStateHolder : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(WINDOWSTATEHOLDER_IID)
  NS_DECL_ISUPPORTS

  WindowStateHolder(nsGlobalWindow *aWindow,
                    nsIXPConnectJSObjectHolder *aHolder,
                    nsNavigator *aNavigator,
                    nsIXPConnectJSObjectHolder *aOuterProto,
                    nsIXPConnectJSObjectHolder *aOuterRealProto);

  nsGlobalWindow* GetInnerWindow() { return mInnerWindow; }
  nsIXPConnectJSObjectHolder *GetInnerWindowHolder()
  { return mInnerWindowHolder; }

  nsNavigator* GetNavigator() { return mNavigator; }
  nsIXPConnectJSObjectHolder* GetOuterProto() { return mOuterProto; }
  nsIXPConnectJSObjectHolder* GetOuterRealProto() { return mOuterRealProto; }

  void DidRestoreWindow()
  {
    mInnerWindow = nsnull;

    mInnerWindowHolder = nsnull;
    mNavigator = nsnull;
    mOuterProto = nsnull;
    mOuterRealProto = nsnull;
  }

protected:
  ~WindowStateHolder();

  nsGlobalWindow *mInnerWindow;
  // We hold onto this to make sure the inner window doesn't go away. The outer
  // window ends up recalculating it anyway.
  nsCOMPtr<nsIXPConnectJSObjectHolder> mInnerWindowHolder;
  nsRefPtr<nsNavigator> mNavigator;
  nsCOMPtr<nsIXPConnectJSObjectHolder> mOuterProto;
  nsCOMPtr<nsIXPConnectJSObjectHolder> mOuterRealProto;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WindowStateHolder, WINDOWSTATEHOLDER_IID)

WindowStateHolder::WindowStateHolder(nsGlobalWindow *aWindow,
                                     nsIXPConnectJSObjectHolder *aHolder,
                                     nsNavigator *aNavigator,
                                     nsIXPConnectJSObjectHolder *aOuterProto,
                                     nsIXPConnectJSObjectHolder *aOuterRealProto)
  : mInnerWindow(aWindow),
    mNavigator(aNavigator),
    mOuterProto(aOuterProto),
    mOuterRealProto(aOuterRealProto)
{
  NS_PRECONDITION(aWindow, "null window");
  NS_PRECONDITION(aWindow->IsInnerWindow(), "Saving an outer window");

  mInnerWindowHolder = aHolder;

  aWindow->SuspendTimeouts();
}

WindowStateHolder::~WindowStateHolder()
{
  if (mInnerWindow) {
    // This window was left in the bfcache and is now going away. We need to
    // free it up.
    // Note that FreeInnerObjects may already have been called on the
    // inner window if its outer has already had SetDocShell(null)
    // called.  In this case the contexts will all be null and the
    // PR_TRUE for aClearScope won't do anything; this is OK since
    // SetDocShell(null) already did it.
    mInnerWindow->FreeInnerObjects(PR_TRUE);
  }
}

NS_IMPL_ISUPPORTS1(WindowStateHolder, WindowStateHolder)

nsresult
nsGlobalWindow::SetNewDocument(nsIDocument* aDocument,
                               nsISupports* aState,
                               PRBool aForceReuseInnerWindow)
{
  NS_TIME_FUNCTION;

  NS_PRECONDITION(mDocumentPrincipal == nsnull,
                  "mDocumentPrincipal prematurely set!");

  if (!aDocument) {
    NS_ERROR("SetNewDocument(null) called!");

    return NS_ERROR_INVALID_ARG;
  }

  if (IsInnerWindow()) {
    if (!mOuterWindow) {
      return NS_ERROR_NOT_INITIALIZED;
    }

    // Refuse to set a new document if the call came from an inner
    // window that's not the current inner window.
    if (mOuterWindow->GetCurrentInnerWindow() != this) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    return GetOuterWindowInternal()->SetNewDocument(aDocument, aState,
                                                    aForceReuseInnerWindow);
  }

  NS_PRECONDITION(IsOuterWindow(), "Must only be called on outer windows");

  if (IsFrozen()) {
    // This outer is now getting its first inner, thaw the outer now
    // that it's ready and is getting an inner window.

    Thaw();
  }

  // XXX Brain transplant outer window JSObject and create new one!

  NS_ASSERTION(!GetCurrentInnerWindow() ||
               GetCurrentInnerWindow()->GetExtantDocument() == mDocument,
               "Uh, mDocument doesn't match the current inner window "
               "document!");

  PRBool wouldReuseInnerWindow = WouldReuseInnerWindow(aDocument);
  if (aForceReuseInnerWindow &&
      !wouldReuseInnerWindow &&
      mDoc &&
      mDoc->NodePrincipal() != aDocument->NodePrincipal()) {
    NS_ERROR("Attempted forced inner window reuse while changing principal");
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = NS_OK;

  nsCOMPtr<nsIDocument> oldDoc(do_QueryInterface(mDocument));

  nsIScriptContext *scx = GetContextInternal();
  NS_ENSURE_TRUE(scx, NS_ERROR_NOT_INITIALIZED);

  JSContext *cx = (JSContext *)scx->GetNativeContext();
#ifndef MOZ_DISABLE_DOMCRYPTO
  // clear smartcard events, our document has gone away.
  if (mCrypto) {
    mCrypto->SetEnableSmartCardEvents(PR_FALSE);
  }
#endif
  if (!mDocument) {
    // First document load.

    // Get our private root. If it is equal to us, then we need to
    // attach our global key bindings that handles browser scrolling
    // and other browser commands.
    nsIDOMWindowInternal *internal = nsGlobalWindow::GetPrivateRoot();

    if (internal == static_cast<nsIDOMWindowInternal *>(this)) {
      nsCOMPtr<nsIXBLService> xblService = do_GetService("@mozilla.org/xbl;1");
      if (xblService) {
        nsCOMPtr<nsPIDOMEventTarget> piTarget =
          do_QueryInterface(mChromeEventHandler);
        xblService->AttachGlobalKeyHandler(piTarget);
      }
    }
  }

  /* No mDocShell means we're already been partially closed down.  When that
     happens, setting status isn't a big requirement, so don't. (Doesn't happen
     under normal circumstances, but bug 49615 describes a case.) */

  nsContentUtils::AddScriptRunner(
    NS_NewRunnableMethod(this, &nsGlobalWindow::ClearStatus));

  PRBool reUseInnerWindow = aForceReuseInnerWindow || wouldReuseInnerWindow;

  // Remember the old document's principal.
  nsIPrincipal *oldPrincipal = nsnull;
  if (oldDoc) {
    oldPrincipal = oldDoc->NodePrincipal();
  }

  // Drop our reference to the navigator object unless we're reusing
  // the existing inner window or the new document is from the same
  // origin as the old document.
  if (!reUseInnerWindow && mNavigator && oldPrincipal) {
    PRBool equal;
    rv = oldPrincipal->Equals(aDocument->NodePrincipal(), &equal);

    if (NS_FAILED(rv) || !equal) {
      // Different origins.  Release the navigator object so it gets
      // recreated for the new document.  The plugins or mime types
      // arrays may have changed. See bug 150087.
      mNavigator->SetDocShell(nsnull);

      mNavigator = nsnull;
    }
  }

  if (mNavigator && aDocument != oldDoc) {
    // We didn't drop our reference to our old navigator object and
    // we're loading a new document. Notify the navigator object about
    // the new document load so that it can make sure it is ready for
    // the new document.

    mNavigator->LoadingNewDocument();
  }

  // Set mDocument even if this is an outer window to avoid
  // having to *always* reach into the inner window to find the
  // document.
  mDocument = do_QueryInterface(aDocument);
  mDoc = aDocument;

#ifdef DEBUG
  mLastOpenedURI = aDocument->GetDocumentURI();
#endif

  mContext->WillInitializeContext();

  nsGlobalWindow *currentInner = GetCurrentInnerWindowInternal();

  nsRefPtr<nsGlobalWindow> newInnerWindow;

  PRBool thisChrome = IsChromeWindow();
  nsCOMPtr<nsIXPConnectJSObjectHolder> navigatorHolder;
  jsval nav;

  PRBool isChrome = PR_FALSE;

  nsCxPusher cxPusher;
  if (!cxPusher.Push(cx)) {
    return NS_ERROR_FAILURE;
  }

  JSAutoRequest ar(cx);

  nsCOMPtr<WindowStateHolder> wsh = do_QueryInterface(aState);
  NS_ASSERTION(!aState || wsh, "What kind of weird state are you giving me here?");

  // Make sure to clear scope on the outer window *before* we
  // initialize the new inner window. If we don't, things
  // (Object.prototype etc) could leak from the old outer to the new
  // inner scope.
  mContext->ClearScope(mJSObject, PR_FALSE);

  // This code should not be called during shutdown any more (now that
  // we don't ever call SetNewDocument(nsnull), so no need to null
  // check xpc here.
  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
  if (reUseInnerWindow) {
    // We're reusing the current inner window.
    NS_ASSERTION(!currentInner->IsFrozen(),
                 "We should never be reusing a shared inner window");
    newInnerWindow = currentInner;

    if (aDocument != oldDoc) {
      nsCommonWindowSH::InvalidateGlobalScopePolluter(cx, currentInner->mJSObject);
    }
  } else {
    if (aState) {
      newInnerWindow = wsh->GetInnerWindow();
      mInnerWindowHolder = wsh->GetInnerWindowHolder();
      
      NS_ASSERTION(newInnerWindow, "Got a state without inner window");

      // These assignments addref.
      mNavigator = wsh->GetNavigator();

      if (mNavigator) {
        // Update mNavigator's docshell pointer now.
        mNavigator->SetDocShell(mDocShell);
        mNavigator->LoadingNewDocument();
      }
    } else if (thisChrome) {
      newInnerWindow = new nsGlobalChromeWindow(this);
      isChrome = PR_TRUE;
    } else if (mIsModalContentWindow) {
      newInnerWindow = new nsGlobalModalWindow(this);
    } else {
      newInnerWindow = new nsGlobalWindow(this);
    }

    if (currentInner && currentInner->mJSObject) {
      if (mNavigator && !aState) {
        // Hold on to the navigator wrapper so that we can set
        // window.navigator in the new window to point to the same
        // object (assuming we didn't change origins etc). See bug
        // 163645 for more on why we need this.

        nsIDOMNavigator* navigator =
          static_cast<nsIDOMNavigator*>(mNavigator.get());
        nsContentUtils::WrapNative(cx, currentInner->mJSObject, navigator,
                                   &NS_GET_IID(nsIDOMNavigator), &nav,
                                   getter_AddRefs(navigatorHolder));
      }
    }

    if (!aState) {
      // This is redundant if we're restoring from a previous inner window.
      nsIScriptGlobalObject *sgo =
        (nsIScriptGlobalObject *)newInnerWindow.get();

      // Freeze the outer window and null out the inner window so
      // that initializing classes on the new inner doesn't end up
      // reaching into the old inner window for classes etc.
      //
      // [This happens with Object.prototype when XPConnect creates
      // a temporary global while initializing classes; the reason
      // being that xpconnect creates the temp global w/o a parent
      // and proto, which makes the JS engine look up classes in
      // cx->globalObject, i.e. this outer window].

      mInnerWindow = nsnull;

      Freeze();
      mCreatingInnerWindow = PR_TRUE;
      // Every script context we are initialized with must create a
      // new global.
      void *&newGlobal = (void *&)newInnerWindow->mJSObject;
      nsCOMPtr<nsIXPConnectJSObjectHolder> &holder = mInnerWindowHolder;
      rv = mContext->CreateNativeGlobalForInner(sgo, isChrome,
                                                aDocument->NodePrincipal(),
                                                &newGlobal,
                                                getter_AddRefs(holder));
      NS_ASSERTION(NS_SUCCEEDED(rv) && newGlobal && holder,
                   "Failed to get script global and holder");

      mCreatingInnerWindow = PR_FALSE;
      Thaw();

      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (currentInner && currentInner->mJSObject) {
      PRBool termFuncSet = PR_FALSE;

      if (oldDoc == aDocument) {
        // Suspend the current context's request before Pop() resumes the old
        // context's request.
        JSAutoSuspendRequest asr(cx);

        // Pop our context here so that we get the correct one for the
        // termination function.
        cxPusher.Pop();

        JSContext *oldCx = nsContentUtils::GetCurrentJSContext();

        nsIScriptContext *callerScx;
        if (oldCx && (callerScx = GetScriptContextFromJSContext(oldCx))) {
          // We're called from document.open() (and document.open() is
          // called from JS), clear the scope etc in a termination
          // function on the calling context to prevent clearing the
          // calling scope.
          NS_ASSERTION(!currentInner->IsFrozen(),
              "How does this opened window get into session history");

          JSAutoRequest ar(oldCx);

          callerScx->SetTerminationFunction(ClearWindowScope,
                                            static_cast<nsIDOMWindow *>
                                                       (currentInner));

          termFuncSet = PR_TRUE;
        }

        // Re-push our context.
        cxPusher.Push(cx);
      }

      // Don't clear scope on our current inner window if it's going to be
      // held in the bfcache.
      if (!currentInner->IsFrozen()) {
        // Skip the ClearScope if we set a termination function to do
        // it ourselves, later.
        currentInner->FreeInnerObjects(!termFuncSet);
      }
    }

    mInnerWindow = newInnerWindow;

    if (!mJSObject) {
      mContext->CreateOuterObject(this, newInnerWindow);
      mContext->DidInitializeContext();
      mJSObject = (JSObject *)mContext->GetNativeGlobal();
    } else {
      // XXX New global object and brain transplant!
      rv = xpc->GetWrappedNativeOfJSObject(cx, mJSObject,
                                           getter_AddRefs(wrapper));
      NS_ENSURE_SUCCESS(rv, rv);

      // Restore our object's prototype to its original value so we're sure to
      // update it under ReparentWrappedNativeIfFound.
      JSObject *proto;
      wrapper->GetJSObjectPrototype(&proto);
      if (!JS_SetPrototype(cx, mJSObject, proto)) {
        NS_ERROR("Can't set prototype");
        return NS_ERROR_UNEXPECTED;
      }

      nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
      xpc->ReparentWrappedNativeIfFound(cx, currentInner->mJSObject,
                                        newInnerWindow->mJSObject,
                                        ToSupports(this),
                                        getter_AddRefs(holder));

      if (aState) {
        if (nsIXPConnectJSObjectHolder *holder = wsh->GetOuterRealProto()) {
          holder->GetJSObject(&proto);
        } else {
          proto = nsnull;
        }

        if (!JS_SetPrototype(cx, mJSObject, proto)) {
          NS_ERROR("can't set prototype");
          return NS_ERROR_FAILURE;
        }
      }
    }
  }

  if (!aState && !reUseInnerWindow) {
    // Loading a new page and creating a new inner window, *not*
    // restoring from session history.

    // InitClassesWithNewWrappedGlobal() (via CreateNativeGlobalForInner)
    // for the new inner window
    // sets the global object in cx to be the new wrapped global. We
    // don't want that, but re-initializing the outer window will
    // fix that for us. And perhaps more importantly, this will
    // ensure that the outer window gets a new prototype so we don't
    // leak prototype properties from the old inner window to the
    // new one.
    mContext->InitOuterWindow();

    // Now that both the the inner and outer windows are initialized
    // let the script context do its magic to hook them together.
    mContext->ConnectToInner(newInnerWindow, mJSObject);

    nsCOMPtr<nsIContent> frame = do_QueryInterface(GetFrameElementInternal());
    if (frame && frame->GetOwnerDoc()) {
      nsPIDOMWindow* parentWindow = frame->GetOwnerDoc()->GetWindow();
      if (parentWindow && parentWindow->TimeoutSuspendCount()) {
        SuspendTimeouts(parentWindow->TimeoutSuspendCount());
      }
    }
  }

  // Tell the contexts we have completed setting up the doc.
  // Add an extra ref in case we release mContext during GC.
  nsCOMPtr<nsIScriptContext> kungFuDeathGrip(mContext);
  nsCOMPtr<nsIDOMDocument> dd(do_QueryInterface(aDocument));
  mContext->DidSetDocument(dd, newInnerWindow->mJSObject);

  // Now that the prototype is all set up, install the global scope
  // polluter. This must happen after the above prototype fixup. If
  // the GSP was to be installed on the inner window's real
  // prototype (as it would be if this was done before the prototype
  // fixup above) we would end up holding the GSP alive (through
  // XPConnect's internal marking of wrapper prototypes) as long as
  // the inner window was around, and if the GSP had properties on
  // it that held an element alive we'd hold the document alive,
  // which could hold event handlers alive, which hold the context
  // alive etc.

  if ((!reUseInnerWindow || aDocument != oldDoc) && !aState) {
    nsCOMPtr<nsIHTMLDocument> html_doc(do_QueryInterface(mDocument));
    nsCommonWindowSH::InstallGlobalScopePolluter(cx, newInnerWindow->mJSObject,
                                                 html_doc);
  }

  if (aDocument) {
    aDocument->SetScriptGlobalObject(newInnerWindow);
  }

  if (!aState) {
    if (reUseInnerWindow) {
      if (newInnerWindow->mDoc != aDocument) {
        newInnerWindow->mDocument = do_QueryInterface(aDocument);
        newInnerWindow->mDoc = aDocument;

        // We're reusing the inner window for a new document. In this
        // case we don't clear the inner window's scope, but we must
        // make sure the cached document property gets updated.

        // XXXmarkh - tell other languages about this?
        ::JS_DeleteProperty(cx, currentInner->mJSObject, "document");
      }
    } else {
      rv = newInnerWindow->InnerSetNewDocument(aDocument);
      NS_ENSURE_SUCCESS(rv, rv);

      // Initialize DOM classes etc on the inner window.
      rv = mContext->InitClasses(mJSObject);
      NS_ENSURE_SUCCESS(rv, rv);

      if (navigatorHolder) {
        // Restore window.navigator onto the new inner window.

        ::JS_DefineProperty(cx, newInnerWindow->mJSObject, "navigator",
                            nav, nsnull, nsnull,
                            JSPROP_ENUMERATE | JSPROP_PERMANENT |
                            JSPROP_READONLY);

        // The Navigator's prototype object keeps a reference to the
        // window in which it was first created and can thus cause that
        // window to stay alive for too long. Reparenting it here allows
        // the window to be collected sooner.
        nsIDOMNavigator* navigator =
          static_cast<nsIDOMNavigator*>(mNavigator);

        xpc->
          ReparentWrappedNativeIfFound(cx, JSVAL_TO_OBJECT(nav),
                                       newInnerWindow->mJSObject,
                                       navigator,
                                       getter_AddRefs(navigatorHolder));
      }
    }

    if (mArguments) {
      newInnerWindow->DefineArgumentsProperty(mArguments);
      newInnerWindow->mArguments = mArguments;
      newInnerWindow->mArgumentsOrigin = mArgumentsOrigin;

      mArguments = nsnull;
      mArgumentsOrigin = nsnull;
    }

    // Give the new inner window our chrome event handler (since it
    // doesn't have one).
    newInnerWindow->mChromeEventHandler = mChromeEventHandler;
  }

  mContext->GC();
  mContext->DidInitializeContext();

  if (!wrapper) {
    rv = xpc->GetWrappedNativeOfJSObject(cx, mJSObject,
                                         getter_AddRefs(wrapper));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = xpc->UpdateXOWs((JSContext *)GetContextInternal()->GetNativeContext(),
                       wrapper, nsIXPConnect::XPC_XOW_NAVIGATED);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aState && !reUseInnerWindow) {
    nsContentUtils::AddScriptRunner(
      NS_NewRunnableMethod(this, &nsGlobalWindow::DispatchDOMWindowCreated));
  }

  return NS_OK;
}

void
nsGlobalWindow::DispatchDOMWindowCreated()
{
  if (!mDoc || !mDocument) {
    return;
  }

  // Fire DOMWindowCreated at chrome event listeners
  nsContentUtils::DispatchChromeEvent(mDoc, mDocument, NS_LITERAL_STRING("DOMWindowCreated"),
                                      PR_TRUE /* bubbles */,
                                      PR_FALSE /* not cancellable */);

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    nsAutoString origin;
    nsIPrincipal* principal = mDoc->NodePrincipal();
    nsContentUtils::GetUTFOrigin(principal, origin);
    observerService->
      NotifyObservers(static_cast<nsIDOMWindow*>(this),
                      nsContentUtils::IsSystemPrincipal(principal) ?
                        "chrome-document-global-created" :
                        "content-document-global-created",
                      origin.get());
  }
}

void
nsGlobalWindow::ClearStatus()
{
  SetStatus(EmptyString());
  SetDefaultStatus(EmptyString());
}

nsresult
nsGlobalWindow::InnerSetNewDocument(nsIDocument* aDocument)
{
  NS_PRECONDITION(IsInnerWindow(), "Must only be called on inner windows");

#ifdef PR_LOGGING
  if (aDocument && gDOMLeakPRLog &&
      PR_LOG_TEST(gDOMLeakPRLog, PR_LOG_DEBUG)) {
    nsIURI *uri = aDocument->GetDocumentURI();
    nsCAutoString spec;
    if (uri)
      uri->GetSpec(spec);
    PR_LogPrint("DOMWINDOW %p SetNewDocument %s", this, spec.get());
  }
#endif

  mDocument = do_QueryInterface(aDocument);
  mDoc = aDocument;
  mLocalStorage = nsnull;
  mSessionStorage = nsnull;

#ifdef DEBUG
  mLastOpenedURI = aDocument->GetDocumentURI();
#endif

  // Clear our mutation bitfield.
  mMutationBits = 0;

  return NS_OK;
}

void
nsGlobalWindow::SetDocShell(nsIDocShell* aDocShell)
{
  NS_ASSERTION(IsOuterWindow(), "Uh, SetDocShell() called on inner window!");

  if (aDocShell == mDocShell)
    return;

  // SetDocShell(nsnull) means the window is being torn down. Drop our
  // reference to the script context, allowing it to be deleted
  // later. Meanwhile, keep our weak reference to the script object
  // (mJSObject) so that it can be retrieved later (until it is
  // finalized by the JS GC).

  if (!aDocShell) {
    NS_ASSERTION(PR_CLIST_IS_EMPTY(&mTimeouts),
                 "Uh, outer window holds timeouts!");

    // Call FreeInnerObjects on all inner windows, not just the current
    // one, since some could be held by WindowStateHolder objects that
    // are GC-owned.
    for (nsRefPtr<nsGlobalWindow> inner = (nsGlobalWindow *)PR_LIST_HEAD(this);
         inner != this;
         inner = (nsGlobalWindow*)PR_NEXT_LINK(inner)) {
      NS_ASSERTION(inner->mOuterWindow == this, "bad outer window pointer");
      inner->FreeInnerObjects(PR_TRUE);
    }

    // Make sure that this is called before we null out the document.
    NotifyDOMWindowDestroyed(this);

    NotifyWindowIDDestroyed("outer-window-destroyed");

    nsGlobalWindow *currentInner = GetCurrentInnerWindowInternal();

    if (currentInner) {
      NS_ASSERTION(mDoc, "Must have doc!");
      
      // Remember the document's principal.
      mDocumentPrincipal = mDoc->NodePrincipal();

      // Release our document reference
      mDocument = nsnull;
      mDoc = nsnull;
    }

    if (mContext) {
      mContext->ClearScope(mJSObject, PR_TRUE);
    }

    ClearControllers();

    mChromeEventHandler = nsnull; // force release now

    if (mArguments) { 
      // We got no new document after someone called
      // SetArguments(), drop our reference to the arguments.
      mArguments = nsnull;
      mArgumentsLast = nsnull;
      mArgumentsOrigin = nsnull;
    }

    if (mContext) {
      mContext->GC();
      mContext->FinalizeContext();
      mContext = nsnull;
    }

#ifdef DEBUG
    nsCycleCollector_DEBUG_shouldBeFreed(mContext);
    nsCycleCollector_DEBUG_shouldBeFreed(static_cast<nsIScriptGlobalObject*>(this));
#endif
  }

  mDocShell = aDocShell;        // Weak Reference

  if (mNavigator)
    mNavigator->SetDocShell(aDocShell);
  if (mHistory)
    mHistory->SetDocShell(aDocShell);
  if (mFrames)
    mFrames->SetDocShell(aDocShell);
  if (mScreen)
    mScreen->SetDocShell(aDocShell);

  // tell our member elements about the new browserwindow
  nsCOMPtr<nsIWebBrowserChrome> browserChrome;
  GetWebBrowserChrome(getter_AddRefs(browserChrome));
  if (mMenubar) {
    mMenubar->SetWebBrowserChrome(browserChrome);
  }
  if (mToolbar) {
    mToolbar->SetWebBrowserChrome(browserChrome);
  }
  if (mLocationbar) {
    mLocationbar->SetWebBrowserChrome(browserChrome);
  }
  if (mPersonalbar) {
    mPersonalbar->SetWebBrowserChrome(browserChrome);
  }
  if (mStatusbar) {
    mStatusbar->SetWebBrowserChrome(browserChrome);
  }
  if (mScrollbars) {
    mScrollbars->SetWebBrowserChrome(browserChrome);
  }

  if (!mDocShell) {
    MaybeForgiveSpamCount();
    CleanUp(PR_FALSE);
  } else {
    // Get our enclosing chrome shell and retrieve its global window impl, so
    // that we can do some forwarding to the chrome document.
    nsCOMPtr<nsIDOMEventTarget> chromeEventHandler;
    mDocShell->GetChromeEventHandler(getter_AddRefs(chromeEventHandler));
    mChromeEventHandler = do_QueryInterface(chromeEventHandler);
    if (!mChromeEventHandler) {
      // We have no chrome event handler. If we have a parent,
      // get our chrome event handler from the parent. If
      // we don't have a parent, then we need to make a new
      // window root object that will function as a chrome event
      // handler and receive all events that occur anywhere inside
      // our window.
      nsCOMPtr<nsIDOMWindow> parentWindow;
      GetParent(getter_AddRefs(parentWindow));
      if (parentWindow.get() != static_cast<nsIDOMWindow*>(this)) {
        nsCOMPtr<nsPIDOMWindow> piWindow(do_QueryInterface(parentWindow));
        mChromeEventHandler = piWindow->GetChromeEventHandler();
      }
      else NS_NewWindowRoot(this, getter_AddRefs(mChromeEventHandler));
    }
  }
}

void
nsGlobalWindow::SetOpenerWindow(nsIDOMWindowInternal* aOpener,
                                PRBool aOriginalOpener)
{
  FORWARD_TO_OUTER_VOID(SetOpenerWindow, (aOpener, aOriginalOpener));

  NS_ASSERTION(!aOriginalOpener || !mSetOpenerWindowCalled,
               "aOriginalOpener is true, but not first call to "
               "SetOpenerWindow!");
  NS_ASSERTION(aOpener || !aOriginalOpener,
               "Shouldn't set mHadOriginalOpener if aOpener is null");

  mOpener = do_GetWeakReference(aOpener);
  NS_ASSERTION(mOpener || !aOpener, "Opener must support weak references!");

  if (aOriginalOpener) {
    mHadOriginalOpener = PR_TRUE;
  }

#ifdef DEBUG
  mSetOpenerWindowCalled = PR_TRUE;
#endif
}

void
nsGlobalWindow::UpdateParentTarget()
{
  nsCOMPtr<nsIFrameLoaderOwner> flo = do_QueryInterface(mChromeEventHandler);
  if (flo) {
    nsRefPtr<nsFrameLoader> fl = flo->GetFrameLoader();
    if (fl) {
      mParentTarget = fl->GetTabChildGlobalAsEventTarget();
    }
  }
  if (!mParentTarget) {
    mParentTarget = mChromeEventHandler;
  }
}

nsresult
nsGlobalWindow::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  NS_PRECONDITION(IsInnerWindow(), "PreHandleEvent is used on outer window!?");
  static PRUint32 count = 0;
  PRUint32 msg = aVisitor.mEvent->message;

  aVisitor.mCanHandle = PR_TRUE;
  aVisitor.mForceContentDispatch = PR_TRUE; //FIXME! Bug 329119
  if ((msg == NS_MOUSE_MOVE) && gEntropyCollector) {
    //Chances are this counter will overflow during the life of the
    //process, but that's OK for our case. Means we get a little
    //more entropy.
    if (count++ % 100 == 0) {
      //Since the high bits seem to be zero's most of the time,
      //let's only take the lowest half of the point structure.
      PRInt16 myCoord[2];

      myCoord[0] = aVisitor.mEvent->refPoint.x;
      myCoord[1] = aVisitor.mEvent->refPoint.y;
      gEntropyCollector->RandomUpdate((void*)myCoord, sizeof(myCoord));
      gEntropyCollector->RandomUpdate((void*)&(aVisitor.mEvent->time),
                                      sizeof(PRUint32));
    }
  } else if (msg == NS_RESIZE_EVENT) {
    mIsHandlingResizeEvent = PR_TRUE;
  } else if (msg == NS_MOUSE_BUTTON_DOWN &&
             NS_IS_TRUSTED_EVENT(aVisitor.mEvent)) {
    gMouseDown = PR_TRUE;
  } else if (msg == NS_MOUSE_BUTTON_UP &&
             NS_IS_TRUSTED_EVENT(aVisitor.mEvent)) {
    gMouseDown = PR_FALSE;
    if (gDragServiceDisabled) {
      nsCOMPtr<nsIDragService> ds =
        do_GetService("@mozilla.org/widget/dragservice;1");
      if (ds) {
        gDragServiceDisabled = PR_FALSE;
        ds->Unsuppress();
      }
    }
  }

  aVisitor.mParentTarget = GetParentTarget();
  return NS_OK;
}

bool
nsGlobalWindow::DialogOpenAttempted()
{
  nsGlobalWindow *topWindow = GetTop();
  if (!topWindow) {
    NS_ERROR("DialogOpenAttempted() called without a top window?");

    return false;
  }

  topWindow = topWindow->GetCurrentInnerWindowInternal();
  if (!topWindow ||
      topWindow->mLastDialogQuitTime.IsNull() ||
      nsContentUtils::IsCallerTrustedForCapability("UniversalXPConnect")) {
    return false;
  }

  TimeDuration dialogDuration(TimeStamp::Now() -
                              topWindow->mLastDialogQuitTime);

  if (dialogDuration.ToSeconds() <
      nsContentUtils::GetIntPref("dom.successive_dialog_time_limit",
                                 SUCCESSIVE_DIALOG_TIME_LIMIT)) {
    topWindow->mDialogAbuseCount++;

    return (topWindow->GetPopupControlState() > openAllowed ||
            topWindow->mDialogAbuseCount > MAX_DIALOG_COUNT);
  }

  topWindow->mDialogAbuseCount = 0;

  return false;
}

bool
nsGlobalWindow::AreDialogsBlocked()
{
  nsGlobalWindow *topWindow = GetTop();
  if (!topWindow) {
    NS_ERROR("AreDialogsBlocked() called without a top window?");

    return true;
  }

  topWindow = topWindow->GetCurrentInnerWindowInternal();

  return !topWindow ||
         (topWindow->mDialogDisabled &&
          (topWindow->GetPopupControlState() > openAllowed ||
           topWindow->mDialogAbuseCount >= MAX_DIALOG_COUNT));
}

bool
nsGlobalWindow::ConfirmDialogAllowed()
{
  NS_ENSURE_TRUE(mDocShell, false);
  nsCOMPtr<nsIPromptService> promptSvc =
    do_GetService("@mozilla.org/embedcomp/prompt-service;1");

  if (!DialogOpenAttempted() || !promptSvc) {
    return true;
  }

  // Reset popup state while opening a modal dialog, and firing events
  // about the dialog, to prevent the current state from being active
  // the whole time a modal dialog is open.
  nsAutoPopupStatePusher popupStatePusher(openAbused, PR_TRUE);

  PRBool disableDialog = PR_FALSE;
  nsXPIDLString label, title;
  nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                     "ScriptDialogLabel", label);
  nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                     "ScriptDialogPreventTitle", title);
  promptSvc->Confirm(this, title.get(), label.get(), &disableDialog);
  if (disableDialog) {
    PreventFurtherDialogs();
    return false;
  }

  return true;
}

void
nsGlobalWindow::PreventFurtherDialogs()
{
  nsGlobalWindow *topWindow = GetTop();
  if (!topWindow) {
    NS_ERROR("PreventFurtherDialogs() called without a top window?");

    return;
  }

  topWindow = topWindow->GetCurrentInnerWindowInternal();

  if (topWindow)
    topWindow->mDialogDisabled = PR_TRUE;
}

nsresult
nsGlobalWindow::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  NS_PRECONDITION(IsInnerWindow(), "PostHandleEvent is used on outer window!?");

  // Return early if there is nothing to do.
  switch (aVisitor.mEvent->message) {
    case NS_RESIZE_EVENT:
    case NS_PAGE_UNLOAD:
    case NS_LOAD:
      break;
    default:
      return NS_OK;
  }

  /* mChromeEventHandler and mContext go dangling in the middle of this
   function under some circumstances (events that destroy the window)
   without this addref. */
  nsCOMPtr<nsPIDOMEventTarget> kungFuDeathGrip1(mChromeEventHandler);
  nsCOMPtr<nsIScriptContext> kungFuDeathGrip2(GetContextInternal());

  if (aVisitor.mEvent->message == NS_RESIZE_EVENT) {
    mIsHandlingResizeEvent = PR_FALSE;
  } else if (aVisitor.mEvent->message == NS_PAGE_UNLOAD &&
             NS_IS_TRUSTED_EVENT(aVisitor.mEvent)) {
    // Execute bindingdetached handlers before we tear ourselves
    // down.
    if (mDocument) {
      NS_ASSERTION(mDoc, "Must have doc");
      mDoc->BindingManager()->ExecuteDetachedHandlers();
    }
    mIsDocumentLoaded = PR_FALSE;
  } else if (aVisitor.mEvent->message == NS_LOAD &&
             NS_IS_TRUSTED_EVENT(aVisitor.mEvent)) {
    // This is page load event since load events don't propagate to |window|.
    // @see nsDocument::PreHandleEvent.
    mIsDocumentLoaded = PR_TRUE;

    nsCOMPtr<nsIContent> content(do_QueryInterface(GetFrameElementInternal()));
    nsCOMPtr<nsIDocShellTreeItem> treeItem =
      do_QueryInterface(GetDocShell());

    PRInt32 itemType = nsIDocShellTreeItem::typeChrome;

    if (treeItem) {
      treeItem->GetItemType(&itemType);
    }

    if (content && GetParentInternal() &&
        itemType != nsIDocShellTreeItem::typeChrome) {
      // If we're not in chrome, or at a chrome boundary, fire the
      // onload event for the frame element.

      nsEventStatus status = nsEventStatus_eIgnore;
      nsEvent event(NS_IS_TRUSTED_EVENT(aVisitor.mEvent), NS_LOAD);
      event.flags |= NS_EVENT_FLAG_CANT_BUBBLE;

      // Most of the time we could get a pres context to pass in here,
      // but not always (i.e. if this window is not shown there won't
      // be a pres context available). Since we're not firing a GUI
      // event we don't need a pres context anyway so we just pass
      // null as the pres context all the time here.

      nsEventDispatcher::Dispatch(content, nsnull, &event, nsnull, &status);
    }
  }

  return NS_OK;
}

nsresult
nsGlobalWindow::DispatchDOMEvent(nsEvent* aEvent,
                                 nsIDOMEvent* aDOMEvent,
                                 nsPresContext* aPresContext,
                                 nsEventStatus* aEventStatus)
{
  return
    nsEventDispatcher::DispatchDOMEvent(static_cast<nsPIDOMWindow*>(this),
                                       aEvent, aDOMEvent, aPresContext,
                                       aEventStatus);
}

void
nsGlobalWindow::OnFinalize(PRUint32 aLangID, void *aObject)
{
  NS_ASSERTION(aLangID == nsIProgrammingLanguage::JAVASCRIPT,
               "We don't support this language ID");

  if (aObject == mJSObject) {
    mJSObject = nsnull;
  }
}

void
nsGlobalWindow::SetScriptsEnabled(PRBool aEnabled, PRBool aFireTimeouts)
{
  FORWARD_TO_INNER_VOID(SetScriptsEnabled, (aEnabled, aFireTimeouts));

  if (aEnabled && aFireTimeouts) {
    // Scripts are enabled (again?) on this context, run timeouts that
    // fired on this context while scripts were disabled.
    void (nsGlobalWindow::*run)() = &nsGlobalWindow::RunTimeout;
    NS_DispatchToCurrentThread(NS_NewRunnableMethod(this, run));
  }
}

nsresult
nsGlobalWindow::SetArguments(nsIArray *aArguments, nsIPrincipal *aOrigin)
{
  FORWARD_TO_OUTER(SetArguments, (aArguments, aOrigin),
                   NS_ERROR_NOT_INITIALIZED);

  // Hold on to the arguments so that we can re-set them once the next
  // document is loaded.
  mArguments = aArguments;
  mArgumentsOrigin = aOrigin;

  nsGlobalWindow *currentInner = GetCurrentInnerWindowInternal();

  if (!mIsModalContentWindow) {
    mArgumentsLast = aArguments;
  } else if (currentInner) {
    // SetArguments() is being called on a modal content window that
    // already has an inner window. This can happen when loading
    // javascript: URIs as modal content dialogs. In this case, we'll
    // set up the dialog window, both inner and outer, before we call
    // SetArguments() on the window, so to deal with that, make sure
    // here that the arguments are propagated to the inner window.

    currentInner->mArguments = aArguments;
    currentInner->mArgumentsOrigin = aOrigin;
  }

  return currentInner ?
    currentInner->DefineArgumentsProperty(aArguments) : NS_OK;
}

nsresult
nsGlobalWindow::DefineArgumentsProperty(nsIArray *aArguments)
{
  JSContext *cx;
  nsIScriptContext *ctx = GetOuterWindowInternal()->mContext;
  NS_ENSURE_TRUE(aArguments && ctx &&
                 (cx = (JSContext *)ctx->GetNativeContext()),
                 NS_ERROR_NOT_INITIALIZED);

  if (mIsModalContentWindow) {
    // Modal content windows don't have an "arguments" property, they
    // have a "dialogArguments" property which is handled
    // separately. See nsCommonWindowSH::NewResolve().

    return NS_OK;
  }

  return GetContextInternal()->SetProperty(mJSObject, "arguments", aArguments);
}

//*****************************************************************************
// nsGlobalWindow::nsIScriptObjectPrincipal
//*****************************************************************************

nsIPrincipal*
nsGlobalWindow::GetPrincipal()
{
  if (mDoc) {
    // If we have a document, get the principal from the document
    return mDoc->NodePrincipal();
  }

  if (mDocumentPrincipal) {
    return mDocumentPrincipal;
  }

  // If we don't have a principal and we don't have a document we
  // ask the parent window for the principal. This can happen when
  // loading a frameset that has a <frame src="javascript:xxx">, in
  // that case the global window is used in JS before we've loaded
  // a document into the window.

  nsCOMPtr<nsIScriptObjectPrincipal> objPrincipal =
    do_QueryInterface(GetParentInternal());

  if (objPrincipal) {
    return objPrincipal->GetPrincipal();
  }

  return nsnull;
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMWindow
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::GetDocument(nsIDOMDocument** aDocument)
{
  // This method *should* forward calls to the outer window, but since
  // there's nothing here that *depends* on anything in the outer
  // (GetDocShell() eliminates that dependency), we won't do that to
  // avoid the extra virtual function call.

  // lazily instantiate an about:blank document if necessary, and if
  // we have what it takes to do so. Note that domdoc here is the same
  // thing as our mDocument, but we don't have to explicitly set the
  // member variable because the docshell has already called
  // SetNewDocument().
  nsIDocShell *docShell;
  if (!mDocument && (docShell = GetDocShell()))
    nsCOMPtr<nsIDOMDocument> domdoc(do_GetInterface(docShell));

  NS_IF_ADDREF(*aDocument = mDocument);

  return NS_OK;
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMWindowInternal
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::GetWindow(nsIDOMWindowInternal** aWindow)
{
  FORWARD_TO_OUTER(GetWindow, (aWindow), NS_ERROR_NOT_INITIALIZED);

  *aWindow = static_cast<nsIDOMWindowInternal *>(this);
  NS_ADDREF(*aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetSelf(nsIDOMWindowInternal** aWindow)
{
  FORWARD_TO_OUTER(GetSelf, (aWindow), NS_ERROR_NOT_INITIALIZED);

  *aWindow = static_cast<nsIDOMWindowInternal *>(this);
  NS_ADDREF(*aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetNavigator(nsIDOMNavigator** aNavigator)
{
  FORWARD_TO_OUTER(GetNavigator, (aNavigator), NS_ERROR_NOT_INITIALIZED);

  *aNavigator = nsnull;

  if (!mNavigator) {
    mNavigator = new nsNavigator(mDocShell);
    if (!mNavigator) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_ADDREF(*aNavigator = mNavigator);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScreen(nsIDOMScreen** aScreen)
{
  FORWARD_TO_OUTER(GetScreen, (aScreen), NS_ERROR_NOT_INITIALIZED);

  *aScreen = nsnull;

  if (!mScreen && mDocShell) {
    mScreen = new nsScreen(mDocShell);
    if (!mScreen) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_IF_ADDREF(*aScreen = mScreen);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetHistory(nsIDOMHistory** aHistory)
{
  FORWARD_TO_OUTER(GetHistory, (aHistory), NS_ERROR_NOT_INITIALIZED);

  *aHistory = nsnull;

  if (!mHistory && mDocShell) {
    mHistory = new nsHistory(mDocShell);
    if (!mHistory) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_IF_ADDREF(*aHistory = mHistory);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetParent(nsIDOMWindow** aParent)
{
  FORWARD_TO_OUTER(GetParent, (aParent), NS_ERROR_NOT_INITIALIZED);

  *aParent = nsnull;
  if (!mDocShell)
    return NS_OK;

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeItem> parent;
  docShellAsItem->GetSameTypeParent(getter_AddRefs(parent));

  if (parent) {
    nsCOMPtr<nsIScriptGlobalObject> globalObject(do_GetInterface(parent));
    NS_ENSURE_SUCCESS(CallQueryInterface(globalObject.get(), aParent),
                      NS_ERROR_FAILURE);
  }
  else {
    *aParent = static_cast<nsIDOMWindowInternal *>(this);
    NS_ADDREF(*aParent);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetTop(nsIDOMWindow** aTop)
{
  FORWARD_TO_OUTER(GetTop, (aTop), NS_ERROR_NOT_INITIALIZED);

  *aTop = nsnull;
  if (mDocShell) {
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
    nsCOMPtr<nsIDocShellTreeItem> root;
    docShellAsItem->GetSameTypeRootTreeItem(getter_AddRefs(root));

    if (root) {
      nsCOMPtr<nsIDOMWindow> top(do_GetInterface(root));
      top.swap(*aTop);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetContent(nsIDOMWindow** aContent)
{
  FORWARD_TO_OUTER(GetContent, (aContent), NS_ERROR_NOT_INITIALIZED);

  *aContent = nsnull;

  nsCOMPtr<nsIDocShellTreeItem> primaryContent;

  if (!nsContentUtils::IsCallerChrome()) {
    // If we're called by non-chrome code, make sure we don't return
    // the primary content window if the calling tab is hidden. In
    // such a case we return the same-type root in the hidden tab,
    // which is "good enough", for now.
    nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(mDocShell));

    if (baseWin) {
      PRBool visible = PR_FALSE;
      baseWin->GetVisibility(&visible);

      if (!visible) {
        nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(mDocShell));

        treeItem->GetSameTypeRootTreeItem(getter_AddRefs(primaryContent));
      }
    }
  }

  if (!primaryContent) {
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    GetTreeOwner(getter_AddRefs(treeOwner));
    NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

    treeOwner->GetPrimaryContentShell(getter_AddRefs(primaryContent));
  }

  nsCOMPtr<nsIDOMWindowInternal> domWindow(do_GetInterface(primaryContent));
  NS_IF_ADDREF(*aContent = domWindow);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetPrompter(nsIPrompt** aPrompt)
{
  FORWARD_TO_OUTER(GetPrompter, (aPrompt), NS_ERROR_NOT_INITIALIZED);

  if (!mDocShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(prompter, NS_ERROR_NO_INTERFACE);

  NS_ADDREF(*aPrompt = prompter);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetMenubar(nsIDOMBarProp** aMenubar)
{
  FORWARD_TO_OUTER(GetMenubar, (aMenubar), NS_ERROR_NOT_INITIALIZED);

  *aMenubar = nsnull;

  if (!mMenubar) {
    mMenubar = new nsMenubarProp();
    if (!mMenubar) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIWebBrowserChrome> browserChrome;
    GetWebBrowserChrome(getter_AddRefs(browserChrome));

    mMenubar->SetWebBrowserChrome(browserChrome);
  }

  NS_ADDREF(*aMenubar = mMenubar);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetToolbar(nsIDOMBarProp** aToolbar)
{
  FORWARD_TO_OUTER(GetToolbar, (aToolbar), NS_ERROR_NOT_INITIALIZED);

  *aToolbar = nsnull;

  if (!mToolbar) {
    mToolbar = new nsToolbarProp();
    if (!mToolbar) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIWebBrowserChrome> browserChrome;
    GetWebBrowserChrome(getter_AddRefs(browserChrome));

    mToolbar->SetWebBrowserChrome(browserChrome);
  }

  NS_ADDREF(*aToolbar = mToolbar);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetLocationbar(nsIDOMBarProp** aLocationbar)
{
  FORWARD_TO_OUTER(GetLocationbar, (aLocationbar), NS_ERROR_NOT_INITIALIZED);

  *aLocationbar = nsnull;

  if (!mLocationbar) {
    mLocationbar = new nsLocationbarProp();
    if (!mLocationbar) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIWebBrowserChrome> browserChrome;
    GetWebBrowserChrome(getter_AddRefs(browserChrome));

    mLocationbar->SetWebBrowserChrome(browserChrome);
  }

  NS_ADDREF(*aLocationbar = mLocationbar);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetPersonalbar(nsIDOMBarProp** aPersonalbar)
{
  FORWARD_TO_OUTER(GetPersonalbar, (aPersonalbar), NS_ERROR_NOT_INITIALIZED);

  *aPersonalbar = nsnull;

  if (!mPersonalbar) {
    mPersonalbar = new nsPersonalbarProp();
    if (!mPersonalbar) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIWebBrowserChrome> browserChrome;
    GetWebBrowserChrome(getter_AddRefs(browserChrome));

    mPersonalbar->SetWebBrowserChrome(browserChrome);
  }

  NS_ADDREF(*aPersonalbar = mPersonalbar);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetStatusbar(nsIDOMBarProp** aStatusbar)
{
  FORWARD_TO_OUTER(GetStatusbar, (aStatusbar), NS_ERROR_NOT_INITIALIZED);

  *aStatusbar = nsnull;

  if (!mStatusbar) {
    mStatusbar = new nsStatusbarProp();
    if (!mStatusbar) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIWebBrowserChrome> browserChrome;
    GetWebBrowserChrome(getter_AddRefs(browserChrome));

    mStatusbar->SetWebBrowserChrome(browserChrome);
  }

  NS_ADDREF(*aStatusbar = mStatusbar);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollbars(nsIDOMBarProp** aScrollbars)
{
  FORWARD_TO_OUTER(GetScrollbars, (aScrollbars), NS_ERROR_NOT_INITIALIZED);

  *aScrollbars = nsnull;

  if (!mScrollbars) {
    mScrollbars = new nsScrollbarsProp(this);
    if (!mScrollbars) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIWebBrowserChrome> browserChrome;
    GetWebBrowserChrome(getter_AddRefs(browserChrome));

    mScrollbars->SetWebBrowserChrome(browserChrome);
  }

  NS_ADDREF(*aScrollbars = mScrollbars);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetClosed(PRBool* aClosed)
{
  FORWARD_TO_OUTER(GetClosed, (aClosed), NS_ERROR_NOT_INITIALIZED);

  // If someone called close(), or if we don't have a docshell, we're
  // closed.
  *aClosed = mIsClosed || !mDocShell;

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetFrames(nsIDOMWindowCollection** aFrames)
{
  FORWARD_TO_OUTER(GetFrames, (aFrames), NS_ERROR_NOT_INITIALIZED);

  *aFrames = nsnull;

  if (!mFrames && mDocShell) {
    mFrames = new nsDOMWindowList(mDocShell);
    if (!mFrames) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  *aFrames = static_cast<nsIDOMWindowCollection *>(mFrames);
  NS_IF_ADDREF(*aFrames);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetApplicationCache(nsIDOMOfflineResourceList **aApplicationCache)
{
  FORWARD_TO_INNER(GetApplicationCache, (aApplicationCache), NS_ERROR_UNEXPECTED);

  NS_ENSURE_ARG_POINTER(aApplicationCache);

  if (!mApplicationCache) {
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetDocShell()));
    if (!webNav) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIURI> uri;
    nsresult rv = webNav->GetCurrentURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
    nsCOMPtr<nsIURI> manifestURI;
    nsContentUtils::GetOfflineAppManifest(doc, getter_AddRefs(manifestURI));

    nsIScriptContext* scriptContext = GetContext();
    NS_ENSURE_STATE(scriptContext);

    nsRefPtr<nsDOMOfflineResourceList> applicationCache =
      new nsDOMOfflineResourceList(manifestURI, uri, this, scriptContext);
    NS_ENSURE_TRUE(applicationCache, NS_ERROR_OUT_OF_MEMORY);

    applicationCache->Init();

    mApplicationCache = applicationCache;
  }

  NS_IF_ADDREF(*aApplicationCache = mApplicationCache);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::CreateBlobURL(nsIDOMBlob* aBlob, nsAString& aURL)
{
  FORWARD_TO_INNER(CreateBlobURL, (aBlob, aURL), NS_ERROR_UNEXPECTED);

  NS_ENSURE_STATE(mDoc);

  nsresult rv = aBlob->GetInternalUrl(mDoc->NodePrincipal(), aURL);
  NS_ENSURE_SUCCESS(rv, rv);

  mDoc->RegisterFileDataUri(NS_LossyConvertUTF16toASCII(aURL));

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::RevokeBlobURL(const nsAString& aURL)
{
  FORWARD_TO_INNER(RevokeBlobURL, (aURL), NS_ERROR_UNEXPECTED);

  NS_ENSURE_STATE(mDoc);

  NS_LossyConvertUTF16toASCII asciiurl(aURL);
  mDoc->UnregisterFileDataUri(asciiurl);
  nsFileDataProtocolHandler::RemoveFileDataEntry(asciiurl);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetCrypto(nsIDOMCrypto** aCrypto)
{
#ifdef MOZ_DISABLE_DOMCRYPTO
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  FORWARD_TO_OUTER(GetCrypto, (aCrypto), NS_ERROR_NOT_INITIALIZED);

  if (!mCrypto) {
    mCrypto = do_CreateInstance(kCryptoContractID);
  }

  NS_IF_ADDREF(*aCrypto = mCrypto);

  return NS_OK;
#endif
}

NS_IMETHODIMP
nsGlobalWindow::GetPkcs11(nsIDOMPkcs11** aPkcs11)
{
  *aPkcs11 = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetControllers(nsIControllers** aResult)
{
  FORWARD_TO_OUTER(GetControllers, (aResult), NS_ERROR_NOT_INITIALIZED);

  if (!mControllers) {
    nsresult rv;
    mControllers = do_CreateInstance(kXULControllersCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Add in the default controller
    nsCOMPtr<nsIController> controller = do_CreateInstance(
                               NS_WINDOWCONTROLLER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    mControllers->InsertControllerAt(0, controller);
    nsCOMPtr<nsIControllerContext> controllerContext = do_QueryInterface(controller);
    if (!controllerContext) return NS_ERROR_FAILURE;

    controllerContext->SetCommandContext(static_cast<nsIDOMWindow*>(this));
  }

  *aResult = mControllers;
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetOpener(nsIDOMWindowInternal** aOpener)
{
  FORWARD_TO_OUTER(GetOpener, (aOpener), NS_ERROR_NOT_INITIALIZED);

  *aOpener = nsnull;

  nsCOMPtr<nsPIDOMWindow> opener = do_QueryReferent(mOpener);
  if (!opener) {
    return NS_OK;
  }

  // First, check if we were called from a privileged chrome script
  if (nsContentUtils::IsCallerTrustedForRead()) {
    NS_ADDREF(*aOpener = opener);
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindow> openerPwin(do_QueryInterface(opener));
  if (!openerPwin) {
    return NS_OK;
  }

  // First, ensure that we're not handing back a chrome window.
  nsGlobalWindow *win = static_cast<nsGlobalWindow *>(openerPwin.get());
  if (win->IsChromeWindow()) {
    return NS_OK;
  }

  // We don't want to reveal the opener if the opener is a mail window,
  // because opener can be used to spoof the contents of a message (bug 105050).
  // So, we look in the opener's root docshell to see if it's a mail window.
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
    do_QueryInterface(openerPwin->GetDocShell());

  if (docShellAsItem) {
    nsCOMPtr<nsIDocShellTreeItem> openerRootItem;
    docShellAsItem->GetRootTreeItem(getter_AddRefs(openerRootItem));
    nsCOMPtr<nsIDocShell> openerRootDocShell(do_QueryInterface(openerRootItem));
    if (openerRootDocShell) {
      PRUint32 appType;
      nsresult rv = openerRootDocShell->GetAppType(&appType);
      if (NS_SUCCEEDED(rv) && appType != nsIDocShell::APP_TYPE_MAIL) {
        *aOpener = opener;
      }
    }
  }

  NS_IF_ADDREF(*aOpener);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetOpener(nsIDOMWindowInternal* aOpener)
{
  // check if we were called from a privileged chrome script.
  // If not, opener is settable only to null.
  if (aOpener && !nsContentUtils::IsCallerTrustedForWrite()) {
    return NS_OK;
  }

  SetOpenerWindow(aOpener, PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetStatus(nsAString& aStatus)
{
  FORWARD_TO_OUTER(GetStatus, (aStatus), NS_ERROR_NOT_INITIALIZED);

  aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetStatus(const nsAString& aStatus)
{
  FORWARD_TO_OUTER(SetStatus, (aStatus), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and dom.disable_window_status_change is true,
   * prevent setting window.status by exiting early
   */

  if (!CanSetProperty("dom.disable_window_status_change")) {
    return NS_OK;
  }

  mStatus = aStatus;

  nsCOMPtr<nsIWebBrowserChrome> browserChrome;
  GetWebBrowserChrome(getter_AddRefs(browserChrome));
  if(browserChrome) {
    browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_SCRIPT,
                             PromiseFlatString(aStatus).get());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetDefaultStatus(nsAString& aDefaultStatus)
{
  FORWARD_TO_OUTER(GetDefaultStatus, (aDefaultStatus),
                   NS_ERROR_NOT_INITIALIZED);

  aDefaultStatus = mDefaultStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetDefaultStatus(const nsAString& aDefaultStatus)
{
  FORWARD_TO_OUTER(SetDefaultStatus, (aDefaultStatus),
                   NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and dom.disable_window_status_change is true,
   * prevent setting window.defaultStatus by exiting early
   */

  if (!CanSetProperty("dom.disable_window_status_change")) {
    return NS_OK;
  }

  mDefaultStatus = aDefaultStatus;

  nsCOMPtr<nsIWebBrowserChrome> browserChrome;
  GetWebBrowserChrome(getter_AddRefs(browserChrome));
  if (browserChrome) {
    browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_SCRIPT_DEFAULT,
                             PromiseFlatString(aDefaultStatus).get());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetName(nsAString& aName)
{
  FORWARD_TO_OUTER(GetName, (aName), NS_ERROR_NOT_INITIALIZED);

  nsXPIDLString name;
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  if (docShellAsItem)
    docShellAsItem->GetName(getter_Copies(name));

  aName.Assign(name);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetName(const nsAString& aName)
{
  FORWARD_TO_OUTER(SetName, (aName), NS_ERROR_NOT_INITIALIZED);

  nsresult result = NS_OK;
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  if (docShellAsItem)
    result = docShellAsItem->SetName(PromiseFlatString(aName).get());
  return result;
}

// Helper functions used by many methods below.
PRInt32
nsGlobalWindow::DevToCSSIntPixels(PRInt32 px)
{
  if (!mDocShell)
    return px; // assume 1:1

  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return px;

  return presContext->DevPixelsToIntCSSPixels(px);
}

PRInt32
nsGlobalWindow::CSSToDevIntPixels(PRInt32 px)
{
  if (!mDocShell)
    return px; // assume 1:1

  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return px;

  return presContext->CSSPixelsToDevPixels(px);
}

nsIntSize
nsGlobalWindow::DevToCSSIntPixels(nsIntSize px)
{
  if (!mDocShell)
    return px; // assume 1:1

  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return px;

  return nsIntSize(
      presContext->DevPixelsToIntCSSPixels(px.width),
      presContext->DevPixelsToIntCSSPixels(px.height));
}

nsIntSize
nsGlobalWindow::CSSToDevIntPixels(nsIntSize px)
{
  if (!mDocShell)
    return px; // assume 1:1

  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return px;

  return nsIntSize(
    presContext->CSSPixelsToDevPixels(px.width),
    presContext->CSSPixelsToDevPixels(px.height));
}


NS_IMETHODIMP
nsGlobalWindow::GetInnerWidth(PRInt32* aInnerWidth)
{
  FORWARD_TO_OUTER(GetInnerWidth, (aInnerWidth), NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_STATE(mDocShell);

  EnsureSizeUpToDate();

  nsCOMPtr<nsIBaseWindow> docShellWin(do_QueryInterface(mDocShell));
  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));

  if (docShellWin && presContext) {
    PRInt32 width, notused;
    docShellWin->GetSize(&width, &notused);
    *aInnerWidth = nsPresContext::
      AppUnitsToIntCSSPixels(presContext->DevPixelsToAppUnits(width));
  } else {
    *aInnerWidth = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetInnerWidth(PRInt32 aInnerWidth)
{
  FORWARD_TO_OUTER(SetInnerWidth, (aInnerWidth), NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_STATE(mDocShell);

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent setting window.innerWidth by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(&aInnerWidth, nsnull),
                    NS_ERROR_FAILURE);

  PRInt32 width = CSSToDevIntPixels(aInnerWidth);

  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
  PRInt32 notused, height = 0;
  docShellAsWin->GetSize(&notused, &height);

  NS_ENSURE_SUCCESS(treeOwner->SizeShellTo(docShellAsItem, width, height),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetInnerHeight(PRInt32* aInnerHeight)
{
  FORWARD_TO_OUTER(GetInnerHeight, (aInnerHeight), NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_STATE(mDocShell);

  EnsureSizeUpToDate();

  nsCOMPtr<nsIBaseWindow> docShellWin(do_QueryInterface(mDocShell));
  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));

  if (docShellWin && presContext) {
    PRInt32 height, notused;
    docShellWin->GetSize(&notused, &height);
    *aInnerHeight = nsPresContext::
      AppUnitsToIntCSSPixels(presContext->DevPixelsToAppUnits(height));
  } else {
    *aInnerHeight = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetInnerHeight(PRInt32 aInnerHeight)
{
  FORWARD_TO_OUTER(SetInnerHeight, (aInnerHeight), NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_STATE(mDocShell);

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent setting window.innerHeight by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(nsnull, &aInnerHeight),
                    NS_ERROR_FAILURE);

  PRInt32 height = CSSToDevIntPixels(aInnerHeight);

  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
  PRInt32 width = 0, notused;
  docShellAsWin->GetSize(&width, &notused);

  NS_ENSURE_SUCCESS(treeOwner->SizeShellTo(docShellAsItem, width, height),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult
nsGlobalWindow::GetOuterSize(nsIntSize* aSizeCSSPixels)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  nsGlobalWindow* rootWindow =
    static_cast<nsGlobalWindow *>(GetPrivateRoot());
  if (rootWindow) {
    rootWindow->FlushPendingNotifications(Flush_Layout);
  }

  nsIntSize sizeDevPixels;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(&sizeDevPixels.width,
                                            &sizeDevPixels.height),
                    NS_ERROR_FAILURE);

  *aSizeCSSPixels = DevToCSSIntPixels(sizeDevPixels);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetOuterWidth(PRInt32* aOuterWidth)
{
  FORWARD_TO_OUTER(GetOuterWidth, (aOuterWidth), NS_ERROR_NOT_INITIALIZED);

  nsIntSize sizeCSSPixels;
  nsresult rv = GetOuterSize(&sizeCSSPixels);
  NS_ENSURE_SUCCESS(rv, rv);

  *aOuterWidth = sizeCSSPixels.width;
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetOuterHeight(PRInt32* aOuterHeight)
{
  FORWARD_TO_OUTER(GetOuterHeight, (aOuterHeight), NS_ERROR_NOT_INITIALIZED);

  nsIntSize sizeCSSPixels;
  nsresult rv = GetOuterSize(&sizeCSSPixels);
  NS_ENSURE_SUCCESS(rv, rv);

  *aOuterHeight = sizeCSSPixels.height;
  return NS_OK;
}

nsresult
nsGlobalWindow::SetOuterSize(PRInt32 aLengthCSSPixels, PRBool aIsWidth)
{
  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent setting window.outerWidth by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(
                        aIsWidth ? &aLengthCSSPixels : nsnull,
                        aIsWidth ? nsnull : &aLengthCSSPixels),
                    NS_ERROR_FAILURE);

  PRInt32 width, height;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(&width, &height), NS_ERROR_FAILURE);

  PRInt32 lengthDevPixels = CSSToDevIntPixels(aLengthCSSPixels);
  if (aIsWidth) {
    width = lengthDevPixels;
  } else {
    height = lengthDevPixels;
  }
  return treeOwnerAsWin->SetSize(width, height, PR_TRUE);    
}

NS_IMETHODIMP
nsGlobalWindow::SetOuterWidth(PRInt32 aOuterWidth)
{
  FORWARD_TO_OUTER(SetOuterWidth, (aOuterWidth), NS_ERROR_NOT_INITIALIZED);

  return SetOuterSize(aOuterWidth, PR_TRUE);
}

NS_IMETHODIMP
nsGlobalWindow::SetOuterHeight(PRInt32 aOuterHeight)
{
  FORWARD_TO_OUTER(SetOuterHeight, (aOuterHeight), NS_ERROR_NOT_INITIALIZED);

  return SetOuterSize(aOuterHeight, PR_FALSE);
}

NS_IMETHODIMP
nsGlobalWindow::GetScreenX(PRInt32* aScreenX)
{
  FORWARD_TO_OUTER(GetScreenX, (aScreenX), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  PRInt32 x, y;

  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, &y),
                    NS_ERROR_FAILURE);

  *aScreenX = DevToCSSIntPixels(x);
  return NS_OK;
}

nsRect
nsGlobalWindow::GetInnerScreenRect()
{
  if (!mDocShell)
    return nsRect();

  nsGlobalWindow* rootWindow =
    static_cast<nsGlobalWindow*>(GetPrivateRoot());
  if (rootWindow) {
    rootWindow->FlushPendingNotifications(Flush_Layout);
  }

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));
  if (!presShell)
    return nsRect();
  nsIFrame* rootFrame = presShell->GetRootFrame();
  if (!rootFrame)
    return nsRect();

  return rootFrame->GetScreenRectInAppUnits();
}

NS_IMETHODIMP
nsGlobalWindow::GetMozInnerScreenX(float* aScreenX)
{
  FORWARD_TO_OUTER(GetMozInnerScreenX, (aScreenX), NS_ERROR_NOT_INITIALIZED);

  nsRect r = GetInnerScreenRect();
  *aScreenX = nsPresContext::AppUnitsToFloatCSSPixels(r.x);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetMozInnerScreenY(float* aScreenY)
{
  FORWARD_TO_OUTER(GetMozInnerScreenY, (aScreenY), NS_ERROR_NOT_INITIALIZED);

  nsRect r = GetInnerScreenRect();
  *aScreenY = nsPresContext::AppUnitsToFloatCSSPixels(r.y);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetMozPaintCount(PRUint64* aResult)
{
  FORWARD_TO_OUTER(GetMozPaintCount, (aResult), NS_ERROR_NOT_INITIALIZED);

  *aResult = 0;

  if (!mDocShell)
    return NS_OK;

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));
  if (!presShell)
    return NS_OK;

  *aResult = presShell->GetPaintCount();
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::MozRequestAnimationFrame(nsIAnimationFrameListener* aListener)
{
  FORWARD_TO_INNER(MozRequestAnimationFrame, (aListener),
                   NS_ERROR_NOT_INITIALIZED);

  if (!mDoc) {
    return NS_OK;
  }

  mDoc->ScheduleBeforePaintEvent(aListener);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetMozAnimationStartTime(PRInt64 *aTime)
{
  FORWARD_TO_INNER(GetMozAnimationStartTime, (aTime), NS_ERROR_NOT_INITIALIZED);

  if (mDoc) {
    nsIPresShell* presShell = mDoc->GetShell();
    if (presShell) {
      *aTime = presShell->GetPresContext()->RefreshDriver()->
        MostRecentRefreshEpochTime() / PR_USEC_PER_MSEC;
      return NS_OK;
    }
  }

  // If all else fails, just be compatible with Date.now()
  *aTime = JS_Now() / PR_USEC_PER_MSEC;
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetScreenX(PRInt32 aScreenX)
{
  FORWARD_TO_OUTER(SetScreenX, (aScreenX), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent setting window.screenX by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityLeftAndTop(&aScreenX, nsnull),
                    NS_ERROR_FAILURE);

  PRInt32 x, y;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, &y),
                    NS_ERROR_FAILURE);

  x = CSSToDevIntPixels(aScreenX);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetPosition(x, y),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScreenY(PRInt32* aScreenY)
{
  FORWARD_TO_OUTER(GetScreenY, (aScreenY), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  PRInt32 x, y;

  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, &y),
                    NS_ERROR_FAILURE);

  *aScreenY = DevToCSSIntPixels(y);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetScreenY(PRInt32 aScreenY)
{
  FORWARD_TO_OUTER(SetScreenY, (aScreenY), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent setting window.screenY by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityLeftAndTop(nsnull, &aScreenY),
                    NS_ERROR_FAILURE);

  PRInt32 x, y;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, &y),
                    NS_ERROR_FAILURE);

  y = CSSToDevIntPixels(aScreenY);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetPosition(x, y),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

// NOTE: Arguments to this function should have values scaled to
// CSS pixels, not device pixels.
nsresult
nsGlobalWindow::CheckSecurityWidthAndHeight(PRInt32* aWidth, PRInt32* aHeight)
{
#ifdef MOZ_XUL
  if (!nsContentUtils::IsCallerTrustedForWrite()) {
    // if attempting to resize the window, hide any open popups
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
    nsContentUtils::HidePopupsInDocument(doc);
  }
#endif

  // This one is easy. Just ensure the variable is greater than 100;
  if ((aWidth && *aWidth < 100) || (aHeight && *aHeight < 100)) {
    // Check security state for use in determing window dimensions

    if (!nsContentUtils::IsCallerTrustedForWrite()) {
      //sec check failed
      if (aWidth && *aWidth < 100) {
        *aWidth = 100;
      }
      if (aHeight && *aHeight < 100) {
        *aHeight = 100;
      }
    }
  }

  return NS_OK;
}

// NOTE: Arguments to this function should have values scaled to
// CSS pixels, not device pixels.
nsresult
nsGlobalWindow::CheckSecurityLeftAndTop(PRInt32* aLeft, PRInt32* aTop)
{
  // This one is harder. We have to get the screen size and window dimensions.

  // Check security state for use in determing window dimensions

  if (!nsContentUtils::IsCallerTrustedForWrite()) {
#ifdef MOZ_XUL
    // if attempting to move the window, hide any open popups
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
    nsContentUtils::HidePopupsInDocument(doc);
#endif

    nsGlobalWindow* rootWindow =
      static_cast<nsGlobalWindow*>(GetPrivateRoot());
    if (rootWindow) {
      rootWindow->FlushPendingNotifications(Flush_Layout);
    }

    nsCOMPtr<nsIBaseWindow> treeOwner;
    GetTreeOwner(getter_AddRefs(treeOwner));

    nsCOMPtr<nsIDOMScreen> screen;
    GetScreen(getter_AddRefs(screen));

    if (treeOwner && screen) {
      PRInt32 screenLeft, screenTop, screenWidth, screenHeight;
      PRInt32 winLeft, winTop, winWidth, winHeight;

      // Get the window size
      treeOwner->GetPositionAndSize(&winLeft, &winTop, &winWidth, &winHeight);

      // convert those values to CSS pixels
      // XXX four separate retrievals of the prescontext
      winLeft   = DevToCSSIntPixels(winLeft);
      winTop    = DevToCSSIntPixels(winTop);
      winWidth  = DevToCSSIntPixels(winWidth);
      winHeight = DevToCSSIntPixels(winHeight);

      // Get the screen dimensions
      // XXX This should use nsIScreenManager once it's fully fleshed out.
      screen->GetAvailLeft(&screenLeft);
      screen->GetAvailWidth(&screenWidth);
      screen->GetAvailHeight(&screenHeight);
#if defined(XP_MAC) || defined(XP_MACOSX)
      /* The mac's coordinate system is different from the assumed Windows'
         system. It offsets by the height of the menubar so that a window
         placed at (0,0) will be entirely visible. Unfortunately that
         correction is made elsewhere (in Widget) and the meaning of
         the Avail... coordinates is overloaded. Here we allow a window
         to be placed at (0,0) because it does make sense to do so.
      */
      screen->GetTop(&screenTop);
#else
      screen->GetAvailTop(&screenTop);
#endif

      if (aLeft) {
        if (screenLeft+screenWidth < *aLeft+winWidth)
          *aLeft = screenLeft+screenWidth - winWidth;
        if (screenLeft > *aLeft)
          *aLeft = screenLeft;
      }
      if (aTop) {
        if (screenTop+screenHeight < *aTop+winHeight)
          *aTop = screenTop+screenHeight - winHeight;
        if (screenTop > *aTop)
          *aTop = screenTop;
      }
    } else {
      if (aLeft)
        *aLeft = 0;
      if (aTop)
        *aTop = 0;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetPageXOffset(PRInt32* aPageXOffset)
{
  return GetScrollX(aPageXOffset);
}

NS_IMETHODIMP
nsGlobalWindow::GetPageYOffset(PRInt32* aPageYOffset)
{
  return GetScrollY(aPageYOffset);
}

nsresult
nsGlobalWindow::GetScrollMaxXY(PRInt32* aScrollMaxX, PRInt32* aScrollMaxY)
{
  FORWARD_TO_OUTER(GetScrollMaxXY, (aScrollMaxX, aScrollMaxY),
                   NS_ERROR_NOT_INITIALIZED);

  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();
  if (!sf)
    return NS_OK;

  nsRect scrollRange = sf->GetScrollRange();

  if (aScrollMaxX)
    *aScrollMaxX = NS_MAX(0,
      (PRInt32)floor(nsPresContext::AppUnitsToFloatCSSPixels(scrollRange.XMost())));
  if (aScrollMaxY)
    *aScrollMaxY = NS_MAX(0,
      (PRInt32)floor(nsPresContext::AppUnitsToFloatCSSPixels(scrollRange.YMost())));

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollMaxX(PRInt32* aScrollMaxX)
{
  NS_ENSURE_ARG_POINTER(aScrollMaxX);
  *aScrollMaxX = 0;
  return GetScrollMaxXY(aScrollMaxX, nsnull);
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollMaxY(PRInt32* aScrollMaxY)
{
  NS_ENSURE_ARG_POINTER(aScrollMaxY);
  *aScrollMaxY = 0;
  return GetScrollMaxXY(nsnull, aScrollMaxY);
}

nsresult
nsGlobalWindow::GetScrollXY(PRInt32* aScrollX, PRInt32* aScrollY,
                            PRBool aDoFlush)
{
  FORWARD_TO_OUTER(GetScrollXY, (aScrollX, aScrollY, aDoFlush),
                   NS_ERROR_NOT_INITIALIZED);

  if (aDoFlush) {
    FlushPendingNotifications(Flush_Layout);
  } else {
    EnsureSizeUpToDate();
  }

  nsIScrollableFrame *sf = GetScrollFrame();
  if (!sf)
    return NS_OK;

  nsPoint scrollPos = sf->GetScrollPosition();
  if (scrollPos != nsPoint(0,0) && !aDoFlush) {
    // Oh, well.  This is the expensive case -- the window is scrolled and we
    // didn't actually flush yet.  Repeat, but with a flush, since the content
    // may get shorter and hence our scroll position may decrease.
    return GetScrollXY(aScrollX, aScrollY, PR_TRUE);
  }

  if (aScrollX)
    *aScrollX = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.x);
  if (aScrollY)
    *aScrollY = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.y);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollX(PRInt32* aScrollX)
{
  NS_ENSURE_ARG_POINTER(aScrollX);
  *aScrollX = 0;
  return GetScrollXY(aScrollX, nsnull, PR_FALSE);
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollY(PRInt32* aScrollY)
{
  NS_ENSURE_ARG_POINTER(aScrollY);
  *aScrollY = 0;
  return GetScrollXY(nsnull, aScrollY, PR_FALSE);
}

NS_IMETHODIMP
nsGlobalWindow::GetLength(PRUint32* aLength)
{
  nsCOMPtr<nsIDOMWindowCollection> frames;
  if (NS_SUCCEEDED(GetFrames(getter_AddRefs(frames))) && frames) {
    return frames->GetLength(aLength);
  }
  return NS_ERROR_FAILURE;
}

PRBool
nsGlobalWindow::DispatchCustomEvent(const char *aEventName)
{
  PRBool defaultActionEnabled = PR_TRUE;
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
  nsContentUtils::DispatchTrustedEvent(doc,
                                       static_cast<nsIScriptGlobalObject*>(this),
                                       NS_ConvertASCIItoUTF16(aEventName),
                                       PR_TRUE, PR_TRUE, &defaultActionEnabled);

  return defaultActionEnabled;
}

static already_AddRefed<nsIDocShellTreeItem>
GetCallerDocShellTreeItem()
{
  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  nsIDocShellTreeItem *callerItem = nsnull;

  if (cx) {
    nsCOMPtr<nsIWebNavigation> callerWebNav =
      do_GetInterface(nsJSUtils::GetDynamicScriptGlobal(cx));

    if (callerWebNav) {
      CallQueryInterface(callerWebNav, &callerItem);
    }
  }

  return callerItem;
}

PRBool
nsGlobalWindow::WindowExists(const nsAString& aName,
                             PRBool aLookForCallerOnJSStack)
{
  NS_PRECONDITION(IsOuterWindow(), "Must be outer window");
  NS_PRECONDITION(mDocShell, "Must have docshell");

  nsCOMPtr<nsIDocShellTreeItem> caller;
  if (aLookForCallerOnJSStack) {
    caller = GetCallerDocShellTreeItem();
  }

  nsCOMPtr<nsIDocShellTreeItem> docShell = do_QueryInterface(mDocShell);
  NS_ASSERTION(docShell,
               "Docshell doesn't implement nsIDocShellTreeItem?");

  if (!caller) {
    caller = docShell;
  }

  nsCOMPtr<nsIDocShellTreeItem> namedItem;
  docShell->FindItemWithName(PromiseFlatString(aName).get(), nsnull, caller,
                             getter_AddRefs(namedItem));
  return namedItem != nsnull;
}

already_AddRefed<nsIWidget>
nsGlobalWindow::GetMainWidget()
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));

  nsIWidget *widget = nsnull;

  if (treeOwnerAsWin) {
    treeOwnerAsWin->GetMainWidget(&widget);
  }

  return widget;
}

nsIWidget*
nsGlobalWindow::GetNearestWidget()
{
  nsIDocShell* docShell = GetDocShell();
  NS_ENSURE_TRUE(docShell, nsnull);
  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
  NS_ENSURE_TRUE(presShell, nsnull);
  nsIFrame* rootFrame = presShell->GetRootFrame();
  NS_ENSURE_TRUE(rootFrame, nsnull);
  return rootFrame->GetView()->GetNearestWidget(nsnull);
}

NS_IMETHODIMP
nsGlobalWindow::SetFullScreen(PRBool aFullScreen)
{
  FORWARD_TO_OUTER(SetFullScreen, (aFullScreen), NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  PRBool rootWinFullScreen;
  GetFullScreen(&rootWinFullScreen);
  // Only chrome can change our fullScreen mode.
  if (aFullScreen == rootWinFullScreen || 
      !nsContentUtils::IsCallerTrustedForWrite()) {
    return NS_OK;
  }

  // SetFullScreen needs to be called on the root window, so get that
  // via the DocShell tree, and if we are not already the root,
  // call SetFullScreen on that window instead.
  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(mDocShell);
  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  treeItem->GetRootTreeItem(getter_AddRefs(rootItem));
  nsCOMPtr<nsIDOMWindowInternal> window = do_GetInterface(rootItem);
  if (!window)
    return NS_ERROR_FAILURE;
  if (rootItem != treeItem)
    return window->SetFullScreen(aFullScreen);

  // make sure we don't try to set full screen on a non-chrome window,
  // which might happen in embedding world
  PRInt32 itemType;
  treeItem->GetItemType(&itemType);
  if (itemType != nsIDocShellTreeItem::typeChrome)
    return NS_ERROR_FAILURE;

  // If we are already in full screen mode, just return.
  if (mFullScreen == aFullScreen)
    return NS_OK;

  // dispatch a "fullscreen" DOM event so that XUL apps can
  // respond visually if we are kicked into full screen mode
  if (!DispatchCustomEvent("fullscreen")) {
    return NS_OK;
  }

  // Prevent chrome documents which are still loading from resizing
  // the window after we set fullscreen mode.
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  nsCOMPtr<nsIXULWindow> xulWin(do_GetInterface(treeOwnerAsWin));
  if (aFullScreen && xulWin) {
    xulWin->SetIntrinsicallySized(PR_FALSE);
  }

  // Set this before so if widget sends an event indicating its
  // gone full screen, the state trap above works.
  mFullScreen = aFullScreen;

  nsCOMPtr<nsIWidget> widget = GetMainWidget();
  if (widget)
    widget->MakeFullScreen(aFullScreen);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetFullScreen(PRBool* aFullScreen)
{
  FORWARD_TO_OUTER(GetFullScreen, (aFullScreen), NS_ERROR_NOT_INITIALIZED);

  // Get the fullscreen value of the root window, to always have the value
  // accurate, even when called from content.
  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(mDocShell);
  if (treeItem) {
    nsCOMPtr<nsIDocShellTreeItem> rootItem;
    treeItem->GetRootTreeItem(getter_AddRefs(rootItem));
    if (rootItem != treeItem) {
      nsCOMPtr<nsIDOMWindowInternal> window = do_GetInterface(rootItem);
      if (window)
        return window->GetFullScreen(aFullScreen);
    }
  }

  // We are the root window, or something went wrong. Return our internal value.
  *aFullScreen = mFullScreen;
  return NS_OK;
}

PRBool
nsGlobalWindow::DOMWindowDumpEnabled()
{
#if !(defined(NS_DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
  // In optimized builds we check a pref that controls if we should
  // enable output from dump() or not, in debug builds it's always
  // enabled.
  return gDOMWindowDumpEnabled;
#else
  return PR_TRUE;
#endif
}

NS_IMETHODIMP
nsGlobalWindow::Dump(const nsAString& aStr)
{
  if (!DOMWindowDumpEnabled()) {
    return NS_OK;
  }

  char *cstr = ToNewUTF8String(aStr);

#if defined(XP_MAC) || defined(XP_MACOSX)
  // have to convert \r to \n so that printing to the console works
  char *c = cstr, *cEnd = cstr + strlen(cstr);
  while (c < cEnd) {
    if (*c == '\r')
      *c = '\n';
    c++;
  }
#endif

  if (cstr) {
    FILE *fp = gDumpFile ? gDumpFile : stdout;
    fputs(cstr, fp);
    fflush(fp);
    nsMemory::Free(cstr);
  }

  return NS_OK;
}

void
nsGlobalWindow::EnsureReflowFlushAndPaint()
{
  NS_ASSERTION(mDocShell, "EnsureReflowFlushAndPaint() called with no "
               "docshell!");

  if (!mDocShell)
    return;

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));

  if (!presShell)
    return;

  // Flush pending reflows.
  if (mDoc) {
    mDoc->FlushPendingNotifications(Flush_Layout);
  }

  // Unsuppress painting.
  presShell->UnsuppressPainting();
}

NS_IMETHODIMP
nsGlobalWindow::GetTextZoom(float *aZoom)
{
  FORWARD_TO_OUTER(GetTextZoom, (aZoom), NS_ERROR_NOT_INITIALIZED);

  if (mDocShell) {
    nsCOMPtr<nsIContentViewer> contentViewer;
    mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
    nsCOMPtr<nsIMarkupDocumentViewer> markupViewer(do_QueryInterface(contentViewer));

    if (markupViewer) {
      return markupViewer->GetTextZoom(aZoom);
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGlobalWindow::SetTextZoom(float aZoom)
{
  FORWARD_TO_OUTER(SetTextZoom, (aZoom), NS_ERROR_NOT_INITIALIZED);

  if (mDocShell) {
    nsCOMPtr<nsIContentViewer> contentViewer;
    mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
    nsCOMPtr<nsIMarkupDocumentViewer> markupViewer(do_QueryInterface(contentViewer));

    if (markupViewer)
      return markupViewer->SetTextZoom(aZoom);
  }
  return NS_ERROR_FAILURE;
}

// static
void
nsGlobalWindow::MakeScriptDialogTitle(nsAString &aOutTitle)
{
  aOutTitle.Truncate();

  // Try to get a host from the running principal -- this will do the
  // right thing for javascript: and data: documents.

  nsresult rv = NS_OK;
  NS_ASSERTION(nsContentUtils::GetSecurityManager(),
    "Global Window has no security manager!");
  if (nsContentUtils::GetSecurityManager()) {
    nsCOMPtr<nsIPrincipal> principal;
    rv = nsContentUtils::GetSecurityManager()->
      GetSubjectPrincipal(getter_AddRefs(principal));

    if (NS_SUCCEEDED(rv) && principal) {
      nsCOMPtr<nsIURI> uri;
      rv = principal->GetURI(getter_AddRefs(uri));

      if (NS_SUCCEEDED(rv) && uri) {
        // remove user:pass for privacy and spoof prevention

        nsCOMPtr<nsIURIFixup> fixup(do_GetService(NS_URIFIXUP_CONTRACTID));
        if (fixup) {
          nsCOMPtr<nsIURI> fixedURI;
          rv = fixup->CreateExposableURI(uri, getter_AddRefs(fixedURI));
          if (NS_SUCCEEDED(rv) && fixedURI) {
            nsCAutoString host;
            fixedURI->GetHost(host);

            if (!host.IsEmpty()) {
              // if this URI has a host we'll show it. For other
              // schemes (e.g. file:) we fall back to the localized
              // generic string

              nsCAutoString prepath;
              fixedURI->GetPrePath(prepath);

              NS_ConvertUTF8toUTF16 ucsPrePath(prepath);
              const PRUnichar *formatStrings[] = { ucsPrePath.get() };
              nsXPIDLString tempString;
              nsContentUtils::FormatLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                                    "ScriptDlgHeading",
                                                    formatStrings, NS_ARRAY_LENGTH(formatStrings),
                                                    tempString);
              aOutTitle = tempString;
            }
          }
        }
      }
    }
    else { // failed to get subject principal
      NS_WARNING("No script principal? Who is calling alert/confirm/prompt?!");
    }
  }

  if (aOutTitle.IsEmpty()) {
    // We didn't find a host so use the generic heading
    nsXPIDLString tempString;
    nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                       "ScriptDlgGenericHeading",
                                       tempString);
    aOutTitle = tempString;
  }

  // Just in case
  if (aOutTitle.IsEmpty()) {
    NS_WARNING("could not get ScriptDlgGenericHeading string from string bundle");
    aOutTitle.AssignLiteral("[Script]");
  }
}

// static
PRBool
nsGlobalWindow::CanMoveResizeWindows()
{
  if (!CanSetProperty("dom.disable_window_move_resize"))
    return PR_FALSE;

  if (gMouseDown && !gDragServiceDisabled) {
    nsCOMPtr<nsIDragService> ds =
      do_GetService("@mozilla.org/widget/dragservice;1");
    if (ds) {
      gDragServiceDisabled = PR_TRUE;
      ds->Suppress();
    }
  }
  return PR_TRUE;
}

NS_IMETHODIMP
nsGlobalWindow::Alert(const nsAString& aString)
{
  FORWARD_TO_OUTER(Alert, (aString), NS_ERROR_NOT_INITIALIZED);

  if (AreDialogsBlocked())
    return NS_ERROR_NOT_AVAILABLE;

  // We have to capture this now so as not to get confused with the
  // popup state we push next
  PRBool shouldEnableDisableDialog = DialogOpenAttempted();

  // Reset popup state while opening a modal dialog, and firing events
  // about the dialog, to prevent the current state from being active
  // the whole time a modal dialog is open.
  nsAutoPopupStatePusher popupStatePusher(openAbused, PR_TRUE);

  // Special handling for alert(null) in JS for backwards
  // compatibility.

  NS_NAMED_LITERAL_STRING(null_str, "null");

  const nsAString *str = DOMStringIsNull(aString) ? &null_str : &aString;

  // Before bringing up the window, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  nsAutoString title;
  MakeScriptDialogTitle(title);

  // Remove non-terminating null characters from the 
  // string. See bug #310037. 
  nsAutoString final;
  nsContentUtils::StripNullChars(*str, final);

  nsresult rv;
  nsCOMPtr<nsIPromptService> promptSvc =
    do_GetService("@mozilla.org/embedcomp/prompt-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  EnterModalState();

  if (shouldEnableDisableDialog) {
    PRBool disallowDialog = PR_FALSE;
    nsXPIDLString label;
    nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                       "ScriptDialogLabel", label);

    rv = promptSvc->AlertCheck(this, title.get(), final.get(), label.get(),
                               &disallowDialog);
    if (disallowDialog)
      PreventFurtherDialogs();
  } else {
    rv = promptSvc->Alert(this, title.get(), final.get());
  }

  LeaveModalState();

  return rv;
}

NS_IMETHODIMP
nsGlobalWindow::Confirm(const nsAString& aString, PRBool* aReturn)
{
  FORWARD_TO_OUTER(Confirm, (aString, aReturn), NS_ERROR_NOT_INITIALIZED);

  if (AreDialogsBlocked())
    return NS_ERROR_NOT_AVAILABLE;

  // We have to capture this now so as not to get confused with the popup state
  // we push next
  PRBool shouldEnableDisableDialog = DialogOpenAttempted();

  // Reset popup state while opening a modal dialog, and firing events
  // about the dialog, to prevent the current state from being active
  // the whole time a modal dialog is open.
  nsAutoPopupStatePusher popupStatePusher(openAbused, PR_TRUE);

  *aReturn = PR_FALSE;

  // Before bringing up the window, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  nsAutoString title;
  MakeScriptDialogTitle(title);

  // Remove non-terminating null characters from the 
  // string. See bug #310037. 
  nsAutoString final;
  nsContentUtils::StripNullChars(aString, final);

  nsresult rv;
  nsCOMPtr<nsIPromptService> promptSvc =
    do_GetService("@mozilla.org/embedcomp/prompt-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  EnterModalState();

  if (shouldEnableDisableDialog) {
    PRBool disallowDialog = PR_FALSE;
    nsXPIDLString label;
    nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                       "ScriptDialogLabel", label);

    rv = promptSvc->ConfirmCheck(this, title.get(), final.get(), label.get(),
                                 &disallowDialog, aReturn);
    if (disallowDialog)
      PreventFurtherDialogs();
  } else {
    rv = promptSvc->Confirm(this, title.get(), final.get(), aReturn);
  }

  LeaveModalState();

  return rv;
}

NS_IMETHODIMP
nsGlobalWindow::Prompt(const nsAString& aMessage, const nsAString& aInitial,
                       nsAString& aReturn)
{
  SetDOMStringToNull(aReturn);

  if (AreDialogsBlocked())
    return NS_ERROR_NOT_AVAILABLE;

  // We have to capture this now so as not to get confused with the popup state
  // we push next
  PRBool shouldEnableDisableDialog = DialogOpenAttempted();

  // Reset popup state while opening a modal dialog, and firing events
  // about the dialog, to prevent the current state from being active
  // the whole time a modal dialog is open.
  nsAutoPopupStatePusher popupStatePusher(openAbused, PR_TRUE);

  // Before bringing up the window, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  nsAutoString title;
  MakeScriptDialogTitle(title);
  
  // Remove non-terminating null characters from the 
  // string. See bug #310037. 
  nsAutoString fixedMessage, fixedInitial;
  nsContentUtils::StripNullChars(aMessage, fixedMessage);
  nsContentUtils::StripNullChars(aInitial, fixedInitial);

  nsresult rv;
  nsCOMPtr<nsIPromptService> promptSvc =
    do_GetService("@mozilla.org/embedcomp/prompt-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Pass in the default value, if any.
  PRUnichar *inoutValue = ToNewUnicode(fixedInitial);
  PRBool disallowDialog = PR_FALSE;

  nsXPIDLString label;
  if (shouldEnableDisableDialog) {
    nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                       "ScriptDialogLabel", label);
  }

  EnterModalState();

  PRBool ok;
  rv = promptSvc->Prompt(this, title.get(), fixedMessage.get(),
                         &inoutValue, label.get(), &disallowDialog, &ok);

  LeaveModalState();

  if (disallowDialog) {
    PreventFurtherDialogs();
  }

  NS_ENSURE_SUCCESS(rv, rv);

  nsAdoptingString outValue(inoutValue);

  if (ok && outValue) {
    aReturn.Assign(outValue);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalWindow::Focus()
{
  FORWARD_TO_OUTER(Focus, (), NS_ERROR_NOT_INITIALIZED);

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm)
    return NS_OK;

  nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(mDocShell);

  PRBool isVisible = PR_FALSE;
  if (baseWin) {
    baseWin->GetVisibility(&isVisible);
  }

  if (!isVisible) {
    // A hidden tab is being focused, ignore this call.
    return NS_OK;
  }

  nsIDOMWindowInternal *caller =
    static_cast<nsIDOMWindowInternal*>(nsContentUtils::GetWindowFromCaller());
  nsCOMPtr<nsIDOMWindowInternal> opener;
  GetOpener(getter_AddRefs(opener));

  // Enforce dom.disable_window_flip (for non-chrome), but still allow the
  // window which opened us to raise us at times when popups are allowed
  // (bugs 355482 and 369306).
  PRBool canFocus = CanSetProperty("dom.disable_window_flip") ||
                    (opener == caller &&
                     RevisePopupAbuseLevel(gPopupControlState) < openAbused);

  nsCOMPtr<nsIDOMWindow> activeWindow;
  fm->GetActiveWindow(getter_AddRefs(activeWindow));

  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(mDocShell);
  NS_ASSERTION(treeItem, "What happened?");
  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  treeItem->GetRootTreeItem(getter_AddRefs(rootItem));
  nsCOMPtr<nsIDOMWindow> rootWin = do_GetInterface(rootItem);
  PRBool isActive = (rootWin == activeWindow);

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  if (treeOwnerAsWin && (canFocus || isActive)) {
    PRBool isEnabled = PR_TRUE;
    if (NS_SUCCEEDED(treeOwnerAsWin->GetEnabled(&isEnabled)) && !isEnabled) {
      NS_WARNING( "Should not try to set the focus on a disabled window" );
      return NS_OK;
    }

    // XXXndeakin not sure what this is for or if it should go somewhere else
    nsCOMPtr<nsIEmbeddingSiteWindow> embeddingWin(do_GetInterface(treeOwnerAsWin));
    if (embeddingWin)
      embeddingWin->SetFocus();
  }

  if (!mDocShell)
    return NS_OK;

  nsCOMPtr<nsIPresShell> presShell;
  // Don't look for a presshell if we're a root chrome window that's got
  // about:blank loaded.  We don't want to focus our widget in that case.
  // XXXbz should we really be checking for IsInitialDocument() instead?
  PRBool lookForPresShell = PR_TRUE;
  PRInt32 itemType = nsIDocShellTreeItem::typeContent;
  treeItem->GetItemType(&itemType);
  if (itemType == nsIDocShellTreeItem::typeChrome &&
      GetPrivateRoot() == static_cast<nsIDOMWindowInternal*>(this) &&
      mDocument) {
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
    NS_ASSERTION(doc, "Bogus doc?");
    nsIURI* ourURI = doc->GetDocumentURI();
    if (ourURI) {
      lookForPresShell = !IsAboutBlank(ourURI);
    }
  }

  if (lookForPresShell) {
    mDocShell->GetEldestPresShell(getter_AddRefs(presShell));
  }

  nsCOMPtr<nsIDocShellTreeItem> parentDsti;
  treeItem->GetParent(getter_AddRefs(parentDsti));

  // set the parent's current focus to the frame containing this window.
  nsCOMPtr<nsIDOMWindow> parent(do_GetInterface(parentDsti));
  if (parent) {
    nsCOMPtr<nsIDOMDocument> parentdomdoc;
    parent->GetDocument(getter_AddRefs(parentdomdoc));

    nsCOMPtr<nsIDocument> parentdoc = do_QueryInterface(parentdomdoc);
    if (!parentdoc)
      return NS_OK;

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
    nsIContent* frame = parentdoc->FindContentForSubDocument(doc);
    nsCOMPtr<nsIDOMElement> frameElement = do_QueryInterface(frame);
    if (frameElement) {
      PRUint32 flags = nsIFocusManager::FLAG_NOSCROLL;
      if (canFocus)
        flags |= nsIFocusManager::FLAG_RAISE;
      return fm->SetFocus(frameElement, flags);
    }
  }
  else if (canFocus) {
    // if there is no parent, this must be a toplevel window, so raise the
    // window if canFocus is true
    return fm->SetActiveWindow(this);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::Blur()
{
  FORWARD_TO_OUTER(Blur, (), NS_ERROR_NOT_INITIALIZED);

  // If dom.disable_window_flip == true, then content should not be allowed
  // to call this function (this would allow popunders, bug 369306)
  if (!CanSetProperty("dom.disable_window_flip")) {
    return NS_OK;
  }

  // If embedding apps don't implement nsIEmbeddingSiteWindow2, we
  // shouldn't throw exceptions to web content.
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  GetTreeOwner(getter_AddRefs(treeOwner));
  nsCOMPtr<nsIEmbeddingSiteWindow2> siteWindow(do_GetInterface(treeOwner));
  if (siteWindow) {
    // This method call may cause mDocShell to become nsnull.
    rv = siteWindow->Blur();

    // if the root is focused, clear the focus
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
    if (fm && mDocument) {
      nsCOMPtr<nsIDOMElement> element;
      fm->GetFocusedElementForWindow(this, PR_FALSE, nsnull, getter_AddRefs(element));
      nsCOMPtr<nsIContent> content = do_QueryInterface(element);
      if (content == doc->GetRootElement())
        fm->ClearFocus(this);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalWindow::Back()
{
  FORWARD_TO_OUTER(Back, (), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

  return webNav->GoBack();
}

NS_IMETHODIMP
nsGlobalWindow::Forward()
{
  FORWARD_TO_OUTER(Forward, (), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

  return webNav->GoForward();
}

NS_IMETHODIMP
nsGlobalWindow::Home()
{
  FORWARD_TO_OUTER(Home, (), NS_ERROR_NOT_INITIALIZED);

  if (!mDocShell)
    return NS_OK;

  nsAdoptingString homeURL =
    nsContentUtils::GetLocalizedStringPref(PREF_BROWSER_STARTUP_HOMEPAGE);

  if (homeURL.IsEmpty()) {
    // if all else fails, use this
#ifdef DEBUG_seth
    printf("all else failed.  using %s as the home page\n", DEFAULT_HOME_PAGE);
#endif
    CopyASCIItoUTF16(DEFAULT_HOME_PAGE, homeURL);
  }

#ifdef MOZ_PHOENIX
  {
    // Firefox lets the user specify multiple home pages to open in
    // individual tabs by separating them with '|'. Since we don't
    // have the machinery in place to easily open new tabs from here,
    // simply truncate the homeURL at the first '|' character to
    // prevent any possibilities of leaking the users list of home
    // pages to the first home page.
    //
    // Once bug https://bugzilla.mozilla.org/show_bug.cgi?id=221445 is
    // fixed we can revisit this.
    PRInt32 firstPipe = homeURL.FindChar('|');

    if (firstPipe > 0) {
      homeURL.Truncate(firstPipe);
    }
  }
#endif

  nsresult rv;
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);
  rv = webNav->LoadURI(homeURL.get(),
                       nsIWebNavigation::LOAD_FLAGS_NONE,
                       nsnull,
                       nsnull,
                       nsnull);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::Stop()
{
  FORWARD_TO_OUTER(Stop, (), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  if (!webNav)
    return NS_OK;

  return webNav->Stop(nsIWebNavigation::STOP_ALL);
}

NS_IMETHODIMP
nsGlobalWindow::Print()
{
#ifdef NS_PRINTING
  FORWARD_TO_OUTER(Print, (), NS_ERROR_NOT_INITIALIZED);

  if (AreDialogsBlocked() || !ConfirmDialogAllowed())
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint;
  if (NS_SUCCEEDED(GetInterface(NS_GET_IID(nsIWebBrowserPrint),
                                getter_AddRefs(webBrowserPrint)))) {

    nsCOMPtr<nsIPrintSettingsService> printSettingsService = 
      do_GetService("@mozilla.org/gfx/printsettings-service;1");

    nsCOMPtr<nsIPrintSettings> printSettings;
    if (printSettingsService) {
      PRBool printSettingsAreGlobal =
        nsContentUtils::GetBoolPref("print.use_global_printsettings", PR_FALSE);

      if (printSettingsAreGlobal) {
        printSettingsService->GetGlobalPrintSettings(getter_AddRefs(printSettings));

        nsXPIDLString printerName;
        printSettings->GetPrinterName(getter_Copies(printerName));
        if (printerName.IsEmpty()) {
          printSettingsService->GetDefaultPrinterName(getter_Copies(printerName));
          printSettings->SetPrinterName(printerName);
        }
        printSettingsService->InitPrintSettingsFromPrinter(printerName, printSettings);
        printSettingsService->InitPrintSettingsFromPrefs(printSettings, 
                                                         PR_TRUE, 
                                                         nsIPrintSettings::kInitSaveAll);
      } else {
        printSettingsService->GetNewPrintSettings(getter_AddRefs(printSettings));
      }

      EnterModalState();
      webBrowserPrint->Print(printSettings, nsnull);
      LeaveModalState();

      PRBool savePrintSettings =
        nsContentUtils::GetBoolPref("print.save_print_settings", PR_FALSE);
      if (printSettingsAreGlobal && savePrintSettings) {
        printSettingsService->
          SavePrintSettingsToPrefs(printSettings,
                                   PR_TRUE,
                                   nsIPrintSettings::kInitSaveAll);
        printSettingsService->
          SavePrintSettingsToPrefs(printSettings,
                                   PR_FALSE,
                                   nsIPrintSettings::kInitSavePrinterName);
      }
    } else {
      webBrowserPrint->GetGlobalPrintSettings(getter_AddRefs(printSettings));
      webBrowserPrint->Print(printSettings, nsnull);
    }
  }
#endif //NS_PRINTING

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::MoveTo(PRInt32 aXPos, PRInt32 aYPos)
{
  FORWARD_TO_OUTER(MoveTo, (aXPos, aYPos), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.moveTo() by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityLeftAndTop(&aXPos, &aYPos),
                    NS_ERROR_FAILURE);

  // mild abuse of a "size" object so we don't need more helper functions
  nsIntSize devPos(CSSToDevIntPixels(nsIntSize(aXPos, aYPos)));

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetPosition(devPos.width, devPos.height),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::MoveBy(PRInt32 aXDif, PRInt32 aYDif)
{
  FORWARD_TO_OUTER(MoveBy, (aXDif, aYDif), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.moveBy() by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  // To do this correctly we have to convert what we get from GetPosition
  // into CSS pixels, add the arguments, do the security check, and
  // then convert back to device pixels for the call to SetPosition.

  PRInt32 x, y;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, &y), NS_ERROR_FAILURE);

  // mild abuse of a "size" object so we don't need more helper functions
  nsIntSize cssPos(DevToCSSIntPixels(nsIntSize(x, y)));

  cssPos.width += aXDif;
  cssPos.height += aYDif;
  
  NS_ENSURE_SUCCESS(CheckSecurityLeftAndTop(&cssPos.width,
                                            &cssPos.height),
                    NS_ERROR_FAILURE);

  nsIntSize newDevPos(CSSToDevIntPixels(cssPos));

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetPosition(newDevPos.width,
                                                newDevPos.height),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ResizeTo(PRInt32 aWidth, PRInt32 aHeight)
{
  FORWARD_TO_OUTER(ResizeTo, (aWidth, aHeight), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.resizeTo() by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);
  
  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(&aWidth, &aHeight),
                    NS_ERROR_FAILURE);

  nsIntSize devSz(CSSToDevIntPixels(nsIntSize(aWidth, aHeight)));

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetSize(devSz.width, devSz.height, PR_TRUE),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ResizeBy(PRInt32 aWidthDif, PRInt32 aHeightDif)
{
  FORWARD_TO_OUTER(ResizeBy, (aWidthDif, aHeightDif), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.resizeBy() by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  PRInt32 width, height;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(&width, &height), NS_ERROR_FAILURE);

  // To do this correctly we have to convert what we got from GetSize
  // into CSS pixels, add the arguments, do the security check, and
  // then convert back to device pixels for the call to SetSize.

  nsIntSize cssSize(DevToCSSIntPixels(nsIntSize(width, height)));

  cssSize.width += aWidthDif;
  cssSize.height += aHeightDif;

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(&cssSize.width,
                                                &cssSize.height),
                    NS_ERROR_FAILURE);

  nsIntSize newDevSize(CSSToDevIntPixels(cssSize));

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetSize(newDevSize.width,
                                            newDevSize.height,
                                            PR_TRUE),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SizeToContent()
{
  FORWARD_TO_OUTER(SizeToContent, (), NS_ERROR_NOT_INITIALIZED);

  if (!mDocShell) {
    return NS_OK;
  }

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.sizeToContent() by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  // The content viewer does a check to make sure that it's a content
  // viewer for a toplevel docshell.
  
  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  nsCOMPtr<nsIMarkupDocumentViewer> markupViewer(do_QueryInterface(cv));
  NS_ENSURE_TRUE(markupViewer, NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(markupViewer->SizeToContent(), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetWindowRoot(nsIDOMEventTarget **aWindowRoot)
{
  nsCOMPtr<nsPIWindowRoot> root = GetTopWindowRoot();
  return CallQueryInterface(root, aWindowRoot);
}

already_AddRefed<nsPIWindowRoot>
nsGlobalWindow::GetTopWindowRoot()
{
  nsIDOMWindowInternal *rootWindow = GetPrivateRoot();
  nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(rootWindow));
  if (!piWin)
    return nsnull;

  nsCOMPtr<nsPIWindowRoot> window = do_QueryInterface(piWin->GetChromeEventHandler());
  return window.forget();
}

NS_IMETHODIMP
nsGlobalWindow::Scroll(PRInt32 aXScroll, PRInt32 aYScroll)
{
  return ScrollTo(aXScroll, aYScroll);
}

NS_IMETHODIMP
nsGlobalWindow::ScrollTo(PRInt32 aXScroll, PRInt32 aYScroll)
{
  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();

  if (sf) {
    // Here we calculate what the max pixel value is that we can
    // scroll to, we do this by dividing maxint with the pixel to
    // twips conversion factor, and substracting 4, the 4 comes from
    // experimenting with this value, anything less makes the view
    // code not scroll correctly, I have no idea why. -- jst
    const PRInt32 maxpx = nsPresContext::AppUnitsToIntCSSPixels(0x7fffffff) - 4;

    if (aXScroll > maxpx) {
      aXScroll = maxpx;
    }

    if (aYScroll > maxpx) {
      aYScroll = maxpx;
    }
    sf->ScrollTo(nsPoint(nsPresContext::CSSPixelsToAppUnits(aXScroll),
                         nsPresContext::CSSPixelsToAppUnits(aYScroll)),
                 nsIScrollableFrame::INSTANT);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ScrollBy(PRInt32 aXScrollDif, PRInt32 aYScrollDif)
{
  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();

  if (sf) {
    nsPoint scrollPos = sf->GetScrollPosition();
    // It seems like it would make more sense for ScrollBy to use
    // SMOOTH mode, but tests seem to depend on the synchronous behaviour.
    // Perhaps Web content does too.
    return ScrollTo(nsPresContext::AppUnitsToIntCSSPixels(scrollPos.x) + aXScrollDif,
                    nsPresContext::AppUnitsToIntCSSPixels(scrollPos.y) + aYScrollDif);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ScrollByLines(PRInt32 numLines)
{
  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();
  if (sf) {
    // It seems like it would make more sense for ScrollByLines to use
    // SMOOTH mode, but tests seem to depend on the synchronous behaviour.
    // Perhaps Web content does too.
    sf->ScrollBy(nsIntPoint(0, numLines), nsIScrollableFrame::LINES,
                 nsIScrollableFrame::INSTANT);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ScrollByPages(PRInt32 numPages)
{
  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();
  if (sf) {
    // It seems like it would make more sense for ScrollByPages to use
    // SMOOTH mode, but tests seem to depend on the synchronous behaviour.
    // Perhaps Web content does too.
    sf->ScrollBy(nsIntPoint(0, numPages), nsIScrollableFrame::PAGES,
                 nsIScrollableFrame::INSTANT);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ClearTimeout()
{
  return ClearTimeoutOrInterval();
}

NS_IMETHODIMP
nsGlobalWindow::ClearInterval()
{
  return ClearTimeoutOrInterval();
}

NS_IMETHODIMP
nsGlobalWindow::SetTimeout(PRInt32 *_retval)
{
  return SetTimeoutOrInterval(PR_FALSE, _retval);
}

NS_IMETHODIMP
nsGlobalWindow::SetInterval(PRInt32 *_retval)
{
  return SetTimeoutOrInterval(PR_TRUE, _retval);
}

NS_IMETHODIMP
nsGlobalWindow::SetResizable(PRBool aResizable)
{
  // nop

  return NS_OK;
}

static void
ReportUseOfDeprecatedMethod(nsGlobalWindow* aWindow, const char* aWarning)
{
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aWindow->GetExtantDocument());
  nsContentUtils::ReportToConsole(nsContentUtils::eDOM_PROPERTIES,
                                  aWarning,
                                  nsnull, 0,
                                  doc ? doc->GetDocumentURI() : nsnull,
                                  EmptyString(), 0, 0,
                                  nsIScriptError::warningFlag,
                                  "DOM Events");
}

NS_IMETHODIMP
nsGlobalWindow::CaptureEvents(PRInt32 aEventFlags)
{
  ReportUseOfDeprecatedMethod(this, "UseOfCaptureEventsWarning");
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ReleaseEvents(PRInt32 aEventFlags)
{
  ReportUseOfDeprecatedMethod(this, "UseOfReleaseEventsWarning");
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::RouteEvent(nsIDOMEvent* aEvt)
{
  ReportUseOfDeprecatedMethod(this, "UseOfRouteEventWarning");
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::EnableExternalCapture()
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGlobalWindow::DisableExternalCapture()
{
  return NS_ERROR_FAILURE;
}

static
PRBool IsPopupBlocked(nsIDOMDocument* aDoc)
{
  nsCOMPtr<nsIPopupWindowManager> pm =
    do_GetService(NS_POPUPWINDOWMANAGER_CONTRACTID);

  if (!pm) {
    return PR_FALSE;
  }

  PRBool blocked = PR_TRUE;
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDoc));

  if (doc) {
    PRUint32 permission = nsIPopupWindowManager::ALLOW_POPUP;
    pm->TestPermission(doc->GetDocumentURI(), &permission);
    blocked = (permission == nsIPopupWindowManager::DENY_POPUP);
  }
  return blocked;
}

/* static */
void 
nsGlobalWindow::FirePopupBlockedEvent(nsIDOMDocument* aDoc,
                                      nsIDOMWindow *aRequestingWindow, nsIURI *aPopupURI,
                                      const nsAString &aPopupWindowName,
                                      const nsAString &aPopupWindowFeatures)
{
  if (aDoc) {
    // Fire a "DOMPopupBlocked" event so that the UI can hear about
    // blocked popups.
    nsCOMPtr<nsIDOMDocumentEvent> docEvent(do_QueryInterface(aDoc));
    nsCOMPtr<nsIDOMEvent> event;
    docEvent->CreateEvent(NS_LITERAL_STRING("PopupBlockedEvents"),
                          getter_AddRefs(event));
    if (event) {
      nsCOMPtr<nsIDOMPopupBlockedEvent> pbev(do_QueryInterface(event));
      pbev->InitPopupBlockedEvent(NS_LITERAL_STRING("DOMPopupBlocked"),
                                  PR_TRUE, PR_TRUE, aRequestingWindow,
                                  aPopupURI, aPopupWindowName,
                                  aPopupWindowFeatures);
      nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(event));
      privateEvent->SetTrusted(PR_TRUE);

      nsCOMPtr<nsIDOMEventTarget> targ(do_QueryInterface(aDoc));
      PRBool defaultActionEnabled;
      targ->DispatchEvent(event, &defaultActionEnabled);
    }
  }
}

void FirePopupWindowEvent(nsIDOMDocument* aDoc)
{
  // Fire a "PopupWindow" event
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDoc));
  nsContentUtils::DispatchTrustedEvent(doc, aDoc,
                                       NS_LITERAL_STRING("PopupWindow"),
                                       PR_TRUE, PR_TRUE);
}

// static
PRBool
nsGlobalWindow::CanSetProperty(const char *aPrefName)
{
  // Chrome can set any property.
  if (nsContentUtils::IsCallerTrustedForWrite()) {
    return PR_TRUE;
  }

  // If the pref is set to true, we can not set the property
  // and vice versa.
  return !nsContentUtils::GetBoolPref(aPrefName, PR_TRUE);
}

PRBool
nsGlobalWindow::PopupWhitelisted()
{
  if (!IsPopupBlocked(mDocument))
    return PR_TRUE;

  nsCOMPtr<nsIDOMWindow> parent;

  if (NS_FAILED(GetParent(getter_AddRefs(parent))) ||
      parent == static_cast<nsIDOMWindow*>(this))
  {
    return PR_FALSE;
  }

  return static_cast<nsGlobalWindow*>
                    (static_cast<nsIDOMWindow*>
                                (parent.get()))->PopupWhitelisted();
}

/*
 * Examine the current document state to see if we're in a way that is
 * typically abused by web designers. The window.open code uses this
 * routine to determine whether to allow the new window.
 * Returns a value from the PopupControlState enum.
 */
PopupControlState
nsGlobalWindow::RevisePopupAbuseLevel(PopupControlState aControl)
{
  FORWARD_TO_OUTER(RevisePopupAbuseLevel, (aControl), aControl);

  NS_ASSERTION(mDocShell, "Must have docshell");
  
  nsCOMPtr<nsIDocShellTreeItem> item(do_QueryInterface(mDocShell));

  NS_ASSERTION(item, "Docshell doesn't implement nsIDocShellTreeItem?");

  PRInt32 type = nsIDocShellTreeItem::typeChrome;
  item->GetItemType(&type);
  if (type != nsIDocShellTreeItem::typeContent)
    return openAllowed;

  PopupControlState abuse = aControl;
  switch (abuse) {
  case openControlled:
  case openAbused:
  case openOverridden:
    if (PopupWhitelisted())
      abuse = PopupControlState(abuse - 1);
  case openAllowed: break;
  default:
    NS_WARNING("Strange PopupControlState!");
  }

  // limit the number of simultaneously open popups
  if (abuse == openAbused || abuse == openControlled) {
    PRInt32 popupMax = nsContentUtils::GetIntPref("dom.popup_maximum", -1);
    if (popupMax >= 0 && gOpenPopupSpamCount >= popupMax)
      abuse = openOverridden;
  }

  return abuse;
}

/* If a window open is blocked, fire the appropriate DOM events.
   aBlocked signifies we just blocked a popup.
   aWindow signifies we just opened what is probably a popup.
*/
void
nsGlobalWindow::FireAbuseEvents(PRBool aBlocked, PRBool aWindow,
                                const nsAString &aPopupURL,
                                const nsAString &aPopupWindowName,
                                const nsAString &aPopupWindowFeatures)
{
  // fetch the URI of the window requesting the opened window

  nsCOMPtr<nsIDOMWindow> topWindow;
  GetTop(getter_AddRefs(topWindow));
  if (!topWindow)
    return;

  nsCOMPtr<nsIDOMDocument> topDoc;
  topWindow->GetDocument(getter_AddRefs(topDoc));

  nsCOMPtr<nsIURI> popupURI;

  // build the URI of the would-have-been popup window
  // (see nsWindowWatcher::URIfromURL)

  // first, fetch the opener's base URI

  nsIURI *baseURL = 0;

  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  nsCOMPtr<nsIDOMWindow> contextWindow;

  if (cx) {
    nsIScriptContext *currentCX = nsJSUtils::GetDynamicScriptContext(cx);
    if (currentCX) {
      contextWindow = do_QueryInterface(currentCX->GetGlobalObject());
    }
  }
  if (!contextWindow)
    contextWindow = static_cast<nsIDOMWindow*>(this);

  nsCOMPtr<nsIDOMDocument> domdoc;
  contextWindow->GetDocument(getter_AddRefs(domdoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domdoc));
  if (doc)
    baseURL = doc->GetDocBaseURI();

  // use the base URI to build what would have been the popup's URI
  nsCOMPtr<nsIIOService> ios(do_GetService(NS_IOSERVICE_CONTRACTID));
  if (ios)
    ios->NewURI(NS_ConvertUTF16toUTF8(aPopupURL), 0, baseURL,
                getter_AddRefs(popupURI));

  // fire an event chock full of informative URIs
  if (aBlocked)
    FirePopupBlockedEvent(topDoc, this, popupURI, aPopupWindowName,
                          aPopupWindowFeatures);
  if (aWindow)
    FirePopupWindowEvent(topDoc);
}

NS_IMETHODIMP
nsGlobalWindow::Open(const nsAString& aUrl, const nsAString& aName,
                     const nsAString& aOptions, nsIDOMWindow **_retval)
{
  return OpenInternal(aUrl, aName, aOptions,
                      PR_FALSE,          // aDialog
                      PR_FALSE,          // aContentModal
                      PR_TRUE,           // aCalledNoScript
                      PR_FALSE,          // aDoJSFixups
                      nsnull, nsnull,    // No args
                      GetPrincipal(),    // aCalleePrincipal
                      nsnull,            // aJSCallerContext
                      _retval);
}

NS_IMETHODIMP
nsGlobalWindow::OpenJS(const nsAString& aUrl, const nsAString& aName,
                       const nsAString& aOptions, nsIDOMWindow **_retval)
{
  return OpenInternal(aUrl, aName, aOptions,
                      PR_FALSE,          // aDialog
                      PR_FALSE,          // aContentModal
                      PR_FALSE,          // aCalledNoScript
                      PR_TRUE,           // aDoJSFixups
                      nsnull, nsnull,    // No args
                      GetPrincipal(),    // aCalleePrincipal
                      nsContentUtils::GetCurrentJSContext(), // aJSCallerContext
                      _retval);
}

// like Open, but attaches to the new window any extra parameters past
// [features] as a JS property named "arguments"
NS_IMETHODIMP
nsGlobalWindow::OpenDialog(const nsAString& aUrl, const nsAString& aName,
                           const nsAString& aOptions,
                           nsISupports* aExtraArgument, nsIDOMWindow** _retval)
{
  return OpenInternal(aUrl, aName, aOptions,
                      PR_TRUE,                    // aDialog
                      PR_FALSE,                   // aContentModal
                      PR_TRUE,                    // aCalledNoScript
                      PR_FALSE,                   // aDoJSFixups
                      nsnull, aExtraArgument,     // Arguments
                      GetPrincipal(),             // aCalleePrincipal
                      nsnull,                     // aJSCallerContext
                      _retval);
}

NS_IMETHODIMP
nsGlobalWindow::OpenDialog(const nsAString& aUrl, const nsAString& aName,
                           const nsAString& aOptions, nsIDOMWindow** _retval)
{
  if (!nsContentUtils::IsCallerTrustedForWrite()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsAXPCNativeCallContext *ncc = nsnull;
  nsresult rv = nsContentUtils::XPConnect()->
    GetCurrentNativeCallContext(&ncc);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ncc)
    return NS_ERROR_NOT_AVAILABLE;

  JSContext *cx = nsnull;

  rv = ncc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 argc;
  jsval *argv = nsnull;

  // XXX - need to get this as nsISupports?
  ncc->GetArgc(&argc);
  ncc->GetArgvPtr(&argv);

  // Strip the url, name and options from the args seen by scripts.
  PRUint32 argOffset = argc < 3 ? argc : 3;
  nsCOMPtr<nsIArray> argvArray;
  rv = NS_CreateJSArgv(cx, argc - argOffset, argv + argOffset,
                       getter_AddRefs(argvArray));
  NS_ENSURE_SUCCESS(rv, rv);

  return OpenInternal(aUrl, aName, aOptions,
                      PR_TRUE,             // aDialog
                      PR_FALSE,            // aContentModal
                      PR_FALSE,            // aCalledNoScript
                      PR_FALSE,            // aDoJSFixups
                      argvArray, nsnull,   // Arguments
                      GetPrincipal(),      // aCalleePrincipal
                      cx,                  // aJSCallerContext
                      _retval);
}

NS_IMETHODIMP
nsGlobalWindow::GetFrames(nsIDOMWindow** aFrames)
{
  FORWARD_TO_OUTER(GetFrames, (aFrames), NS_ERROR_NOT_INITIALIZED);

  *aFrames = this;
  NS_ADDREF(*aFrames);

  FlushPendingNotifications(Flush_ContentAndNotify);

  return NS_OK;
}

nsGlobalWindow*
nsGlobalWindow::CallerInnerWindow()
{
  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  if (!cx) {
    NS_ERROR("Please don't call this method from C++!");

    return nsnull;
  }

  JSObject *scope = ::JS_GetScopeChain(cx);
  if (!scope)
    return nsnull;

  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
  nsContentUtils::XPConnect()->
    GetWrappedNativeOfJSObject(cx, ::JS_GetGlobalForObject(cx, scope),
                               getter_AddRefs(wrapper));
  if (!wrapper)
    return nsnull;

  // The calling window must be holding a reference, so we can just return a
  // raw pointer here and let the QI's addref be balanced by the nsCOMPtr
  // destructor's release.
  nsCOMPtr<nsPIDOMWindow> win = do_QueryWrappedNative(wrapper);
  if (!win)
    return GetCurrentInnerWindowInternal();
  return static_cast<nsGlobalWindow*>(win.get());
}


/**
 * Class used to represent events generated by calls to Window.postMessage,
 * which asynchronously creates and dispatches events.
 */
class PostMessageEvent : public nsRunnable
{
  public:
    NS_DECL_NSIRUNNABLE

    PostMessageEvent(nsGlobalWindow* aSource,
                     const nsAString& aCallerOrigin,
                     const nsAString& aMessage,
                     nsGlobalWindow* aTargetWindow,
                     nsIURI* aProvidedOrigin,
                     PRBool aTrustedCaller)
    : mSource(aSource),
      mCallerOrigin(aCallerOrigin),
      mMessage(aMessage),
      mTargetWindow(aTargetWindow),
      mProvidedOrigin(aProvidedOrigin),
      mTrustedCaller(aTrustedCaller)
    {
      MOZ_COUNT_CTOR(PostMessageEvent);
    }
    
    ~PostMessageEvent()
    {
      MOZ_COUNT_DTOR(PostMessageEvent);
    }

  private:
    nsRefPtr<nsGlobalWindow> mSource;
    nsString mCallerOrigin;
    nsString mMessage;
    nsRefPtr<nsGlobalWindow> mTargetWindow;
    nsCOMPtr<nsIURI> mProvidedOrigin;
    PRBool mTrustedCaller;
};

NS_IMETHODIMP
PostMessageEvent::Run()
{
  NS_ABORT_IF_FALSE(mTargetWindow->IsOuterWindow(),
                    "should have been passed an outer window!");
  NS_ABORT_IF_FALSE(!mSource || mSource->IsOuterWindow(),
                    "should have been passed an outer window!");

  nsRefPtr<nsGlobalWindow> targetWindow;
  if (mTargetWindow->IsClosedOrClosing() ||
      !(targetWindow = mTargetWindow->GetCurrentInnerWindowInternal()) ||
      targetWindow->IsClosedOrClosing())
    return NS_OK;

  NS_ABORT_IF_FALSE(targetWindow->IsInnerWindow(),
                    "we ordered an inner window!");

  // Ensure that any origin which might have been provided is the origin of this
  // window's document.  Note that we do this *now* instead of when postMessage
  // is called because the target window might have been navigated to a
  // different location between then and now.  If this check happened when
  // postMessage was called, it would be fairly easy for a malicious webpage to
  // intercept messages intended for another site by carefully timing navigation
  // of the target window so it changed location after postMessage but before
  // now.
  if (mProvidedOrigin) {
    // Get the target's origin either from its principal or, in the case the
    // principal doesn't carry a URI (e.g. the system principal), the target's
    // document.
    nsIPrincipal* targetPrin = targetWindow->GetPrincipal();
    if (!targetPrin)
      return NS_OK;
    nsCOMPtr<nsIURI> targetURI;
    if (NS_FAILED(targetPrin->GetURI(getter_AddRefs(targetURI))))
      return NS_OK;
    if (!targetURI) {
      targetURI = targetWindow->mDoc->GetDocumentURI();
      if (!targetURI)
        return NS_OK;
    }

    // Note: This is contrary to the spec with respect to file: URLs, which
    //       the spec groups into a single origin, but given we intentionally
    //       don't do that in other places it seems better to hold the line for
    //       now.  Long-term, we want HTML5 to address this so that we can
    //       be compliant while being safer.
    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    nsresult rv =
      ssm->CheckSameOriginURI(mProvidedOrigin, targetURI, PR_TRUE);
    if (NS_FAILED(rv))
      return NS_OK;
  }


  // Create the event
  nsCOMPtr<nsIDOMDocumentEvent> docEvent =
    do_QueryInterface(targetWindow->mDocument);
  if (!docEvent)
    return NS_OK;
  nsCOMPtr<nsIDOMEvent> event;
  docEvent->CreateEvent(NS_LITERAL_STRING("MessageEvent"),
                        getter_AddRefs(event));
  if (!event)
    return NS_OK;

  nsCOMPtr<nsIDOMMessageEvent> message = do_QueryInterface(event);
  nsresult rv = message->InitMessageEvent(NS_LITERAL_STRING("message"),
                                          PR_FALSE /* non-bubbling */,
                                          PR_TRUE /* cancelable */,
                                          mMessage,
                                          mCallerOrigin,
                                          EmptyString(),
                                          mSource);
  if (NS_FAILED(rv))
    return NS_OK;


  // We can't simply call dispatchEvent on the window because doing so ends
  // up flipping the trusted bit on the event, and we don't want that to
  // happen because then untrusted content can call postMessage on a chrome
  // window if it can get a reference to it.

  nsIPresShell *shell = targetWindow->mDoc->GetShell();
  nsRefPtr<nsPresContext> presContext;
  if (shell)
    presContext = shell->GetPresContext();

  nsCOMPtr<nsIPrivateDOMEvent> privEvent = do_QueryInterface(message);
  privEvent->SetTrusted(mTrustedCaller);
  nsEvent *internalEvent = privEvent->GetInternalNSEvent();

  nsEventStatus status = nsEventStatus_eIgnore;
  nsEventDispatcher::Dispatch(static_cast<nsPIDOMWindow*>(mTargetWindow),
                              presContext,
                              internalEvent,
                              message,
                              &status);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::PostMessageMoz(const nsAString& aMessage, const nsAString& aOrigin)
{
  // NB: Since much of what this method does must happen at event dispatch time,
  //     this method does not forward to the inner window, unlike most other
  //     methods.  We do this because the only time we need to refer to this
  //     window, we need a reference to the outer window (the PostMessageEvent
  //     ctor call), and we don't want to pay the price of forwarding to the
  //     inner window for no actual benefit.  Furthermore, this function must
  //     only be called from script anyway, which should only have references to
  //     outer windows (and if script has an inner window we've already lost).
  NS_ABORT_IF_FALSE(IsOuterWindow(), "only call this method on outer windows");

  //
  // Window.postMessage is an intentional subversion of the same-origin policy.
  // As such, this code must be particularly careful in the information it
  // exposes to calling code.
  //
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/section-crossDocumentMessages.html
  //

  // First, get the caller's window
  nsRefPtr<nsGlobalWindow> callerInnerWin = CallerInnerWindow();
  if (!callerInnerWin)
    return NS_OK;
  NS_ABORT_IF_FALSE(callerInnerWin->IsInnerWindow(),
                    "should have gotten an inner window here");

  // Compute the caller's origin either from its principal or, in the case the
  // principal doesn't carry a URI (e.g. the system principal), the caller's
  // document.  We must get this now instead of when the event is created and
  // dispatched, because ultimately it is the identity of the calling window
  // *now* that determines who sent the message (and not an identity which might
  // have changed due to intervening navigations).
  nsIPrincipal* callerPrin = callerInnerWin->GetPrincipal();
  if (!callerPrin)
    return NS_OK;
  
  nsCOMPtr<nsIURI> callerOuterURI;
  if (NS_FAILED(callerPrin->GetURI(getter_AddRefs(callerOuterURI))))
    return NS_OK;

  nsAutoString origin;
  if (callerOuterURI) {
    // if the principal has a URI, use that to generate the origin
    nsContentUtils::GetUTFOrigin(callerPrin, origin);
  }
  else {
    // otherwise use the URI of the document to generate origin
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(callerInnerWin->mDocument);
    if (!doc)
      return NS_OK;
    callerOuterURI = doc->GetDocumentURI();
    // if the principal has a URI, use that to generate the origin
    nsContentUtils::GetUTFOrigin(callerOuterURI, origin);
  }

  // Convert the provided origin string into a URI for comparison purposes.
  // "*" indicates no specific origin is required.
  nsCOMPtr<nsIURI> providedOrigin;
  if (!aOrigin.EqualsASCII("*")) {
    if (NS_FAILED(NS_NewURI(getter_AddRefs(providedOrigin), aOrigin)))
      return NS_ERROR_DOM_SYNTAX_ERR;
    if (NS_FAILED(providedOrigin->SetUserPass(EmptyCString())) ||
        NS_FAILED(providedOrigin->SetPath(EmptyCString())))
      return NS_OK;
  }

  // Create and asynchronously dispatch a runnable which will handle actual DOM
  // event creation and dispatch.
  nsRefPtr<PostMessageEvent> event =
    new PostMessageEvent(nsContentUtils::IsCallerChrome()
                         ? nsnull
                         : callerInnerWin->GetOuterWindowInternal(),
                         origin,
                         aMessage,
                         this,
                         providedOrigin,
                         nsContentUtils::IsCallerTrustedForWrite());
  return NS_DispatchToCurrentThread(event);
}

class nsCloseEvent : public nsRunnable {

  nsRefPtr<nsGlobalWindow> mWindow;

  nsCloseEvent(nsGlobalWindow *aWindow)
    : mWindow(aWindow)
  {}

public:

  static nsresult
  PostCloseEvent(nsGlobalWindow* aWindow) {
    nsCOMPtr<nsIRunnable> ev = new nsCloseEvent(aWindow);
    nsresult rv = NS_DispatchToCurrentThread(ev);
    if (NS_SUCCEEDED(rv))
      aWindow->MaybeForgiveSpamCount();
    return rv;
  }

  NS_IMETHOD Run() {
    if (mWindow)
      mWindow->ReallyCloseWindow();
    return NS_OK;
  }

};

PRBool
nsGlobalWindow::CanClose()
{
  if (!mDocShell)
    return PR_TRUE;

  // Ask the content viewer whether the toplevel window can close.
  // If the content viewer returns false, it is responsible for calling
  // Close() as soon as it is possible for the window to close.
  // This allows us to not close the window while printing is happening.

  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  if (cv) {
    PRBool canClose;
    nsresult rv = cv->PermitUnload(PR_FALSE, &canClose);
    if (NS_SUCCEEDED(rv) && !canClose)
      return PR_FALSE;

    rv = cv->RequestWindowClose(&canClose);
    if (NS_SUCCEEDED(rv) && !canClose)
      return PR_FALSE;
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsGlobalWindow::Close()
{
  FORWARD_TO_OUTER(Close, (), NS_ERROR_NOT_INITIALIZED);

  if (IsFrame() || !mDocShell || IsInModalState()) {
    // window.close() is called on a frame in a frameset, on a window
    // that's already closed, or on a window for which there's
    // currently a modal dialog open. Ignore such calls.

    return NS_OK;
  }

  if (mHavePendingClose) {
    // We're going to be closed anyway; do nothing since we don't want
    // to double-close
    return NS_OK;
  }

  if (mBlockScriptedClosingFlag)
  {
    // A script's popup has been blocked and we don't want
    // the window to be closed directly after this event,
    // so the user can see that there was a blocked popup.
    return NS_OK;
  }

  // Don't allow scripts from content to close windows
  // that were not opened by script
  if (!mHadOriginalOpener && !nsContentUtils::IsCallerTrustedForWrite()) {
    PRBool allowClose =
      nsContentUtils::GetBoolPref("dom.allow_scripts_to_close_windows",
                                  PR_TRUE);
    if (!allowClose) {
      // We're blocking the close operation
      // report localized error msg in JS console
      nsContentUtils::ReportToConsole(
          nsContentUtils::eDOM_PROPERTIES,
          "WindowCloseBlockedWarning",
          nsnull, 0, // No params
          nsnull, // No URI.  Not clear which URI we should be using
                  // here anyway
          EmptyString(), 0, 0, // No source, or column/line number
          nsIScriptError::warningFlag,
          "DOM Window");  // Better name for the category?

      return NS_OK;
    }
  }

  if (!mInClose && !mIsClosed && !CanClose())
    return NS_OK;

  // Fire a DOM event notifying listeners that this window is about to
  // be closed. The tab UI code may choose to cancel the default
  // action for this event, if so, we won't actually close the window
  // (since the tab UI code will close the tab in stead). Sure, this
  // could be abused by content code, but do we care? I don't think
  // so...

  PRBool wasInClose = mInClose;
  mInClose = PR_TRUE;

  if (!DispatchCustomEvent("DOMWindowClose")) {
    // Someone chose to prevent the default action for this event, if
    // so, let's not close this window after all...

    mInClose = wasInClose;
    return NS_OK;
  }

  return FinalClose();
}

nsresult
nsGlobalWindow::ForceClose()
{
  if (IsFrame() || !mDocShell) {
    // This may be a frame in a frameset, or a window that's already closed.
    // Ignore such calls.

    return NS_OK;
  }

  if (mHavePendingClose) {
    // We're going to be closed anyway; do nothing since we don't want
    // to double-close
    return NS_OK;
  }

  mInClose = PR_TRUE;

  DispatchCustomEvent("DOMWindowClose");

  return FinalClose();
}

nsresult
nsGlobalWindow::FinalClose()
{
  nsresult rv;
  // Flag that we were closed.
  mIsClosed = PR_TRUE;

  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService(sJSStackContractID);

  JSContext *cx = nsnull;

  if (stack) {
    stack->Peek(&cx);
  }

  if (cx) {
    nsIScriptContext *currentCX = nsJSUtils::GetDynamicScriptContext(cx);

    if (currentCX && currentCX == GetContextInternal()) {
      // We ignore the return value here.  If setting the termination function
      // fails, it's better to fail to close the window than it is to crash
      // (which is what would tend to happen if we did this synchronously
      // here).
      rv = currentCX->SetTerminationFunction(CloseWindow,
                                             static_cast<nsIDOMWindow *>
                                                        (this));
      if (NS_SUCCEEDED(rv)) {
        mHavePendingClose = PR_TRUE;
      }
      return NS_OK;
    }
  }

  
  // We may have plugins on the page that have issued this close from their
  // event loop and because we currently destroy the plugin window with
  // frames, we crash. So, if we are called from Javascript, post an event
  // to really close the window.
  rv = NS_ERROR_FAILURE;
  if (!nsContentUtils::IsCallerChrome()) {
    rv = nsCloseEvent::PostCloseEvent(this);
  }
  
  if (NS_FAILED(rv)) {
    ReallyCloseWindow();
    rv = NS_OK;
  } else {
    mHavePendingClose = PR_TRUE;
  }
  
  return rv;
}


void
nsGlobalWindow::ReallyCloseWindow()
{
  FORWARD_TO_OUTER_VOID(ReallyCloseWindow, ());

  // Make sure we never reenter this method.
  mHavePendingClose = PR_TRUE;

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));

  // If there's no treeOwnerAsWin, this window must already be closed.

  if (treeOwnerAsWin) {

    // but if we're a browser window we could be in some nasty
    // self-destroying cascade that we should mostly ignore

    nsCOMPtr<nsIDocShellTreeItem> docItem(do_QueryInterface(mDocShell));
    if (docItem) {
      nsCOMPtr<nsIBrowserDOMWindow> bwin;
      nsCOMPtr<nsIDocShellTreeItem> rootItem;
      docItem->GetRootTreeItem(getter_AddRefs(rootItem));
      nsCOMPtr<nsIDOMWindow> rootWin(do_GetInterface(rootItem));
      nsCOMPtr<nsIDOMChromeWindow> chromeWin(do_QueryInterface(rootWin));
      if (chromeWin)
        chromeWin->GetBrowserDOMWindow(getter_AddRefs(bwin));

      if (rootWin) {
        /* Normally we destroy the entire window, but not if
           this DOM window belongs to a tabbed browser and doesn't
           correspond to a tab. This allows a well-behaved tab
           to destroy the container as it should but is a final measure
           to prevent an errant tab from doing so when it shouldn't.
           This works because we reach this code when we shouldn't only
           in the particular circumstance that we belong to a tab
           that has just been closed (and is therefore already missing
           from the list of browsers) (and has an unload handler
           that closes the window). */
        // XXXbz now that we have mHavePendingClose, is this needed?
        PRBool isTab = PR_FALSE;
        if (rootWin == this ||
            !bwin || (bwin->IsTabContentWindow(GetOuterWindowInternal(),
                                               &isTab), isTab))
          treeOwnerAsWin->Destroy();
      }
    }

    CleanUp(PR_FALSE);
  }
}

void
nsGlobalWindow::EnterModalState()
{
  nsGlobalWindow* topWin = GetTop();

  if (!topWin) {
    NS_ERROR("Uh, EnterModalState() called w/o a reachable top window?");

    return;
  }

  if (topWin->mModalStateDepth == 0) {
    NS_ASSERTION(!mSuspendedDoc, "Shouldn't have mSuspendedDoc here!");

    mSuspendedDoc = do_QueryInterface(topWin->GetExtantDocument());
    if (mSuspendedDoc && mSuspendedDoc->EventHandlingSuppressed()) {
      mSuspendedDoc->SuppressEventHandling();
    } else {
      mSuspendedDoc = nsnull;
    }
  }
  topWin->mModalStateDepth++;

  JSContext *cx = nsContentUtils::GetCurrentJSContext();

  nsIScriptContext *scx;
  if (cx && (scx = GetScriptContextFromJSContext(cx))) {
    scx->EnterModalState();
  }
}

// static
void
nsGlobalWindow::RunPendingTimeoutsRecursive(nsGlobalWindow *aTopWindow,
                                            nsGlobalWindow *aWindow)
{
  nsGlobalWindow *inner;

  // Return early if we're frozen or have no inner window.
  if (!(inner = aWindow->GetCurrentInnerWindowInternal()) ||
      inner->IsFrozen()) {
    return;
  }

  inner->RunTimeout(nsnull);

  // Check again if we're frozen since running pending timeouts
  // could've frozen us.
  if (inner->IsFrozen()) {
    return;
  }

  nsCOMPtr<nsIDOMWindowCollection> frames;
  aWindow->GetFrames(getter_AddRefs(frames));

  if (!frames) {
    return;
  }

  PRUint32 i, length;
  if (NS_FAILED(frames->GetLength(&length)) || !length) {
    return;
  }

  for (i = 0; i < length && aTopWindow->mModalStateDepth == 0; i++) {
    nsCOMPtr<nsIDOMWindow> child;
    frames->Item(i, getter_AddRefs(child));

    if (!child) {
      return;
    }

    nsGlobalWindow *childWin =
      static_cast<nsGlobalWindow *>
                 (static_cast<nsIDOMWindow *>
                             (child.get()));

    RunPendingTimeoutsRecursive(aTopWindow, childWin);
  }
}

class nsPendingTimeoutRunner : public nsRunnable
{
public:
  nsPendingTimeoutRunner(nsGlobalWindow *aWindow)
    : mWindow(aWindow)
  {
    NS_ASSERTION(mWindow, "mWindow is null.");
  }

  NS_IMETHOD Run()
  {
    nsGlobalWindow::RunPendingTimeoutsRecursive(mWindow, mWindow);

    return NS_OK;
  }

private:
  nsRefPtr<nsGlobalWindow> mWindow;
};

void
nsGlobalWindow::LeaveModalState()
{
  nsGlobalWindow *topWin = GetTop();

  if (!topWin) {
    NS_ERROR("Uh, LeaveModalState() called w/o a reachable top window?");
    return;
  }

  topWin->mModalStateDepth--;

  if (topWin->mModalStateDepth == 0) {
    nsCOMPtr<nsIRunnable> runner = new nsPendingTimeoutRunner(topWin);
    if (NS_FAILED(NS_DispatchToCurrentThread(runner)))
      NS_WARNING("failed to dispatch pending timeout runnable");

    if (mSuspendedDoc) {
      nsCOMPtr<nsIDocument> currentDoc =
        do_QueryInterface(topWin->GetExtantDocument());
      mSuspendedDoc->UnsuppressEventHandlingAndFireEvents(currentDoc == mSuspendedDoc);
      mSuspendedDoc = nsnull;
    }
  }

  JSContext *cx = nsContentUtils::GetCurrentJSContext();

  nsIScriptContext *scx;
  if (cx && (scx = GetScriptContextFromJSContext(cx))) {
    scx->LeaveModalState();
  }

  // Remember the time of the last dialog quit.
  nsGlobalWindow *inner = topWin->GetCurrentInnerWindowInternal();
  if (inner)
    inner->mLastDialogQuitTime = TimeStamp::Now();
}

PRBool
nsGlobalWindow::IsInModalState()
{
  nsGlobalWindow *topWin = GetTop();

  if (!topWin) {
    NS_ERROR("Uh, IsInModalState() called w/o a reachable top window?");

    return PR_FALSE;
  }

  return topWin->mModalStateDepth != 0;
}

// static
void
nsGlobalWindow::NotifyDOMWindowDestroyed(nsGlobalWindow* aWindow) {
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (observerService) {
    observerService->
      NotifyObservers(static_cast<nsIScriptGlobalObject*>(aWindow),
                      DOM_WINDOW_DESTROYED_TOPIC, nsnull);
  }
}

class WindowDestroyedEvent : public nsRunnable
{
public:
  WindowDestroyedEvent(PRUint64 aID, const char* aTopic) :
    mID(aID), mTopic(aTopic) {}

  NS_IMETHOD Run()
  {
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
    if (observerService) {
      nsCOMPtr<nsISupportsPRUint64> wrapper =
        do_CreateInstance(NS_SUPPORTS_PRUINT64_CONTRACTID);
      if (wrapper) {
        wrapper->SetData(mID);
        observerService->NotifyObservers(wrapper, mTopic.get(), nsnull);
      }
    }
    return NS_OK;
  }

private:
  PRUint64 mID;
  nsCString mTopic;
};

void
nsGlobalWindow::NotifyWindowIDDestroyed(const char* aTopic)
{
  nsRefPtr<nsIRunnable> runnable = new WindowDestroyedEvent(mWindowID, aTopic);
  nsresult rv = NS_DispatchToCurrentThread(runnable);
  if (NS_SUCCEEDED(rv)) {
    mNotifiedIDDestroyed = PR_TRUE;
  }
}

void
nsGlobalWindow::InitJavaProperties()
{
  nsIScriptContext *scx = GetContextInternal();

  if (mDidInitJavaProperties || IsOuterWindow() || !scx || !mJSObject) {
    return;
  }

  // Set mDidInitJavaProperties to true here even if initialization
  // can fail. If it fails, we won't try again...
  mDidInitJavaProperties = PR_TRUE;

  // Check whether the plugin supports NPRuntime, if so, init through
  // it.

  nsCOMPtr<nsIPluginHost> host(do_GetService(MOZ_PLUGIN_HOST_CONTRACTID));
  if (!host) {
    return;
  }

  mDummyJavaPluginOwner = new nsDummyJavaPluginOwner(mDoc);
  if (!mDummyJavaPluginOwner) {
    return;
  }

  host->InstantiateDummyJavaPlugin(mDummyJavaPluginOwner);

  // It's possible for us (or the Java plugin, rather) to process
  // events during the above call, which can lead to this window being
  // torn down or what not, so re-check that the dummy plugin is still
  // around.
  if (!mDummyJavaPluginOwner) {
    return;
  }

  nsCOMPtr<nsIPluginInstance> dummyPlugin;
  mDummyJavaPluginOwner->GetInstance(*getter_AddRefs(dummyPlugin));

  if (dummyPlugin) {
    // A dummy plugin was instantiated. This means we have a Java
    // plugin that supports NPRuntime. For such a plugin, the plugin
    // instantiation code defines the Java properties for us, so we're
    // done here.

    return;
  }

  // No NPRuntime enabled Java plugin found, null out the owner we
  // would have used in that case as it's no longer needed.
  mDummyJavaPluginOwner = nsnull;
}

void*
nsGlobalWindow::GetCachedXBLPrototypeHandler(nsXBLPrototypeHandler* aKey)
{
  void* handler = nsnull;
  if (mCachedXBLPrototypeHandlers.IsInitialized()) {
    mCachedXBLPrototypeHandlers.Get(aKey, &handler);
  }
  return handler;
}

void
nsGlobalWindow::CacheXBLPrototypeHandler(nsXBLPrototypeHandler* aKey,
                                         nsScriptObjectHolder& aHandler)
{
  if (!mCachedXBLPrototypeHandlers.IsInitialized() &&
      !mCachedXBLPrototypeHandlers.Init()) {
    NS_ERROR("Failed to initiailize hashtable!");
    return;
  }

  if (!mCachedXBLPrototypeHandlers.Count()) {
    // Can't use macros to get the participant because nsGlobalChromeWindow also
    // runs through this code. Use QueryInterface to get the correct objects.
    nsXPCOMCycleCollectionParticipant* participant;
    CallQueryInterface(this, &participant);
    NS_ASSERTION(participant,
                 "Failed to QI to nsXPCOMCycleCollectionParticipant!");

    nsCOMPtr<nsISupports> thisSupports;
    QueryInterface(NS_GET_IID(nsCycleCollectionISupports),
                   getter_AddRefs(thisSupports));
    NS_ASSERTION(thisSupports, "Failed to QI to nsCycleCollectionISupports!");

    nsresult rv = nsContentUtils::HoldJSObjects(thisSupports, participant);
    if (NS_FAILED(rv)) {
      NS_ERROR("nsContentUtils::HoldJSObjects failed!");
      return;
    }
  }

  mCachedXBLPrototypeHandlers.Put(aKey, aHandler);
}

NS_IMETHODIMP
nsGlobalWindow::GetFrameElement(nsIDOMElement** aFrameElement)
{
  FORWARD_TO_OUTER(GetFrameElement, (aFrameElement), NS_ERROR_NOT_INITIALIZED);

  *aFrameElement = nsnull;

  nsCOMPtr<nsIDocShellTreeItem> docShellTI(do_QueryInterface(mDocShell));

  if (!docShellTI) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShellTreeItem> parent;
  docShellTI->GetSameTypeParent(getter_AddRefs(parent));

  if (!parent || parent == docShellTI) {
    // We're at a chrome boundary, don't expose the chrome iframe
    // element to content code.

    return NS_OK;
  }

  *aFrameElement = mFrameElement;
  NS_IF_ADDREF(*aFrameElement);

  return NS_OK;
}

// Helper for converting window.showModalDialog() options (list of ';'
// separated name (:|=) value pairs) to a format that's parsable by
// our normal window opening code.

void
ConvertDialogOptions(const nsAString& aOptions, nsAString& aResult)
{
  nsAString::const_iterator end;
  aOptions.EndReading(end);

  nsAString::const_iterator iter;
  aOptions.BeginReading(iter);

  while (iter != end) {
    // Skip whitespace.
    while (nsCRT::IsAsciiSpace(*iter) && iter != end) {
      ++iter;
    }

    nsAString::const_iterator name_start = iter;

    // Skip characters until we find whitespace, ';', ':', or '='
    while (iter != end && !nsCRT::IsAsciiSpace(*iter) &&
           *iter != ';' &&
           *iter != ':' &&
           *iter != '=') {
      ++iter;
    }

    nsAString::const_iterator name_end = iter;

    // Skip whitespace.
    while (nsCRT::IsAsciiSpace(*iter) && iter != end) {
      ++iter;
    }

    if (*iter == ';') {
      // No value found, skip the ';' and keep going.
      ++iter;

      continue;
    }

    nsAString::const_iterator value_start = iter;
    nsAString::const_iterator value_end = iter;

    if (*iter == ':' || *iter == '=') {
      // We found name followed by ':' or '='. Look for a value.

      iter++; // Skip the ':' or '='

      // Skip whitespace.
      while (nsCRT::IsAsciiSpace(*iter) && iter != end) {
        ++iter;
      }

      value_start = iter;

      // Skip until we find whitespace, or ';'.
      while (iter != end && !nsCRT::IsAsciiSpace(*iter) &&
             *iter != ';') {
        ++iter;
      }

      value_end = iter;

      // Skip whitespace.
      while (nsCRT::IsAsciiSpace(*iter) && iter != end) {
        ++iter;
      }
    }

    const nsDependentSubstring& name = Substring(name_start, name_end);
    const nsDependentSubstring& value = Substring(value_start, value_end);

    if (name.LowerCaseEqualsLiteral("center")) {
      if (value.LowerCaseEqualsLiteral("on")  ||
          value.LowerCaseEqualsLiteral("yes") ||
          value.LowerCaseEqualsLiteral("1")) {
        aResult.AppendLiteral(",centerscreen=1");
      }
    } else if (name.LowerCaseEqualsLiteral("dialogwidth")) {
      if (!value.IsEmpty()) {
        aResult.AppendLiteral(",width=");
        aResult.Append(value);
      }
    } else if (name.LowerCaseEqualsLiteral("dialogheight")) {
      if (!value.IsEmpty()) {
        aResult.AppendLiteral(",height=");
        aResult.Append(value);
      }
    } else if (name.LowerCaseEqualsLiteral("dialogtop")) {
      if (!value.IsEmpty()) {
        aResult.AppendLiteral(",top=");
        aResult.Append(value);
      }
    } else if (name.LowerCaseEqualsLiteral("dialogleft")) {
      if (!value.IsEmpty()) {
        aResult.AppendLiteral(",left=");
        aResult.Append(value);
      }
    } else if (name.LowerCaseEqualsLiteral("resizable")) {
      if (value.LowerCaseEqualsLiteral("on")  ||
          value.LowerCaseEqualsLiteral("yes") ||
          value.LowerCaseEqualsLiteral("1")) {
        aResult.AppendLiteral(",resizable=1");
      }
    } else if (name.LowerCaseEqualsLiteral("scroll")) {
      if (value.LowerCaseEqualsLiteral("off")  ||
          value.LowerCaseEqualsLiteral("no") ||
          value.LowerCaseEqualsLiteral("0")) {
        aResult.AppendLiteral(",scrollbars=0");
      }
    }

    if (iter == end) {
      break;
    }

    iter++;
  }
}

NS_IMETHODIMP
nsGlobalWindow::ShowModalDialog(const nsAString& aURI, nsIVariant *aArgs,
                                const nsAString& aOptions,
                                nsIVariant **aRetVal)
{
  *aRetVal = nsnull;

  // Before bringing up the window/dialog, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  if (AreDialogsBlocked() || !ConfirmDialogAllowed())
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsIDOMWindow> dlgWin;
  nsAutoString options(NS_LITERAL_STRING("-moz-internal-modal=1,status=1"));

  ConvertDialogOptions(aOptions, options);

  options.AppendLiteral(",scrollbars=1,centerscreen=1,resizable=0");

  EnterModalState();
  nsresult rv = OpenInternal(aURI, EmptyString(), options,
                             PR_FALSE,          // aDialog
                             PR_TRUE,           // aContentModal
                             PR_TRUE,           // aCalledNoScript
                             PR_TRUE,           // aDoJSFixups
                             nsnull, aArgs,     // args
                             GetPrincipal(),    // aCalleePrincipal
                             nsnull,            // aJSCallerContext
                             getter_AddRefs(dlgWin));
  LeaveModalState();

  NS_ENSURE_SUCCESS(rv, rv);
  
  if (dlgWin) {
    nsCOMPtr<nsIPrincipal> subjectPrincipal;
    rv = nsContentUtils::GetSecurityManager()->
      GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));
    if (NS_FAILED(rv)) {
      return rv;
    }

    PRBool canAccess = PR_TRUE;

    if (subjectPrincipal) {
      nsCOMPtr<nsIScriptObjectPrincipal> objPrincipal =
        do_QueryInterface(dlgWin);
      nsCOMPtr<nsIPrincipal> dialogPrincipal;

      if (objPrincipal) {
        dialogPrincipal = objPrincipal->GetPrincipal();

        rv = subjectPrincipal->Subsumes(dialogPrincipal, &canAccess);
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        // Uh, not sure what kind of dialog this is. Prevent access to
        // be on the safe side...

        canAccess = PR_FALSE;
      }
    }

    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(dlgWin));

    if (canAccess) {
      nsPIDOMWindow *inner = win->GetCurrentInnerWindow();

      nsCOMPtr<nsIDOMModalContentWindow> dlgInner(do_QueryInterface(inner));

      if (dlgInner) {
        dlgInner->GetReturnValue(aRetVal);
      }
    }

    nsRefPtr<nsGlobalWindow> winInternal =
      static_cast<nsGlobalWindow*>(win.get());
    if (winInternal->mCallCleanUpAfterModalDialogCloses) {
      winInternal->mCallCleanUpAfterModalDialogCloses = PR_FALSE;
      winInternal->CleanUp(PR_TRUE);
    }
  }
  
  return NS_OK;
}

class CommandDispatcher : public nsRunnable
{
public:
  CommandDispatcher(nsIDOMXULCommandDispatcher* aDispatcher,
                    const nsAString& aAction)
  : mDispatcher(aDispatcher), mAction(aAction) {}

  NS_IMETHOD Run()
  {
    return mDispatcher->UpdateCommands(mAction);
  }

  nsCOMPtr<nsIDOMXULCommandDispatcher> mDispatcher;
  nsString                             mAction;
};

NS_IMETHODIMP
nsGlobalWindow::UpdateCommands(const nsAString& anAction)
{
  nsPIDOMWindow *rootWindow = nsGlobalWindow::GetPrivateRoot();
  if (!rootWindow)
    return NS_OK;

  nsCOMPtr<nsIDOMXULDocument> xulDoc =
    do_QueryInterface(rootWindow->GetExtantDocument());
  // See if we contain a XUL document.
  if (xulDoc) {
    // Retrieve the command dispatcher and call updateCommands on it.
    nsCOMPtr<nsIDOMXULCommandDispatcher> xulCommandDispatcher;
    xulDoc->GetCommandDispatcher(getter_AddRefs(xulCommandDispatcher));
    if (xulCommandDispatcher) {
      nsContentUtils::AddScriptRunner(new CommandDispatcher(xulCommandDispatcher,
                                                            anAction));
    }
  }

  return NS_OK;
}

PRBool
nsGlobalWindow::GetBlurSuppression()
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  PRBool suppress = PR_FALSE;
  if (treeOwnerAsWin)
    treeOwnerAsWin->GetBlurSuppression(&suppress);
  return suppress;
}

NS_IMETHODIMP
nsGlobalWindow::GetSelection(nsISelection** aSelection)
{
  FORWARD_TO_OUTER(GetSelection, (aSelection), NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_ARG_POINTER(aSelection);
  *aSelection = nsnull;

  if (!mDocShell)
    return NS_OK;

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));

  if (!presShell)
    return NS_OK;
    
  *aSelection = presShell->GetCurrentSelection(nsISelectionController::SELECTION_NORMAL);
  
  NS_IF_ADDREF(*aSelection);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::Find(const nsAString& aStr, PRBool aCaseSensitive,
                     PRBool aBackwards, PRBool aWrapAround, PRBool aWholeWord,
                     PRBool aSearchInFrames, PRBool aShowDialog,
                     PRBool *aDidFind)
{
  FORWARD_TO_OUTER(Find, (aStr, aCaseSensitive, aBackwards, aWrapAround,
                          aWholeWord, aSearchInFrames, aShowDialog, aDidFind),
                   NS_ERROR_NOT_INITIALIZED);

  nsresult rv = NS_OK;
  *aDidFind = PR_FALSE;

  nsCOMPtr<nsIWebBrowserFind> finder(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(finder, NS_ERROR_FAILURE);

  // Set the options of the search
  rv = finder->SetSearchString(PromiseFlatString(aStr).get());
  NS_ENSURE_SUCCESS(rv, rv);
  finder->SetMatchCase(aCaseSensitive);
  finder->SetFindBackwards(aBackwards);
  finder->SetWrapFind(aWrapAround);
  finder->SetEntireWord(aWholeWord);
  finder->SetSearchFrames(aSearchInFrames);

  // the nsIWebBrowserFind is initialized to use this window
  // as the search root, but uses focus to set the current search
  // frame. If we're being called from JS (as here), this window
  // should be the current search frame.
  nsCOMPtr<nsIWebBrowserFindInFrames> framesFinder(do_QueryInterface(finder));
  if (framesFinder) {
    framesFinder->SetRootSearchFrame(this);   // paranoia
    framesFinder->SetCurrentSearchFrame(this);
  }
  
  // The Find API does not accept empty strings. Launch the Find Dialog.
  if (aStr.IsEmpty() || aShowDialog) {
    // See if the find dialog is already up using nsIWindowMediator
    nsCOMPtr<nsIWindowMediator> windowMediator =
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);

    nsCOMPtr<nsIDOMWindowInternal> findDialog;

    if (windowMediator) {
      windowMediator->GetMostRecentWindow(NS_LITERAL_STRING("findInPage").get(),
                                          getter_AddRefs(findDialog));
    }

    if (findDialog) {
      // The Find dialog is already open, bring it to the top.
      rv = findDialog->Focus();
    } else { // Open a Find dialog
      if (finder) {
        nsCOMPtr<nsIDOMWindow> dialog;
        rv = OpenDialog(NS_LITERAL_STRING("chrome://global/content/finddialog.xul"),
                        NS_LITERAL_STRING("_blank"),
                        NS_LITERAL_STRING("chrome, resizable=no, dependent=yes"),
                        finder, getter_AddRefs(dialog));
      }
    }
  } else {
    // Launch the search with the passed in search string
    rv = finder->FindNext(aDidFind);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return rv;
}

static PRBool
Is8bit(const nsAString& aString)
{
  static const PRUnichar EIGHT_BIT = PRUnichar(~0x00FF);

  nsAString::const_iterator done_reading;
  aString.EndReading(done_reading);

  // for each chunk of |aString|...
  PRUint32 fragmentLength = 0;
  nsAString::const_iterator iter;
  for (aString.BeginReading(iter); iter != done_reading;
       iter.advance(PRInt32(fragmentLength))) {
    fragmentLength = PRUint32(iter.size_forward());
    const PRUnichar* c = iter.get();
    const PRUnichar* fragmentEnd = c + fragmentLength;

    // for each character in this chunk...
    while (c < fragmentEnd)
      if (*c++ & EIGHT_BIT)
        return PR_FALSE;
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsGlobalWindow::Atob(const nsAString& aAsciiBase64String,
                     nsAString& aBinaryData)
{
  aBinaryData.Truncate();

  if (!Is8bit(aAsciiBase64String)) {
    return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
  }

  PRUint32 dataLen = aAsciiBase64String.Length();

  NS_LossyConvertUTF16toASCII base64(aAsciiBase64String);
  if (base64.Length() != dataLen) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRInt32 resultLen = dataLen;
  if (!base64.IsEmpty() && base64[dataLen - 1] == '=') {
    if (base64.Length() > 1 && base64[dataLen - 2] == '=') {
      resultLen = dataLen - 2;
    } else {
      resultLen = dataLen - 1;
    }
  }

  resultLen = ((resultLen * 3) / 4);
  // Add 4 extra bytes (one is needed for sure for null termination)
  // to the malloc size just to make sure we don't end up writing past
  // the allocated memory (the PL_Base64Decode API should really
  // provide a guaranteed way to figure this out w/o needing to do the
  // above yourself).
  char *dest = static_cast<char *>(nsMemory::Alloc(resultLen + 4));
  if (!dest) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  char *bin_data = PL_Base64Decode(base64.get(), dataLen, dest);

  nsresult rv = NS_OK;

  if (bin_data) {
    CopyASCIItoUTF16(Substring(bin_data, bin_data + resultLen), aBinaryData);
  } else {
    rv = NS_ERROR_DOM_INVALID_CHARACTER_ERR;
  }

  nsMemory::Free(dest);

  return rv;
}

NS_IMETHODIMP
nsGlobalWindow::Btoa(const nsAString& aBinaryData,
                     nsAString& aAsciiBase64String)
{
  aAsciiBase64String.Truncate();

  if (!Is8bit(aBinaryData)) {
    return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
  }

  char *bin_data = ToNewCString(aBinaryData);
  if (!bin_data) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRInt32 resultLen = ((aBinaryData.Length() + 2) / 3) * 4;

  char *base64 = PL_Base64Encode(bin_data, aBinaryData.Length(), nsnull);
  if (!base64) {
    nsMemory::Free(bin_data);

    return NS_ERROR_OUT_OF_MEMORY;
  }

  CopyASCIItoUTF16(nsDependentCString(base64, resultLen), aAsciiBase64String);

  PR_Free(base64);
  nsMemory::Free(bin_data);

  return NS_OK;
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMEventTarget
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::AddEventListener(const nsAString& aType,
                                 nsIDOMEventListener* aListener,
                                 PRBool aUseCapture)
{
  FORWARD_TO_INNER_CREATE(AddEventListener, (aType, aListener, aUseCapture),
                          NS_ERROR_NOT_AVAILABLE);

  return AddEventListener(aType, aListener, aUseCapture, PR_FALSE, 0);
}

NS_IMETHODIMP
nsGlobalWindow::RemoveEventListener(const nsAString& aType,
                                    nsIDOMEventListener* aListener,
                                    PRBool aUseCapture)
{
  return RemoveGroupedEventListener(aType, aListener, aUseCapture, nsnull);
}

NS_IMETHODIMP
nsGlobalWindow::DispatchEvent(nsIDOMEvent* aEvent, PRBool* _retval)
{
  FORWARD_TO_INNER(DispatchEvent, (aEvent, _retval), NS_OK);

  if (!mDoc) {
    return NS_ERROR_FAILURE;
  }

  // Obtain a presentation shell
  nsIPresShell *shell = mDoc->GetShell();
  nsRefPtr<nsPresContext> presContext;
  if (shell) {
    // Retrieve the context
    presContext = shell->GetPresContext();
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv =
    nsEventDispatcher::DispatchDOMEvent(GetOuterWindow(), nsnull, aEvent,
                                        presContext, &status);

  *_retval = (status != nsEventStatus_eConsumeNoDefault);
  return rv;
}

//*****************************************************************************
// nsGlobalWindow::nsIDOM3EventTarget
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::AddGroupedEventListener(const nsAString & aType,
                                        nsIDOMEventListener *aListener,
                                        PRBool aUseCapture,
                                        nsIDOMEventGroup *aEvtGrp)
{
  FORWARD_TO_INNER_CREATE(AddGroupedEventListener,
                          (aType, aListener, aUseCapture, aEvtGrp),
                          NS_ERROR_NOT_AVAILABLE);

  nsIEventListenerManager* manager = GetListenerManager(PR_TRUE);
  NS_ENSURE_STATE(manager);
  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;
  return manager->AddEventListenerByType(aListener, aType, flags, aEvtGrp);
}

NS_IMETHODIMP
nsGlobalWindow::RemoveGroupedEventListener(const nsAString & aType,
                                           nsIDOMEventListener *aListener,
                                           PRBool aUseCapture,
                                           nsIDOMEventGroup *aEvtGrp)
{
  FORWARD_TO_INNER(RemoveGroupedEventListener,
                   (aType, aListener, aUseCapture, aEvtGrp),
                   NS_ERROR_NOT_INITIALIZED);

  if (mListenerManager) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    mListenerManager->RemoveEventListenerByType(aListener, aType, flags,
                                                aEvtGrp);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::CanTrigger(const nsAString & type, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGlobalWindow::IsRegisteredHere(const nsAString & type, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGlobalWindow::AddEventListener(const nsAString& aType,
                                 nsIDOMEventListener *aListener,
                                 PRBool aUseCapture, PRBool aWantsUntrusted,
                                 PRUint8 optional_argc)
{
  NS_ASSERTION(!aWantsUntrusted || optional_argc > 0,
               "Won't check if this is chrome, you want to set "
               "aWantsUntrusted to PR_FALSE or make the aWantsUntrusted "
               "explicit by making optional_argc non-zero.");

  if (IsOuterWindow() && mInnerWindow &&
      !nsContentUtils::CanCallerAccess(mInnerWindow)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsIEventListenerManager* manager = GetListenerManager(PR_TRUE);
  NS_ENSURE_STATE(manager);

  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

  if (aWantsUntrusted ||
      (optional_argc == 0 && !nsContentUtils::IsChromeDoc(mDoc))) {
    flags |= NS_PRIV_EVENT_UNTRUSTED_PERMITTED;
  }

  return manager->AddEventListenerByType(aListener, aType, flags, nsnull);
}

nsresult
nsGlobalWindow::AddEventListenerByIID(nsIDOMEventListener* aListener,
                                      const nsIID& aIID)
{
  nsIEventListenerManager* manager = GetListenerManager(PR_TRUE);
  NS_ENSURE_STATE(manager);
  return manager->AddEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
}

nsresult
nsGlobalWindow::RemoveEventListenerByIID(nsIDOMEventListener* aListener,
                                         const nsIID& aIID)
{
  FORWARD_TO_INNER(RemoveEventListenerByIID, (aListener, aIID),
                   NS_ERROR_NOT_INITIALIZED);

  if (mListenerManager) {
    mListenerManager->RemoveEventListenerByIID(aListener, aIID,
                                               NS_EVENT_FLAG_BUBBLE);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsIEventListenerManager*
nsGlobalWindow::GetListenerManager(PRBool aCreateIfNotFound)
{
  FORWARD_TO_INNER_CREATE(GetListenerManager, (aCreateIfNotFound), nsnull);

  if (!mListenerManager) {
    if (!aCreateIfNotFound) {
      return nsnull;
    }

    static NS_DEFINE_CID(kEventListenerManagerCID,
                         NS_EVENTLISTENERMANAGER_CID);

    mListenerManager = do_CreateInstance(kEventListenerManagerCID);
    if (mListenerManager) {
      mListenerManager->SetListenerTarget(
        static_cast<nsPIDOMEventTarget*>(this));
    }
  }

  return mListenerManager;
}

nsresult
nsGlobalWindow::GetSystemEventGroup(nsIDOMEventGroup **aGroup)
{
  nsIEventListenerManager* manager = GetListenerManager(PR_TRUE);
  NS_ENSURE_STATE(manager);
  return manager->GetSystemEventGroupLM(aGroup);
}

nsIScriptContext*
nsGlobalWindow::GetContextForEventHandlers(nsresult* aRv)
{
  nsIScriptContext* scx = GetContext();
  *aRv = scx ? NS_OK : NS_ERROR_UNEXPECTED;
  return scx;
}

//*****************************************************************************
// nsGlobalWindow::nsPIDOMWindow
//*****************************************************************************

nsPIDOMWindow*
nsGlobalWindow::GetPrivateParent()
{
  FORWARD_TO_OUTER(GetPrivateParent, (), nsnull);

  nsCOMPtr<nsIDOMWindow> parent;
  GetParent(getter_AddRefs(parent));

  if (static_cast<nsIDOMWindow *>(this) == parent.get()) {
    nsCOMPtr<nsIContent> chromeElement(do_QueryInterface(mChromeEventHandler));
    if (!chromeElement)
      return nsnull;             // This is ok, just means a null parent.

    nsIDocument* doc = chromeElement->GetDocument();
    if (!doc)
      return nsnull;             // This is ok, just means a null parent.

    nsIScriptGlobalObject *globalObject = doc->GetScriptGlobalObject();
    if (!globalObject)
      return nsnull;             // This is ok, just means a null parent.

    parent = do_QueryInterface(globalObject);
  }

  if (parent) {
    return static_cast<nsGlobalWindow *>
                      (static_cast<nsIDOMWindow*>(parent.get()));
  }

  return nsnull;
}

nsPIDOMWindow*
nsGlobalWindow::GetPrivateRoot()
{
  FORWARD_TO_OUTER(GetPrivateRoot, (), nsnull);

  nsCOMPtr<nsIDOMWindow> top;
  GetTop(getter_AddRefs(top));

  nsCOMPtr<nsPIDOMWindow> ptop = do_QueryInterface(top);
  NS_ASSERTION(ptop, "cannot get ptop");
  if (!ptop)
    return nsnull;

  nsIDocShell *docShell = ptop->GetDocShell();

  // Get the chrome event handler from the doc shell, since we only
  // want to deal with XUL chrome handlers and not the new kind of
  // window root handler.
  nsCOMPtr<nsIDOMEventTarget> chromeEventHandler;
  docShell->GetChromeEventHandler(getter_AddRefs(chromeEventHandler));

  nsCOMPtr<nsIContent> chromeElement(do_QueryInterface(mChromeEventHandler));
  if (chromeElement) {
    nsIDocument* doc = chromeElement->GetDocument();
    if (doc) {
      nsIDOMWindow *parent = doc->GetWindow();
      if (parent) {
        parent->GetTop(getter_AddRefs(top));
      }
    }
  }

  return static_cast<nsGlobalWindow *>
                    (static_cast<nsIDOMWindow *>(top));
}


NS_IMETHODIMP
nsGlobalWindow::GetLocation(nsIDOMLocation ** aLocation)
{
  FORWARD_TO_INNER(GetLocation, (aLocation), NS_ERROR_NOT_INITIALIZED);

  *aLocation = nsnull;

  nsIDocShell *docShell = GetDocShell();
  if (!mLocation && docShell) {
    mLocation = new nsLocation(docShell);
    if (!mLocation) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_IF_ADDREF(*aLocation = mLocation);

  return NS_OK;
}

void
nsGlobalWindow::ActivateOrDeactivate(PRBool aActivate)
{
  // Set / unset mIsActive on the top level window, which is used for the
  // :-moz-window-inactive pseudoclass.
  nsCOMPtr<nsIWidget> mainWidget = GetMainWidget();
  if (!mainWidget)
    return;

  // Get the top level widget (if the main widget is a sheet, this will
  // be the sheet's top (non-sheet) parent).
  nsCOMPtr<nsIWidget> topLevelWidget = mainWidget->GetSheetWindowParent();
  if (!topLevelWidget) {
    topLevelWidget = mainWidget;
  }

  // Get the top level widget's nsGlobalWindow
  nsCOMPtr<nsIDOMWindowInternal> topLevelWindow;
  if (topLevelWidget == mainWidget) {
    topLevelWindow = static_cast<nsIDOMWindowInternal*>(this);
  } else {
    // This is a workaround for the following problem:
    // When a window with an open sheet loses focus, only the sheet window
    // receives the NS_DEACTIVATE event. However, it's not the sheet that
    // should lose the active styling, but the containing top level window.
    void* clientData;
    topLevelWidget->GetClientData(clientData); // clientData is nsXULWindow
    nsISupports* data = static_cast<nsISupports*>(clientData);
    nsCOMPtr<nsIInterfaceRequestor> req(do_QueryInterface(data));
    topLevelWindow = do_GetInterface(req);
  }
  if (topLevelWindow) {
    nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(topLevelWindow));
    piWin->SetActive(aActivate);
  }
}

static PRBool
NotifyDocumentTree(nsIDocument* aDocument, void* aData)
{
  aDocument->EnumerateSubDocuments(NotifyDocumentTree, nsnull);
  aDocument->DocumentStatesChanged(NS_DOCUMENT_STATE_WINDOW_INACTIVE);
  return PR_TRUE;
}

void
nsGlobalWindow::SetActive(PRBool aActive)
{
  nsPIDOMWindow::SetActive(aActive);
  NotifyDocumentTree(mDoc, nsnull);
}

void nsGlobalWindow::MaybeUpdateTouchState()
{
  FORWARD_TO_INNER_VOID(MaybeUpdateTouchState, ());

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();

  nsCOMPtr<nsIDOMWindow> focusedWindow;
  fm->GetFocusedWindow(getter_AddRefs(focusedWindow));

  if(this == focusedWindow) {
    UpdateTouchState();
  }
}

void nsGlobalWindow::UpdateTouchState()
{
  FORWARD_TO_INNER_VOID(UpdateTouchState, ());

  nsCOMPtr<nsIWidget> mainWidget = GetMainWidget();
  if (!mainWidget)
    return;

  if (mMayHaveTouchEventListener) {
    mainWidget->RegisterTouchWindow();
  } else {
    mainWidget->UnregisterTouchWindow();
  }
}

void
nsGlobalWindow::SetChromeEventHandler(nsPIDOMEventTarget* aChromeEventHandler)
{
  SetChromeEventHandlerInternal(aChromeEventHandler);
  if (IsOuterWindow()) {
    // update the chrome event handler on all our inner windows
    for (nsGlobalWindow *inner = (nsGlobalWindow *)PR_LIST_HEAD(this);
         inner != this;
         inner = (nsGlobalWindow*)PR_NEXT_LINK(inner)) {
      NS_ASSERTION(inner->mOuterWindow == this, "bad outer window pointer");
      inner->SetChromeEventHandlerInternal(aChromeEventHandler);
    }
  } else if (mOuterWindow) {
    // Need the cast to be able to call the protected method on a
    // superclass. We could make the method public instead, but it's really
    // better this way.
    static_cast<nsGlobalWindow*>(mOuterWindow)->
      SetChromeEventHandlerInternal(aChromeEventHandler);
  }
}

static PRBool IsLink(nsIContent* aContent)
{
  nsCOMPtr<nsIDOMHTMLAnchorElement> anchor = do_QueryInterface(aContent);
  return (anchor || (aContent &&
                     aContent->AttrValueIs(kNameSpaceID_XLink, nsGkAtoms::type,
                                           nsGkAtoms::simple, eCaseMatters)));
}

void
nsGlobalWindow::SetFocusedNode(nsIContent* aNode,
                               PRUint32 aFocusMethod,
                               PRBool aNeedsFocus)
{
  FORWARD_TO_INNER_VOID(SetFocusedNode, (aNode, aFocusMethod, aNeedsFocus));

  NS_ASSERTION(!aNode || aNode->GetCurrentDoc() == mDoc,
               "setting focus to a node from the wrong document");

  if (mFocusedNode != aNode) {
    UpdateCanvasFocus(PR_FALSE, aNode);
    mFocusedNode = aNode;
    mFocusMethod = aFocusMethod & FOCUSMETHOD_MASK;
    mShowFocusRingForContent = PR_FALSE;
  }

  if (mFocusedNode) {
    // if a node was focused by a keypress, turn on focus rings for the
    // window.
    if (mFocusMethod & nsIFocusManager::FLAG_BYKEY) {
      mFocusByKeyOccurred = PR_TRUE;
    } else if (
      // otherwise, we set mShowFocusRingForContent, as we don't want this to
      // be permanent for the window. On Windows, focus rings are only shown
      // when the FLAG_SHOWRING flag is used. On other platforms, focus rings
      // are only hidden for clicks on links.
#ifndef XP_WIN
      !(mFocusMethod & nsIFocusManager::FLAG_BYMOUSE) || !IsLink(aNode) ||
#endif
      aFocusMethod & nsIFocusManager::FLAG_SHOWRING) {
        mShowFocusRingForContent = PR_TRUE;
    }
  }

  if (aNeedsFocus)
    mNeedsFocus = aNeedsFocus;
}

PRUint32
nsGlobalWindow::GetFocusMethod()
{
  FORWARD_TO_INNER(GetFocusMethod, (), 0);

  return mFocusMethod;
}

PRBool
nsGlobalWindow::ShouldShowFocusRing()
{
  FORWARD_TO_INNER(ShouldShowFocusRing, (), PR_FALSE);

  return mShowFocusRings || mShowFocusRingForContent || mFocusByKeyOccurred;
}

void
nsGlobalWindow::SetKeyboardIndicators(UIStateChangeType aShowAccelerators,
                                      UIStateChangeType aShowFocusRings)
{
  FORWARD_TO_INNER_VOID(SetKeyboardIndicators, (aShowAccelerators, aShowFocusRings));

  // only change the flags that have been modified
  if (aShowAccelerators != UIStateChangeType_NoChange)
    mShowAccelerators = aShowAccelerators == UIStateChangeType_Set;
  if (aShowFocusRings != UIStateChangeType_NoChange)
    mShowFocusRings = aShowFocusRings == UIStateChangeType_Set;

  // propagate the indicators to child windows
  nsCOMPtr<nsIDocShellTreeNode> node = do_QueryInterface(GetDocShell());
  if (node) {
    PRInt32 childCount = 0;
    node->GetChildCount(&childCount);

    for (PRInt32 i = 0; i < childCount; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> childShell;
      node->GetChildAt(i, getter_AddRefs(childShell));
      nsCOMPtr<nsPIDOMWindow> childWindow = do_GetInterface(childShell);
      if (childWindow) {
        childWindow->SetKeyboardIndicators(aShowAccelerators, aShowFocusRings);
      }
    }
  }

  if (mHasFocus) {
    // send content state notifications
    nsCOMPtr<nsPresContext> presContext;
    if (mDocShell) {
      mDocShell->GetPresContext(getter_AddRefs(presContext));
      if (presContext) {
        presContext->EventStateManager()->
          SetContentState(mFocusedNode, NS_EVENT_STATE_FOCUS);
      }
    }
  }
}

void
nsGlobalWindow::GetKeyboardIndicators(PRBool* aShowAccelerators,
                                      PRBool* aShowFocusRings)
{
  FORWARD_TO_INNER_VOID(GetKeyboardIndicators, (aShowAccelerators, aShowFocusRings));

  *aShowAccelerators = mShowAccelerators;
  *aShowFocusRings = mShowFocusRings;
}

PRBool
nsGlobalWindow::TakeFocus(PRBool aFocus, PRUint32 aFocusMethod)
{
  FORWARD_TO_INNER(TakeFocus, (aFocus, aFocusMethod), PR_FALSE);

  if (aFocus)
    mFocusMethod = aFocusMethod & FOCUSMETHOD_MASK;

  if (mHasFocus != aFocus) {
    mHasFocus = aFocus;
    UpdateCanvasFocus(PR_TRUE, mFocusedNode);
  }

  // if mNeedsFocus is true, then the document has not yet received a
  // document-level focus event. If there is a root content node, then return
  // true to tell the calling focus manager that a focus event is expected. If
  // there is no root content node, the document hasn't loaded enough yet, or
  // there isn't one and there is no point in firing a focus event.
  if (aFocus && mNeedsFocus && mDoc && mDoc->GetRootElement() != nsnull) {
    mNeedsFocus = PR_FALSE;
    return PR_TRUE;
  }

  mNeedsFocus = PR_FALSE;
  return PR_FALSE;
}

void
nsGlobalWindow::SetReadyForFocus()
{
  FORWARD_TO_INNER_VOID(SetReadyForFocus, ());

  PRBool oldNeedsFocus = mNeedsFocus;
  mNeedsFocus = PR_FALSE;

  // update whether focus rings need to be shown using the state from the
  // root window
  nsPIDOMWindow* root = GetPrivateRoot();
  if (root) {
    PRBool showAccelerators, showFocusRings;
    root->GetKeyboardIndicators(&showAccelerators, &showFocusRings);
    mShowFocusRings = showFocusRings;
  }

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm)
    fm->WindowShown(this, oldNeedsFocus);
}

void
nsGlobalWindow::PageHidden()
{
  FORWARD_TO_INNER_VOID(PageHidden, ());

  // the window is being hidden, so tell the focus manager that the frame is
  // no longer valid. Use the persisted field to determine if the document
  // is being destroyed.

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm)
    fm->WindowHidden(this);

  mNeedsFocus = PR_TRUE;
}

nsresult
nsGlobalWindow::DispatchSyncHashchange()
{
  FORWARD_TO_INNER(DispatchSyncHashchange, (), NS_OK);
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Must be safe to run script here.");

  // Don't do anything if the window is frozen.
  if (IsFrozen())
    return NS_OK;

  // Dispatch the hashchange event, which doesn't bubble and isn't cancelable,
  // to the outer window.
  return nsContentUtils::DispatchTrustedEvent(mDoc, GetOuterWindow(),
                                              NS_LITERAL_STRING("hashchange"),
                                              PR_FALSE, PR_FALSE);
}

nsresult
nsGlobalWindow::DispatchSyncPopState()
{
  FORWARD_TO_INNER(DispatchSyncPopState, (), NS_OK);

  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Must be safe to run script here.");

  // Check that PopState hasn't been pref'ed off.
  if (!nsContentUtils::GetBoolPref(sPopStatePrefStr, PR_FALSE))
    return NS_OK;

  nsresult rv = NS_OK;

  // Bail if the window is frozen.
  if (IsFrozen()) {
    return NS_OK;
  }

  // Bail if there's no document or the document's readystate isn't "complete".
  if (!mDoc) {
    return NS_OK;
  }

  nsIDocument::ReadyState readyState = mDoc->GetReadyStateEnum();
  if (readyState != nsIDocument::READYSTATE_COMPLETE) {
    return NS_OK;
  }

  // Get the document's pending state object -- it contains the data we're
  // going to send along with the popstate event.  The object is serialized as
  // JSON.
  nsAString& stateObjJSON = mDoc->GetPendingStateObject();

  nsCOMPtr<nsIVariant> stateObj;
  // Parse the JSON, if there's any to parse.
  if (!stateObjJSON.IsEmpty()) {
    // Get the JSContext associated with our document. We need this for
    // deserialization.
    nsCOMPtr<nsIDocument> document = do_QueryInterface(mDocument);
    NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

    // Get the JSContext from the document, like we do in
    // nsContentUtils::GetContextFromDocument().
    nsIScriptGlobalObject *sgo = document->GetScopeObject();
    NS_ENSURE_TRUE(sgo, NS_ERROR_FAILURE);

    nsIScriptContext *scx = sgo->GetContext();
    NS_ENSURE_TRUE(scx, NS_ERROR_FAILURE);

    JSContext *cx = (JSContext*) scx->GetNativeContext();

    // If our json call triggers a JS-to-C++ call, we want that call to use cx
    // as the context.  So we push cx onto the context stack.
    nsCxPusher cxPusher;

    jsval jsStateObj = JSVAL_NULL;
    // Root the container which will hold our decoded state object.
    nsAutoGCRoot root(&jsStateObj, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Deserialize the state object into an nsIVariant.
    nsCOMPtr<nsIJSON> json = do_GetService("@mozilla.org/dom/json;1");
    NS_ENSURE_TRUE(cxPusher.Push(cx), NS_ERROR_FAILURE);
    rv = json->DecodeToJSVal(stateObjJSON, cx, &jsStateObj);
    NS_ENSURE_SUCCESS(rv, rv);
    cxPusher.Pop();

    nsCOMPtr<nsIXPConnect> xpconnect = do_GetService(nsIXPConnect::GetCID());
    NS_ENSURE_TRUE(xpconnect, NS_ERROR_FAILURE);
    rv = xpconnect->JSValToVariant(cx, &jsStateObj, getter_AddRefs(stateObj));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Obtain a presentation shell for use in creating a popstate event.
  nsIPresShell *shell = mDoc->GetShell();
  nsRefPtr<nsPresContext> presContext;
  if (shell) {
    presContext = shell->GetPresContext();
  }

  // Create a new popstate event
  nsCOMPtr<nsIDOMEvent> domEvent;
  rv = nsEventDispatcher::CreateEvent(presContext, nsnull,
                                      NS_LITERAL_STRING("popstateevent"),
                                      getter_AddRefs(domEvent));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(domEvent);
  NS_ENSURE_TRUE(privateEvent, NS_ERROR_FAILURE);

  // Initialize the popstate event, which does bubble but isn't cancellable.
  nsCOMPtr<nsIDOMPopStateEvent> popstateEvent = do_QueryInterface(domEvent);
  rv = popstateEvent->InitPopStateEvent(NS_LITERAL_STRING("popstate"),
                                        PR_TRUE, PR_FALSE,
                                        stateObj);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = privateEvent->SetTrusted(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMEventTarget> outerWindow =
    do_QueryInterface(GetOuterWindow());
  NS_ENSURE_TRUE(outerWindow, NS_ERROR_UNEXPECTED);

  rv = privateEvent->SetTarget(outerWindow);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool dummy; // default action
  return DispatchEvent(popstateEvent, &dummy);
}

// Find an nsICanvasFrame under aFrame.  Only search the principal
// child lists.  aFrame must be non-null.
static nsCanvasFrame* FindCanvasFrame(nsIFrame* aFrame)
{
    nsCanvasFrame* canvasFrame = do_QueryFrame(aFrame);
    if (canvasFrame) {
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
void
nsGlobalWindow::UpdateCanvasFocus(PRBool aFocusChanged, nsIContent* aNewContent)
{
  // this is called from the inner window so use GetDocShell
  nsIDocShell* docShell = GetDocShell();
  if (!docShell)
    return;

  nsCOMPtr<nsIEditorDocShell> editorDocShell = do_QueryInterface(docShell);
  if (editorDocShell) {
    PRBool editable;
    editorDocShell->GetEditable(&editable);
    if (editable)
      return;
  }

  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
  if (!presShell || !mDocument)
    return;

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
  Element *rootElement = doc->GetRootElement();
  if (rootElement) {
      if ((mHasFocus || aFocusChanged) &&
          (mFocusedNode == rootElement || aNewContent == rootElement)) {
          nsIFrame* frame = rootElement->GetPrimaryFrame();
          if (frame) {
              frame = frame->GetParent();
              nsCanvasFrame* canvasFrame = do_QueryFrame(frame);
              if (canvasFrame) {
                  canvasFrame->SetHasFocus(mHasFocus && rootElement == aNewContent);
              }
          }
      }
  } else {
      // Look for the frame the hard way
      nsIFrame* frame = presShell->GetRootFrame();
      if (frame) {
          nsCanvasFrame* canvasFrame = FindCanvasFrame(frame);
          if (canvasFrame) {
              canvasFrame->SetHasFocus(PR_FALSE);
          }
      }      
  }
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMViewCSS
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::GetComputedStyle(nsIDOMElement* aElt,
                                 const nsAString& aPseudoElt,
                                 nsIDOMCSSStyleDeclaration** aReturn)
{
  FORWARD_TO_OUTER(GetComputedStyle, (aElt, aPseudoElt, aReturn),
                   NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  if (!aElt) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  if (!mDocShell) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));

  if (!presShell) {
    return NS_OK;
  }

  nsRefPtr<nsComputedDOMStyle> compStyle;
  nsresult rv = NS_NewComputedDOMStyle(aElt, aPseudoElt, presShell,
                                       getter_AddRefs(compStyle));
  NS_ENSURE_SUCCESS(rv, rv);

  *aReturn = compStyle.forget().get();

  return NS_OK;
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMAbstractView
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::GetDocument(nsIDOMDocumentView ** aDocumentView)
{
  NS_ENSURE_ARG_POINTER(aDocumentView);

  nsresult rv = NS_OK;

  if (mDocument) {
    rv = CallQueryInterface(mDocument, aDocumentView);
  }
  else {
    *aDocumentView = nsnull;
  }

  return rv;
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMStorageWindow
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::GetSessionStorage(nsIDOMStorage ** aSessionStorage)
{
  FORWARD_TO_INNER(GetSessionStorage, (aSessionStorage), NS_ERROR_UNEXPECTED);

  nsIPrincipal *principal = GetPrincipal();
  nsIDocShell* docShell = GetDocShell();

  if (!principal || !docShell) {
    return NS_OK;
  }

  if (mSessionStorage) {
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gDOMLeakPRLog, PR_LOG_DEBUG)) {
      PR_LogPrint("nsGlobalWindow %p has %p sessionStorage", this, mSessionStorage.get());
    }
#endif
    nsCOMPtr<nsPIDOMStorage> piStorage = do_QueryInterface(mSessionStorage);
    if (piStorage) {
      PRBool canAccess = piStorage->CanAccess(principal);
      NS_ASSERTION(canAccess,
                   "window %x owned sessionStorage "
                   "that could not be accessed!");
      if (!canAccess) {
          mSessionStorage = nsnull;
      }
    }
  }

  if (!mSessionStorage) {
    *aSessionStorage = nsnull;

    nsString documentURI;
    nsCOMPtr<nsIDOM3Document> document3 = do_QueryInterface(mDoc);
    if (document3)
        document3->GetDocumentURI(documentURI);

    nsresult rv = docShell->GetSessionStorageForPrincipal(principal,
                                                          documentURI,
                                                          PR_TRUE,
                                                          getter_AddRefs(mSessionStorage));
    NS_ENSURE_SUCCESS(rv, rv);

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gDOMLeakPRLog, PR_LOG_DEBUG)) {
      PR_LogPrint("nsGlobalWindow %p tried to get a new sessionStorage %p", this, mSessionStorage.get());
    }
#endif

    if (!mSessionStorage) {
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }
  }

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gDOMLeakPRLog, PR_LOG_DEBUG)) {
      PR_LogPrint("nsGlobalWindow %p returns %p sessionStorage", this, mSessionStorage.get());
    }
#endif

  NS_ADDREF(*aSessionStorage = mSessionStorage);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetGlobalStorage(nsIDOMStorageList ** aGlobalStorage)
{
  NS_ENSURE_ARG_POINTER(aGlobalStorage);

#ifdef MOZ_STORAGE
  if (!sGlobalStorageList) {
    nsresult rv = NS_NewDOMStorageList(&sGlobalStorageList);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aGlobalStorage = sGlobalStorageList;
  NS_IF_ADDREF(*aGlobalStorage);

  return NS_OK;
#else
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
#endif
}

NS_IMETHODIMP
nsGlobalWindow::GetLocalStorage(nsIDOMStorage ** aLocalStorage)
{
  FORWARD_TO_INNER(GetLocalStorage, (aLocalStorage), NS_ERROR_UNEXPECTED);

  NS_ENSURE_ARG(aLocalStorage);

  if (!mLocalStorage) {
    *aLocalStorage = nsnull;

    nsresult rv;

    PRPackedBool unused;
    if (!nsDOMStorage::CanUseStorage(&unused))
      return NS_ERROR_DOM_SECURITY_ERR;

    nsIPrincipal *principal = GetPrincipal();
    if (!principal)
      return NS_OK;

    nsCOMPtr<nsIDOMStorageManager> storageManager =
      do_GetService("@mozilla.org/dom/storagemanager;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsString documentURI;
    nsCOMPtr<nsIDOM3Document> document3 = do_QueryInterface(mDoc);
    if (document3)
        document3->GetDocumentURI(documentURI);

    rv = storageManager->GetLocalStorageForPrincipal(principal,
                                                     documentURI,
                                                     getter_AddRefs(mLocalStorage));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ADDREF(*aLocalStorage = mLocalStorage);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetMoz_indexedDB(nsIIDBFactory** _retval)
{
  if (!mIndexedDB) {
    mIndexedDB = mozilla::dom::indexedDB::IDBFactory::Create();
    NS_ENSURE_TRUE(mIndexedDB, NS_ERROR_FAILURE);
  }

  nsCOMPtr<nsIIDBFactory> request(mIndexedDB);
  request.forget(_retval);
  return NS_OK;
}

//*****************************************************************************
// nsGlobalWindow::nsIInterfaceRequestor
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::GetInterface(const nsIID & aIID, void **aSink)
{
  NS_ENSURE_ARG_POINTER(aSink);
  *aSink = nsnull;

  if (aIID.Equals(NS_GET_IID(nsIDocCharset))) {
    FORWARD_TO_OUTER(GetInterface, (aIID, aSink), NS_ERROR_NOT_INITIALIZED);

    if (mDocShell) {
      nsCOMPtr<nsIDocCharset> docCharset(do_QueryInterface(mDocShell));
      if (docCharset) {
        *aSink = docCharset;
        NS_ADDREF(((nsISupports *) *aSink));
      }
    }
  }
  else if (aIID.Equals(NS_GET_IID(nsIWebNavigation))) {
    FORWARD_TO_OUTER(GetInterface, (aIID, aSink), NS_ERROR_NOT_INITIALIZED);

    if (mDocShell) {
      nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
      if (webNav) {
        *aSink = webNav;
        NS_ADDREF(((nsISupports *) *aSink));
      }
    }
  }
#ifdef NS_PRINTING
  else if (aIID.Equals(NS_GET_IID(nsIWebBrowserPrint))) {
    FORWARD_TO_OUTER(GetInterface, (aIID, aSink), NS_ERROR_NOT_INITIALIZED);

    if (mDocShell) {
      nsCOMPtr<nsIContentViewer> viewer;
      mDocShell->GetContentViewer(getter_AddRefs(viewer));
      if (viewer) {
        nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint(do_QueryInterface(viewer));
        if (webBrowserPrint) {
          *aSink = webBrowserPrint;
          NS_ADDREF(((nsISupports *) *aSink));
        }
      }
    }
  }
#endif
  else if (aIID.Equals(NS_GET_IID(nsIScriptEventManager))) {
    if (mDoc) {
      nsIScriptEventManager* mgr = mDoc->GetScriptEventManager();
      if (mgr) {
        *aSink = mgr;
        NS_ADDREF(((nsISupports *) *aSink));
      }
    }
  }
  else if (aIID.Equals(NS_GET_IID(nsIDOMWindowUtils))) {
    FORWARD_TO_OUTER(GetInterface, (aIID, aSink), NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsISupports> utils(do_QueryReferent(mWindowUtils));
    if (utils) {
      *aSink = utils;
      NS_ADDREF(((nsISupports *) *aSink));
    } else {
      nsDOMWindowUtils *utilObj = new nsDOMWindowUtils(this);
      nsCOMPtr<nsISupports> utilsIfc =
                              NS_ISUPPORTS_CAST(nsIDOMWindowUtils *, utilObj);
      if (utilsIfc) {
        mWindowUtils = do_GetWeakReference(utilsIfc);
        *aSink = utilsIfc;
        NS_ADDREF(((nsISupports *) *aSink));
      }
    }
  }
  else {
    return QueryInterface(aIID, aSink);
  }

  return *aSink ? NS_OK : NS_ERROR_NO_INTERFACE;
}

void
nsGlobalWindow::FireOfflineStatusEvent()
{
  if (!mDoc)
    return;
  nsAutoString name;
  if (NS_IsOffline()) {
    name.AssignLiteral("offline");
  } else {
    name.AssignLiteral("online");
  }
  // The event is fired at the body element, or if there is no body element,
  // at the document.
  nsCOMPtr<nsISupports> eventTarget = mDoc.get();
  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(mDoc);
  if (htmlDoc) {
    nsCOMPtr<nsIDOMHTMLElement> body;
    htmlDoc->GetBody(getter_AddRefs(body));
    if (body) {
      eventTarget = body;
    }
  }
  else {
    nsCOMPtr<nsIDOMElement> documentElement;
    mDocument->GetDocumentElement(getter_AddRefs(documentElement));
    if(documentElement) {        
      eventTarget = documentElement;
    }
  }
  nsContentUtils::DispatchTrustedEvent(mDoc, eventTarget, name, PR_TRUE, PR_FALSE);
}

nsresult
nsGlobalWindow::Observe(nsISupports* aSubject, const char* aTopic,
                        const PRUnichar* aData)
{
  if (!nsCRT::strcmp(aTopic, NS_IOSERVICE_OFFLINE_STATUS_TOPIC)) {
    if (IsFrozen()) {
      // if an even number of notifications arrive while we're frozen,
      // we don't need to fire.
      mFireOfflineStatusChangeEventOnThaw = !mFireOfflineStatusChangeEventOnThaw;
    } else {
      FireOfflineStatusEvent();
    }
    return NS_OK;
  }

  if (IsInnerWindow() && !nsCRT::strcmp(aTopic, "dom-storage-changed")) {
    nsIPrincipal *principal;
    nsresult rv;

    principal = GetPrincipal();
    if (principal) {
      // A global storage object changed, check to see if it's one
      // this window can access.

      nsCOMPtr<nsIURI> codebase;
      principal->GetURI(getter_AddRefs(codebase));

      if (!codebase) {
        return NS_OK;
      }

      nsCAutoString currentDomain;
      rv = codebase->GetAsciiHost(currentDomain);
      if (NS_FAILED(rv)) {
        return NS_OK;
      }

      if (!nsDOMStorageList::CanAccessDomain(NS_ConvertUTF16toUTF8(aData),
                                             currentDomain)) {
        // This window can't reach the global storage object for the
        // domain for which the change happened, so don't fire any
        // events in this window.

        return NS_OK;
      }
    }

    nsAutoString domain(aData);

    if (IsFrozen()) {
      // This window is frozen, rather than firing the events here,
      // store the domain in which the change happened and fire the
      // events if we're ever thawed.

      if (!mPendingStorageEventsObsolete) {
        mPendingStorageEventsObsolete = new nsDataHashtable<nsStringHashKey, PRBool>;
        NS_ENSURE_TRUE(mPendingStorageEventsObsolete, NS_ERROR_OUT_OF_MEMORY);

        rv = mPendingStorageEventsObsolete->Init();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      mPendingStorageEventsObsolete->Put(domain, PR_TRUE);

      return NS_OK;
    }

    nsRefPtr<nsDOMStorageEventObsolete> event = new nsDOMStorageEventObsolete();
    NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

    rv = event->InitStorageEvent(NS_LITERAL_STRING("storage"), PR_FALSE, PR_FALSE, domain);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(mDocument));

    nsCOMPtr<nsIDOMEventTarget> target;

    if (htmlDoc) {
      nsCOMPtr<nsIDOMHTMLElement> body;
      htmlDoc->GetBody(getter_AddRefs(body));

      target = do_QueryInterface(body);
    }

    if (!target) {
      target = this;
    }

    PRBool defaultActionEnabled;
    target->DispatchEvent((nsIDOMStorageEventObsolete *)event, &defaultActionEnabled);

    return NS_OK;
  }

  if (IsInnerWindow() && !nsCRT::strcmp(aTopic, "dom-storage2-changed")) {
    nsIPrincipal *principal;
    nsresult rv;

    nsCOMPtr<nsIDOMStorageEvent> event = do_QueryInterface(aSubject, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMStorage> changingStorage;
    rv = event->GetStorageArea(getter_AddRefs(changingStorage));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsPIDOMStorage> pistorage = do_QueryInterface(changingStorage);
    nsPIDOMStorage::nsDOMStorageType storageType = pistorage->StorageType();

    principal = GetPrincipal();
    switch (storageType)
    {
    case nsPIDOMStorage::SessionStorage:
    {
      if (SameCOMIdentity(mSessionStorage, changingStorage)) {
        // Do not fire any events for the same storage object, it's not shared
        // among windows, see nsGlobalWindow::GetSessionStoarge()
        return NS_OK;
      }

      nsCOMPtr<nsIDOMStorage> storage = mSessionStorage;
      if (!storage) {
        nsIDocShell* docShell = GetDocShell();
        if (principal && docShell) {
          // No need to pass documentURI here, it's only needed when we want
          // to create a new storage, the third paramater would be PR_TRUE
          docShell->GetSessionStorageForPrincipal(principal,
                                                  EmptyString(),
                                                  PR_FALSE,
                                                  getter_AddRefs(storage));
        }
      }

      if (!pistorage->IsForkOf(storage)) {
        // This storage event is coming from a different doc shell,
        // i.e. it is a clone, ignore this event.
        return NS_OK;
      }

#ifdef PR_LOGGING
      if (PR_LOG_TEST(gDOMLeakPRLog, PR_LOG_DEBUG)) {
        PR_LogPrint("nsGlobalWindow %p with sessionStorage %p passing event from %p", this, mSessionStorage.get(), pistorage.get());
      }
#endif

      break;
    }
    case nsPIDOMStorage::LocalStorage:
    {
      if (SameCOMIdentity(mLocalStorage, changingStorage)) {
        // Do not fire any events for the same storage object, it's not shared
        // among windows, see nsGlobalWindow::GetLocalStoarge()
        return NS_OK;
      }

      // Allow event fire only for the same principal storages
      // XXX We have to use EqualsIgnoreDomain after bug 495337 lands
      nsIPrincipal *storagePrincipal = pistorage->Principal();
      PRBool equals;

      rv = storagePrincipal->Equals(principal, &equals);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!equals)
        return NS_OK;

      break;
    }
    default:
      return NS_OK;
    }

    if (IsFrozen()) {
      // This window is frozen, rather than firing the events here,
      // store the domain in which the change happened and fire the
      // events if we're ever thawed.

      mPendingStorageEvents.AppendElement(event);
      return NS_OK;
    }

    PRBool defaultActionEnabled;
    DispatchEvent((nsIDOMStorageEvent *)event, &defaultActionEnabled);

    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, "offline-cache-update-added")) {
    if (mApplicationCache)
      return NS_OK;

    // Instantiate the application object now. It observes update belonging to
    // this window's document and correctly updates the applicationCache object
    // state.
    nsCOMPtr<nsIDOMOfflineResourceList> applicationCache;
    GetApplicationCache(getter_AddRefs(applicationCache));
    nsCOMPtr<nsIObserver> observer = do_QueryInterface(applicationCache);
    if (observer)
      observer->Observe(aSubject, aTopic, aData);

    return NS_OK;
  }

  NS_WARNING("unrecognized topic in nsGlobalWindow::Observe");
  return NS_ERROR_FAILURE;
}

static PLDHashOperator
FirePendingStorageEvents(const nsAString& aKey, PRBool aData, void *userArg)
{
  nsGlobalWindow *win = static_cast<nsGlobalWindow *>(userArg);

  nsCOMPtr<nsIDOMStorage> storage;
  win->GetSessionStorage(getter_AddRefs(storage));

  if (storage) {
    win->Observe(storage, "dom-storage-changed",
                 aKey.IsEmpty() ? nsnull : PromiseFlatString(aKey).get());
  }

  return PL_DHASH_NEXT;
}

nsresult
nsGlobalWindow::FireDelayedDOMEvents()
{
  FORWARD_TO_INNER(FireDelayedDOMEvents, (), NS_ERROR_UNEXPECTED);

  for (PRUint32 i = 0; i < mPendingStorageEvents.Length(); ++i) {
    Observe(mPendingStorageEvents[i], "dom-storage2-changed", nsnull);
  }

  if (mPendingStorageEventsObsolete) {
    // Fire pending storage events.
    mPendingStorageEventsObsolete->EnumerateRead(FirePendingStorageEvents, this);

    delete mPendingStorageEventsObsolete;
    mPendingStorageEventsObsolete = nsnull;
  }

  if (mApplicationCache) {
    static_cast<nsDOMOfflineResourceList*>(mApplicationCache.get())->FirePendingEvents();
  }

  if (mFireOfflineStatusChangeEventOnThaw) {
    mFireOfflineStatusChangeEventOnThaw = PR_FALSE;
    FireOfflineStatusEvent();
  }

  nsCOMPtr<nsIDocShellTreeNode> node =
    do_QueryInterface(GetDocShell());
  if (node) {
    PRInt32 childCount = 0;
    node->GetChildCount(&childCount);

    for (PRInt32 i = 0; i < childCount; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> childShell;
      node->GetChildAt(i, getter_AddRefs(childShell));
      NS_ASSERTION(childShell, "null child shell");

      nsCOMPtr<nsPIDOMWindow> pWin = do_GetInterface(childShell);
      if (pWin) {
        nsGlobalWindow *win =
          static_cast<nsGlobalWindow*>
                     (static_cast<nsPIDOMWindow*>(pWin));
        win->FireDelayedDOMEvents();
      }
    }
  }

  return NS_OK;
}

//*****************************************************************************
// nsGlobalWindow: Window Control Functions
//*****************************************************************************

nsIDOMWindowInternal *
nsGlobalWindow::GetParentInternal()
{
  FORWARD_TO_OUTER(GetParentInternal, (), nsnull);

  nsIDOMWindowInternal *parentInternal = nsnull;

  nsCOMPtr<nsIDOMWindow> parent;
  GetParent(getter_AddRefs(parent));

  if (parent && parent != static_cast<nsIDOMWindow *>(this)) {
    nsCOMPtr<nsIDOMWindowInternal> tmp(do_QueryInterface(parent));
    NS_ASSERTION(parent, "Huh, parent not an nsIDOMWindowInternal?");

    parentInternal = tmp;
  }

  return parentInternal;
}

// static
void
nsGlobalWindow::CloseBlockScriptTerminationFunc(nsISupports *aRef)
{
  nsGlobalWindow* pwin = static_cast<nsGlobalWindow*>
                                    (static_cast<nsPIDOMWindow*>(aRef));
  pwin->mBlockScriptedClosingFlag = PR_FALSE;
}

nsresult
nsGlobalWindow::OpenInternal(const nsAString& aUrl, const nsAString& aName,
                             const nsAString& aOptions, PRBool aDialog,
                             PRBool aContentModal, PRBool aCalledNoScript,
                             PRBool aDoJSFixups, nsIArray *argv,
                             nsISupports *aExtraArgument,
                             nsIPrincipal *aCalleePrincipal,
                             JSContext *aJSCallerContext,
                             nsIDOMWindow **aReturn)
{
  FORWARD_TO_OUTER(OpenInternal, (aUrl, aName, aOptions, aDialog,
                                  aContentModal, aCalledNoScript, aDoJSFixups,
                                  argv, aExtraArgument, aCalleePrincipal,
                                  aJSCallerContext, aReturn),
                   NS_ERROR_NOT_INITIALIZED);

#ifdef NS_DEBUG
  PRUint32 argc = 0;
  if (argv)
      argv->GetLength(&argc);
#endif
  NS_PRECONDITION(!aExtraArgument || (!argv && argc == 0),
                  "Can't pass in arguments both ways");
  NS_PRECONDITION(!aCalledNoScript || (!argv && argc == 0),
                  "Can't pass JS args when called via the noscript methods");
  NS_PRECONDITION(!aJSCallerContext || !aCalledNoScript,
                  "Shouldn't have caller context when called noscript");

  *aReturn = nsnull;

  nsCOMPtr<nsIWebBrowserChrome> chrome;
  GetWebBrowserChrome(getter_AddRefs(chrome));
  if (!chrome) {
    // No chrome means we don't want to go through with this open call
    // -- see nsIWindowWatcher.idl
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ASSERTION(mDocShell, "Must have docshell here");

  const PRBool checkForPopup =
    !aDialog && !WindowExists(aName, !aCalledNoScript);

  // Note: it's very important that this be an nsXPIDLCString, since we want
  // .get() on it to return nsnull until we write stuff to it.  The window
  // watcher expects a null URL string if there is no URL to load.
  nsXPIDLCString url;
  nsresult rv = NS_OK;

  // It's important to do this security check before determining whether this
  // window opening should be blocked, to ensure that we don't FireAbuseEvents
  // for a window opening that wouldn't have succeeded in the first place.
  if (!aUrl.IsEmpty()) {
    AppendUTF16toUTF8(aUrl, url);

    /* Check whether the URI is allowed, but not for dialogs --
       see bug 56851. The security of this function depends on
       window.openDialog being inaccessible from web scripts */
    if (url.get() && !aDialog)
      rv = SecurityCheckURL(url.get());
  }

  if (NS_FAILED(rv))
    return rv;

  PopupControlState abuseLevel = gPopupControlState;
  if (checkForPopup) {
    abuseLevel = RevisePopupAbuseLevel(abuseLevel);
    if (abuseLevel >= openAbused) {
      if (aJSCallerContext) {
        // If script in some other window is doing a window.open on us and
        // it's being blocked, then it's OK to close us afterwards, probably.
        // But if we're doing a window.open on ourselves and block the popup,
        // prevent this window from closing until after this script terminates
        // so that whatever popup blocker UI the app has will be visible.
        if (mContext == GetScriptContextFromJSContext(aJSCallerContext)) {
          mBlockScriptedClosingFlag = PR_TRUE;
          mContext->SetTerminationFunction(CloseBlockScriptTerminationFunc,
                                           static_cast<nsPIDOMWindow*>
                                                      (this));
        }
      }

      FireAbuseEvents(PR_TRUE, PR_FALSE, aUrl, aName, aOptions);
      return aDoJSFixups ? NS_OK : NS_ERROR_FAILURE;
    }
  }    

  nsCOMPtr<nsIDOMWindow> domReturn;

  nsCOMPtr<nsIWindowWatcher> wwatch =
    do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  NS_ENSURE_TRUE(wwatch, rv);

  NS_ConvertUTF16toUTF8 options(aOptions);
  NS_ConvertUTF16toUTF8 name(aName);

  const char *options_ptr = aOptions.IsEmpty() ? nsnull : options.get();
  const char *name_ptr = aName.IsEmpty() ? nsnull : name.get();

  {
    // Reset popup state while opening a window to prevent the
    // current state from being active the whole time a modal
    // dialog is open.
    nsAutoPopupStatePusher popupStatePusher(openAbused, PR_TRUE);

    if (!aCalledNoScript) {
      nsCOMPtr<nsPIWindowWatcher> pwwatch(do_QueryInterface(wwatch));
      NS_ASSERTION(pwwatch,
                   "Unable to open windows from JS because window watcher "
                   "is broken");
      NS_ENSURE_TRUE(pwwatch, NS_ERROR_UNEXPECTED);
        
      rv = pwwatch->OpenWindowJS(this, url.get(), name_ptr, options_ptr,
                                 aDialog, argv,
                                 getter_AddRefs(domReturn));
    } else {
      // Push a null JSContext here so that the window watcher won't screw us
      // up.  We do NOT want this case looking at the JS context on the stack
      // when searching.  Compare comments on
      // nsIDOMWindowInternal::OpenWindow and nsIWindowWatcher::OpenWindow.
      nsCOMPtr<nsIJSContextStack> stack;

      if (!aContentModal) {
        stack = do_GetService(sJSStackContractID);
      }

      if (stack) {
        rv = stack->Push(nsnull);
        NS_ENSURE_SUCCESS(rv, rv);
      }
        
      rv = wwatch->OpenWindow(this, url.get(), name_ptr, options_ptr,
                              aExtraArgument, getter_AddRefs(domReturn));

      if (stack) {
        JSContext* cx;
        stack->Pop(&cx);
        NS_ASSERTION(!cx, "Unexpected JSContext popped!");
      }
    }
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // success!

  domReturn.swap(*aReturn);

  if (aDoJSFixups) {      
    nsCOMPtr<nsIDOMChromeWindow> chrome_win(do_QueryInterface(*aReturn));
    if (!chrome_win) {
      // A new non-chrome window was created from a call to
      // window.open() from JavaScript, make sure there's a document in
      // the new window. We do this by simply asking the new window for
      // its document, this will synchronously create an empty document
      // if there is no document in the window.
      // XXXbz should this just use EnsureInnerWindow()?
#ifdef DEBUG_jst
      {
        nsCOMPtr<nsPIDOMWindow> pidomwin(do_QueryInterface(*aReturn));

        nsIDOMDocument *temp = pidomwin->GetExtantDocument();

        NS_ASSERTION(temp, "No document in new window!!!");
      }
#endif

      nsCOMPtr<nsIDOMDocument> doc;
      (*aReturn)->GetDocument(getter_AddRefs(doc));
    }
  }
    
  if (checkForPopup) {
    if (abuseLevel >= openControlled) {
      nsGlobalWindow *opened = static_cast<nsGlobalWindow *>(*aReturn);
      if (!opened->IsPopupSpamWindow()) {
        opened->SetPopupSpamWindow(PR_TRUE);
        ++gOpenPopupSpamCount;
      }
    }
    if (abuseLevel >= openAbused)
      FireAbuseEvents(PR_FALSE, PR_TRUE, aUrl, aName, aOptions);
  }

  return rv;
}

// static
void
nsGlobalWindow::CloseWindow(nsISupports *aWindow)
{
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(aWindow));

  nsGlobalWindow* globalWin =
    static_cast<nsGlobalWindow *>
               (static_cast<nsPIDOMWindow*>(win));

  // Need to post an event for closing, otherwise window and 
  // presshell etc. may get destroyed while creating frames, bug 338897.
  nsCloseEvent::PostCloseEvent(globalWin);
  // else if OOM, better not to close. That might cause a crash.
}

// static
void
nsGlobalWindow::ClearWindowScope(nsISupports *aWindow)
{
  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(aWindow));
  nsIScriptContext *scx = sgo->GetContext();
  if (scx) {
    scx->ClearScope(sgo->GetGlobalJSObject(), PR_TRUE);
  }
}

//*****************************************************************************
// nsGlobalWindow: Timeout Functions
//*****************************************************************************

PRUint32 sNestingLevel;

nsresult
nsGlobalWindow::SetTimeoutOrInterval(nsIScriptTimeoutHandler *aHandler,
                                     PRInt32 interval,
                                     PRBool aIsInterval, PRInt32 *aReturn)
{
  FORWARD_TO_INNER(SetTimeoutOrInterval, (aHandler, interval, aIsInterval, aReturn),
                   NS_ERROR_NOT_INITIALIZED);

  // If we don't have a document (we could have been unloaded since
  // the call to setTimeout was made), do nothing.
  if (!mDocument) {
    return NS_OK;
  }

  PRUint32 nestingLevel = sNestingLevel + 1;
  if (interval < DOM_MIN_TIMEOUT_VALUE) {
    if (aIsInterval || nestingLevel >= DOM_CLAMP_TIMEOUT_NESTING_LEVEL) {
      // Don't allow timeouts less than DOM_MIN_TIMEOUT_VALUE from
      // now...

      interval = DOM_MIN_TIMEOUT_VALUE;
    }
    else if (interval < 0) {
      // Clamp negative intervals to 0.

      interval = 0;
    }
  }

  NS_ASSERTION(interval >= 0, "DOM_MIN_TIMEOUT_VALUE lies");
  PRUint32 realInterval = interval;

  // Make sure we don't proceed with a interval larger than our timer
  // code can handle.
  if (realInterval > PR_IntervalToMilliseconds(DOM_MAX_TIMEOUT_VALUE)) {
    realInterval = PR_IntervalToMilliseconds(DOM_MAX_TIMEOUT_VALUE);
  }

  nsTimeout *timeout = new nsTimeout();
  if (!timeout)
    return NS_ERROR_OUT_OF_MEMORY;

  // Increment the timeout's reference count to represent this function's hold
  // on the timeout.
  timeout->AddRef();

  if (aIsInterval) {
    timeout->mInterval = realInterval;
  }
  timeout->mScriptHandler = aHandler;

  // Get principal of currently executing code, save for execution of timeout.
  // If our principals subsume the subject principal then use the subject
  // principal. Otherwise, use our principal to avoid running script in
  // elevated principals.

  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  nsresult rv;
  rv = nsContentUtils::GetSecurityManager()->
    GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));
  if (NS_FAILED(rv)) {
    timeout->Release();

    return NS_ERROR_FAILURE;
  }

  PRBool subsumes = PR_FALSE;
  nsCOMPtr<nsIPrincipal> ourPrincipal = GetPrincipal();

  // Note the direction of this test: We don't allow setTimeouts running with
  // chrome privileges on content windows, but we do allow setTimeouts running
  // with content privileges on chrome windows (where they can't do very much,
  // of course).
  rv = ourPrincipal->Subsumes(subjectPrincipal, &subsumes);
  if (NS_FAILED(rv)) {
    timeout->Release();

    return NS_ERROR_FAILURE;
  }

  if (subsumes) {
    timeout->mPrincipal = subjectPrincipal;
  } else {
    timeout->mPrincipal = ourPrincipal;
  }

  TimeDuration delta = TimeDuration::FromMilliseconds(realInterval);

  if (!IsFrozen() && !mTimeoutsSuspendDepth) {
    // If we're not currently frozen, then we set timeout->mWhen to be the
    // actual firing time of the timer (i.e., now + delta). We also actually
    // create a timer and fire it off.

    timeout->mWhen = TimeStamp::Now() + delta;

    timeout->mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if (NS_FAILED(rv)) {
      timeout->Release();

      return rv;
    }

    rv = timeout->mTimer->InitWithFuncCallback(TimerCallback, timeout,
                                               realInterval,
                                               nsITimer::TYPE_ONE_SHOT);
    if (NS_FAILED(rv)) {
      timeout->Release();

      return rv;
    }

    // The timeout is now also held in the timer's closure.
    timeout->AddRef();
  } else {
    // If we are frozen, however, then we instead simply set
    // timeout->mTimeRemaining to be the "time remaining" in the timeout (i.e.,
    // the interval itself). We don't create a timer for it, since that will
    // happen when we are thawed and the timeout will then get a timer and run
    // to completion.

    timeout->mTimeRemaining = delta;
  }

  timeout->mWindow = this;

  if (!aIsInterval) {
    timeout->mNestingLevel = nestingLevel;
  }

  // No popups from timeouts by default
  timeout->mPopupState = openAbused;

  if (gRunningTimeoutDepth == 0 && gPopupControlState < openAbused) {
    // This timeout is *not* set from another timeout and it's set
    // while popups are enabled. Propagate the state to the timeout if
    // its delay (interval) is equal to or less than what
    // "dom.disable_open_click_delay" is set to (in ms).

    PRInt32 delay =
      nsContentUtils::GetIntPref("dom.disable_open_click_delay");

    if (interval <= delay) {
      timeout->mPopupState = gPopupControlState;
    }
  }

  InsertTimeoutIntoList(timeout);

  timeout->mPublicId = ++mTimeoutPublicIdCounter;
  *aReturn = timeout->mPublicId;

  // Our hold on the timeout is expiring. Note that this should not actually
  // free the timeout (since the list should have taken ownership as well).
  timeout->Release();

  return NS_OK;

}

nsresult
nsGlobalWindow::SetTimeoutOrInterval(PRBool aIsInterval, PRInt32 *aReturn)
{
  // This needs to forward to the inner window, but since the current
  // inner may not be the inner in the calling scope, we need to treat
  // this specially here as we don't want timeouts registered in a
  // dying inner window to get registered and run on the current inner
  // window. To get this right, we need to forward this call to the
  // inner window that's calling window.setTimeout().

  if (IsOuterWindow()) {
    nsGlobalWindow* callerInner = CallerInnerWindow();
    NS_ENSURE_TRUE(callerInner, NS_ERROR_NOT_AVAILABLE);

    // If the caller and the callee share the same outer window,
    // forward to the callee inner. Else, we forward to the current
    // inner (e.g. someone is calling setTimeout() on a reference to
    // some other window).

    if (callerInner->GetOuterWindow() == this &&
        callerInner->IsInnerWindow()) {
      return callerInner->SetTimeoutOrInterval(aIsInterval, aReturn);
    }

    FORWARD_TO_INNER(SetTimeoutOrInterval, (aIsInterval, aReturn),
                     NS_ERROR_NOT_INITIALIZED);
  }

  PRInt32 interval = 0;
  PRBool isInterval = aIsInterval;
  nsCOMPtr<nsIScriptTimeoutHandler> handler;
  nsresult rv = NS_CreateJSTimeoutHandler(this,
                                          &isInterval,
                                          &interval,
                                          getter_AddRefs(handler));
  if (NS_FAILED(rv))
    return (rv == NS_ERROR_DOM_TYPE_ERR) ? NS_OK : rv;

  return SetTimeoutOrInterval(handler, interval, isInterval, aReturn);
}

// static
void
nsGlobalWindow::RunTimeout(nsTimeout *aTimeout)
{
  // If a modal dialog is open for this window, return early. Pending
  // timeouts will run when the modal dialog is dismissed.
  if (IsInModalState() || mTimeoutsSuspendDepth) {
    return;
  }

  NS_TIME_FUNCTION;

  NS_ASSERTION(IsInnerWindow(), "Timeout running on outer window!");
  NS_ASSERTION(!IsFrozen(), "Timeout running on a window in the bfcache!");

  nsTimeout *nextTimeout, *timeout;
  nsTimeout *last_expired_timeout, *last_insertion_point;
  nsTimeout dummy_timeout;
  PRUint32 firingDepth = mTimeoutFiringDepth + 1;

  // Make sure that the window and the script context don't go away as
  // a result of running timeouts
  nsCOMPtr<nsIScriptGlobalObject> windowKungFuDeathGrip(this);

  // A native timer has gone off. See which of our timeouts need
  // servicing
  TimeStamp now = TimeStamp::Now();
  TimeStamp deadline;

  if (aTimeout && aTimeout->mWhen > now) {
    // The OS timer fired early (yikes!), and possibly out of order
    // too. Set |deadline| to be the time when the OS timer *should*
    // have fired so that any timers that *should* have fired before
    // aTimeout *will* be fired now. This happens most of the time on
    // Win2k.

    deadline = aTimeout->mWhen;
  } else {
    deadline = now;
  }

  // The timeout list is kept in deadline order. Discover the latest
  // timeout whose deadline has expired. On some platforms, native
  // timeout events fire "early", so we need to test the timer as well
  // as the deadline.
  last_expired_timeout = nsnull;
  for (timeout = FirstTimeout(); IsTimeout(timeout); timeout = timeout->Next()) {
    if (((timeout == aTimeout) || (timeout->mWhen <= deadline)) &&
        (timeout->mFiringDepth == 0)) {
      // Mark any timeouts that are on the list to be fired with the
      // firing depth so that we can reentrantly run timeouts
      timeout->mFiringDepth = firingDepth;
      last_expired_timeout = timeout;
    }
  }

  // Maybe the timeout that the event was fired for has been deleted
  // and there are no others timeouts with deadlines that make them
  // eligible for execution yet. Go away.
  if (!last_expired_timeout) {
    return;
  }

  // Insert a dummy timeout into the list of timeouts between the
  // portion of the list that we are about to process now and those
  // timeouts that will be processed in a future call to
  // win_run_timeout(). This dummy timeout serves as the head of the
  // list for any timeouts inserted as a result of running a timeout.
  dummy_timeout.mFiringDepth = firingDepth;
  dummy_timeout.mWhen = now;
  PR_INSERT_AFTER(&dummy_timeout, last_expired_timeout);

  // Don't let ClearWindowTimeouts throw away our stack-allocated
  // dummy timeout.
  dummy_timeout.AddRef();
  dummy_timeout.AddRef();

  last_insertion_point = mTimeoutInsertionPoint;
  mTimeoutInsertionPoint = &dummy_timeout;

  for (timeout = FirstTimeout();
       timeout != &dummy_timeout && !IsFrozen();
       timeout = nextTimeout) {
    nextTimeout = timeout->Next();

    if (timeout->mFiringDepth != firingDepth) {
      // We skip the timeout since it's on the list to run at another
      // depth.

      continue;
    }

    if (mTimeoutsSuspendDepth) {
      // Some timer did suspend us. Make sure the
      // rest of the timers get executed later.
      timeout->mFiringDepth = 0;
      continue;
    }

    // The timeout is on the list to run at this depth, go ahead and
    // process it.

    // Get the script context (a strong ref to prevent it going away)
    // for this timeout and ensure the script language is enabled.
    nsCOMPtr<nsIScriptContext> scx = GetScriptContextInternal(
                                timeout->mScriptHandler->GetScriptTypeID());

    if (!scx) {
      // No context means this window was closed or never properly
      // initialized for this language.
      continue;
    }

    // The "scripts disabled" concept is still a little vague wrt
    // multiple languages.  Prepare for the day when languages can be
    // disabled independently of the other languages...
    if (!scx->GetScriptsEnabled()) {
      // Scripts were enabled once in this window (unless aTimeout ==
      // nsnull) but now scripts are disabled (we might be in
      // print-preview, for instance), this means we shouldn't run any
      // timeouts at this point.
      //
      // If scripts are enabled for this language in this window again
      // we'll fire the timeouts that are due at that point.
      continue;
    }

    // This timeout is good to run
    nsTimeout *last_running_timeout = mRunningTimeout;
    mRunningTimeout = timeout;
    timeout->mRunning = PR_TRUE;

    // Push this timeout's popup control state, which should only be
    // eabled the first time a timeout fires that was created while
    // popups were enabled and with a delay less than
    // "dom.disable_open_click_delay".
    nsAutoPopupStatePusher popupStatePusher(timeout->mPopupState);

    // Clear the timeout's popup state, if any, to prevent interval
    // timeouts from repeatedly opening poups.
    timeout->mPopupState = openAbused;

    // Hold on to the timeout in case mExpr or mFunObj releases its
    // doc.
    timeout->AddRef();

    ++gRunningTimeoutDepth;
    ++mTimeoutFiringDepth;

    PRBool trackNestingLevel = !timeout->mInterval;
    PRUint32 nestingLevel;
    if (trackNestingLevel) {
      nestingLevel = sNestingLevel;
      sNestingLevel = timeout->mNestingLevel;
    }

    nsCOMPtr<nsIScriptTimeoutHandler> handler(timeout->mScriptHandler);
    void *scriptObject = handler->GetScriptObject();
    if (!scriptObject) {
      // Evaluate the timeout expression.
      const PRUnichar *script = handler->GetHandlerText();
      NS_ASSERTION(script, "timeout has no script nor handler text!");

      const char *filename = nsnull;
      PRUint32 lineNo = 0;
      handler->GetLocation(&filename, &lineNo);

      NS_TIME_FUNCTION_MARK("(file: %s, line: %d)", filename, lineNo);

      PRBool is_undefined;
      scx->EvaluateString(nsDependentString(script), 
                          GetScriptGlobal(handler->GetScriptTypeID()),
                          timeout->mPrincipal, filename, lineNo,
                          handler->GetScriptVersion(), nsnull,
                          &is_undefined);
    } else {
      // Let the script handler know about the "secret" final argument that
      // indicates timeout lateness in milliseconds
      TimeDuration lateness = now - timeout->mWhen;

      handler->SetLateness(lateness.ToMilliseconds());

      nsCOMPtr<nsIVariant> dummy;
      nsCOMPtr<nsISupports> me(static_cast<nsIDOMWindow *>(this));
      scx->CallEventHandler(me,
                            GetScriptGlobal(handler->GetScriptTypeID()),
                            scriptObject, handler->GetArgv(),
                            // XXXmarkh - consider allowing CallEventHandler to
                            // accept nsnull?
                            getter_AddRefs(dummy));

    }
    handler = nsnull; // drop reference before dropping timeout refs.

    if (trackNestingLevel) {
      sNestingLevel = nestingLevel;
    }

    --mTimeoutFiringDepth;
    --gRunningTimeoutDepth;

    mRunningTimeout = last_running_timeout;
    timeout->mRunning = PR_FALSE;

    // We ignore any failures from calling EvaluateString() or
    // CallEventHandler() on the context here since we're in a loop
    // where we're likely to be running timeouts whose OS timers
    // didn't fire in time and we don't want to not fire those timers
    // now just because execution of one timer failed. We can't
    // propagate the error to anyone who cares about it from this
    // point anyway, and the script context should have already reported
    // the script error in the usual way - so we just drop it.

    // If all timeouts were cleared and |timeout != aTimeout| then
    // |timeout| may be the last reference to the timeout so check if
    // it was cleared before releasing it.
    PRBool timeout_was_cleared = timeout->mCleared;

    timeout->Release();

    if (timeout_was_cleared) {
      // The running timeout's window was cleared, this means that
      // ClearAllTimeouts() was called from a *nested* call, possibly
      // through a timeout that fired while a modal (to this window)
      // dialog was open or through other non-obvious paths.

      mTimeoutInsertionPoint = last_insertion_point;

      return;
    }

    PRBool isInterval = PR_FALSE;

    // If we have a regular interval timer, we re-schedule the
    // timeout, accounting for clock drift.
    if (timeout->mInterval) {
      // Compute time to next timeout for interval timer.
      // Make sure nextInterval is at least DOM_MIN_TIMEOUT_VALUE.
      TimeDuration nextInterval =
        TimeDuration::FromMilliseconds(NS_MAX(timeout->mInterval,
                                              PRUint32(DOM_MIN_TIMEOUT_VALUE)));

      // If we're running pending timeouts because they've been temporarily
      // disabled (!aTimeout), set the next interval to be relative to "now",
      // and not to when the timeout that was pending should have fired.  Also
      // check if the next interval timeout is overdue.  If so, then restart
      // the interval from now.
      TimeStamp firingTime;
      if (!aTimeout || timeout->mWhen + nextInterval <= now)
        firingTime = now + nextInterval;
      else
        firingTime = timeout->mWhen + nextInterval;

      TimeDuration delay = firingTime - TimeStamp::Now();

      // And make sure delay is nonnegative; that might happen if the timer
      // thread is firing our timers somewhat early.
      if (delay < TimeDuration(0)) {
        delay = TimeDuration(0);
      }

      if (timeout->mTimer) {
        timeout->mWhen = firingTime;

        // Reschedule the OS timer. Don't bother returning any error
        // codes if this fails since the callers of this method
        // doesn't care about them nobody who cares about them
        // anyways.

        // Make sure to cast the unsigned PR_USEC_PER_MSEC to signed
        // PRTime to make the division do the right thing on 64-bit
        // platforms whether delay is positive or negative (which we
        // know is always positive here, but cast anyways for
        // consistency).
        nsresult rv = timeout->mTimer->
          InitWithFuncCallback(TimerCallback, timeout,
                               delay.ToMilliseconds(),
                               nsITimer::TYPE_ONE_SHOT);

        if (NS_FAILED(rv)) {
          NS_ERROR("Error initializing timer for DOM timeout!");

          // We failed to initialize the new OS timer, this timer does
          // us no good here so we just cancel it (just in case) and
          // null out the pointer to the OS timer, this will release the
          // OS timer. As we continue executing the code below we'll end
          // up deleting the timeout since it's not an interval timeout
          // any more (since timeout->mTimer == nsnull).
          timeout->mTimer->Cancel();
          timeout->mTimer = nsnull;

          // Now that the OS timer no longer has a reference to the
          // timeout we need to drop that reference.
          timeout->Release();
        }
      } else {
        NS_ASSERTION(IsFrozen() || mTimeoutsSuspendDepth,
                     "How'd our timer end up null if we're not frozen or "
                     "suspended?");

        timeout->mTimeRemaining = delay;
        isInterval = PR_TRUE;
      }
    }

    if (timeout->mTimer) {
      if (timeout->mInterval) {
        isInterval = PR_TRUE;
      } else {
        // The timeout still has an OS timer, and it's not an
        // interval, that means that the OS timer could still fire (if
        // it didn't already, i.e. aTimeout == timeout), cancel the OS
        // timer and release its reference to the timeout.
        timeout->mTimer->Cancel();
        timeout->mTimer = nsnull;

        timeout->Release();
      }
    }

    // Running a timeout can cause another timeout to be deleted, so
    // we need to reset the pointer to the following timeout.
    nextTimeout = timeout->Next();

    PR_REMOVE_LINK(timeout);

    if (isInterval) {
      // Reschedule an interval timeout. Insert interval timeout
      // onto list sorted in deadline order.
      // AddRefs timeout.
      InsertTimeoutIntoList(timeout);
    }

    // Release the timeout struct since it's possibly out of the list
    timeout->Release();
  }

  // Take the dummy timeout off the head of the list
  PR_REMOVE_LINK(&dummy_timeout);

  mTimeoutInsertionPoint = last_insertion_point;
}

nsrefcnt
nsTimeout::Release()
{
  if (--mRefCnt > 0)
    return mRefCnt;

  // language specific cleanup done as mScriptHandler destructs...

  // Kill the timer if it is still alive.
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }

  delete this;
  return 0;
}

nsrefcnt
nsTimeout::AddRef()
{
  return ++mRefCnt;
}


nsresult
nsGlobalWindow::ClearTimeoutOrInterval(PRInt32 aTimerID)
{
  FORWARD_TO_INNER(ClearTimeoutOrInterval, (aTimerID), NS_ERROR_NOT_INITIALIZED);

  PRUint32 public_id = (PRUint32)aTimerID;
  nsTimeout *timeout;

  for (timeout = FirstTimeout();
       IsTimeout(timeout);
       timeout = timeout->Next()) {
    if (timeout->mPublicId == public_id) {
      if (timeout->mRunning) {
        /* We're running from inside the timeout. Mark this
           timeout for deferred deletion by the code in
           RunTimeout() */
        timeout->mInterval = 0;
      }
      else {
        /* Delete the timeout from the pending timeout list */
        PR_REMOVE_LINK(timeout);

        if (timeout->mTimer) {
          timeout->mTimer->Cancel();
          timeout->mTimer = nsnull;
          timeout->Release();
        }
        timeout->Release();
      }
      break;
    }
  }

  return NS_OK;
}

// A JavaScript specific version.
nsresult
nsGlobalWindow::ClearTimeoutOrInterval()
{
  FORWARD_TO_INNER(ClearTimeoutOrInterval, (), NS_ERROR_NOT_INITIALIZED);

  nsresult rv = NS_OK;
  nsAXPCNativeCallContext *ncc = nsnull;

  rv = nsContentUtils::XPConnect()->
    GetCurrentNativeCallContext(&ncc);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ncc)
    return NS_ERROR_NOT_AVAILABLE;

  JSContext *cx = nsnull;

  rv = ncc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 argc;

  ncc->GetArgc(&argc);

  if (argc < 1) {
    // No arguments, return early.

    return NS_OK;
  }

  jsval *argv = nsnull;

  ncc->GetArgvPtr(&argv);

  int32 timer_id;

  JSAutoRequest ar(cx);

  // XXXjst: Can we deal with this w/o using GetCurrentNativeCallContext()
  if (argv[0] == JSVAL_VOID || !::JS_ValueToInt32(cx, argv[0], &timer_id) ||
      timer_id <= 0) {
    // Undefined or non-positive number passed as argument, return
    // early. Make sure that JS_ValueToInt32 didn't set an exception.

    ::JS_ClearPendingException(cx);
    return NS_OK;
  }

  return ClearTimeoutOrInterval(timer_id);
}

void
nsGlobalWindow::ClearAllTimeouts()
{
  nsTimeout *timeout, *nextTimeout;

  for (timeout = FirstTimeout(); IsTimeout(timeout); timeout = nextTimeout) {
    /* If RunTimeout() is higher up on the stack for this
       window, e.g. as a result of document.write from a timeout,
       then we need to reset the list insertion point for
       newly-created timeouts in case the user adds a timeout,
       before we pop the stack back to RunTimeout. */
    if (mRunningTimeout == timeout)
      mTimeoutInsertionPoint = nsnull;

    nextTimeout = timeout->Next();

    if (timeout->mTimer) {
      timeout->mTimer->Cancel();
      timeout->mTimer = nsnull;

      // Drop the count since the timer isn't going to hold on
      // anymore.
      timeout->Release();
    }

    // Set timeout->mCleared to true to indicate that the timeout was
    // cleared and taken out of the list of timeouts
    timeout->mCleared = PR_TRUE;

    // Drop the count since we're removing it from the list.
    timeout->Release();
  }

  // Clear out our list
  PR_INIT_CLIST(&mTimeouts);
}

void
nsGlobalWindow::InsertTimeoutIntoList(nsTimeout *aTimeout)
{
  NS_ASSERTION(IsInnerWindow(),
               "InsertTimeoutIntoList() called on outer window!");

  // Start at mLastTimeout and go backwards.  Don't go further than
  // mTimeoutInsertionPoint, though.  This optimizes for the common case of
  // insertion at the end.
  nsTimeout* prevSibling;
  for (prevSibling = LastTimeout();
       IsTimeout(prevSibling) && prevSibling != mTimeoutInsertionPoint &&
         // This condition needs to match the one in SetTimeoutOrInterval that
         // determines whether to set mWhen or mTimeRemaining.
         ((IsFrozen() || mTimeoutsSuspendDepth) ?
          prevSibling->mTimeRemaining > aTimeout->mTimeRemaining :
          prevSibling->mWhen > aTimeout->mWhen);
       prevSibling = prevSibling->Prev()) {
    /* Do nothing; just searching */
  }

  // Now link in aTimeout after prevSibling.
  PR_INSERT_AFTER(aTimeout, prevSibling);

  aTimeout->mFiringDepth = 0;

  // Increment the timeout's reference count since it's now held on to
  // by the list
  aTimeout->AddRef();
}

// static
void
nsGlobalWindow::TimerCallback(nsITimer *aTimer, void *aClosure)
{
  nsTimeout *timeout = (nsTimeout *)aClosure;

  // Hold on to the timeout to ensure it doesn't go away while it's
  // being handled (aka kungFuDeathGrip).
  timeout->AddRef();

  timeout->mWindow->RunTimeout(timeout);

  // Drop our reference to the timeout now that we're done with it.
  timeout->Release();
}

//*****************************************************************************
// nsGlobalWindow: Helper Functions
//*****************************************************************************

nsresult
nsGlobalWindow::GetTreeOwner(nsIDocShellTreeOwner **aTreeOwner)
{
  FORWARD_TO_OUTER(GetTreeOwner, (aTreeOwner), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));

  // If there's no docShellAsItem, this window must have been closed,
  // in that case there is no tree owner.

  if (!docShellAsItem) {
    *aTreeOwner = nsnull;

    return NS_OK;
  }

  return docShellAsItem->GetTreeOwner(aTreeOwner);
}

nsresult
nsGlobalWindow::GetTreeOwner(nsIBaseWindow **aTreeOwner)
{
  FORWARD_TO_OUTER(GetTreeOwner, (aTreeOwner), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;

  // If there's no docShellAsItem, this window must have been closed,
  // in that case there is no tree owner.

  if (docShellAsItem) {
    docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
  }

  if (!treeOwner) {
    *aTreeOwner = nsnull;
    return NS_OK;
  }

  return CallQueryInterface(treeOwner, aTreeOwner);
}

nsresult
nsGlobalWindow::GetWebBrowserChrome(nsIWebBrowserChrome **aBrowserChrome)
{
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  GetTreeOwner(getter_AddRefs(treeOwner));

  nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(treeOwner));
  NS_IF_ADDREF(*aBrowserChrome = browserChrome);

  return NS_OK;
}

nsIScrollableFrame *
nsGlobalWindow::GetScrollFrame()
{
  FORWARD_TO_OUTER(GetScrollFrame, (), nsnull);

  if (!mDocShell) {
    return nsnull;
  }

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));
  if (presShell) {
    return presShell->GetRootScrollFrameAsScrollable();
  }
  return nsnull;
}

nsresult
nsGlobalWindow::BuildURIfromBase(const char *aURL, nsIURI **aBuiltURI,
                                 PRBool *aFreeSecurityPass,
                                 JSContext **aCXused)
{
  nsIScriptContext *scx = GetContextInternal();
  JSContext *cx = nsnull;

  *aBuiltURI = nsnull;
  *aFreeSecurityPass = PR_FALSE;
  if (aCXused)
    *aCXused = nsnull;

  // get JSContext
  NS_ASSERTION(scx, "opening window missing its context");
  NS_ASSERTION(mDocument, "opening window missing its document");
  if (!scx || !mDocument)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMChromeWindow> chrome_win =
    do_QueryInterface(static_cast<nsIDOMWindow *>(this));

  if (nsContentUtils::IsCallerChrome() && !chrome_win) {
    // If open() is called from chrome on a non-chrome window, we'll
    // use the context from the window on which open() is being called
    // to prevent giving chrome priveleges to new windows opened in
    // such a way. This also makes us get the appropriate base URI for
    // the below URI resolution code.

    cx = (JSContext *)scx->GetNativeContext();
  } else {
    // get the JSContext from the call stack
    nsCOMPtr<nsIThreadJSContextStack> stack(do_GetService(sJSStackContractID));
    if (stack)
      stack->Peek(&cx);
  }

  /* resolve the URI, which could be relative to the calling window
     (note the algorithm to get the base URI should match the one
     used to actually kick off the load in nsWindowWatcher.cpp). */
  nsCAutoString charset(NS_LITERAL_CSTRING("UTF-8")); // default to utf-8
  nsIURI* baseURI = nsnull;
  nsCOMPtr<nsIURI> uriToLoad;
  nsCOMPtr<nsIDOMWindow> sourceWindow;

  if (cx) {
    nsIScriptContext *scriptcx = nsJSUtils::GetDynamicScriptContext(cx);
    if (scriptcx)
      sourceWindow = do_QueryInterface(scriptcx->GetGlobalObject());
  }

  if (!sourceWindow) {
    sourceWindow = do_QueryInterface(NS_ISUPPORTS_CAST(nsIDOMWindow *, this));
    *aFreeSecurityPass = PR_TRUE;
  }

  if (sourceWindow) {
    nsCOMPtr<nsIDOMDocument> domDoc;
    sourceWindow->GetDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
    if (doc) {
      baseURI = doc->GetDocBaseURI();
      charset = doc->GetDocumentCharacterSet();
    }
  }

  if (aCXused)
    *aCXused = cx;
  return NS_NewURI(aBuiltURI, nsDependentCString(aURL), charset.get(), baseURI);
}

nsresult
nsGlobalWindow::SecurityCheckURL(const char *aURL)
{
  JSContext       *cx;
  PRBool           freePass;
  nsCOMPtr<nsIURI> uri;

  if (NS_FAILED(BuildURIfromBase(aURL, getter_AddRefs(uri), &freePass, &cx)))
    return NS_ERROR_FAILURE;

  if (!freePass && NS_FAILED(nsContentUtils::GetSecurityManager()->
        CheckLoadURIFromScript(cx, uri)))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

void
nsGlobalWindow::FlushPendingNotifications(mozFlushType aType)
{
  if (mDoc) {
    mDoc->FlushPendingNotifications(aType);
  }
}

void
nsGlobalWindow::EnsureSizeUpToDate()
{
  // If we're a subframe, make sure our size is up to date.  It's OK that this
  // crosses the content/chrome boundary, since chrome can have pending reflows
  // too.
  nsGlobalWindow *parent =
    static_cast<nsGlobalWindow *>(GetPrivateParent());
  if (parent) {
    parent->FlushPendingNotifications(Flush_Layout);
  }
}

nsresult
nsGlobalWindow::SaveWindowState(nsISupports **aState)
{
  NS_PRECONDITION(IsOuterWindow(), "Can't save the inner window's state");

  *aState = nsnull;

  if (!mContext || !mJSObject) {
    // The window may be getting torn down; don't bother saving state.
    return NS_OK;
  }

  nsGlobalWindow *inner = GetCurrentInnerWindowInternal();
  NS_ASSERTION(inner, "No inner window to save");

  // Don't do anything else to this inner window! After this point, all
  // calls to SetTimeoutOrInterval will create entries in the timeout
  // list that will only run after this window has come out of the bfcache.
  // Also, while we're frozen, we won't dispatch online/offline events
  // to the page.
  inner->Freeze();

  // Remember the outer window's prototype.
  JSContext *cx = (JSContext *)mContext->GetNativeContext();
  JSAutoRequest req(cx);

  nsIXPConnect *xpc = nsContentUtils::XPConnect();

  nsCOMPtr<nsIClassInfo> ci =
    do_QueryInterface((nsIScriptGlobalObject *)this);
  nsCOMPtr<nsIXPConnectJSObjectHolder> proto;
  nsresult rv = xpc->GetWrappedNativePrototype(cx, mJSObject, ci,
                                               getter_AddRefs(proto));
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject *realProto = JS_GetPrototype(cx, mJSObject);
  nsCOMPtr<nsIXPConnectJSObjectHolder> realProtoHolder;
  if (realProto) {
    rv = xpc->HoldObject(cx, realProto, getter_AddRefs(realProtoHolder));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsISupports> state = new WindowStateHolder(inner,
                                                      mInnerWindowHolder,
                                                      mNavigator,
                                                      proto,
                                                      realProtoHolder);
  NS_ENSURE_TRUE(state, NS_ERROR_OUT_OF_MEMORY);

  JSObject *wnProto;
  proto->GetJSObject(&wnProto);
  if (!JS_SetPrototype(cx, mJSObject, wnProto)) {
    return NS_ERROR_FAILURE;
  }

#ifdef DEBUG_PAGE_CACHE
  printf("saving window state, state = %p\n", (void*)state);
#endif

  state.swap(*aState);
  return NS_OK;
}

nsresult
nsGlobalWindow::RestoreWindowState(nsISupports *aState)
{
  NS_ASSERTION(IsOuterWindow(), "Cannot restore an inner window");

  if (!mContext || !mJSObject) {
    // The window may be getting torn down; don't bother restoring state.
    return NS_OK;
  }

  nsCOMPtr<WindowStateHolder> holder = do_QueryInterface(aState);
  NS_ENSURE_TRUE(holder, NS_ERROR_FAILURE);

#ifdef DEBUG_PAGE_CACHE
  printf("restoring window state, state = %p\n", (void*)holder);
#endif

  // And we're ready to go!
  nsGlobalWindow *inner = GetCurrentInnerWindowInternal();

  // if a link is focused, refocus with the FLAG_SHOWRING flag set. This makes
  // it easy to tell which link was last clicked when going back a page.
  nsIContent* focusedNode = inner->GetFocusedNode();
  if (IsLink(focusedNode)) {
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm) {
      nsCOMPtr<nsIDOMElement> focusedElement(do_QueryInterface(focusedNode));
      fm->SetFocus(focusedElement, nsIFocusManager::FLAG_NOSCROLL |
                                   nsIFocusManager::FLAG_SHOWRING);
    }
  }

  inner->Thaw();

  holder->DidRestoreWindow();

  return NS_OK;
}

void
nsGlobalWindow::SuspendTimeouts(PRUint32 aIncrease,
                                PRBool aFreezeChildren)
{
  FORWARD_TO_INNER_VOID(SuspendTimeouts, (aIncrease, aFreezeChildren));

  PRBool suspended = (mTimeoutsSuspendDepth != 0);
  mTimeoutsSuspendDepth += aIncrease;

  if (!suspended) {
    nsDOMThreadService* dts = nsDOMThreadService::get();
    if (dts) {
      dts->SuspendWorkersForGlobal(static_cast<nsIScriptGlobalObject*>(this));
    }
  
    TimeStamp now = TimeStamp::Now();
    for (nsTimeout *t = FirstTimeout(); IsTimeout(t); t = t->Next()) {
      // Set mTimeRemaining to be the time remaining for this timer.
      if (t->mWhen > now)
        t->mTimeRemaining = t->mWhen - now;
      else
        t->mTimeRemaining = TimeDuration(0);
  
      // Drop the XPCOM timer; we'll reschedule when restoring the state.
      if (t->mTimer) {
        t->mTimer->Cancel();
        t->mTimer = nsnull;
  
        // Drop the reference that the timer's closure had on this timeout, we'll
        // add it back in ResumeTimeouts. Note that it shouldn't matter that we're
        // passing null for the context, since this shouldn't actually release this
        // timeout.
        t->Release();
      }
    }
  }

  // Suspend our children as well.
  nsCOMPtr<nsIDocShellTreeNode> node(do_QueryInterface(GetDocShell()));
  if (node) {
    PRInt32 childCount = 0;
    node->GetChildCount(&childCount);

    for (PRInt32 i = 0; i < childCount; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> childShell;
      node->GetChildAt(i, getter_AddRefs(childShell));
      NS_ASSERTION(childShell, "null child shell");

      nsCOMPtr<nsPIDOMWindow> pWin = do_GetInterface(childShell);
      if (pWin) {
        nsGlobalWindow *win =
          static_cast<nsGlobalWindow*>
                     (static_cast<nsPIDOMWindow*>(pWin));
        NS_ASSERTION(win->IsOuterWindow(), "Expected outer window");
        nsGlobalWindow* inner = win->GetCurrentInnerWindowInternal();

        // This is a bit hackish. Only freeze/suspend windows which are truly our
        // subwindows.
        nsCOMPtr<nsIContent> frame = do_QueryInterface(pWin->GetFrameElementInternal());
        if (!mDoc || !frame || mDoc != frame->GetOwnerDoc() || !inner) {
          continue;
        }

        win->SuspendTimeouts(aIncrease, aFreezeChildren);

        if (inner && aFreezeChildren) {
          inner->Freeze();
        }
      }
    }
  }
}

nsresult
nsGlobalWindow::ResumeTimeouts(PRBool aThawChildren)
{
  FORWARD_TO_INNER(ResumeTimeouts, (), NS_ERROR_NOT_INITIALIZED);

  NS_ASSERTION(mTimeoutsSuspendDepth, "Mismatched calls to ResumeTimeouts!");
  --mTimeoutsSuspendDepth;
  PRBool shouldResume = (mTimeoutsSuspendDepth == 0);
  nsresult rv;

  if (shouldResume) {
    nsDOMThreadService* dts = nsDOMThreadService::get();
    if (dts) {
      dts->ResumeWorkersForGlobal(static_cast<nsIScriptGlobalObject*>(this));
    }

    // Restore all of the timeouts, using the stored time remaining
    // (stored in timeout->mTimeRemaining).

    TimeStamp now = TimeStamp::Now();

#ifdef DEBUG
    PRBool _seenDummyTimeout = PR_FALSE;
#endif

    for (nsTimeout *t = FirstTimeout(); IsTimeout(t); t = t->Next()) {
      // There's a chance we're being called with RunTimeout on the stack in which
      // case we have a dummy timeout in the list that *must not* be resumed. It
      // can be identified by a null mWindow.
      if (!t->mWindow) {
#ifdef DEBUG
        NS_ASSERTION(!_seenDummyTimeout, "More than one dummy timeout?!");
        _seenDummyTimeout = PR_TRUE;
#endif
        continue;
      }

      // XXXbz the combination of the way |delay| and |t->mWhen| are set here
      // makes no sense.  Are we trying to impose that min timeout value or
      // not???
      PRUint32 delay =
        NS_MAX(PRInt32(t->mTimeRemaining.ToMilliseconds()),
               DOM_MIN_TIMEOUT_VALUE);

      // Set mWhen back to the time when the timer is supposed to
      // fire.
      t->mWhen = now + t->mTimeRemaining;

      t->mTimer = do_CreateInstance("@mozilla.org/timer;1");
      NS_ENSURE_TRUE(t->mTimer, NS_ERROR_OUT_OF_MEMORY);

      rv = t->mTimer->InitWithFuncCallback(TimerCallback, t, delay,
                                           nsITimer::TYPE_ONE_SHOT);
      if (NS_FAILED(rv)) {
        t->mTimer = nsnull;
        return rv;
      }

      // Add a reference for the new timer's closure.
      t->AddRef();
    }
  }

  // Resume our children as well.
  nsCOMPtr<nsIDocShellTreeNode> node =
    do_QueryInterface(GetDocShell());
  if (node) {
    PRInt32 childCount = 0;
    node->GetChildCount(&childCount);

    for (PRInt32 i = 0; i < childCount; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> childShell;
      node->GetChildAt(i, getter_AddRefs(childShell));
      NS_ASSERTION(childShell, "null child shell");

      nsCOMPtr<nsPIDOMWindow> pWin = do_GetInterface(childShell);
      if (pWin) {
        nsGlobalWindow *win =
          static_cast<nsGlobalWindow*>
                     (static_cast<nsPIDOMWindow*>(pWin));

        NS_ASSERTION(win->IsOuterWindow(), "Expected outer window");
        nsGlobalWindow* inner = win->GetCurrentInnerWindowInternal();

        // This is a bit hackish. Only thaw/resume windows which are truly our
        // subwindows.
        nsCOMPtr<nsIContent> frame = do_QueryInterface(pWin->GetFrameElementInternal());
        if (!mDoc || !frame || mDoc != frame->GetOwnerDoc() || !inner) {
          continue;
        }

        if (inner && aThawChildren) {
          inner->Thaw();
        }

        rv = win->ResumeTimeouts(aThawChildren);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  return NS_OK;
}

PRUint32
nsGlobalWindow::TimeoutSuspendCount()
{
  FORWARD_TO_INNER(TimeoutSuspendCount, (), 0);
  return mTimeoutsSuspendDepth;
}

NS_IMETHODIMP
nsGlobalWindow::GetScriptTypeID(PRUint32 *aScriptType)
{
  NS_ERROR("No default script type here - ask some element");
  return nsIProgrammingLanguage::UNKNOWN;
}

NS_IMETHODIMP
nsGlobalWindow::SetScriptTypeID(PRUint32 aScriptType)
{
  NS_ERROR("Can't change default script type for a document");
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
nsGlobalWindow::SetHasOrientationEventListener()
{
  nsCOMPtr<nsIAccelerometer> ac = 
    do_GetService(NS_ACCELEROMETER_CONTRACTID);

  if (ac) {
    mHasAcceleration = PR_TRUE;
    ac->AddWindowListener(this);
  }
}

// nsGlobalChromeWindow implementation

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGlobalChromeWindow)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsGlobalChromeWindow,
                                                  nsGlobalWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mBrowserDOMWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mMessageManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(ChromeWindow, nsGlobalChromeWindow)
DOMCI_DATA(InnerChromeWindow, nsGlobalChromeWindow)

// QueryInterface implementation for nsGlobalChromeWindow
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsGlobalChromeWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMChromeWindow)
  WINDOW_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ChromeWindow)
NS_INTERFACE_MAP_END_INHERITING(nsGlobalWindow)

NS_IMPL_ADDREF_INHERITED(nsGlobalChromeWindow, nsGlobalWindow)
NS_IMPL_RELEASE_INHERITED(nsGlobalChromeWindow, nsGlobalWindow)

NS_IMETHODIMP
nsGlobalChromeWindow::GetWindowState(PRUint16* aWindowState)
{
  *aWindowState = nsIDOMChromeWindow::STATE_NORMAL;

  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  PRInt32 aMode = 0;

  if (widget) {
    nsresult rv = widget->GetSizeMode(&aMode);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  switch (aMode) {
    case nsSizeMode_Minimized:
      *aWindowState = nsIDOMChromeWindow::STATE_MINIMIZED;
      break;
    case nsSizeMode_Maximized:
      *aWindowState = nsIDOMChromeWindow::STATE_MAXIMIZED;
      break;
    case nsSizeMode_Fullscreen:
      *aWindowState = nsIDOMChromeWindow::STATE_FULLSCREEN;
      break;
    case nsSizeMode_Normal:
      *aWindowState = nsIDOMChromeWindow::STATE_NORMAL;
      break;
    default:
      NS_WARNING("Illegal window state for this chrome window");
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalChromeWindow::Maximize()
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();
  nsresult rv = NS_OK;

  if (widget) {
    rv = widget->SetSizeMode(nsSizeMode_Maximized);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalChromeWindow::Minimize()
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();
  nsresult rv = NS_OK;

  if (widget)
    rv = widget->SetSizeMode(nsSizeMode_Minimized);

  return rv;
}

NS_IMETHODIMP
nsGlobalChromeWindow::Restore()
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();
  nsresult rv = NS_OK;

  if (widget) {
    rv = widget->SetSizeMode(nsSizeMode_Normal);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetAttention()
{
  return GetAttentionWithCycleCount(-1);
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetAttentionWithCycleCount(PRInt32 aCycleCount)
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();
  nsresult rv = NS_OK;

  if (widget) {
    rv = widget->GetAttention(aCycleCount);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalChromeWindow::BeginWindowMove(nsIDOMEvent *aMouseDownEvent)
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();
  if (!widget) {
    return NS_OK;
  }

  nsCOMPtr<nsIPrivateDOMEvent> privEvent = do_QueryInterface(aMouseDownEvent);
  NS_ENSURE_TRUE(privEvent, NS_ERROR_FAILURE);
  nsEvent *internalEvent = privEvent->GetInternalNSEvent();
  NS_ENSURE_TRUE(internalEvent &&
                 internalEvent->eventStructType == NS_MOUSE_EVENT,
                 NS_ERROR_FAILURE);
  nsMouseEvent *mouseEvent = static_cast<nsMouseEvent*>(internalEvent);

  return widget->BeginMoveDrag(mouseEvent);
}

//Note: This call will lock the cursor, it will not change as it moves.
//To unlock, the cursor must be set back to CURSOR_AUTO.
NS_IMETHODIMP
nsGlobalChromeWindow::SetCursor(const nsAString& aCursor)
{
  FORWARD_TO_OUTER_CHROME(SetCursor, (aCursor), NS_ERROR_NOT_INITIALIZED);

  nsresult rv = NS_OK;
  PRInt32 cursor;

  // use C strings to keep the code/data size down
  NS_ConvertUTF16toUTF8 cursorString(aCursor);

  if (cursorString.Equals("auto"))
    cursor = NS_STYLE_CURSOR_AUTO;
  else {
    nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(aCursor);
    if (eCSSKeyword_UNKNOWN == keyword ||
        !nsCSSProps::FindKeyword(keyword, nsCSSProps::kCursorKTable, cursor)) {
      // XXX remove the following three values (leave return NS_OK) after 1.8
      // XXX since they should have been -moz- prefixed (covered by FindKeyword).
      // XXX (also remove |cursorString| at that point?).
      if (cursorString.Equals("grab"))
        cursor = NS_STYLE_CURSOR_GRAB;
      else if (cursorString.Equals("grabbing"))
        cursor = NS_STYLE_CURSOR_GRABBING;
      else if (cursorString.Equals("spinning"))
        cursor = NS_STYLE_CURSOR_SPINNING;
      else
        return NS_OK;
    }
  }

  nsRefPtr<nsPresContext> presContext;
  if (mDocShell) {
    mDocShell->GetPresContext(getter_AddRefs(presContext));
  }

  if (presContext) {
    // Need root widget.
    nsCOMPtr<nsIPresShell> presShell;
    mDocShell->GetPresShell(getter_AddRefs(presShell));
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    nsIViewManager* vm = presShell->GetViewManager();
    NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

    nsIView *rootView;
    vm->GetRootView(rootView);
    NS_ENSURE_TRUE(rootView, NS_ERROR_FAILURE);

    nsIWidget* widget = rootView->GetNearestWidget(nsnull);
    NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

    // Call esm and set cursor.
    rv = presContext->EventStateManager()->SetCursor(cursor, nsnull,
                                                     PR_FALSE, 0.0f, 0.0f,
                                                     widget, PR_TRUE);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetBrowserDOMWindow(nsIBrowserDOMWindow **aBrowserWindow)
{
  FORWARD_TO_OUTER_CHROME(GetBrowserDOMWindow, (aBrowserWindow),
                          NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_ARG_POINTER(aBrowserWindow);

  *aBrowserWindow = mBrowserDOMWindow;
  NS_IF_ADDREF(*aBrowserWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalChromeWindow::SetBrowserDOMWindow(nsIBrowserDOMWindow *aBrowserWindow)
{
  FORWARD_TO_OUTER_CHROME(SetBrowserDOMWindow, (aBrowserWindow),
                          NS_ERROR_NOT_INITIALIZED);

  mBrowserDOMWindow = aBrowserWindow;
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalChromeWindow::NotifyDefaultButtonLoaded(nsIDOMElement* aDefaultButton)
{
#ifdef MOZ_XUL
  NS_ENSURE_ARG(aDefaultButton);

  // Don't snap to a disabled button.
  nsCOMPtr<nsIDOMXULControlElement> xulControl =
                                      do_QueryInterface(aDefaultButton);
  NS_ENSURE_TRUE(xulControl, NS_ERROR_FAILURE);
  PRBool disabled;
  nsresult rv = xulControl->GetDisabled(&disabled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (disabled)
    return NS_OK;

  // Get the button rect in screen coordinates.
  nsCOMPtr<nsIContent> content(do_QueryInterface(aDefaultButton));
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);
  nsIFrame *frame = content->GetPrimaryFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);
  nsIntRect buttonRect = frame->GetScreenRect();

  // Get the widget rect in screen coordinates.
  nsIWidget *widget = GetNearestWidget();
  NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);
  nsIntRect widgetRect;
  rv = widget->GetScreenBounds(widgetRect);
  NS_ENSURE_SUCCESS(rv, rv);

  // Convert the buttonRect coordinates from screen to the widget.
  buttonRect -= widgetRect.TopLeft();
  rv = widget->OnDefaultButtonLoaded(buttonRect);
  if (rv == NS_ERROR_NOT_IMPLEMENTED)
    return NS_OK;
  return rv;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetMessageManager(nsIChromeFrameMessageManager** aManager)
{
  FORWARD_TO_INNER_CHROME(GetMessageManager, (aManager), NS_ERROR_FAILURE);
  if (!mMessageManager) {
    nsIScriptContext* scx = GetContextInternal();
    NS_ENSURE_STATE(scx);
    JSContext* cx = (JSContext *)scx->GetNativeContext();
    NS_ENSURE_STATE(cx);
    nsCOMPtr<nsIChromeFrameMessageManager> globalMM =
      do_GetService("@mozilla.org/globalmessagemanager;1");
    mMessageManager =
      new nsFrameMessageManager(PR_TRUE,
                                nsnull,
                                nsnull,
                                nsnull,
                                nsnull,
                                static_cast<nsFrameMessageManager*>(globalMM.get()),
                                cx);
    NS_ENSURE_TRUE(mMessageManager, NS_ERROR_OUT_OF_MEMORY);
  }
  CallQueryInterface(mMessageManager, aManager);
  return NS_OK;
}

// nsGlobalModalWindow implementation

// QueryInterface implementation for nsGlobalModalWindow
NS_IMPL_CYCLE_COLLECTION_CLASS(nsGlobalModalWindow)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsGlobalModalWindow,
                                                  nsGlobalWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mReturnValue)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(ModalContentWindow, nsGlobalModalWindow)
DOMCI_DATA(InnerModalContentWindow, nsGlobalModalWindow)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsGlobalModalWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMModalContentWindow)
  WINDOW_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ModalContentWindow)
NS_INTERFACE_MAP_END_INHERITING(nsGlobalWindow)

NS_IMPL_ADDREF_INHERITED(nsGlobalModalWindow, nsGlobalWindow)
NS_IMPL_RELEASE_INHERITED(nsGlobalModalWindow, nsGlobalWindow)


NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsGlobalModalWindow,
                                                nsGlobalWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mReturnValue)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


NS_IMETHODIMP
nsGlobalModalWindow::GetDialogArguments(nsIArray **aArguments)
{
  FORWARD_TO_INNER_MODAL_CONTENT_WINDOW(GetDialogArguments, (aArguments),
                                        NS_ERROR_NOT_INITIALIZED);

  PRBool subsumes = PR_FALSE;
  nsIPrincipal *self = GetPrincipal();
  if (self && NS_SUCCEEDED(self->Subsumes(mArgumentsOrigin, &subsumes)) &&
      subsumes) {
    NS_IF_ADDREF(*aArguments = mArguments);
  } else {
    *aArguments = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalModalWindow::GetReturnValue(nsIVariant **aRetVal)
{
  FORWARD_TO_OUTER_MODAL_CONTENT_WINDOW(GetReturnValue, (aRetVal), NS_OK);

  NS_IF_ADDREF(*aRetVal = mReturnValue);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalModalWindow::SetReturnValue(nsIVariant *aRetVal)
{
  FORWARD_TO_OUTER_MODAL_CONTENT_WINDOW(SetReturnValue, (aRetVal), NS_OK);

  mReturnValue = aRetVal;

  return NS_OK;
}

nsresult
nsGlobalModalWindow::SetNewDocument(nsIDocument *aDocument,
                                    nsISupports *aState,
                                    PRBool aForceReuseInnerWindow)
{
  // If we're loading a new document into a modal dialog, clear the
  // return value that was set, if any, by the current document.
  if (aDocument) {
    mReturnValue = nsnull;
  }

  return nsGlobalWindow::SetNewDocument(aDocument, aState,
                                        aForceReuseInnerWindow);
}

//*****************************************************************************
// nsGlobalWindow: Creator Function (This should go away)
//*****************************************************************************

nsresult
NS_NewScriptGlobalObject(PRBool aIsChrome, PRBool aIsModalContentWindow,
                         nsIScriptGlobalObject **aResult)
{
  *aResult = nsnull;

  nsGlobalWindow *global;

  if (aIsChrome) {
    global = new nsGlobalChromeWindow(nsnull);
  } else if (aIsModalContentWindow) {
    global = new nsGlobalModalWindow(nsnull);
  } else {
    global = new nsGlobalWindow(nsnull);
  }

  NS_ENSURE_TRUE(global, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aResult = global);

  return NS_OK;
}

//*****************************************************************************
//***    nsNavigator: Object Management
//*****************************************************************************

nsNavigator::nsNavigator(nsIDocShell *aDocShell)
  : mDocShell(aDocShell)
{
}

nsNavigator::~nsNavigator()
{
  if (mMimeTypes)
    mMimeTypes->Invalidate();
  if (mPlugins)
    mPlugins->Invalidate();
}

//*****************************************************************************
//    nsNavigator::nsISupports
//*****************************************************************************


DOMCI_DATA(Navigator, nsNavigator)

// QueryInterface implementation for nsNavigator
NS_INTERFACE_MAP_BEGIN(nsNavigator)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMNavigator)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigator)
  NS_INTERFACE_MAP_ENTRY(nsIDOMClientInformation)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigatorGeolocation)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigatorDesktopNotification)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Navigator)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsNavigator)
NS_IMPL_RELEASE(nsNavigator)


void
nsNavigator::SetDocShell(nsIDocShell *aDocShell)
{
  mDocShell = aDocShell;
  if (mPlugins)
    mPlugins->SetDocShell(aDocShell);

  // if there is a page transition, make sure delete the geolocation object
  if (mGeolocation)
  {
    mGeolocation->Shutdown();
    mGeolocation = nsnull;
  }
}

//*****************************************************************************
//    nsNavigator::nsIDOMNavigator
//*****************************************************************************

NS_IMETHODIMP
nsNavigator::GetUserAgent(nsAString& aUserAgent)
{
  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString ua;
    rv = service->GetUserAgent(ua);
    CopyASCIItoUTF16(ua, aUserAgent);
  }

  return rv;
}

NS_IMETHODIMP
nsNavigator::GetAppCodeName(nsAString& aAppCodeName)
{
  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString appName;
    rv = service->GetAppName(appName);
    CopyASCIItoUTF16(appName, aAppCodeName);
  }

  return rv;
}

NS_IMETHODIMP
nsNavigator::GetAppVersion(nsAString& aAppVersion)
{
  if (!nsContentUtils::IsCallerTrustedForRead()) {
    const nsAdoptingCString& override = 
      nsContentUtils::GetCharPref("general.appversion.override");

    if (override) {
      CopyUTF8toUTF16(override, aAppVersion);
      return NS_OK;
    }
  }

  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString str;
    rv = service->GetAppVersion(str);
    CopyASCIItoUTF16(str, aAppVersion);
    if (NS_FAILED(rv))
      return rv;

    aAppVersion.AppendLiteral(" (");

    rv = service->GetPlatform(str);
    if (NS_FAILED(rv))
      return rv;

    AppendASCIItoUTF16(str, aAppVersion);

    aAppVersion.Append(PRUnichar(')'));
  }

  return rv;
}

NS_IMETHODIMP
nsNavigator::GetAppName(nsAString& aAppName)
{
  if (!nsContentUtils::IsCallerTrustedForRead()) {
    const nsAdoptingCString& override =
      nsContentUtils::GetCharPref("general.appname.override");

    if (override) {
      CopyUTF8toUTF16(override, aAppName);
      return NS_OK;
    }
  }

  aAppName.AssignLiteral("Netscape");
  return NS_OK;
}

NS_IMETHODIMP
nsNavigator::GetLanguage(nsAString& aLanguage)
{
  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString lang;
    rv = service->GetLanguage(lang);
    CopyASCIItoUTF16(lang, aLanguage);
  }

  return rv;
}

NS_IMETHODIMP
nsNavigator::GetPlatform(nsAString& aPlatform)
{
  if (!nsContentUtils::IsCallerTrustedForRead()) {
    const nsAdoptingCString& override =
      nsContentUtils::GetCharPref("general.platform.override");

    if (override) {
      CopyUTF8toUTF16(override, aPlatform);
      return NS_OK;
    }
  }

  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    // sorry for the #if platform ugliness, but Communicator is
    // likewise hardcoded and we're seeking backward compatibility
    // here (bug 47080)
#if defined(_WIN64)
    aPlatform.AssignLiteral("Win64");
#elif defined(WIN32)
    aPlatform.AssignLiteral("Win32");
#elif defined(XP_MACOSX) && defined(__ppc__)
    aPlatform.AssignLiteral("MacPPC");
#elif defined(XP_MACOSX) && defined(__i386__)
    aPlatform.AssignLiteral("MacIntel");
#elif defined(XP_MACOSX) && defined(__x86_64__)
    aPlatform.AssignLiteral("MacIntel");
#elif defined(XP_OS2)
    aPlatform.AssignLiteral("OS/2");
#else
    // XXX Communicator uses compiled-in build-time string defines
    // to indicate the platform it was compiled *for*, not what it is
    // currently running *on* which is what this does.
    nsCAutoString plat;
    rv = service->GetOscpu(plat);
    CopyASCIItoUTF16(plat, aPlatform);
#endif
  }

  return rv;
}

NS_IMETHODIMP
nsNavigator::GetOscpu(nsAString& aOSCPU)
{
  if (!nsContentUtils::IsCallerTrustedForRead()) {
    const nsAdoptingCString& override =
      nsContentUtils::GetCharPref("general.oscpu.override");

    if (override) {
      CopyUTF8toUTF16(override, aOSCPU);
      return NS_OK;
    }
  }

  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString oscpu;
    rv = service->GetOscpu(oscpu);
    CopyASCIItoUTF16(oscpu, aOSCPU);
  }

  return rv;
}

NS_IMETHODIMP
nsNavigator::GetVendor(nsAString& aVendor)
{
  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString vendor;
    rv = service->GetVendor(vendor);
    CopyASCIItoUTF16(vendor, aVendor);
  }

  return rv;
}


NS_IMETHODIMP
nsNavigator::GetVendorSub(nsAString& aVendorSub)
{
  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString vendor;
    rv = service->GetVendorSub(vendor);
    CopyASCIItoUTF16(vendor, aVendorSub);
  }

  return rv;
}

NS_IMETHODIMP
nsNavigator::GetProduct(nsAString& aProduct)
{
  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString product;
    rv = service->GetProduct(product);
    CopyASCIItoUTF16(product, aProduct);
  }

  return rv;
}

NS_IMETHODIMP
nsNavigator::GetProductSub(nsAString& aProductSub)
{
  if (!nsContentUtils::IsCallerTrustedForRead()) {
    const nsAdoptingCString& override =
      nsContentUtils::GetCharPref("general.productSub.override");

    if (override) {
      CopyUTF8toUTF16(override, aProductSub);
      return NS_OK;
    } else {
      // 'general.useragent.productSub' backwards compatible with 1.8 branch.
      const nsAdoptingCString& override2 =
        nsContentUtils::GetCharPref("general.useragent.productSub");

      if (override2) {
        CopyUTF8toUTF16(override2, aProductSub);
        return NS_OK;
      }
    }
  }

  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString productSub;
    rv = service->GetProductSub(productSub);
    CopyASCIItoUTF16(productSub, aProductSub);
  }

  return rv;
}

NS_IMETHODIMP
nsNavigator::GetSecurityPolicy(nsAString& aSecurityPolicy)
{
  return NS_OK;
}

NS_IMETHODIMP
nsNavigator::GetMimeTypes(nsIDOMMimeTypeArray **aMimeTypes)
{
  if (!mMimeTypes) {
    mMimeTypes = new nsMimeTypeArray(this);
    if (!mMimeTypes) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_ADDREF(*aMimeTypes = mMimeTypes);

  return NS_OK;
}

NS_IMETHODIMP
nsNavigator::GetPlugins(nsIDOMPluginArray **aPlugins)
{
  if (!mPlugins) {
    mPlugins = new nsPluginArray(this, mDocShell);
    if (!mPlugins) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_ADDREF(*aPlugins = mPlugins);

  return NS_OK;
}

// values for the network.cookie.cookieBehavior pref are documented in
// nsCookieService.cpp.
#define COOKIE_BEHAVIOR_REJECT 2

NS_IMETHODIMP
nsNavigator::GetCookieEnabled(PRBool *aCookieEnabled)
{
  *aCookieEnabled =
    (nsContentUtils::GetIntPref("network.cookie.cookieBehavior",
                                COOKIE_BEHAVIOR_REJECT) !=
     COOKIE_BEHAVIOR_REJECT);

  return NS_OK;
}

NS_IMETHODIMP
nsNavigator::GetOnLine(PRBool* aOnline)
{
  NS_PRECONDITION(aOnline, "Null out param");
  
  *aOnline = !NS_IsOffline();
  return NS_OK;
}

NS_IMETHODIMP
nsNavigator::GetBuildID(nsAString& aBuildID)
{
  if (!nsContentUtils::IsCallerTrustedForRead()) {
    const nsAdoptingCString& override =
      nsContentUtils::GetCharPref("general.buildID.override");

    if (override) {
      CopyUTF8toUTF16(override, aBuildID);
      return NS_OK;
    }
  }

  nsCOMPtr<nsIXULAppInfo> appInfo =
    do_GetService("@mozilla.org/xre/app-info;1");
  if (!appInfo)
    return NS_ERROR_NOT_IMPLEMENTED;

  nsCAutoString buildID;
  nsresult rv = appInfo->GetAppBuildID(buildID);
  if (NS_FAILED(rv))
    return rv;

  aBuildID.Truncate();
  AppendASCIItoUTF16(buildID, aBuildID);
  return NS_OK;
}

NS_IMETHODIMP
nsNavigator::JavaEnabled(PRBool *aReturn)
{
  // Return true if we have a handler for "application/x-java-vm",
  // otherwise return false.
  *aReturn = PR_FALSE;

  if (!mMimeTypes) {
    mMimeTypes = new nsMimeTypeArray(this);
    if (!mMimeTypes)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  RefreshMIMEArray();

  PRUint32 count;
  mMimeTypes->GetLength(&count);
  for (PRUint32 i = 0; i < count; i++) {
    nsresult rv;
    nsIDOMMimeType* type = mMimeTypes->GetItemAt(i, &rv);
    nsAutoString mimeString;
    if (type && NS_SUCCEEDED(type->GetType(mimeString))) {
      if (mimeString.EqualsLiteral("application/x-java-vm")) {
        *aReturn = PR_TRUE;
        break;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNavigator::TaintEnabled(PRBool *aReturn)
{
  *aReturn = PR_FALSE;
  return NS_OK;
}

void
nsNavigator::LoadingNewDocument()
{
  // Release these so that they will be recreated for the
  // new document (if requested).  The plugins or mime types
  // arrays may have changed.  See bug 150087.
  if (mMimeTypes) {
    mMimeTypes->Invalidate();
    mMimeTypes = nsnull;
  }

  if (mPlugins) {
    mPlugins->Invalidate();
    mPlugins = nsnull;
  }

  if (mGeolocation)
  {
    mGeolocation->Shutdown();
    mGeolocation = nsnull;
  }
}

nsresult
nsNavigator::RefreshMIMEArray()
{
  nsresult rv = NS_OK;
  if (mMimeTypes)
    rv = mMimeTypes->Refresh();
  return rv;
}

//*****************************************************************************
//    nsNavigator::nsIDOMClientInformation
//*****************************************************************************

NS_IMETHODIMP
nsNavigator::RegisterContentHandler(const nsAString& aMIMEType, 
                                    const nsAString& aURI, 
                                    const nsAString& aTitle)
{
  nsCOMPtr<nsIWebContentHandlerRegistrar> registrar = 
    do_GetService(NS_WEBCONTENTHANDLERREGISTRAR_CONTRACTID);
  if (registrar && mDocShell) {
    nsCOMPtr<nsIDOMWindow> contentDOMWindow(do_GetInterface(mDocShell));
    if (contentDOMWindow)
      return registrar->RegisterContentHandler(aMIMEType, aURI, aTitle,
                                               contentDOMWindow);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNavigator::RegisterProtocolHandler(const nsAString& aProtocol, 
                                     const nsAString& aURI, 
                                     const nsAString& aTitle)
{
  nsCOMPtr<nsIWebContentHandlerRegistrar> registrar = 
    do_GetService(NS_WEBCONTENTHANDLERREGISTRAR_CONTRACTID);
  if (registrar && mDocShell) {
    nsCOMPtr<nsIDOMWindow> contentDOMWindow(do_GetInterface(mDocShell));
    if (contentDOMWindow)
      return registrar->RegisterProtocolHandler(aProtocol, aURI, aTitle,
                                                contentDOMWindow);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavigator::MozIsLocallyAvailable(const nsAString &aURI,
                                   PRBool aWhenOffline,
                                   PRBool *aIsAvailable)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  // This method of checking the cache will only work for http/https urls
  PRBool match;
  rv = uri->SchemeIs("http", &match);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!match) {
    rv = uri->SchemeIs("https", &match);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!match) return NS_ERROR_DOM_BAD_URI;
  }

  // Same origin check
  nsCOMPtr<nsIJSContextStack> stack = do_GetService(sJSStackContractID);
  NS_ENSURE_TRUE(stack, NS_ERROR_FAILURE);

  JSContext *cx = nsnull;
  rv = stack->Peek(&cx);
  NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);

  rv = nsContentUtils::GetSecurityManager()->CheckSameOrigin(cx, uri);
  NS_ENSURE_SUCCESS(rv, rv);

  // these load flags cause an error to be thrown if there is no
  // valid cache entry, and skip the load if there is.
  // if the cache is busy, assume that it is not yet available rather
  // than waiting for it to become available.
  PRUint32 loadFlags = nsIChannel::INHIBIT_CACHING |
                       nsICachingChannel::LOAD_NO_NETWORK_IO |
                       nsICachingChannel::LOAD_ONLY_IF_MODIFIED |
                       nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE_IF_BUSY;

  if (aWhenOffline) {
    loadFlags |= nsICachingChannel::LOAD_CHECK_OFFLINE_CACHE |
                 nsICachingChannel::LOAD_ONLY_FROM_CACHE |
                 nsIRequest::LOAD_FROM_CACHE;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), uri,
                     nsnull, nsnull, nsnull, loadFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> stream;
  rv = channel->Open(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  stream->Close();

  nsresult status;
  rv = channel->GetStatus(&status);
  NS_ENSURE_SUCCESS(rv, rv);

  if (NS_SUCCEEDED(status)) {
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
    rv = httpChannel->GetRequestSucceeded(aIsAvailable);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    *aIsAvailable = PR_FALSE;
  }

  return NS_OK;
}

//*****************************************************************************
//    nsNavigator::nsIDOMNavigatorGeolocation
//*****************************************************************************

NS_IMETHODIMP nsNavigator::GetGeolocation(nsIDOMGeoGeolocation **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  if (mGeolocation) {
    NS_ADDREF(*_retval = mGeolocation);
    return NS_OK;
  }

  if (!mDocShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindow> contentDOMWindow(do_GetInterface(mDocShell));
  if (!contentDOMWindow)
    return NS_ERROR_FAILURE;
    
  mGeolocation = new nsGeolocation();
  if (!mGeolocation)
    return NS_ERROR_FAILURE;
  
  if (NS_FAILED(mGeolocation->Init(contentDOMWindow)))
    return NS_ERROR_FAILURE;
  
  NS_ADDREF(*_retval = mGeolocation);    
  return NS_OK; 
}


//*****************************************************************************
//    nsNavigator::nsIDOMNavigatorDesktopNotification
//*****************************************************************************

NS_IMETHODIMP nsNavigator::GetMozNotification(nsIDOMDesktopNotificationCenter **aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);
  *aRetVal = nsnull;

  nsCOMPtr<nsPIDOMWindow> window(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
    
  nsCOMPtr<nsIDocument> document = do_GetInterface(mDocShell);
  NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

  nsIScriptGlobalObject *sgo = document->GetScopeObject();
  NS_ENSURE_TRUE(sgo, NS_ERROR_FAILURE);

  nsIScriptContext *scx = sgo->GetContext();
  NS_ENSURE_TRUE(scx, NS_ERROR_FAILURE);

  nsRefPtr<nsDesktopNotificationCenter> notification =
    new nsDesktopNotificationCenter(window->GetCurrentInnerWindow(),
                                    scx);

  if (!notification) {
    return NS_ERROR_FAILURE;
  }

  *aRetVal = notification.forget().get();
  return NS_OK; 
}
