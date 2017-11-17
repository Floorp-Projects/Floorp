/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGlobalWindow.h"

#include <algorithm>

#include "mozilla/MemoryReporting.h"

// Local Includes
#include "Navigator.h"
#include "nsContentSecurityManager.h"
#include "nsScreen.h"
#include "nsHistory.h"
#include "nsDOMNavigationTiming.h"
#include "nsIDOMStorageManager.h"
#include "mozilla/dom/LocalStorage.h"
#include "mozilla/dom/Storage.h"
#include "mozilla/dom/IdleRequest.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/StorageEvent.h"
#include "mozilla/dom/StorageEventBinding.h"
#include "mozilla/dom/StorageNotifierService.h"
#include "mozilla/dom/StorageUtils.h"
#include "mozilla/dom/Timeout.h"
#include "mozilla/dom/TimeoutHandler.h"
#include "mozilla/dom/TimeoutManager.h"
#include "mozilla/IntegerPrintfMacros.h"
#if defined(MOZ_WIDGET_ANDROID)
#include "mozilla/dom/WindowOrientationObserver.h"
#endif
#include "nsDOMOfflineResourceList.h"
#include "nsError.h"
#include "nsIIdleService.h"
#include "nsISizeOfEventTarget.h"
#include "nsDOMJSUtils.h"
#include "nsArrayUtils.h"
#include "nsIDOMWindowCollection.h"
#include "nsDOMWindowList.h"
#include "mozilla/dom/WakeLock.h"
#include "mozilla/dom/power/PowerManagerService.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPermissionManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptTimeoutHandler.h"
#include "nsITimeoutHandler.h"
#include "nsIController.h"
#include "nsScriptNameSpaceManager.h"
#include "nsISlowScriptDebug.h"
#include "nsWindowMemoryReporter.h"
#include "nsWindowSizes.h"
#include "WindowNamedPropertiesHandler.h"
#include "nsFrameSelection.h"
#include "nsNetUtil.h"
#include "nsVariant.h"
#include "nsPrintfCString.h"
#include "mozilla/intl/LocaleService.h"
#include "WindowDestroyedEvent.h"

// Helper Classes
#include "nsJSUtils.h"
#include "jsapi.h"              // for JSAutoRequest
#include "jswrapper.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsReadableUtils.h"
#include "nsDOMClassInfo.h"
#include "nsJSEnvironment.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/Preferences.h"
#include "mozilla/Likely.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Unused.h"

// Other Classes
#include "mozilla/dom/BarProps.h"
#include "nsContentCID.h"
#include "nsLayoutStatics.h"
#include "nsCCUncollectableMarker.h"
#include "mozilla/dom/workers/Workers.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsJSPrincipals.h"
#include "mozilla/Attributes.h"
#include "mozilla/Debug.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/ProcessHangMonitor.h"
#include "mozilla/ThrottledEventQueue.h"
#include "AudioChannelService.h"
#include "nsAboutProtocolUtils.h"
#include "nsCharTraits.h" // NS_IS_HIGH/LOW_SURROGATE
#include "PostMessageEvent.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/TabGroup.h"

// Interfaces Needed
#include "nsIFrame.h"
#include "nsCanvasFrame.h"
#include "nsIWidget.h"
#include "nsIWidgetListener.h"
#include "nsIBaseWindow.h"
#include "nsIDeviceSensors.h"
#include "nsIContent.h"
#include "nsIDocShell.h"
#include "nsIDocCharset.h"
#include "nsIDocument.h"
#include "Crypto.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMOfflineResourceList.h"
#include "nsDOMString.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsThreadUtils.h"
#include "nsILoadContext.h"
#include "nsIPresShell.h"
#include "nsIScrollableFrame.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsISelectionController.h"
#include "nsISelection.h"
#include "nsIPrompt.h"
#include "nsIPromptService.h"
#include "nsIPromptFactory.h"
#include "nsIAddonPolicyService.h"
#include "nsIWritablePropertyBag2.h"
#include "nsIWebNavigation.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserFind.h"  // For window.find()
#include "nsIWindowMediator.h"  // For window.find()
#include "nsComputedDOMStyle.h"
#include "nsDOMCID.h"
#include "nsDOMWindowUtils.h"
#include "nsIWindowWatcher.h"
#include "nsPIWindowWatcher.h"
#include "nsIContentViewer.h"
#include "nsIScriptError.h"
#include "nsIControllers.h"
#include "nsIControllerContext.h"
#include "nsGlobalWindowCommands.h"
#include "nsQueryObject.h"
#include "nsContentUtils.h"
#include "nsCSSProps.h"
#include "nsIDOMFileList.h"
#include "nsIURIFixup.h"
#ifndef DEBUG
#include "nsIAppStartup.h"
#include "nsToolkitCompsCID.h"
#endif
#include "nsCDefaultURIFixup.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "nsIObserverService.h"
#include "nsFocusManager.h"
#include "nsIXULWindow.h"
#include "nsITimedChannel.h"
#include "nsServiceManagerUtils.h"
#ifdef MOZ_XUL
#include "nsIDOMXULControlElement.h"
#include "nsMenuPopupFrame.h"
#endif
#include "mozilla/dom/CustomEvent.h"
#include "nsIJARChannel.h"
#include "nsIScreenManager.h"
#include "nsIEffectiveTLDService.h"

