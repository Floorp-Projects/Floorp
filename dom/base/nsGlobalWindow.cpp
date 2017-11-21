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
static bool                 gIdleObserversAPIFuzzTimeDisabled = false;

#ifdef DEBUG_jst
int32_t gTimeoutCnt                                    = 0;
#endif

#if defined(DEBUG_bryner) || defined(DEBUG_chb)
#define DEBUG_PAGE_CACHE
#endif

#define DOM_TOUCH_LISTENER_ADDED "dom-touch-listener-added"

#define MEMORY_PRESSURE_OBSERVER_TOPIC "memory-pressure"

// The interval at which we execute idle callbacks
static uint32_t gThrottledIdlePeriodLength;

#define DEFAULT_THROTTLED_IDLE_PERIOD_LENGTH 10000

// CIDs
static NS_DEFINE_CID(kXULControllersCID, NS_XULCONTROLLERS_CID);

#define NETWORK_UPLOAD_EVENT_NAME     NS_LITERAL_STRING("moznetworkupload")
#define NETWORK_DOWNLOAD_EVENT_NAME   NS_LITERAL_STRING("moznetworkdownload")

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

PopupControlState
PushPopupControlState(PopupControlState aState, bool aForce)
{
  MOZ_ASSERT(NS_IsMainThread());

  PopupControlState oldState = gPopupControlState;

  if (aState < gPopupControlState || aForce) {
    gPopupControlState = aState;
  }

  return oldState;
}

void
PopPopupControlState(PopupControlState aState)
{
  MOZ_ASSERT(NS_IsMainThread());

  gPopupControlState = aState;
}

// We need certain special behavior for remote XUL whitelisted domains, but we
// don't want that behavior to take effect in automation, because we whitelist
// all the mochitest domains. So we need to check a pref here.
static bool
TreatAsRemoteXUL(nsIPrincipal* aPrincipal)
{
  MOZ_ASSERT(!nsContentUtils::IsSystemPrincipal(aPrincipal));
  return nsContentUtils::AllowXULXBLForPrincipal(aPrincipal) &&
         !Preferences::GetBool("dom.use_xbl_scopes_for_remote_xul", false);
}

static bool
EnablePrivilege(JSContext* cx, unsigned argc, JS::Value* vp)
{
  Telemetry::Accumulate(Telemetry::ENABLE_PRIVILEGE_EVER_CALLED, true);
  return xpc::EnableUniversalXPConnect(cx);
}

static const JSFunctionSpec EnablePrivilegeSpec[] = {
  JS_FN("enablePrivilege", EnablePrivilege, 1, 0),
  JS_FS_END
};

static bool
InitializeLegacyNetscapeObject(JSContext* aCx, JS::Handle<JSObject*> aGlobal)
{
  JSAutoCompartment ac(aCx, aGlobal);

  // Note: MathJax depends on window.netscape being exposed. See bug 791526.
  JS::Rooted<JSObject*> obj(aCx);
  obj = JS_DefineObject(aCx, aGlobal, "netscape", nullptr);
  NS_ENSURE_TRUE(obj, false);

  obj = JS_DefineObject(aCx, obj, "security", nullptr);
  NS_ENSURE_TRUE(obj, false);

  // We hide enablePrivilege behind a pref because it has been altered in a
  // way that makes it fundamentally insecure to use in production. Mozilla
  // uses this pref during automated testing to support legacy test code that
  // uses enablePrivilege. If you're not doing test automation, you _must_ not
  // flip this pref, or you will be exposing all your users to security
  // vulnerabilities.
  if (!xpc::IsInAutomation()) {
    return true;
  }

  /* Define PrivilegeManager object with the necessary "static" methods. */
  obj = JS_DefineObject(aCx, obj, "PrivilegeManager", nullptr);
  NS_ENSURE_TRUE(obj, false);

  return JS_DefineFunctions(aCx, obj, EnablePrivilegeSpec);
}