#include "xpcprivate.h"

#ifdef NS_PRINTING
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsService.h"
#include "nsIWebBrowserPrint.h"
#endif

#include "nsWindowRoot.h"
#include "nsNetCID.h"
#include "nsIArray.h"

// XXX An unfortunate dependency exists here (two XUL files).
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULCommandDispatcher.h"

#include "nsBindingManager.h"
#include "nsXBLService.h"

// used for popup blocking, needs to be converted to something
// belonging to the back-end like nsIContentPolicy
#include "nsIPopupWindowManager.h"

#include "nsIDragService.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"
#include "nsFrameLoader.h"
#include "nsISupportsPrimitives.h"
#include "nsXPCOMCID.h"
#include "mozilla/Logging.h"
#include "prenv.h"

#include "mozilla/dom/IDBFactory.h"
#include "mozilla/dom/MessageChannel.h"
#include "mozilla/dom/Promise.h"

#include "mozilla/dom/Gamepad.h"
#include "mozilla/dom/GamepadManager.h"

#include "gfxVR.h"
#include "mozilla/dom/VRDisplay.h"
#include "mozilla/dom/VRDisplayEvent.h"
#include "mozilla/dom/VRDisplayEventBinding.h"
#include "mozilla/dom/VREventObserver.h"

#include "nsRefreshDriver.h"
#include "Layers.h"

#include "mozilla/AddonPathService.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/Location.h"
#include "nsHTMLDocument.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "prrng.h"
#include "nsSandboxFlags.h"
#include "TimeChangeObserver.h"
#include "mozilla/dom/AudioContext.h"
#include "mozilla/dom/BrowserElementDictionariesBinding.h"
#include "mozilla/dom/cache/CacheStorage.h"
#include "mozilla/dom/Console.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/HashChangeEvent.h"
#include "mozilla/dom/IntlUtils.h"
#include "mozilla/dom/MozSelfSupportBinding.h"
#include "mozilla/dom/PopStateEvent.h"
#include "mozilla/dom/PopupBlockedEvent.h"
#include "mozilla/dom/PrimitiveConversions.h"
#include "mozilla/dom/WindowBinding.h"
#include "nsITabChild.h"
#include "mozilla/dom/MediaQueryList.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/NavigatorBinding.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/ServiceWorkerRegistration.h"
#include "mozilla/dom/U2F.h"
#include "mozilla/dom/WebIDLGlobalNameHash.h"
#include "mozilla/dom/Worklet.h"
#ifdef HAVE_SIDEBAR
#include "mozilla/dom/ExternalBinding.h"
#endif

#ifdef MOZ_WEBSPEECH
#include "mozilla/dom/SpeechSynthesis.h"
#endif

// Apple system headers seem to have a check() macro.  <sigh>
#ifdef check
class nsIScriptTimeoutHandler;
#undef check
#endif // check
#include "AccessCheck.h"

#ifdef ANDROID
#include <android/log.h>
#endif

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h> // for getpid()
#endif

static const char kStorageEnabled[] = "dom.storage.enabled";

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;
using mozilla::BasePrincipal;
using mozilla::OriginAttributes;
using mozilla::TimeStamp;
using mozilla::TimeDuration;
using mozilla::dom::cache::CacheStorage;

static LazyLogModule gDOMLeakPRLog("DOMLeak");

static int32_t              gRefCnt                           = 0;
static int32_t              gOpenPopupSpamCount               = 0;
static PopupControlState    gPopupControlState                = openAbused;
static bool                 gMouseDown                        = false;
static bool                 gDragServiceDisabled              = false;
static FILE                *gDumpFile                         = nullptr;
static uint32_t             gSerialCounter                    = 0;

#ifdef DEBUG_jst
int32_t gTimeoutCnt                                    = 0;
#endif

#if defined(DEBUG_bryner) || defined(DEBUG_chb)
#define DEBUG_PAGE_CACHE
#endif

namespace mozilla {
namespace dom {
extern uint64_t
NextWindowID();
} // namespace dom
} // namespace mozilla

template<class T>
nsPIDOMWindow<T>::nsPIDOMWindow(nsPIDOMWindowOuter *aOuterWindow)
: mFrameElement(nullptr), mDocShell(nullptr), mModalStateDepth(0),
  mMutationBits(0), mActivePeerConnections(0), mIsDocumentLoaded(false),
  mIsHandlingResizeEvent(false), mIsInnerWindow(aOuterWindow != nullptr),
  mMayHavePaintEventListener(false), mMayHaveTouchEventListener(false),
  mMayHaveSelectionChangeEventListener(false),
  mMayHaveMouseEnterLeaveEventListener(false),
  mMayHavePointerEnterLeaveEventListener(false),
  mInnerObjectsFreed(false),
  mIsActive(false), mIsBackground(false),
  mMediaSuspend(
    Preferences::GetBool("media.block-autoplay-until-in-foreground", true) &&
    Preferences::GetBool("media.autoplay.enabled", true) ?
      nsISuspendedTypes::SUSPENDED_BLOCK : nsISuspendedTypes::NONE_SUSPENDED),
  mAudioMuted(false), mAudioVolume(1.0), mAudioCaptured(false),
  mDesktopModeViewport(false), mIsRootOuterWindow(false), mInnerWindow(nullptr),
  mOuterWindow(aOuterWindow),
  // Make sure no actual window ends up with mWindowID == 0
  mWindowID(NextWindowID()), mHasNotifiedGlobalCreated(false),
  mMarkedCCGeneration(0), mServiceWorkersTestingEnabled(false),
  mLargeAllocStatus(LargeAllocStatus::NONE),
  mHasTriedToCacheTopInnerWindow(false),
  mNumOfIndexedDBDatabases(0),
  mNumOfOpenWebSockets(0)
{
  if (aOuterWindow) {
    mTimeoutManager =
      MakeUnique<mozilla::dom::TimeoutManager>(*nsGlobalWindowInner::Cast(AsInner()));
  }
}

template<class T>
nsPIDOMWindow<T>::~nsPIDOMWindow() {}

template <class T>
nsIURI*
nsPIDOMWindow<T>::GetDocumentURI() const
{
  return mDoc ? mDoc->GetDocumentURI() : mDocumentURI.get();
}

template <class T>
nsIURI*
nsPIDOMWindow<T>::GetDocBaseURI() const
{
  return mDoc ? mDoc->GetDocBaseURI() : mDocBaseURI.get();
}

template <class T>
void
nsPIDOMWindow<T>::MaybeCreateDoc()
{
  MOZ_ASSERT(!mDoc);
  if (nsIDocShell* docShell = GetDocShell()) {
    // Note that |document| here is the same thing as our mDoc, but we
    // don't have to explicitly set the member variable because the docshell
    // has already called SetNewDocument().
    nsCOMPtr<nsIDocument> document = docShell->GetDocument();
    Unused << document;
  }
}

template<typename T>
mozilla::dom::DocGroup*
nsPIDOMWindow<T>::GetDocGroup() const
{
  nsIDocument* doc = GetExtantDoc();
  if (doc) {
    return doc->GetDocGroup();
  }
  return nullptr;
}

// Include the implementations for the inner and outer windows respectively.
#include "nsGlobalWindowOuter.cpp"
#include "nsGlobalWindowInner.cpp"

template class nsPIDOMWindow<mozIDOMWindowProxy>;
template class nsPIDOMWindow<mozIDOMWindow>;