static JS::CompartmentCreationOptions&
SelectZoneGroup(nsGlobalWindowInner* aNewInner,
                JS::CompartmentCreationOptions& aOptions)
{
  JS::CompartmentCreationOptions options;

  if (aNewInner->GetOuterWindow()) {
    nsGlobalWindowOuter *top = aNewInner->GetTopInternal();

    // If we have a top-level window, use its zone (and zone group).
    if (top && top->GetGlobalJSObject()) {
      return aOptions.setExistingZone(top->GetGlobalJSObject());
    }
  }

  // If we're in the parent process, don't bother with zone groups.
  if (XRE_IsParentProcess()) {
    return aOptions.setNewZoneInSystemZoneGroup();
  }

  // Otherwise, find a zone group from the TabGroup. Typically we only have to
  // go through one iteration of this loop.
  RefPtr<TabGroup> tabGroup = aNewInner->TabGroup();
  for (nsPIDOMWindowOuter* outer : tabGroup->GetWindows()) {
    nsGlobalWindowOuter* window = nsGlobalWindowOuter::Cast(outer);
    if (JSObject* global = window->GetGlobalJSObject()) {
      return aOptions.setNewZoneInExistingZoneGroup(global);
    }
  }

  return aOptions.setNewZoneInNewZoneGroup();
}

/**
 * Create a new global object that will be used for an inner window.
 * Return the native global and an nsISupports 'holder' that can be used
 * to manage the lifetime of it.
 */
static nsresult
CreateNativeGlobalForInner(JSContext* aCx,
                           nsGlobalWindowInner* aNewInner,
                           nsIURI* aURI,
                           nsIPrincipal* aPrincipal,
                           JS::MutableHandle<JSObject*> aGlobal,
                           bool aIsSecureContext)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aNewInner);
  MOZ_ASSERT(aNewInner->IsInnerWindow());
  MOZ_ASSERT(aPrincipal);

  // DOMWindow with nsEP is not supported, we have to make sure
  // no one creates one accidentally.
  nsCOMPtr<nsIExpandedPrincipal> nsEP = do_QueryInterface(aPrincipal);
  MOZ_RELEASE_ASSERT(!nsEP, "DOMWindow with nsEP is not supported");

  JS::CompartmentOptions options;

  SelectZoneGroup(aNewInner, options.creationOptions());

  // Sometimes add-ons load their own XUL windows, either as separate top-level
  // windows or inside a browser element. In such cases we want to tag the
  // window's compartment with the add-on ID. See bug 1092156.
  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    options.creationOptions().setAddonId(MapURIToAddonID(aURI));
  }

  options.creationOptions().setSecureContext(aIsSecureContext);

  xpc::InitGlobalObjectOptions(options, aPrincipal);

  // Determine if we need the Components object.
  bool needComponents = nsContentUtils::IsSystemPrincipal(aPrincipal) ||
                        TreatAsRemoteXUL(aPrincipal);
  uint32_t flags = needComponents ? 0 : xpc::OMIT_COMPONENTS_OBJECT;
  flags |= xpc::DONT_FIRE_ONNEWGLOBALHOOK;

  if (!WindowBinding::Wrap(aCx, aNewInner, aNewInner, options,
                           nsJSPrincipals::get(aPrincipal), false, aGlobal) ||
      !xpc::InitGlobalObject(aCx, aGlobal, flags)) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(aNewInner->GetWrapperPreserveColor() == aGlobal);

  // Set the location information for the new global, so that tools like
  // about:memory may use that information
  xpc::SetLocationForGlobal(aGlobal, aURI);

  if (!InitializeLegacyNetscapeObject(aCx, aGlobal)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

static
already_AddRefed<EventTarget>
TryGetTabChildGlobalAsEventTarget(nsISupports *aFrom)
{
  nsCOMPtr<nsIFrameLoaderOwner> frameLoaderOwner = do_QueryInterface(aFrom);
  if (!frameLoaderOwner) {
    return nullptr;
  }

  RefPtr<nsFrameLoader> frameLoader = frameLoaderOwner->GetFrameLoader();
  if (!frameLoader) {
    return nullptr;
  }

  nsCOMPtr<EventTarget> target = frameLoader->GetTabChildGlobalAsEventTarget();
  return target.forget();
}

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

// Try to match compartments that are not web content by matching compartments
// with principals that are either the system principal or an expanded principal.
// This may not return true for all non-web-content compartments.
struct BrowserCompartmentMatcher : public js::CompartmentFilter {
  bool match(JSCompartment* aC) const override
    {
      nsCOMPtr<nsIPrincipal> pc = nsJSPrincipals::get(JS_GetCompartmentPrincipals(aC));
      return nsContentUtils::IsSystemOrExpandedPrincipal(pc);
    }
};

class WindowDestroyedEvent final : public Runnable
{
public:
  WindowDestroyedEvent(nsGlobalWindowInner* aWindow, uint64_t aID, const char* aTopic)
    : mozilla::Runnable("WindowDestroyedEvent")
    , mID(aID)
    , mPhase(Phase::Destroying)
    , mTopic(aTopic)
    , mIsInnerWindow(true)
  {
    mWindow = do_GetWeakReference(aWindow);
  }
  WindowDestroyedEvent(nsGlobalWindowOuter* aWindow, uint64_t aID, const char* aTopic)
    : mozilla::Runnable("WindowDestroyedEvent")
    , mID(aID)
    , mPhase(Phase::Destroying)
    , mTopic(aTopic)
    , mIsInnerWindow(false)
  {
    mWindow = do_GetWeakReference(aWindow);
  }

  enum class Phase
  {
    Destroying,
    Nuking
  };

  NS_IMETHOD Run() override
  {
    AUTO_PROFILER_LABEL("WindowDestroyedEvent::Run", OTHER);

    nsCOMPtr<nsIObserverService> observerService =
      services::GetObserverService();
    if (!observerService) {
      return NS_OK;
    }

    nsCOMPtr<nsISupportsPRUint64> wrapper =
      do_CreateInstance(NS_SUPPORTS_PRUINT64_CONTRACTID);
    if (wrapper) {
      wrapper->SetData(mID);
      observerService->NotifyObservers(wrapper, mTopic.get(), nullptr);
    }

    switch (mPhase) {
      case Phase::Destroying:
      {
        bool skipNukeCrossCompartment = false;
#ifndef DEBUG
        nsCOMPtr<nsIAppStartup> appStartup =
          do_GetService(NS_APPSTARTUP_CONTRACTID);

        if (appStartup) {
          appStartup->GetShuttingDown(&skipNukeCrossCompartment);
        }
#endif

        if (!skipNukeCrossCompartment) {
          // The compartment nuking phase might be too expensive, so do that
          // part off of idle dispatch.

          // For the compartment nuking phase, we dispatch either an
          // inner-window-nuked or an outer-window-nuked notification.
          // This will allow tests to wait for compartment nuking to happen.
          if (mTopic.EqualsLiteral("inner-window-destroyed")) {
            mTopic.AssignLiteral("inner-window-nuked");
          } else if (mTopic.EqualsLiteral("outer-window-destroyed")) {
            mTopic.AssignLiteral("outer-window-nuked");
          }
          mPhase = Phase::Nuking;

          nsCOMPtr<nsIRunnable> copy(this);
          NS_IdleDispatchToCurrentThread(copy.forget(), 1000);
        }
      }
      break;

      case Phase::Nuking:
      {
        nsCOMPtr<nsISupports> window = do_QueryReferent(mWindow);
        if (window) {
          nsGlobalWindowInner* currentInner;
          if (mIsInnerWindow) {
            currentInner = nsGlobalWindowInner::FromSupports(window);
          } else {
            nsGlobalWindowOuter* outer = nsGlobalWindowOuter::FromSupports(window);
            currentInner = outer->GetCurrentInnerWindowInternal();
          }
          NS_ENSURE_TRUE(currentInner, NS_OK);

          AutoSafeJSContext cx;
          JS::Rooted<JSObject*> obj(cx, currentInner->FastGetGlobalJSObject());
          if (obj && !js::IsSystemCompartment(js::GetObjectCompartment(obj))) {
            JSCompartment* cpt = js::GetObjectCompartment(obj);
            nsCOMPtr<nsIPrincipal> pc = nsJSPrincipals::get(JS_GetCompartmentPrincipals(cpt));

            if (BasePrincipal::Cast(pc)->AddonPolicy()) {
              // We want to nuke all references to the add-on compartment.
              xpc::NukeAllWrappersForCompartment(cx, cpt,
                                                 mIsInnerWindow ? js::DontNukeWindowReferences
                                                                : js::NukeWindowReferences);
            } else {
              // We only want to nuke wrappers for the chrome->content case
              js::NukeCrossCompartmentWrappers(cx, BrowserCompartmentMatcher(), cpt,
                                               mIsInnerWindow ? js::DontNukeWindowReferences
                                                              : js::NukeWindowReferences,
                                               js::NukeIncomingReferences);
            }
          }
        }
      }
      break;
    }

    return NS_OK;
  }

private:
  uint64_t mID;
  Phase mPhase;
  nsCString mTopic;
  nsWeakPtr mWindow;
  bool mIsInnerWindow;
};

class ChildCommandDispatcher : public Runnable
{
public:
  ChildCommandDispatcher(nsPIWindowRoot* aRoot,
                         nsITabChild* aTabChild,
                         const nsAString& aAction)
    : mozilla::Runnable("ChildCommandDispatcher")
    , mRoot(aRoot)
    , mTabChild(aTabChild)
    , mAction(aAction)
  {
  }

  NS_IMETHOD Run() override
  {
    nsTArray<nsCString> enabledCommands, disabledCommands;
    mRoot->GetEnabledDisabledCommands(enabledCommands, disabledCommands);
    if (enabledCommands.Length() || disabledCommands.Length()) {
      mTabChild->EnableDisableCommands(mAction, enabledCommands, disabledCommands);
    }

    return NS_OK;
  }

private:
  nsCOMPtr<nsPIWindowRoot>             mRoot;
  nsCOMPtr<nsITabChild>                mTabChild;
  nsString                             mAction;
};

class CommandDispatcher : public Runnable
{
public:
  CommandDispatcher(nsIDOMXULCommandDispatcher* aDispatcher,
                    const nsAString& aAction)
    : mozilla::Runnable("CommandDispatcher")
    , mDispatcher(aDispatcher)
    , mAction(aAction)
  {
  }

  NS_IMETHOD Run() override
  {
    return mDispatcher->UpdateCommands(mAction);
  }

  nsCOMPtr<nsIDOMXULCommandDispatcher> mDispatcher;
  nsString                             mAction;
};

static bool IsLink(nsIContent* aContent)
{
  return aContent && (aContent->IsHTMLElement(nsGkAtoms::a) ||
                      aContent->AttrValueIs(kNameSpaceID_XLink, nsGkAtoms::type,
                                            nsGkAtoms::simple, eCaseMatters));
}

static bool ShouldShowFocusRingIfFocusedByMouse(nsIContent* aNode)
{
  if (!aNode) {
    return true;
  }
  return !IsLink(aNode) &&
    !aNode->IsAnyOfHTMLElements(nsGkAtoms::video, nsGkAtoms::audio);
}

static bool
IsInterval(const Optional<int32_t>& aTimeout, int32_t& aResultTimeout)
{
  if (aTimeout.WasPassed()) {
    aResultTimeout = aTimeout.Value();
    return true;
  }

  // If no interval was specified, treat this like a timeout, to avoid setting
  // an interval of 0 milliseconds.
  aResultTimeout = 0;
  return false;
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

static void
EnsurePrefCaches()
{
  static bool sFirstTime = true;
  if (sFirstTime) {
    TimeoutManager::Initialize();
    Preferences::AddBoolVarCache(&gIdleObserversAPIFuzzTimeDisabled,
                                 "dom.idle-observers-api.fuzz_time.disabled",
                                 false);

    Preferences::AddUintVarCache(&gThrottledIdlePeriodLength,
                                 "dom.idle_period.throttled_length",
                                 DEFAULT_THROTTLED_IDLE_PERIOD_LENGTH);
    sFirstTime = false;
  }
}

// Include the implementations for the inner and outer windows respectively.
#include "nsGlobalWindowOuter.cpp"
#include "nsGlobalWindowInner.cpp"

template class nsPIDOMWindow<mozIDOMWindowProxy>;
template class nsPIDOMWindow<mozIDOMWindow>;
