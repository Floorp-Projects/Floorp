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
#include "nsISecureBrowserUI.h"
#include "nsIWebProgressListener.h"
#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/dom/EventTarget.h"
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
#include "nsError.h"
#include "nsIIdleService.h"
#include "nsISizeOfEventTarget.h"
#include "nsDOMJSUtils.h"
#include "nsArrayUtils.h"
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
#include "nsDocShellLoadInfo.h"

// Helper Classes
#include "nsJSUtils.h"
#include "jsapi.h"              // for JSAutoRequest
#include "js/Wrapper.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsReadableUtils.h"
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
#include "mozilla/dom/WorkerCommon.h"
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
#include "nsIDocument.h"
#include "Crypto.h"
#include "nsDOMString.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsThreadUtils.h"
#include "nsILoadContext.h"
#include "nsIPresShell.h"
#include "nsIScrollableFrame.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsISelectionController.h"
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
#include "nsIURIFixup.h"
#include "nsIURIMutator.h"
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

#include "XULDocument.h"
#include "nsIDOMXULCommandDispatcher.h"

#include "nsBindingManager.h"
#include "nsXBLService.h"

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

#include "mozilla/BasePrincipal.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/Location.h"
#include "nsHTMLDocument.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "prrng.h"
#include "nsSandboxFlags.h"
#include "mozilla/dom/AudioContext.h"
#include "mozilla/dom/BrowserElementDictionariesBinding.h"
#include "mozilla/dom/cache/CacheStorage.h"
#include "mozilla/dom/Console.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/HashChangeEvent.h"
#include "mozilla/dom/IntlUtils.h"
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

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;
using mozilla::BasePrincipal;
using mozilla::OriginAttributes;
using mozilla::TimeStamp;

#define FORWARD_TO_INNER(method, args, err_rval)                        \
  PR_BEGIN_MACRO                                                        \
  if (!mInnerWindow) {                                                  \
    NS_WARNING("No inner window available!");                           \
    return err_rval;                                                    \
  }                                                                     \
  return GetCurrentInnerWindowInternal()->method args;                  \
  PR_END_MACRO

#define FORWARD_TO_INNER_VOID(method, args)                             \
  PR_BEGIN_MACRO                                                        \
  if (!mInnerWindow) {                                                  \
    NS_WARNING("No inner window available!");                           \
    return;                                                             \
  }                                                                     \
  GetCurrentInnerWindowInternal()->method args;                         \
  return;                                                               \
  PR_END_MACRO

// Same as FORWARD_TO_INNER, but this will create a fresh inner if an
// inner doesn't already exists.
#define FORWARD_TO_INNER_CREATE(method, args, err_rval)                 \
  PR_BEGIN_MACRO                                                        \
  if (!mInnerWindow) {                                                  \
    if (mIsClosed) {                                                    \
      return err_rval;                                                  \
    }                                                                   \
    nsCOMPtr<nsIDocument> kungFuDeathGrip = GetDoc();                   \
    ::mozilla::Unused << kungFuDeathGrip;                               \
    if (!mInnerWindow) {                                                \
      return err_rval;                                                  \
    }                                                                   \
  }                                                                     \
  return GetCurrentInnerWindowInternal()->method args;                  \
  PR_END_MACRO

#define DEFAULT_HOME_PAGE "www.mozilla.org"
#define PREF_BROWSER_STARTUP_HOMEPAGE "browser.startup.homepage"

static LazyLogModule gDOMLeakPRLogOuter("DOMLeakOuter");

static int32_t              gOpenPopupSpamCount               = 0;

nsGlobalWindowOuter::OuterWindowByIdTable *nsGlobalWindowOuter::sOuterWindowsById = nullptr;

// CIDs
static NS_DEFINE_CID(kXULControllersCID, NS_XULCONTROLLERS_CID);

/* static */
nsPIDOMWindowOuter*
nsPIDOMWindowOuter::GetFromCurrentInner(nsPIDOMWindowInner* aInner)
{
  if (!aInner) {
    return nullptr;
  }

  nsPIDOMWindowOuter* outer = aInner->GetOuterWindow();
  if (!outer || outer->GetCurrentInnerWindow() != aInner) {
    return nullptr;
  }

  return outer;
}

//*****************************************************************************
// nsOuterWindowProxy: Outer Window Proxy
//*****************************************************************************

class nsOuterWindowProxy : public js::Wrapper
{
public:
  constexpr nsOuterWindowProxy() : js::Wrapper(0) { }

  bool finalizeInBackground(const JS::Value& priv) const override {
    return false;
  }

  // Standard internal methods
  bool getOwnPropertyDescriptor(JSContext* cx,
                                JS::Handle<JSObject*> proxy,
                                JS::Handle<jsid> id,
                                JS::MutableHandle<JS::PropertyDescriptor> desc)
                                const override;
  bool defineProperty(JSContext* cx,
                      JS::Handle<JSObject*> proxy,
                      JS::Handle<jsid> id,
                      JS::Handle<JS::PropertyDescriptor> desc,
                      JS::ObjectOpResult &result) const override;
  bool ownPropertyKeys(JSContext *cx,
                       JS::Handle<JSObject*> proxy,
                       JS::AutoIdVector &props) const override;
  bool delete_(JSContext *cx, JS::Handle<JSObject*> proxy,
                       JS::Handle<jsid> id,
                       JS::ObjectOpResult &result) const override;

  bool getPrototypeIfOrdinary(JSContext* cx,
                              JS::Handle<JSObject*> proxy,
                              bool* isOrdinary,
                              JS::MutableHandle<JSObject*> protop) const override;

  JSObject* enumerate(JSContext *cx, JS::Handle<JSObject*> proxy) const override;
  bool preventExtensions(JSContext* cx,
                         JS::Handle<JSObject*> proxy,
                         JS::ObjectOpResult& result) const override;
  bool isExtensible(JSContext *cx, JS::Handle<JSObject*> proxy, bool *extensible)
                    const override;
  bool has(JSContext *cx, JS::Handle<JSObject*> proxy,
           JS::Handle<jsid> id, bool *bp) const override;
  bool get(JSContext *cx, JS::Handle<JSObject*> proxy,
           JS::Handle<JS::Value> receiver,
           JS::Handle<jsid> id,
           JS::MutableHandle<JS::Value> vp) const override;
  bool set(JSContext *cx, JS::Handle<JSObject*> proxy,
           JS::Handle<jsid> id, JS::Handle<JS::Value> v,
           JS::Handle<JS::Value> receiver,
           JS::ObjectOpResult &result) const override;

  // SpiderMonkey extensions
  bool getPropertyDescriptor(JSContext* cx,
                             JS::Handle<JSObject*> proxy,
                             JS::Handle<jsid> id,
                             JS::MutableHandle<JS::PropertyDescriptor> desc)
                             const override;
  bool hasOwn(JSContext *cx, JS::Handle<JSObject*> proxy,
              JS::Handle<jsid> id, bool *bp) const override;
  bool getOwnEnumerablePropertyKeys(JSContext *cx, JS::Handle<JSObject*> proxy,
                                    JS::AutoIdVector &props) const override;
  const char *className(JSContext *cx,
                        JS::Handle<JSObject*> wrapper) const override;

  void finalize(JSFreeOp *fop, JSObject *proxy) const override;
  size_t objectMoved(JSObject* proxy, JSObject* old) const override;

  bool isCallable(JSObject *obj) const override {
    return false;
  }
  bool isConstructor(JSObject *obj) const override {
    return false;
  }

  static const nsOuterWindowProxy singleton;

protected:
  static nsGlobalWindowOuter* GetOuterWindow(JSObject *proxy)
  {
    nsGlobalWindowOuter* outerWindow = nsGlobalWindowOuter::FromSupports(
      static_cast<nsISupports*>(js::GetProxyReservedSlot(proxy, 0).toPrivate()));
    return outerWindow;
  }

  // False return value means we threw an exception.  True return value
  // but false "found" means we didn't have a subframe at that index.
  bool GetSubframeWindow(JSContext *cx, JS::Handle<JSObject*> proxy,
                         JS::Handle<jsid> id,
                         JS::MutableHandle<JS::Value> vp,
                         bool &found) const;

  // Returns a non-null window only if id is an index and we have a
  // window at that index.
  already_AddRefed<nsPIDOMWindowOuter>
  GetSubframeWindow(JSContext *cx,
                    JS::Handle<JSObject*> proxy,
                    JS::Handle<jsid> id) const;

  bool AppendIndexedPropertyNames(JSContext *cx, JSObject *proxy,
                                  JS::AutoIdVector &props) const;
};

// Give OuterWindowProxyClass 2 reserved slots, like the other wrappers, so
// JSObject::swap can swap it with CrossCompartmentWrappers without requiring
// malloc.
const js::Class OuterWindowProxyClass = PROXY_CLASS_DEF(
    "Proxy",
    JSCLASS_HAS_RESERVED_SLOTS(2)); /* additional class flags */

const char *
nsOuterWindowProxy::className(JSContext *cx, JS::Handle<JSObject*> proxy) const
{
    MOZ_ASSERT(js::IsProxy(proxy));

    return "Window";
}

void
nsOuterWindowProxy::finalize(JSFreeOp *fop, JSObject *proxy) const
{
  nsGlobalWindowOuter* outerWindow = GetOuterWindow(proxy);
  if (outerWindow) {
    outerWindow->ClearWrapper(proxy);

    // Ideally we would use OnFinalize here, but it's possible that
    // EnsureScriptEnvironment will later be called on the window, and we don't
    // want to create a new script object in that case. Therefore, we need to
    // write a non-null value that will reliably crash when dereferenced.
    outerWindow->PoisonOuterWindowProxy(proxy);
  }
}

bool
nsOuterWindowProxy::getPropertyDescriptor(JSContext* cx,
                                          JS::Handle<JSObject*> proxy,
                                          JS::Handle<jsid> id,
                                          JS::MutableHandle<JS::PropertyDescriptor> desc) const
{
  // The only thing we can do differently from js::Wrapper is shadow stuff with
  // our indexed properties, so we can just try getOwnPropertyDescriptor and if
  // that gives us nothing call on through to js::Wrapper.
  desc.object().set(nullptr);
  if (!getOwnPropertyDescriptor(cx, proxy, id, desc)) {
    return false;
  }

  if (desc.object()) {
    return true;
  }

  return js::Wrapper::getPropertyDescriptor(cx, proxy, id, desc);
}

bool
nsOuterWindowProxy::getOwnPropertyDescriptor(JSContext* cx,
                                             JS::Handle<JSObject*> proxy,
                                             JS::Handle<jsid> id,
                                             JS::MutableHandle<JS::PropertyDescriptor> desc)
                                             const
{
  bool found;
  if (!GetSubframeWindow(cx, proxy, id, desc.value(), found)) {
    return false;
  }
  if (found) {
    FillPropertyDescriptor(desc, proxy, true);
    return true;
  }
  // else fall through to js::Wrapper

  return js::Wrapper::getOwnPropertyDescriptor(cx, proxy, id, desc);
}

bool
nsOuterWindowProxy::defineProperty(JSContext* cx,
                                   JS::Handle<JSObject*> proxy,
                                   JS::Handle<jsid> id,
                                   JS::Handle<JS::PropertyDescriptor> desc,
                                   JS::ObjectOpResult &result) const
{
  if (IsArrayIndex(GetArrayIndexFromId(cx, id))) {
    // Spec says to Reject whether this is a supported index or not,
    // since we have no indexed setter or indexed creator.  It is up
    // to the caller to decide whether to throw a TypeError.
    return result.failCantDefineWindowElement();
  }

  return js::Wrapper::defineProperty(cx, proxy, id, desc, result);
}

bool
nsOuterWindowProxy::ownPropertyKeys(JSContext *cx,
                                    JS::Handle<JSObject*> proxy,
                                    JS::AutoIdVector &props) const
{
  // Just our indexed stuff followed by our "normal" own property names.
  if (!AppendIndexedPropertyNames(cx, proxy, props)) {
    return false;
  }

  JS::AutoIdVector innerProps(cx);
  if (!js::Wrapper::ownPropertyKeys(cx, proxy, innerProps)) {
    return false;
  }
  return js::AppendUnique(cx, props, innerProps);
}

bool
nsOuterWindowProxy::delete_(JSContext *cx, JS::Handle<JSObject*> proxy,
                            JS::Handle<jsid> id, JS::ObjectOpResult &result) const
{
  if (nsCOMPtr<nsPIDOMWindowOuter> frame = GetSubframeWindow(cx, proxy, id)) {
    // Fail (which means throw if strict, else return false).
    return result.failCantDeleteWindowElement();
  }

  if (IsArrayIndex(GetArrayIndexFromId(cx, id))) {
    // Indexed, but not supported.  Spec says return true.
    return result.succeed();
  }

  return js::Wrapper::delete_(cx, proxy, id, result);
}

bool
nsOuterWindowProxy::getPrototypeIfOrdinary(JSContext* cx,
                                           JS::Handle<JSObject*> proxy,
                                           bool* isOrdinary,
                                           JS::MutableHandle<JSObject*> protop) const
{
  // Window's [[GetPrototypeOf]] trap isn't the ordinary definition:
  //
  //   https://html.spec.whatwg.org/multipage/browsers.html#windowproxy-getprototypeof
  //
  // We nonetheless can implement it with a static [[Prototype]], because
  // wrapper-class handlers (particularly, XOW in FilteringWrapper.cpp) supply
  // all non-ordinary behavior.
  //
  // But from a spec point of view, it's the exact same object in both cases --
  // only the observer's changed.  So this getPrototypeIfOrdinary trap on the
  // non-wrapper object *must* report non-ordinary, even if static [[Prototype]]
  // usually means ordinary.
  *isOrdinary = false;
  return true;
}

bool
nsOuterWindowProxy::preventExtensions(JSContext* cx,
                                      JS::Handle<JSObject*> proxy,
                                      JS::ObjectOpResult& result) const
{
  // If [[Extensible]] could be false, then navigating a window could navigate
  // to a window that's [[Extensible]] after being at one that wasn't: an
  // invariant violation.  So never change a window's extensibility.
  return result.failCantPreventExtensions();
}

bool
nsOuterWindowProxy::isExtensible(JSContext *cx, JS::Handle<JSObject*> proxy,
                                 bool *extensible) const
{
  // See above.
  *extensible = true;
  return true;
}

bool
nsOuterWindowProxy::has(JSContext *cx, JS::Handle<JSObject*> proxy,
                        JS::Handle<jsid> id, bool *bp) const
{
  if (nsCOMPtr<nsPIDOMWindowOuter> frame = GetSubframeWindow(cx, proxy, id)) {
    *bp = true;
    return true;
  }

  return js::Wrapper::has(cx, proxy, id, bp);
}

bool
nsOuterWindowProxy::hasOwn(JSContext *cx, JS::Handle<JSObject*> proxy,
                           JS::Handle<jsid> id, bool *bp) const
{
  if (nsCOMPtr<nsPIDOMWindowOuter> frame = GetSubframeWindow(cx, proxy, id)) {
    *bp = true;
    return true;
  }

  return js::Wrapper::hasOwn(cx, proxy, id, bp);
}

bool
nsOuterWindowProxy::get(JSContext *cx, JS::Handle<JSObject*> proxy,
                        JS::Handle<JS::Value> receiver,
                        JS::Handle<jsid> id,
                        JS::MutableHandle<JS::Value> vp) const
{
  if (id == GetJSIDByIndex(cx, XPCJSContext::IDX_WRAPPED_JSOBJECT) &&
      xpc::AccessCheck::isChrome(js::GetContextCompartment(cx))) {
    vp.set(JS::ObjectValue(*proxy));
    return true;
  }

  bool found;
  if (!GetSubframeWindow(cx, proxy, id, vp, found)) {
    return false;
  }
  if (found) {
    return true;
  }
  // Else fall through to js::Wrapper

  return js::Wrapper::get(cx, proxy, receiver, id, vp);
}

bool
nsOuterWindowProxy::set(JSContext *cx, JS::Handle<JSObject*> proxy,
                        JS::Handle<jsid> id,
                        JS::Handle<JS::Value> v,
                        JS::Handle<JS::Value> receiver,
                        JS::ObjectOpResult &result) const
{
  if (IsArrayIndex(GetArrayIndexFromId(cx, id))) {
    // Reject the set.  It's up to the caller to decide whether to throw a
    // TypeError.  If the caller is strict mode JS code, it'll throw.
    return result.failReadOnly();
  }

  return js::Wrapper::set(cx, proxy, id, v, receiver, result);
}

bool
nsOuterWindowProxy::getOwnEnumerablePropertyKeys(JSContext *cx, JS::Handle<JSObject*> proxy,
                                                 JS::AutoIdVector &props) const
{
  // Like ownPropertyKeys, our indexed stuff followed by our "normal" enumerable
  // own property names.
  //
  // Note that this does not match current spec per
  // https://github.com/whatwg/html/issues/2753 but as that issue says I believe
  // the spec is wrong.
  if (!AppendIndexedPropertyNames(cx, proxy, props)) {
    return false;
  }

  JS::AutoIdVector innerProps(cx);
  if (!js::Wrapper::getOwnEnumerablePropertyKeys(cx, proxy, innerProps)) {
    return false;
  }
  return js::AppendUnique(cx, props, innerProps);
}

JSObject*
nsOuterWindowProxy::enumerate(JSContext *cx, JS::Handle<JSObject*> proxy) const
{
  // BaseProxyHandler::enumerate seems to do what we want here: fall
  // back on the property names returned from js::GetPropertyKeys()
  return js::BaseProxyHandler::enumerate(cx, proxy);
}

bool
nsOuterWindowProxy::GetSubframeWindow(JSContext *cx,
                                      JS::Handle<JSObject*> proxy,
                                      JS::Handle<jsid> id,
                                      JS::MutableHandle<JS::Value> vp,
                                      bool& found) const
{
  nsCOMPtr<nsPIDOMWindowOuter> frame = GetSubframeWindow(cx, proxy, id);
  if (!frame) {
    found = false;
    return true;
  }

  found = true;
  // Just return the window's global
  nsGlobalWindowOuter* global = nsGlobalWindowOuter::Cast(frame);
  frame->EnsureInnerWindow();
  JSObject* obj = global->FastGetGlobalJSObject();
  // This null check fixes a hard-to-reproduce crash that occurs when we
  // get here when we're mid-call to nsDocShell::Destroy. See bug 640904
  // comment 105.
  if (MOZ_UNLIKELY(!obj)) {
    return xpc::Throw(cx, NS_ERROR_FAILURE);
  }
  JS::ExposeObjectToActiveJS(obj);
  vp.setObject(*obj);
  return JS_WrapValue(cx, vp);
}

already_AddRefed<nsPIDOMWindowOuter>
nsOuterWindowProxy::GetSubframeWindow(JSContext *cx,
                                      JS::Handle<JSObject*> proxy,
                                      JS::Handle<jsid> id) const
{
  uint32_t index = GetArrayIndexFromId(cx, id);
  if (!IsArrayIndex(index)) {
    return nullptr;
  }

  nsGlobalWindowOuter* win = GetOuterWindow(proxy);
  return win->IndexedGetterOuter(index);
}

bool
nsOuterWindowProxy::AppendIndexedPropertyNames(JSContext *cx, JSObject *proxy,
                                               JS::AutoIdVector &props) const
{
  uint32_t length = GetOuterWindow(proxy)->Length();
  MOZ_ASSERT(int32_t(length) >= 0);
  if (!props.reserve(props.length() + length)) {
    return false;
  }
  for (int32_t i = 0; i < int32_t(length); ++i) {
    if (!props.append(INT_TO_JSID(i))) {
      return false;
    }
  }

  return true;
}

size_t
nsOuterWindowProxy::objectMoved(JSObject *obj, JSObject *old) const
{
  nsGlobalWindowOuter* outerWindow = GetOuterWindow(obj);
  if (outerWindow) {
    outerWindow->UpdateWrapper(obj, old);
  }
  return 0;
}

const nsOuterWindowProxy
nsOuterWindowProxy::singleton;

class nsChromeOuterWindowProxy : public nsOuterWindowProxy
{
public:
  constexpr nsChromeOuterWindowProxy() : nsOuterWindowProxy() { }

  const char *className(JSContext *cx, JS::Handle<JSObject*> wrapper) const override;

  static const nsChromeOuterWindowProxy singleton;
};

const char *
nsChromeOuterWindowProxy::className(JSContext *cx,
                                    JS::Handle<JSObject*> proxy) const
{
    MOZ_ASSERT(js::IsProxy(proxy));

    return "ChromeWindow";
}

const nsChromeOuterWindowProxy
nsChromeOuterWindowProxy::singleton;

static JSObject*
NewOuterWindowProxy(JSContext *cx, JS::Handle<JSObject*> global, bool isChrome)
{
  MOZ_ASSERT(JS_IsGlobalObject(global));

  JSAutoRealm ar(cx, global);

  js::WrapperOptions options;
  options.setClass(&OuterWindowProxyClass);
  options.setSingleton(true);
  JSObject *obj = js::Wrapper::New(cx, global,
                                   isChrome ? &nsChromeOuterWindowProxy::singleton
                                            : &nsOuterWindowProxy::singleton,
                                   options);
  MOZ_ASSERT_IF(obj, js::IsWindowProxy(obj));
  return obj;
}

//*****************************************************************************
//***    nsGlobalWindowOuter: Object Management
//*****************************************************************************

nsGlobalWindowOuter::nsGlobalWindowOuter()
  : nsPIDOMWindowOuter(),
    mIdleFuzzFactor(0),
    mIdleCallbackIndex(-1),
    mCurrentlyIdle(false),
    mAddActiveEventFuzzTime(true),
    mFullScreen(false),
    mFullscreenMode(false),
    mIsClosed(false),
    mInClose(false),
    mHavePendingClose(false),
    mHadOriginalOpener(false),
    mIsPopupSpam(false),
    mBlockScriptedClosingFlag(false),
    mWasOffline(false),
    mCreatingInnerWindow(false),
    mIsChrome(false),
    mAllowScriptsToClose(false),
    mTopLevelOuterContentWindow(false),
    mSerial(0),
#ifdef DEBUG
    mSetOpenerWindowCalled(false),
#endif
    mCleanedUp(false),
#ifdef DEBUG
    mIsValidatingTabGroup(false),
#endif
    mCanSkipCCGeneration(0),
    mAutoActivateVRDisplayID(0)
{
  AssertIsOnMainThread();

  nsLayoutStatics::AddRef();

  // Initialize the PRCList (this).
  PR_INIT_CLIST(this);

  // |this| is an outer window. Outer windows start out frozen and
  // remain frozen until they get an inner window.
  MOZ_ASSERT(IsFrozen());

  if (XRE_IsContentProcess()) {
    nsCOMPtr<nsIDocShell> docShell = GetDocShell();
    if (docShell) {
      mTabChild = docShell->GetTabChild();
    }
  }

  // We could have failed the first time through trying
  // to create the entropy collector, so we should
  // try to get one until we succeed.

  mSerial = nsContentUtils::InnerOrOuterWindowCreated();

#ifdef DEBUG
  if (!PR_GetEnv("MOZ_QUIET")) {
    printf_stderr("++DOMWINDOW == %d (%p) [pid = %d] [serial = %d] [outer = %p]\n",
                  nsContentUtils::GetCurrentInnerOrOuterWindowCount(),
                  static_cast<void*>(ToCanonicalSupports(this)),
                  getpid(),
                  mSerial,
                  nullptr);
  }
#endif

  MOZ_LOG(gDOMLeakPRLogOuter, LogLevel::Debug,
          ("DOMWINDOW %p created outer=nullptr", this));

  // Add ourselves to the outer windows list.
  MOZ_ASSERT(sOuterWindowsById, "Outer Windows hash table must be created!");

  // |this| is an outer window, add to the outer windows list.
  MOZ_ASSERT(!sOuterWindowsById->Get(mWindowID),
             "This window shouldn't be in the hash table yet!");
  // We seem to see crashes in release builds because of null |sOuterWindowsById|.
  if (sOuterWindowsById) {
    sOuterWindowsById->Put(mWindowID, this);
  }
}

#ifdef DEBUG

/* static */
void
nsGlobalWindowOuter::AssertIsOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
}

#endif // DEBUG

/* static */
void
nsGlobalWindowOuter::Init()
{
  AssertIsOnMainThread();

  NS_ASSERTION(gDOMLeakPRLogOuter, "gDOMLeakPRLogOuter should have been initialized!");

  sOuterWindowsById = new OuterWindowByIdTable();
}

nsGlobalWindowOuter::~nsGlobalWindowOuter()
{
  AssertIsOnMainThread();

  if (sOuterWindowsById) {
    MOZ_ASSERT(sOuterWindowsById->Get(mWindowID),
                "This window should be in the hash table");
    sOuterWindowsById->Remove(mWindowID);
  }

  nsContentUtils::InnerOrOuterWindowDestroyed();

#ifdef DEBUG
  if (!PR_GetEnv("MOZ_QUIET")) {
    nsAutoCString url;
    if (mLastOpenedURI) {
      url = mLastOpenedURI->GetSpecOrDefault();

      // Data URLs can be very long, so truncate to avoid flooding the log.
      const uint32_t maxURLLength = 1000;
      if (url.Length() > maxURLLength) {
        url.Truncate(maxURLLength);
      }
    }

    printf_stderr("--DOMWINDOW == %d (%p) [pid = %d] [serial = %d] [outer = %p] [url = %s]\n",
                  nsContentUtils::GetCurrentInnerOrOuterWindowCount(),
                  static_cast<void*>(ToCanonicalSupports(this)),
                  getpid(),
                  mSerial,
                  nullptr,
                  url.get());
  }
#endif

  MOZ_LOG(gDOMLeakPRLogOuter, LogLevel::Debug, ("DOMWINDOW %p destroyed", this));

  JSObject *proxy = GetWrapperMaybeDead();
  if (proxy) {
    js::SetProxyReservedSlot(proxy, 0, js::PrivateValue(nullptr));
  }

  // An outer window is destroyed with inner windows still possibly
  // alive, iterate through the inner windows and null out their
  // back pointer to this outer, and pull them out of the list of
  // inner windows.
  //
  // Our linked list of inner windows both contains (an nsGlobalWindowOuter),
  // and our inner windows (nsGlobalWindowInners). This means that we need to
  // use PRCList*. We can then compare that PRCList* to `this` to see if its an
  // inner or outer window.
  PRCList* w;
  while ((w = PR_LIST_HEAD(this)) != this) {
    PR_REMOVE_AND_INIT_LINK(w);
  }

  DropOuterWindowDocs();

  if (mTabGroup) {
    mTabGroup->Leave(this);
  }

  // Outer windows are always supposed to call CleanUp before letting themselves
  // be destroyed.
  MOZ_ASSERT(mCleanedUp);

  nsCOMPtr<nsIDeviceSensors> ac = do_GetService(NS_DEVICE_SENSORS_CONTRACTID);
  if (ac)
    ac->RemoveWindowAsListener(this);

  nsLayoutStatics::Release();
}

// static
void
nsGlobalWindowOuter::ShutDown()
{
  AssertIsOnMainThread();

  delete sOuterWindowsById;
  sOuterWindowsById = nullptr;
}

void
nsGlobalWindowOuter::MaybeForgiveSpamCount()
{
  if (IsPopupSpamWindow()) {
    SetIsPopupSpamWindow(false);
  }
}

void
nsGlobalWindowOuter::SetIsPopupSpamWindow(bool aIsPopupSpam)
{
  mIsPopupSpam = aIsPopupSpam;
  if (aIsPopupSpam) {
    ++gOpenPopupSpamCount;
  } else {
    --gOpenPopupSpamCount;
    NS_ASSERTION(gOpenPopupSpamCount >= 0,
                 "Unbalanced decrement of gOpenPopupSpamCount");
  }
}

void
nsGlobalWindowOuter::DropOuterWindowDocs()
{
  MOZ_ASSERT_IF(mDoc, !mDoc->EventHandlingSuppressed());
  mDoc = nullptr;
  mSuspendedDoc = nullptr;
}

void
nsGlobalWindowOuter::CleanUp()
{
  // Guarantee idempotence.
  if (mCleanedUp)
    return;
  mCleanedUp = true;

  StartDying();

  mFrames = nullptr;
  mWindowUtils = nullptr;

  ClearControllers();

  mOpener = nullptr;             // Forces Release
  if (mContext) {
    mContext = nullptr;            // Forces Release
  }
  mChromeEventHandler = nullptr; // Forces Release
  mParentTarget = nullptr;

  mArguments = nullptr;

  if (mIdleTimer) {
    mIdleTimer->Cancel();
    mIdleTimer = nullptr;
  }
}

void
nsGlobalWindowOuter::ClearControllers()
{
  if (mControllers) {
    uint32_t count;
    mControllers->GetControllerCount(&count);

    while (count--) {
      nsCOMPtr<nsIController> controller;
      mControllers->GetControllerAt(count, getter_AddRefs(controller));

      nsCOMPtr<nsIControllerContext> context = do_QueryInterface(controller);
      if (context)
        context->SetCommandContext(nullptr);
    }

    mControllers = nullptr;
  }
}

//*****************************************************************************
// nsGlobalWindowOuter::nsISupports
//*****************************************************************************

// QueryInterface implementation for nsGlobalWindowOuter
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsGlobalWindowOuter)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, EventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindow)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::EventTarget)
  NS_INTERFACE_MAP_ENTRY(nsPIDOMWindowOuter)
  NS_INTERFACE_MAP_ENTRY(mozIDOMWindowProxy)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIDOMChromeWindow, IsChromeWindow())
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
NS_INTERFACE_MAP_END


NS_IMPL_CYCLE_COLLECTING_ADDREF(nsGlobalWindowOuter)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsGlobalWindowOuter)

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsGlobalWindowOuter)
  if (tmp->IsBlackForCC(false)) {
    if (nsCCUncollectableMarker::InGeneration(tmp->mCanSkipCCGeneration)) {
      return true;
    }
    tmp->mCanSkipCCGeneration = nsCCUncollectableMarker::sGeneration;
    if (EventListenerManager* elm = tmp->GetExistingListenerManager()) {
      elm->MarkForCC();
    }
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsGlobalWindowOuter)
  return tmp->IsBlackForCC(true);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsGlobalWindowOuter)
  return tmp->IsBlackForCC(false);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGlobalWindowOuter)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsGlobalWindowOuter)
  if (MOZ_UNLIKELY(cb.WantDebugInfo())) {
    char name[512];
    nsAutoCString uri;
    if (tmp->mDoc && tmp->mDoc->GetDocumentURI()) {
      uri = tmp->mDoc->GetDocumentURI()->GetSpecOrDefault();
    }
    SprintfLiteral(name, "nsGlobalWindowOuter # %" PRIu64 " outer %s", tmp->mWindowID,
                   uri.get());
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name);
  } else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsGlobalWindowOuter, tmp->mRefCnt.get())
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContext)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mControllers)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mArguments)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocalStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSuspendedDoc)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocumentPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTabChild)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDoc)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIdleService)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIdleObservers)

  // Traverse stuff from nsPIDOMWindow
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChromeEventHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParentTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFrameElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOpenerForInitialContentBrowser)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocShell)

  tmp->TraverseHostObjectURIs(cb);

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChromeFields.mBrowserDOMWindow)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsGlobalWindowOuter)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContext)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mControllers)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mArguments)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocalStorage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSuspendedDoc)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocumentPrincipal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTabChild)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDoc)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIdleService)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIdleObservers)

  // Unlink stuff from nsPIDOMWindow
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChromeEventHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParentTarget)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFrameElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOpenerForInitialContentBrowser)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocShell)

  tmp->UnlinkHostObjectURIs();

  if (tmp->IsChromeWindow()) {
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mChromeFields.mBrowserDOMWindow)
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsGlobalWindowOuter)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

bool
nsGlobalWindowOuter::IsBlackForCC(bool aTracingNeeded)
{
  if (!nsCCUncollectableMarker::sGeneration) {
    return false;
  }

  return (nsCCUncollectableMarker::InGeneration(GetMarkedCCGeneration()) ||
          HasKnownLiveWrapper()) &&
         (!aTracingNeeded ||
          HasNothingToTrace(ToSupports(this)));
}

//*****************************************************************************
// nsGlobalWindowOuter::nsIScriptGlobalObject
//*****************************************************************************

nsresult
nsGlobalWindowOuter::EnsureScriptEnvironment()
{
  if (GetWrapperPreserveColor()) {
    return NS_OK;
  }

  NS_ASSERTION(!GetCurrentInnerWindowInternal(),
               "No cached wrapper, but we have an inner window?");

  // If this window is a [i]frame, don't bother GC'ing when the frame's context
  // is destroyed since a GC will happen when the frameset or host document is
  // destroyed anyway.
  nsCOMPtr<nsIScriptContext> context = new nsJSContext(!IsFrame(), this);

  NS_ASSERTION(!mContext, "Will overwrite mContext!");

  // should probably assert the context is clean???
  context->WillInitializeContext();

  nsresult rv = context->InitContext();
  NS_ENSURE_SUCCESS(rv, rv);

  mContext = context;
  return NS_OK;
}

nsIScriptContext *
nsGlobalWindowOuter::GetScriptContext()
{
  return mContext;
}

JSObject *
nsGlobalWindowOuter::GetGlobalJSObject()
{
  return FastGetGlobalJSObject();
}

void
nsGlobalWindowOuter::TraceGlobalJSObject(JSTracer* aTrc)
{
  TraceWrapper(aTrc, "active window global");
}

bool
nsGlobalWindowOuter::WouldReuseInnerWindow(nsIDocument* aNewDocument)
{
  // We reuse the inner window when:
  // a. We are currently at our original document.
  // b. At least one of the following conditions are true:
  // -- The new document is the same as the old document. This means that we're
  //    getting called from document.open().
  // -- The new document has the same origin as what we have loaded right now.

  if (!mDoc || !aNewDocument) {
    return false;
  }

  if (!mDoc->IsInitialDocument()) {
    return false;
  }

#ifdef DEBUG
{
  nsCOMPtr<nsIURI> uri;
  mDoc->GetDocumentURI()->CloneIgnoringRef(getter_AddRefs(uri));
  NS_ASSERTION(NS_IsAboutBlank(uri), "How'd this happen?");
}
#endif

  // Great, we're the original document, check for one of the other
  // conditions.

  if (mDoc == aNewDocument) {
    return true;
  }

  bool equal;
  if (NS_SUCCEEDED(mDoc->NodePrincipal()->Equals(aNewDocument->NodePrincipal(),
                                                 &equal)) &&
      equal) {
    // The origin is the same.
    return true;
  }

  return false;
}

void
nsGlobalWindowOuter::SetInitialPrincipalToSubject()
{
  // First, grab the subject principal.
  nsCOMPtr<nsIPrincipal> newWindowPrincipal = nsContentUtils::SubjectPrincipalOrSystemIfNativeCaller();

  // We should never create windows with an expanded principal.
  // If we have a system principal, make sure we're not using it for a content
  // docshell.
  // NOTE: Please keep this logic in sync with nsWebShellWindow::Initialize().
  if (nsContentUtils::IsExpandedPrincipal(newWindowPrincipal) ||
      (nsContentUtils::IsSystemPrincipal(newWindowPrincipal) &&
       GetDocShell()->ItemType() != nsIDocShellTreeItem::typeChrome)) {
    newWindowPrincipal = nullptr;
  }

  // If there's an existing document, bail if it either:
  if (mDoc) {
    // (a) is not an initial about:blank document, or
    if (!mDoc->IsInitialDocument())
      return;
    // (b) already has the correct principal.
    if (mDoc->NodePrincipal() == newWindowPrincipal)
      return;

#ifdef DEBUG
    // If we have a document loaded at this point, it had better be about:blank.
    // Otherwise, something is really weird. An about:blank page has a
    // NullPrincipal.
    bool isNullPrincipal;
    MOZ_ASSERT(NS_SUCCEEDED(mDoc->NodePrincipal()->GetIsNullPrincipal(&isNullPrincipal)) &&
               isNullPrincipal);
#endif
  }

  GetDocShell()->CreateAboutBlankContentViewer(newWindowPrincipal);

  if (mDoc) {
    mDoc->SetIsInitialDocument(true);
  }

  nsCOMPtr<nsIPresShell> shell = GetDocShell()->GetPresShell();

  if (shell && !shell->DidInitialize()) {
    // Ensure that if someone plays with this document they will get
    // layout happening.
    shell->Initialize();
  }
}

PopupControlState
nsGlobalWindowOuter::PushPopupControlState(PopupControlState aState,
                                           bool aForce) const
{
  return nsContentUtils::PushPopupControlState(aState, aForce);
}

void
nsGlobalWindowOuter::PopPopupControlState(PopupControlState aState) const
{
  nsContentUtils::PopPopupControlState(aState);
}

PopupControlState
nsGlobalWindowOuter::GetPopupControlState() const
{
  return nsContentUtils::GetPopupControlState();
}

#define WINDOWSTATEHOLDER_IID \
{0x0b917c3e, 0xbd50, 0x4683, {0xaf, 0xc9, 0xc7, 0x81, 0x07, 0xae, 0x33, 0x26}}

class WindowStateHolder final : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(WINDOWSTATEHOLDER_IID)
  NS_DECL_ISUPPORTS

  explicit WindowStateHolder(nsGlobalWindowInner *aWindow);

  nsGlobalWindowInner* GetInnerWindow() { return mInnerWindow; }

  void DidRestoreWindow()
  {
    mInnerWindow = nullptr;
    mInnerWindowReflector = nullptr;
  }

protected:
  ~WindowStateHolder();

  nsGlobalWindowInner *mInnerWindow;
  // We hold onto this to make sure the inner window doesn't go away. The outer
  // window ends up recalculating it anyway.
  JS::PersistentRooted<JSObject*> mInnerWindowReflector;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WindowStateHolder, WINDOWSTATEHOLDER_IID)

WindowStateHolder::WindowStateHolder(nsGlobalWindowInner* aWindow)
  : mInnerWindow(aWindow),
    mInnerWindowReflector(RootingCx(), aWindow->GetWrapper())
{
  MOZ_ASSERT(aWindow, "null window");

  aWindow->Suspend();

  // When a global goes into the bfcache, we disable script.
  xpc::Scriptability::Get(mInnerWindowReflector).SetDocShellAllowsScript(false);
}

WindowStateHolder::~WindowStateHolder()
{
  if (mInnerWindow) {
    // This window was left in the bfcache and is now going away. We need to
    // free it up.
    // Note that FreeInnerObjects may already have been called on the
    // inner window if its outer has already had SetDocShell(null)
    // called.
    mInnerWindow->FreeInnerObjects();
  }
}

NS_IMPL_ISUPPORTS(WindowStateHolder, WindowStateHolder)

bool
nsGlobalWindowOuter::ComputeIsSecureContext(nsIDocument* aDocument, SecureContextFlags aFlags)
{
  nsCOMPtr<nsIPrincipal> principal = aDocument->NodePrincipal();
  if (nsContentUtils::IsSystemPrincipal(principal)) {
    return true;
  }

  // Implement https://w3c.github.io/webappsec-secure-contexts/#settings-object
  // With some modifications to allow for aFlags.

  bool hadNonSecureContextCreator = false;

  nsPIDOMWindowOuter* parentOuterWin = GetScriptableParent();
  MOZ_ASSERT(parentOuterWin, "How can we get here? No docShell somehow?");
  if (nsGlobalWindowOuter::Cast(parentOuterWin) != this) {
    // There may be a small chance that parentOuterWin has navigated in
    // the time that it took us to start loading this sub-document.  If that
    // were the case then parentOuterWin->GetCurrentInnerWindow() wouldn't
    // return the window for the document that is embedding us.  For this
    // reason we only use the GetScriptableParent call above to check that we
    // have a same-type parent, but actually get the inner window via the
    // document that we know is embedding us.
    nsIDocument* creatorDoc = aDocument->GetParentDocument();
    if (!creatorDoc) {
      return false; // we must be tearing down
    }
    nsGlobalWindowInner* parentWin =
      nsGlobalWindowInner::Cast(creatorDoc->GetInnerWindow());
    if (!parentWin) {
      return false; // we must be tearing down
    }
    MOZ_ASSERT(parentWin ==
               nsGlobalWindowInner::Cast(parentOuterWin->GetCurrentInnerWindow()),
               "Creator window mismatch while setting Secure Context state");
    hadNonSecureContextCreator = !parentWin->IsSecureContext();
  }

  if (hadNonSecureContextCreator) {
    return false;
  }

  if (nsContentUtils::HttpsStateIsModern(aDocument)) {
    return true;
  }

  if (principal->GetIsNullPrincipal()) {
    nsCOMPtr<nsIURI> uri = aDocument->GetOriginalURI();
    // IsOriginPotentiallyTrustworthy doesn't care about origin attributes so
    // it doesn't actually matter what we use here, but reusing the document
    // principal's attributes is convenient.
    const OriginAttributes& attrs = principal->OriginAttributesRef();
    // CreateCodebasePrincipal correctly gets a useful principal for blob: and
    // other URI_INHERITS_SECURITY_CONTEXT URIs.
    principal = BasePrincipal::CreateCodebasePrincipal(uri, attrs);
    if (NS_WARN_IF(!principal)) {
      return false;
    }
  }

  nsCOMPtr<nsIContentSecurityManager> csm =
    do_GetService(NS_CONTENTSECURITYMANAGER_CONTRACTID);
  NS_WARNING_ASSERTION(csm, "csm is null");
  if (csm) {
    bool isTrustworthyOrigin = false;
    csm->IsOriginPotentiallyTrustworthy(principal, &isTrustworthyOrigin);
    if (isTrustworthyOrigin) {
      return true;
    }
  }

  return false;
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
  JSAutoRealm ar(aCx, aGlobal);

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

static JS::RealmCreationOptions&
SelectZone(nsGlobalWindowInner* aNewInner,
           JS::RealmCreationOptions& aOptions)
{
  if (aNewInner->GetOuterWindow()) {
    nsGlobalWindowOuter *top = aNewInner->GetTopInternal();

    // If we have a top-level window, use its zone.
    if (top && top->GetGlobalJSObject()) {
      return aOptions.setNewCompartmentInExistingZone(top->GetGlobalJSObject());
    }
  }

  return aOptions.setNewCompartmentAndZone();
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
  MOZ_ASSERT(aPrincipal);

  // DOMWindow with nsEP is not supported, we have to make sure
  // no one creates one accidentally.
  nsCOMPtr<nsIExpandedPrincipal> nsEP = do_QueryInterface(aPrincipal);
  MOZ_RELEASE_ASSERT(!nsEP, "DOMWindow with nsEP is not supported");

  JS::RealmOptions options;

  SelectZone(aNewInner, options.creationOptions());

  options.creationOptions().setSecureContext(aIsSecureContext);

  xpc::InitGlobalObjectOptions(options, aPrincipal);

  // Determine if we need the Components object.
  bool needComponents = nsContentUtils::IsSystemPrincipal(aPrincipal) ||
                        TreatAsRemoteXUL(aPrincipal);
  uint32_t flags = needComponents ? 0 : xpc::OMIT_COMPONENTS_OBJECT;
  flags |= xpc::DONT_FIRE_ONNEWGLOBALHOOK;

  if (!Window_Binding::Wrap(aCx, aNewInner, aNewInner, options,
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

nsresult
nsGlobalWindowOuter::SetNewDocument(nsIDocument* aDocument,
                                    nsISupports* aState,
                                    bool aForceReuseInnerWindow)
{
  MOZ_ASSERT(mDocumentPrincipal == nullptr,
                  "mDocumentPrincipal prematurely set!");
  MOZ_ASSERT(aDocument);

  // Bail out early if we're in process of closing down the window.
  NS_ENSURE_STATE(!mCleanedUp);

  NS_ASSERTION(!GetCurrentInnerWindow() ||
               GetCurrentInnerWindow()->GetExtantDoc() == mDoc,
               "Uh, mDoc doesn't match the current inner window "
               "document!");

  bool wouldReuseInnerWindow = WouldReuseInnerWindow(aDocument);
  if (aForceReuseInnerWindow &&
      !wouldReuseInnerWindow &&
      mDoc &&
      mDoc->NodePrincipal() != aDocument->NodePrincipal()) {
    NS_ERROR("Attempted forced inner window reuse while changing principal");
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIDocument> oldDoc = mDoc;

  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext *cx = jsapi.cx();

  // Check if we're anywhere near the stack limit before we reach the
  // transplanting code, since it has no good way to handle errors. This uses
  // the untrusted script limit, which is not strictly necessary since no
  // actual script should run.
  if (!js::CheckRecursionLimitConservativeDontReport(cx)) {
    NS_WARNING("Overrecursion in SetNewDocument");
    return NS_ERROR_FAILURE;
  }

  if (!mDoc) {
    // First document load.

    // Get our private root. If it is equal to us, then we need to
    // attach our global key bindings that handles browser scrolling
    // and other browser commands.
    nsPIDOMWindowOuter* privateRoot = GetPrivateRoot();

    if (privateRoot == this) {
      nsXBLService::AttachGlobalKeyHandler(mChromeEventHandler);
    }
  }

  /* No mDocShell means we're already been partially closed down.  When that
     happens, setting status isn't a big requirement, so don't. (Doesn't happen
     under normal circumstances, but bug 49615 describes a case.) */

  nsContentUtils::AddScriptRunner(NewRunnableMethod(
    "nsGlobalWindowOuter::ClearStatus", this, &nsGlobalWindowOuter::ClearStatus));

  // Sometimes, WouldReuseInnerWindow() returns true even if there's no inner
  // window (see bug 776497). Be safe.
  bool reUseInnerWindow = (aForceReuseInnerWindow || wouldReuseInnerWindow) &&
                          GetCurrentInnerWindowInternal();

  nsresult rv = NS_OK;

  // We set mDoc even though this is an outer window to avoid
  // having to *always* reach into the inner window to find the
  // document.
  mDoc = aDocument;

  // Take this opportunity to clear mSuspendedDoc. Our old inner window is now
  // responsible for unsuspending it.
  mSuspendedDoc = nullptr;

#ifdef DEBUG
  mLastOpenedURI = aDocument->GetDocumentURI();
#endif

  mContext->WillInitializeContext();

  RefPtr<nsGlobalWindowInner> currentInner = GetCurrentInnerWindowInternal();

  if (currentInner && currentInner->mNavigator) {
    currentInner->mNavigator->OnNavigation();
  }

  RefPtr<nsGlobalWindowInner> newInnerWindow;
  bool createdInnerWindow = false;

  bool thisChrome = IsChromeWindow();

  nsCOMPtr<WindowStateHolder> wsh = do_QueryInterface(aState);
  NS_ASSERTION(!aState || wsh, "What kind of weird state are you giving me here?");

  bool handleDocumentOpen = false;
  bool doomCurrentInner = false;

  JS::Rooted<JSObject*> newInnerGlobal(cx);
  if (reUseInnerWindow) {
    // We're reusing the current inner window.
    NS_ASSERTION(!currentInner->IsFrozen(),
                 "We should never be reusing a shared inner window");
    newInnerWindow = currentInner;
    newInnerGlobal = currentInner->GetWrapperPreserveColor();

    if (aDocument != oldDoc) {
      JS::ExposeObjectToActiveJS(newInnerGlobal);
    }

    // We're reusing the inner window, but this still counts as a navigation,
    // so all expandos and such defined on the outer window should go away. Force
    // all Xray wrappers to be recomputed.
    JS::Rooted<JSObject*> rootedObject(cx, GetWrapper());
    if (!JS_RefreshCrossCompartmentWrappers(cx, rootedObject)) {
      return NS_ERROR_FAILURE;
    }

    // Inner windows are only reused for same-origin principals, but the principals
    // don't necessarily match exactly. Update the principal on the realm to match
    // the new document.
    // NB: We don't just call currentInner->RefreshRealmPrincipals() here
    // because we haven't yet set its mDoc to aDocument.
    JS::Realm* realm = js::GetNonCCWObjectRealm(newInnerGlobal);
#ifdef DEBUG
    bool sameOrigin = false;
    nsIPrincipal *existing =
      nsJSPrincipals::get(JS::GetRealmPrincipals(realm));
    aDocument->NodePrincipal()->Equals(existing, &sameOrigin);
    MOZ_ASSERT(sameOrigin);
    MOZ_ASSERT_IF(aDocument == oldDoc,
                  xpc::GetRealmPrincipal(realm) == aDocument->NodePrincipal());
#endif
    if (aDocument != oldDoc) {
      JS::SetRealmPrincipals(realm,
                             nsJSPrincipals::get(aDocument->NodePrincipal()));
      // Make sure we clear out the old content XBL scope, so the new one will
      // get created with a principal that subsumes our new principal.
      xpc::ClearContentXBLScope(newInnerGlobal);
    }
  } else {
    if (aState) {
      newInnerWindow = wsh->GetInnerWindow();
      newInnerGlobal = newInnerWindow->GetWrapperPreserveColor();
    } else {
      newInnerWindow = nsGlobalWindowInner::Create(this, thisChrome);

      // The outer window is automatically treated as frozen when we
      // null out the inner window. As a result, initializing classes
      // on the new inner won't end up reaching into the old inner
      // window for classes etc.
      //
      // [This happens with Object.prototype when XPConnect creates
      // a temporary global while initializing classes; the reason
      // being that xpconnect creates the temp global w/o a parent
      // and proto, which makes the JS engine look up classes in
      // cx->globalObject, i.e. this outer window].

      mInnerWindow = nullptr;

      mCreatingInnerWindow = true;
      // Every script context we are initialized with must create a
      // new global.
      rv = CreateNativeGlobalForInner(cx, newInnerWindow,
                                      aDocument->GetDocumentURI(),
                                      aDocument->NodePrincipal(),
                                      &newInnerGlobal,
                                      ComputeIsSecureContext(aDocument));
      NS_ASSERTION(NS_SUCCEEDED(rv) && newInnerGlobal &&
                   newInnerWindow->GetWrapperPreserveColor() == newInnerGlobal,
                   "Failed to get script global");

      mCreatingInnerWindow = false;
      createdInnerWindow = true;

      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (currentInner && currentInner->GetWrapperPreserveColor()) {
      if (oldDoc == aDocument) {
        handleDocumentOpen = true;
      }

      // Don't free objects on our current inner window if it's going to be
      // held in the bfcache.
      if (!currentInner->IsFrozen()) {
        doomCurrentInner = true;
      }
    }

    mInnerWindow = newInnerWindow->AsInner();
    MOZ_ASSERT(mInnerWindow);
    mInnerWindow->TryToCacheTopInnerWindow();

    if (!GetWrapperPreserveColor()) {
      JS::Rooted<JSObject*> outer(cx,
        NewOuterWindowProxy(cx, newInnerGlobal, thisChrome));
      NS_ENSURE_TRUE(outer, NS_ERROR_FAILURE);

      js::SetProxyReservedSlot(outer, 0, js::PrivateValue(ToSupports(this)));

      // Inform the nsJSContext, which is the canonical holder of the outer.
      mContext->SetWindowProxy(outer);
      mContext->DidInitializeContext();

      SetWrapper(mContext->GetWindowProxy());
    } else {
      JS::ExposeObjectToActiveJS(newInnerGlobal);
      JS::Rooted<JSObject*> outerObject(cx,
        NewOuterWindowProxy(cx, newInnerGlobal, thisChrome));
      if (!outerObject) {
        NS_ERROR("out of memory");
        return NS_ERROR_FAILURE;
      }

      JS::Rooted<JSObject*> obj(cx, GetWrapperPreserveColor());

      js::SetProxyReservedSlot(obj, 0, js::PrivateValue(nullptr));
      js::SetProxyReservedSlot(outerObject, 0, js::PrivateValue(nullptr));

      outerObject = xpc::TransplantObject(cx, obj, outerObject);
      if (!outerObject) {
        NS_ERROR("unable to transplant wrappers, probably OOM");
        return NS_ERROR_FAILURE;
      }

      js::SetProxyReservedSlot(outerObject, 0, js::PrivateValue(ToSupports(this)));

      SetWrapper(outerObject);

      MOZ_ASSERT(JS::GetNonCCWObjectGlobal(outerObject) == newInnerGlobal);

      // Inform the nsJSContext, which is the canonical holder of the outer.
      mContext->SetWindowProxy(outerObject);
    }

    // Enter the new global's realm.
    JSAutoRealm ar(cx, GetWrapperPreserveColor());

    {
      JS::Rooted<JSObject*> outer(cx, GetWrapperPreserveColor());
      js::SetWindowProxy(cx, newInnerGlobal, outer);
    }

    // Set scriptability based on the state of the docshell.
    bool allow = GetDocShell()->GetCanExecuteScripts();
    xpc::Scriptability::Get(GetWrapperPreserveColor()).SetDocShellAllowsScript(allow);

    if (!aState) {
      // Get the "window" property once so it will be cached on our inner.  We
      // have to do this here, not in binding code, because this has to happen
      // after we've created the outer window proxy and stashed it in the outer
      // nsGlobalWindowOuter, so GetWrapperPreserveColor() on that outer
      // nsGlobalWindowOuter doesn't return null and nsGlobalWindowOuter::OuterObject
      // works correctly.
      JS::Rooted<JS::Value> unused(cx);
      if (!JS_GetProperty(cx, newInnerGlobal, "window", &unused)) {
        NS_ERROR("can't create the 'window' property");
        return NS_ERROR_FAILURE;
      }

      // And same thing for the "self" property.
      if (!JS_GetProperty(cx, newInnerGlobal, "self", &unused)) {
        NS_ERROR("can't create the 'self' property");
        return NS_ERROR_FAILURE;
      }
    }
  }

  JSAutoRealm ar(cx, GetWrapperPreserveColor());

  if (!aState && !reUseInnerWindow) {
    // Loading a new page and creating a new inner window, *not*
    // restoring from session history.

    // Now that both the the inner and outer windows are initialized
    // let the script context do its magic to hook them together.
    MOZ_ASSERT(mContext->GetWindowProxy() == GetWrapperPreserveColor());
#ifdef DEBUG
    JS::Rooted<JSObject*> rootedJSObject(cx, GetWrapperPreserveColor());
    JS::Rooted<JSObject*> proto1(cx), proto2(cx);
    JS_GetPrototype(cx, rootedJSObject, &proto1);
    JS_GetPrototype(cx, newInnerGlobal, &proto2);
    NS_ASSERTION(proto1 == proto2,
                 "outer and inner globals should have the same prototype");
#endif

    mInnerWindow->SyncStateFromParentWindow();
  }

  // Add an extra ref in case we release mContext during GC.
  nsCOMPtr<nsIScriptContext> kungFuDeathGrip(mContext);

  aDocument->SetScriptGlobalObject(newInnerWindow);
  MOZ_ASSERT(newInnerWindow->mTabGroup,
             "We must have a TabGroup cached at this point");

  if (!aState) {
    if (reUseInnerWindow) {

      if (newInnerWindow->mDoc != aDocument) {
        newInnerWindow->mDoc = aDocument;

        // The storage objects contain the URL of the window. We have to
        // recreate them when the innerWindow is reused.
        newInnerWindow->mLocalStorage = nullptr;
        newInnerWindow->mSessionStorage = nullptr;

        newInnerWindow->ClearDocumentDependentSlots(cx);

        // When replacing an initial about:blank document we call
        // ExecutionReady again to update the client creation URL.
        rv = newInnerWindow->ExecutionReady();
        NS_ENSURE_SUCCESS(rv, rv);
      }
    } else {
      newInnerWindow->InnerSetNewDocument(cx, aDocument);

      // Initialize DOM classes etc on the inner window.
      JS::Rooted<JSObject*> obj(cx, newInnerGlobal);
      rv = kungFuDeathGrip->InitClasses(obj);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = newInnerWindow->ExecutionReady();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (mArguments) {
      newInnerWindow->DefineArgumentsProperty(mArguments);
      mArguments = nullptr;
    }

    // Give the new inner window our chrome event handler (since it
    // doesn't have one).
    newInnerWindow->mChromeEventHandler = mChromeEventHandler;
  }

  // Handle any document.open() logic after we setup the new inner window
  // so that any bound DETH objects can see the top window, document, etc.
  if (handleDocumentOpen) {
    newInnerWindow->MigrateStateForDocumentOpen(currentInner);
  }

  // We no longer need the old inner window.  Start its destruction if
  // its not being reused and clear our reference.
  if (doomCurrentInner) {
    currentInner->FreeInnerObjects();
  }
  currentInner = nullptr;

  // Ask the JS engine to assert that it's valid to access our DocGroup whenever
  // it runs JS code for this realm. We skip the check if this window is for
  // chrome JS or an add-on.
  nsCOMPtr<nsIPrincipal> principal = mDoc->NodePrincipal();
  if (GetDocGroup() && !nsContentUtils::IsSystemPrincipal(principal) &&
      !BasePrincipal::Cast(principal)->AddonPolicy()) {
    js::SetRealmValidAccessPtr(cx, newInnerGlobal,
                               newInnerWindow->GetDocGroup()->GetValidAccessPtr());
  }

  kungFuDeathGrip->DidInitializeContext();

  // We wait to fire the debugger hook until the window is all set up and hooked
  // up with the outer. See bug 969156.
  if (createdInnerWindow) {
    nsContentUtils::AddScriptRunner(
      NewRunnableMethod("nsGlobalWindowInner::FireOnNewGlobalObject",
                        newInnerWindow,
                        &nsGlobalWindowInner::FireOnNewGlobalObject));
  }

  if (newInnerWindow && !newInnerWindow->mHasNotifiedGlobalCreated && mDoc) {
    // We should probably notify. However if this is the, arguably bad,
    // situation when we're creating a temporary non-chrome-about-blank
    // document in a chrome docshell, don't notify just yet. Instead wait
    // until we have a real chrome doc.
    if (!mDocShell ||
        mDocShell->ItemType() != nsIDocShellTreeItem::typeChrome ||
        nsContentUtils::IsSystemPrincipal(mDoc->NodePrincipal())) {
      newInnerWindow->mHasNotifiedGlobalCreated = true;
      nsContentUtils::AddScriptRunner(
        NewRunnableMethod("nsGlobalWindowOuter::DispatchDOMWindowCreated",
                          this,
                          &nsGlobalWindowOuter::DispatchDOMWindowCreated));
    }
  }

  PreloadLocalStorage();

  // If we have a recorded interesting Large-Allocation header status, report it
  // to the newly attached document.
  ReportLargeAllocStatus();
  mLargeAllocStatus = LargeAllocStatus::NONE;

  return NS_OK;
}

void
nsGlobalWindowOuter::PreloadLocalStorage()
{
  if (!Storage::StoragePrefIsEnabled()) {
    return;
  }

  if (IsChromeWindow()) {
    return;
  }

  nsIPrincipal* principal = GetPrincipal();
  if (!principal) {
    return;
  }

  nsresult rv;

  nsCOMPtr<nsIDOMStorageManager> storageManager =
    do_GetService("@mozilla.org/dom/localStorage-manager;1", &rv);
  if (NS_FAILED(rv)) {
    return;
  }

  // private browsing windows do not persist local storage to disk so we should
  // only try to precache storage when we're not a private browsing window.
  if (principal->GetPrivateBrowsingId() == 0) {
    nsCOMPtr<nsIDOMStorage> storage;
    rv = storageManager->PrecacheStorage(principal, getter_AddRefs(storage));
    if (NS_SUCCEEDED(rv)) {
      mLocalStorage = static_cast<Storage*>(storage.get());
    }
  }
}

void
nsGlobalWindowOuter::DispatchDOMWindowCreated()
{
  if (!mDoc) {
    return;
  }

  // Fire DOMWindowCreated at chrome event listeners
  nsContentUtils::DispatchChromeEvent(mDoc, mDoc, NS_LITERAL_STRING("DOMWindowCreated"),
                                      CanBubble::eYes,
                                      Cancelable::eNo);

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();

  // The event dispatching could possibly cause docshell destory, and
  // consequently cause mDoc to be set to nullptr by DropOuterWindowDocs(),
  // so check it again here.
  if (observerService && mDoc) {
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
nsGlobalWindowOuter::ClearStatus()
{
  SetStatusOuter(EmptyString());
}

void
nsGlobalWindowOuter::SetDocShell(nsIDocShell* aDocShell)
{
  MOZ_ASSERT(aDocShell);

  if (aDocShell == mDocShell) {
    return;
  }

  mDocShell = aDocShell; // Weak Reference

  nsCOMPtr<nsPIDOMWindowOuter> parentWindow = GetScriptableParentOrNull();
  MOZ_RELEASE_ASSERT(!parentWindow || !mTabGroup ||
                     mTabGroup == nsGlobalWindowOuter::Cast(parentWindow)->mTabGroup);

  mTopLevelOuterContentWindow =
    !mIsChrome && GetScriptableTopInternal() == this;

  if (mFrames) {
    mFrames->SetDocShell(aDocShell);
  }

  // Get our enclosing chrome shell and retrieve its global window impl, so
  // that we can do some forwarding to the chrome document.
  RefPtr<EventTarget> chromeEventHandler;
  mDocShell->GetChromeEventHandler(getter_AddRefs(chromeEventHandler));
  mChromeEventHandler = do_QueryInterface(chromeEventHandler);
  if (!mChromeEventHandler) {
    // We have no chrome event handler. If we have a parent,
    // get our chrome event handler from the parent. If
    // we don't have a parent, then we need to make a new
    // window root object that will function as a chrome event
    // handler and receive all events that occur anywhere inside
    // our window.
    nsCOMPtr<nsPIDOMWindowOuter> parentWindow = GetParent();
    if (parentWindow.get() != this) {
      mChromeEventHandler = parentWindow->GetChromeEventHandler();
    }
    else {
      mChromeEventHandler = NS_NewWindowRoot(this);
      mIsRootOuterWindow = true;
    }
  }

  bool docShellActive;
  mDocShell->GetIsActive(&docShellActive);
  SetIsBackgroundInternal(!docShellActive);
}

void
nsGlobalWindowOuter::DetachFromDocShell()
{
  // DetachFromDocShell means the window is being torn down. Drop our
  // reference to the script context, allowing it to be deleted
  // later. Meanwhile, keep our weak reference to the script object
  // so that it can be retrieved later (until it is finalized by the JS GC).

  // Call FreeInnerObjects on all inner windows, not just the current
  // one, since some could be held by WindowStateHolder objects that
  // are GC-owned.
  RefPtr<nsGlobalWindowInner> inner;
  for (PRCList* node = PR_LIST_HEAD(this);
       node != this;
       node = PR_NEXT_LINK(inner)) {
    // This cast is safe because `node != this`. Non-this nodes are inner windows.
    inner = static_cast<nsGlobalWindowInner*>(node);
    MOZ_ASSERT(!inner->mOuterWindow || inner->mOuterWindow == this);
    inner->FreeInnerObjects();
  }

  // Don't report that we were detached to the nsWindowMemoryReporter, as it
  // only tracks inner windows.

  NotifyWindowIDDestroyed("outer-window-destroyed");

  nsGlobalWindowInner *currentInner = GetCurrentInnerWindowInternal();

  if (currentInner) {
    NS_ASSERTION(mDoc, "Must have doc!");

    // Remember the document's principal and URI.
    mDocumentPrincipal = mDoc->NodePrincipal();
    mDocumentURI = mDoc->GetDocumentURI();
    mDocBaseURI = mDoc->GetDocBaseURI();

    // Release our document reference
    DropOuterWindowDocs();
  }

  ClearControllers();

  mChromeEventHandler = nullptr; // force release now

  if (mContext) {
    // When we're about to destroy a top level content window
    // (for example a tab), we trigger a full GC by passing null as the last
    // param. We also trigger a full GC for chrome windows.
    nsJSContext::PokeGC(JS::gcreason::SET_DOC_SHELL,
                        (mTopLevelOuterContentWindow || mIsChrome) ?
                          nullptr : GetWrapperPreserveColor());
    mContext = nullptr;
  }

  mDocShell = nullptr; // Weak Reference

  if (mFrames) {
    mFrames->SetDocShell(nullptr);
  }

  MaybeForgiveSpamCount();
  CleanUp();
}

void
nsGlobalWindowOuter::SetOpenerWindow(nsPIDOMWindowOuter* aOpener,
                                     bool aOriginalOpener)
{
  nsWeakPtr opener = do_GetWeakReference(aOpener);
  if (opener == mOpener) {
    return;
  }

  NS_ASSERTION(!aOriginalOpener || !mSetOpenerWindowCalled,
               "aOriginalOpener is true, but not first call to "
               "SetOpenerWindow!");
  NS_ASSERTION(aOpener || !aOriginalOpener,
               "Shouldn't set mHadOriginalOpener if aOpener is null");

  mOpener = opener.forget();
  NS_ASSERTION(mOpener || !aOpener, "Opener must support weak references!");

  // Check that the js visible opener matches! We currently don't depend on this
  // being true outside of nightly, so we disable the assertion in optimized
  // release / beta builds.
  nsPIDOMWindowOuter* contentOpener = GetSanitizedOpener(aOpener);

  // contentOpener is not used when the DIAGNOSTIC_ASSERT is compiled out.
  mozilla::Unused << contentOpener;
  MOZ_DIAGNOSTIC_ASSERT(!contentOpener || !mTabGroup ||
    mTabGroup == nsGlobalWindowOuter::Cast(contentOpener)->mTabGroup);

  if (aOriginalOpener) {
    MOZ_ASSERT(!mHadOriginalOpener,
               "Probably too late to call ComputeIsSecureContext again");
    mHadOriginalOpener = true;
  }

#ifdef DEBUG
  mSetOpenerWindowCalled = true;
#endif
}

void
nsGlobalWindowOuter::UpdateParentTarget()
{
  // NOTE: This method is identical to
  // nsGlobalWindowInner::UpdateParentTarget(). IF YOU UPDATE THIS METHOD,
  // UPDATE THE OTHER ONE TOO!

  // Try to get our frame element's tab child global (its in-process message
  // manager).  If that fails, fall back to the chrome event handler's tab
  // child global, and if it doesn't have one, just use the chrome event
  // handler itself.

  nsCOMPtr<Element> frameElement = GetOuterWindow()->GetFrameElementInternal();
  nsCOMPtr<EventTarget> eventTarget =
    nsContentUtils::TryGetTabChildGlobalAsEventTarget(frameElement);

  if (!eventTarget) {
    nsGlobalWindowOuter* topWin = GetScriptableTopInternal();
    if (topWin) {
      frameElement = topWin->GetFrameElementInternal();
      eventTarget =
        nsContentUtils::TryGetTabChildGlobalAsEventTarget(frameElement);
    }
  }

  if (!eventTarget) {
    eventTarget =
      nsContentUtils::TryGetTabChildGlobalAsEventTarget(mChromeEventHandler);
  }

  if (!eventTarget) {
    eventTarget = mChromeEventHandler;
  }

  mParentTarget = eventTarget;
}

EventTarget*
nsGlobalWindowOuter::GetTargetForEventTargetChain()
{
  return GetCurrentInnerWindowInternal();
}

void
nsGlobalWindowOuter::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  MOZ_CRASH("The outer window should not be part of an event path");
}

bool
nsGlobalWindowOuter::ShouldPromptToBlockDialogs()
{
  nsGlobalWindowOuter *topWindowOuter = GetScriptableTopInternal();
  if (!topWindowOuter) {
    NS_ASSERTION(!mDocShell, "ShouldPromptToBlockDialogs() called without a top window?");
    return true;
  }

  nsGlobalWindowInner* topWindow = topWindowOuter->GetCurrentInnerWindowInternal();
  if (!topWindow) {
    return true;
  }

  return topWindow->DialogsAreBeingAbused();
}

bool
nsGlobalWindowOuter::AreDialogsEnabled()
{
  nsGlobalWindowOuter *topWindowOuter = GetScriptableTopInternal();
  if (!topWindowOuter) {
    NS_ERROR("AreDialogsEnabled() called without a top window?");
    return false;
  }

  // TODO: Warn if no top window?
  nsGlobalWindowInner* topWindow = topWindowOuter->GetCurrentInnerWindowInternal();
  if (!topWindow) {
    return false;
  }

  // Dialogs are blocked if the content viewer is hidden
  if (mDocShell) {
    nsCOMPtr<nsIContentViewer> cv;
    mDocShell->GetContentViewer(getter_AddRefs(cv));

    bool isHidden;
    cv->GetIsHidden(&isHidden);
    if (isHidden) {
      return false;
    }
  }

  // Dialogs are also blocked if the document is sandboxed with SANDBOXED_MODALS
  // (or if we have no document, of course).  Which document?  Who knows; the
  // spec is daft.  See <https://github.com/whatwg/html/issues/1206>.  For now
  // just go ahead and check mDoc, since in everything except edge cases in
  // which a frame is allow-same-origin but not allow-scripts and is being poked
  // at by some other window this should be the right thing anyway.
  if (!mDoc || (mDoc->GetSandboxFlags() & SANDBOXED_MODALS)) {
    return false;
  }

  return topWindow->mAreDialogsEnabled;
}

bool
nsGlobalWindowOuter::ConfirmDialogIfNeeded()
{
  NS_ENSURE_TRUE(mDocShell, false);
  nsCOMPtr<nsIPromptService> promptSvc =
    do_GetService("@mozilla.org/embedcomp/prompt-service;1");

  if (!promptSvc) {
    return true;
  }

  // Reset popup state while opening a modal dialog, and firing events
  // about the dialog, to prevent the current state from being active
  // the whole time a modal dialog is open.
  nsAutoPopupStatePusher popupStatePusher(openAbused, true);

  bool disableDialog = false;
  nsAutoString label, title;
  nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                     "ScriptDialogLabel", label);
  nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                     "ScriptDialogPreventTitle", title);
  promptSvc->Confirm(this, title.get(), label.get(), &disableDialog);
  if (disableDialog) {
    DisableDialogs();
    return false;
  }

  return true;
}

void
nsGlobalWindowOuter::DisableDialogs()
{
  nsGlobalWindowOuter *topWindowOuter = GetScriptableTopInternal();
  if (!topWindowOuter) {
    NS_ERROR("DisableDialogs() called without a top window?");
    return;
  }

  nsGlobalWindowInner* topWindow = topWindowOuter->GetCurrentInnerWindowInternal();
  // TODO: Warn if no top window?
  if (topWindow) {
    topWindow->mAreDialogsEnabled = false;
  }
}

void
nsGlobalWindowOuter::EnableDialogs()
{
  nsGlobalWindowOuter *topWindowOuter = GetScriptableTopInternal();
  if (!topWindowOuter) {
    NS_ERROR("EnableDialogs() called without a top window?");
    return;
  }

  // TODO: Warn if no top window?
  nsGlobalWindowInner* topWindow = topWindowOuter->GetCurrentInnerWindowInternal();
  if (topWindow) {
    topWindow->mAreDialogsEnabled = true;
  }
}

nsresult
nsGlobalWindowOuter::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  MOZ_CRASH("The outer window should not be part of an event path");
}

void
nsGlobalWindowOuter::PoisonOuterWindowProxy(JSObject *aObject)
{
  if (aObject == GetWrapperMaybeDead()) {
    PoisonWrapper();
  }
}

nsresult
nsGlobalWindowOuter::SetArguments(nsIArray *aArguments)
{
  nsresult rv;

  // Historically, we've used the same machinery to handle openDialog arguments
  // (exposed via window.arguments) and showModalDialog arguments (exposed via
  // window.dialogArguments), even though the former is XUL-only and uses an XPCOM
  // array while the latter is web-exposed and uses an arbitrary JS value.
  // Moreover, per-spec |dialogArguments| is a property of the browsing context
  // (outer), whereas |arguments| lives on the inner.
  //
  // We've now mostly separated them, but the difference is still opaque to
  // nsWindowWatcher (the caller of SetArguments in this little back-and-forth
  // embedding waltz we do here).
  //
  // So we need to demultiplex the two cases here.
  nsGlobalWindowInner *currentInner = GetCurrentInnerWindowInternal();

  mArguments = aArguments;
  rv = currentInner->DefineArgumentsProperty(aArguments);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

//*****************************************************************************
// nsGlobalWindowOuter::nsIScriptObjectPrincipal
//*****************************************************************************

nsIPrincipal*
nsGlobalWindowOuter::GetPrincipal()
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

  return nullptr;
}

//*****************************************************************************
// nsGlobalWindowOuter::nsIDOMWindow
//*****************************************************************************

void
nsPIDOMWindowOuter::SetInitialKeyboardIndicators(
  UIStateChangeType aShowAccelerators, UIStateChangeType aShowFocusRings)
{
  MOZ_ASSERT(!GetCurrentInnerWindow());

  nsPIDOMWindowOuter* piWin = GetPrivateRoot();
  if (!piWin) {
    return;
  }

  MOZ_ASSERT(piWin == this);

  // only change the flags that have been modified
  nsCOMPtr<nsPIWindowRoot> windowRoot = do_QueryInterface(mChromeEventHandler);
  if (!windowRoot) {
    return;
  }

  if (aShowAccelerators != UIStateChangeType_NoChange) {
    windowRoot->SetShowAccelerators(aShowAccelerators == UIStateChangeType_Set);
  }
  if (aShowFocusRings != UIStateChangeType_NoChange) {
    windowRoot->SetShowFocusRings(aShowFocusRings == UIStateChangeType_Set);
  }

  nsContentUtils::SetKeyboardIndicatorsOnRemoteChildren(GetOuterWindow(),
                                                        aShowAccelerators,
                                                        aShowFocusRings);
}

Element*
nsPIDOMWindowOuter::GetFrameElementInternal() const
{
  return mFrameElement;
}

void
nsPIDOMWindowOuter::SetFrameElementInternal(Element* aFrameElement)
{
  mFrameElement = aFrameElement;
}

Navigator*
nsGlobalWindowOuter::GetNavigator()
{
  FORWARD_TO_INNER(Navigator, (), nullptr);
}

nsScreen*
nsGlobalWindowOuter::GetScreen()
{
  FORWARD_TO_INNER(GetScreen, (IgnoreErrors()), nullptr);
}

void
nsPIDOMWindowOuter::MaybeActiveMediaComponents()
{
  if (mMediaSuspend != nsISuspendedTypes::SUSPENDED_BLOCK) {
    return;
  }

  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
         ("nsPIDOMWindowOuter, MaybeActiveMediaComponents, "
          "resume the window from blocked, this = %p\n", this));

  SetMediaSuspend(nsISuspendedTypes::NONE_SUSPENDED);
}

SuspendTypes
nsPIDOMWindowOuter::GetMediaSuspend() const
{
  return mMediaSuspend;
}

void
nsPIDOMWindowOuter::SetMediaSuspend(SuspendTypes aSuspend)
{
  if (!IsDisposableSuspend(aSuspend)) {
    MaybeNotifyMediaResumedFromBlock(aSuspend);
    mMediaSuspend = aSuspend;
  }

  RefreshMediaElementsSuspend(aSuspend);
}

void
nsPIDOMWindowOuter::MaybeNotifyMediaResumedFromBlock(SuspendTypes aSuspend)
{
  if (mMediaSuspend == nsISuspendedTypes::SUSPENDED_BLOCK &&
      aSuspend == nsISuspendedTypes::NONE_SUSPENDED) {
    RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
    if (service) {
      service->NotifyMediaResumedFromBlock(GetOuterWindow());
    }
  }
}

bool
nsPIDOMWindowOuter::GetAudioMuted() const
{
  return mAudioMuted;
}

void
nsPIDOMWindowOuter::SetAudioMuted(bool aMuted)
{
  if (mAudioMuted == aMuted) {
    return;
  }

  mAudioMuted = aMuted;
  RefreshMediaElementsVolume();
}

float
nsPIDOMWindowOuter::GetAudioVolume() const
{
  return mAudioVolume;
}

nsresult
nsPIDOMWindowOuter::SetAudioVolume(float aVolume)
{
  if (aVolume < 0.0) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  if (mAudioVolume == aVolume) {
    return NS_OK;
  }

  mAudioVolume = aVolume;
  RefreshMediaElementsVolume();
  return NS_OK;
}

void
nsPIDOMWindowOuter::RefreshMediaElementsVolume()
{
  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  if (service) {
    service->RefreshAgentsVolume(GetOuterWindow());
  }
}

void
nsPIDOMWindowOuter::RefreshMediaElementsSuspend(SuspendTypes aSuspend)
{
  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  if (service) {
    service->RefreshAgentsSuspend(GetOuterWindow(), aSuspend);
  }
}

bool
nsPIDOMWindowOuter::IsDisposableSuspend(SuspendTypes aSuspend) const
{
  return (aSuspend == nsISuspendedTypes::SUSPENDED_PAUSE_DISPOSABLE ||
          aSuspend == nsISuspendedTypes::SUSPENDED_STOP_DISPOSABLE);
}

void
nsPIDOMWindowOuter::SetServiceWorkersTestingEnabled(bool aEnabled)
{
  // Devtools should only be setting this on the top level window.  Its
  // ok if devtools clears the flag on clean up of nested windows, though.
  // It will have no affect.
#ifdef DEBUG
  nsCOMPtr<nsPIDOMWindowOuter> topWindow = GetScriptableTop();
  MOZ_ASSERT_IF(aEnabled, this == topWindow);
#endif
  mServiceWorkersTestingEnabled = aEnabled;
}

bool
nsPIDOMWindowOuter::GetServiceWorkersTestingEnabled()
{
  // Automatically get this setting from the top level window so that nested
  // iframes get the correct devtools setting.
  nsCOMPtr<nsPIDOMWindowOuter> topWindow = GetScriptableTop();
  if (!topWindow) {
    return false;
  }
  return topWindow->mServiceWorkersTestingEnabled;
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindowOuter::GetParentOuter()
{
  if (!mDocShell) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> parent;
  if (mDocShell->GetIsMozBrowser()) {
    parent = this;
  } else {
    parent = GetParent();
  }

  return parent.forget();
}

/**
 * GetScriptableParent is called when script reads window.parent.
 *
 * In contrast to GetRealParent, GetScriptableParent respects <iframe
 * mozbrowser> boundaries, so if |this| is contained by an <iframe
 * mozbrowser>, we will return |this| as its own parent.
 */
nsPIDOMWindowOuter*
nsGlobalWindowOuter::GetScriptableParent()
{
  nsCOMPtr<nsPIDOMWindowOuter> parent = GetParentOuter();
  return parent.get();
}

/**
 * Behavies identically to GetScriptableParent extept that it returns null
 * if GetScriptableParent would return this window.
 */
nsPIDOMWindowOuter*
nsGlobalWindowOuter::GetScriptableParentOrNull()
{
  nsPIDOMWindowOuter* parent = GetScriptableParent();
  return (nsGlobalWindowOuter::Cast(parent) == this) ? nullptr : parent;
}

/**
 * nsPIDOMWindow::GetParent (when called from C++) is just a wrapper around
 * GetRealParent.
 */
already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindowOuter::GetParent()
{
  if (!mDocShell) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShell> parent;
  mDocShell->GetSameTypeParentIgnoreBrowserBoundaries(getter_AddRefs(parent));

  if (parent) {
    nsCOMPtr<nsPIDOMWindowOuter> win = parent->GetWindow();
    return win.forget();
  }

  nsCOMPtr<nsPIDOMWindowOuter> win(this);
  return win.forget();
}

static nsresult
GetTopImpl(nsGlobalWindowOuter* aWin, nsPIDOMWindowOuter** aTop, bool aScriptable)
{
  *aTop = nullptr;

  // Walk up the parent chain.

  nsCOMPtr<nsPIDOMWindowOuter> prevParent = aWin;
  nsCOMPtr<nsPIDOMWindowOuter> parent = aWin;
  do {
    if (!parent) {
      break;
    }

    prevParent = parent;

    nsCOMPtr<nsPIDOMWindowOuter> newParent;
    if (aScriptable) {
      newParent = parent->GetScriptableParent();
    }
    else {
      newParent = parent->GetParent();
    }

    parent = newParent;

  } while (parent != prevParent);

  if (parent) {
    parent.swap(*aTop);
  }

  return NS_OK;
}

/**
 * GetScriptableTop is called when script reads window.top.
 *
 * In contrast to GetRealTop, GetScriptableTop respects <iframe mozbrowser>
 * boundaries.  If we encounter a window owned by an <iframe mozbrowser> while
 * walking up the window hierarchy, we'll stop and return that window.
 */
nsPIDOMWindowOuter*
nsGlobalWindowOuter::GetScriptableTop()
{
  nsCOMPtr<nsPIDOMWindowOuter> window;
  GetTopImpl(this, getter_AddRefs(window), /* aScriptable = */ true);
  return window.get();
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindowOuter::GetTop()
{
  nsCOMPtr<nsPIDOMWindowOuter> window;
  GetTopImpl(this, getter_AddRefs(window), /* aScriptable = */ false);
  return window.forget();
}

void
nsGlobalWindowOuter::GetContentOuter(JSContext* aCx,
                                     JS::MutableHandle<JSObject*> aRetval,
                                     CallerType aCallerType,
                                     ErrorResult& aError)
{
  nsCOMPtr<nsPIDOMWindowOuter> content =
    GetContentInternal(aError, aCallerType);
  if (aError.Failed()) {
    return;
  }

  if (content) {
    JS::Rooted<JS::Value> val(aCx);
    aError = nsContentUtils::WrapNative(aCx, content, &val);
    if (aError.Failed()) {
      return;
    }

    aRetval.set(&val.toObject());
    return;
  }

  aRetval.set(nullptr);
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindowOuter::GetContentInternal(ErrorResult& aError, CallerType aCallerType)
{
  // First check for a named frame named "content"
  nsCOMPtr<nsPIDOMWindowOuter> domWindow =
    GetChildWindow(NS_LITERAL_STRING("content"));
  if (domWindow) {
    return domWindow.forget();
  }

  // If we're contained in <iframe mozbrowser>, then GetContent is the same as
  // window.top.
  if (mDocShell && mDocShell->GetIsInMozBrowser()) {
    return GetTopOuter();
  }

  nsCOMPtr<nsIDocShellTreeItem> primaryContent;
  if (aCallerType != CallerType::System) {
    if (mDoc) {
      mDoc->WarnOnceAbout(nsIDocument::eWindowContentUntrusted);
    }
    // If we're called by non-chrome code, make sure we don't return
    // the primary content window if the calling tab is hidden. In
    // such a case we return the same-type root in the hidden tab,
    // which is "good enough", for now.
    nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(mDocShell));

    if (baseWin) {
      bool visible = false;
      baseWin->GetVisibility(&visible);

      if (!visible) {
        mDocShell->GetSameTypeRootTreeItem(getter_AddRefs(primaryContent));
      }
    }
  }

  if (!primaryContent) {
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner = GetTreeOwner();
    if (!treeOwner) {
      aError.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    treeOwner->GetPrimaryContentShell(getter_AddRefs(primaryContent));
  }

  if (!primaryContent) {
    return nullptr;
  }

  domWindow = primaryContent->GetWindow();
  return domWindow.forget();
}

nsresult
nsGlobalWindowOuter::GetPrompter(nsIPrompt** aPrompt)
{
  if (!mDocShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(prompter, NS_ERROR_NO_INTERFACE);

  prompter.forget(aPrompt);
  return NS_OK;
}

bool
nsGlobalWindowOuter::GetClosedOuter()
{
  // If someone called close(), or if we don't have a docshell, we're closed.
  return mIsClosed || !mDocShell;
}

bool
nsGlobalWindowOuter::Closed()
{
  return GetClosedOuter();
}

nsDOMWindowList*
nsGlobalWindowOuter::GetFrames()
{
  if (!mFrames && mDocShell) {
    mFrames = new nsDOMWindowList(mDocShell);
  }

  return mFrames;
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindowOuter::IndexedGetterOuter(uint32_t aIndex)
{
  nsDOMWindowList* windows = GetFrames();
  NS_ENSURE_TRUE(windows, nullptr);

  return windows->IndexedGetter(aIndex);
}

nsIControllers*
nsGlobalWindowOuter::GetControllersOuter(ErrorResult& aError)
{
  if (!mControllers) {
    nsresult rv;
    mControllers = do_CreateInstance(kXULControllersCID, &rv);
    if (NS_FAILED(rv)) {
      aError.Throw(rv);
      return nullptr;
    }

    // Add in the default controller
    nsCOMPtr<nsIController> controller = do_CreateInstance(
                               NS_WINDOWCONTROLLER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      aError.Throw(rv);
      return nullptr;
    }

    mControllers->InsertControllerAt(0, controller);
    nsCOMPtr<nsIControllerContext> controllerContext = do_QueryInterface(controller);
    if (!controllerContext) {
      aError.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    controllerContext->SetCommandContext(static_cast<nsIDOMWindow*>(this));
  }

  return mControllers;
}

nsresult
nsGlobalWindowOuter::GetControllers(nsIControllers** aResult)
{
  FORWARD_TO_INNER(GetControllers, (aResult), NS_ERROR_UNEXPECTED);
}

nsPIDOMWindowOuter*
nsGlobalWindowOuter::GetSanitizedOpener(nsPIDOMWindowOuter* aOpener)
{
  if (!aOpener) {
    return nullptr;
  }

  nsGlobalWindowOuter* win = nsGlobalWindowOuter::Cast(aOpener);

  // First, ensure that we're not handing back a chrome window to content:
  if (win->IsChromeWindow()) {
    return nullptr;
  }

  // We don't want to reveal the opener if the opener is a mail window,
  // because opener can be used to spoof the contents of a message (bug 105050).
  // So, we look in the opener's root docshell to see if it's a mail window.
  nsCOMPtr<nsIDocShell> openerDocShell = aOpener->GetDocShell();

  if (openerDocShell) {
    nsCOMPtr<nsIDocShellTreeItem> openerRootItem;
    openerDocShell->GetRootTreeItem(getter_AddRefs(openerRootItem));
    nsCOMPtr<nsIDocShell> openerRootDocShell(do_QueryInterface(openerRootItem));
    if (openerRootDocShell) {
      uint32_t appType;
      nsresult rv = openerRootDocShell->GetAppType(&appType);
      if (NS_SUCCEEDED(rv) && appType != nsIDocShell::APP_TYPE_MAIL) {
        return aOpener;
      }
    }
  }

  return nullptr;
}

nsPIDOMWindowOuter*
nsGlobalWindowOuter::GetOpenerWindowOuter()
{
  nsCOMPtr<nsPIDOMWindowOuter> opener = do_QueryReferent(mOpener);

  if (!opener) {
    return nullptr;
  }

  // First, check if we were called from a privileged chrome script
  if (nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
    // Catch the case where we're chrome but the opener is not...
    if (GetPrincipal() == nsContentUtils::GetSystemPrincipal() &&
        nsGlobalWindowOuter::Cast(opener)->GetPrincipal() != nsContentUtils::GetSystemPrincipal()) {
      return nullptr;
    }
    return opener;
  }

  return GetSanitizedOpener(opener);
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindowOuter::GetOpener()
{
  nsCOMPtr<nsPIDOMWindowOuter> opener = GetOpenerWindowOuter();
  return opener.forget();
}

void
nsGlobalWindowOuter::GetStatusOuter(nsAString& aStatus)
{
  aStatus = mStatus;
}

void
nsGlobalWindowOuter::SetStatusOuter(const nsAString& aStatus)
{
  mStatus = aStatus;

  // We don't support displaying window.status in the UI, so there's nothing
  // left to do here.
}

void
nsGlobalWindowOuter::GetNameOuter(nsAString& aName)
{
  if (mDocShell) {
    mDocShell->GetName(aName);
  }
}

void
nsGlobalWindowOuter::SetNameOuter(const nsAString& aName, mozilla::ErrorResult& aError)
{
  if (mDocShell) {
    aError = mDocShell->SetName(aName);
  }
}

// Helper functions used by many methods below.
int32_t
nsGlobalWindowOuter::DevToCSSIntPixels(int32_t px)
{
  if (!mDocShell)
    return px; // assume 1:1

  RefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return px;

  return presContext->DevPixelsToIntCSSPixels(px);
}

int32_t
nsGlobalWindowOuter::CSSToDevIntPixels(int32_t px)
{
  if (!mDocShell)
    return px; // assume 1:1

  RefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return px;

  return presContext->CSSPixelsToDevPixels(px);
}

nsIntSize
nsGlobalWindowOuter::DevToCSSIntPixels(nsIntSize px)
{
  if (!mDocShell)
    return px; // assume 1:1

  RefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return px;

  return nsIntSize(
      presContext->DevPixelsToIntCSSPixels(px.width),
      presContext->DevPixelsToIntCSSPixels(px.height));
}

nsIntSize
nsGlobalWindowOuter::CSSToDevIntPixels(nsIntSize px)
{
  if (!mDocShell)
    return px; // assume 1:1

  RefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return px;

  return nsIntSize(
    presContext->CSSPixelsToDevPixels(px.width),
    presContext->CSSPixelsToDevPixels(px.height));
}

nsresult
nsGlobalWindowOuter::GetInnerSize(CSSIntSize& aSize)
{
  EnsureSizeAndPositionUpToDate();

  NS_ENSURE_STATE(mDocShell);

  RefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  RefPtr<nsIPresShell> presShell = mDocShell->GetPresShell();

  if (!presContext || !presShell) {
    aSize = CSSIntSize(0, 0);
    return NS_OK;
  }

  /*
   * On platforms with resolution-based zooming, the CSS viewport
   * and visual viewport may not be the same. The inner size should
   * be the visual viewport, but we fall back to the CSS viewport
   * if it is not set.
   */
  if (presShell->IsScrollPositionClampingScrollPortSizeSet()) {
    aSize = CSSIntRect::FromAppUnitsRounded(
      presShell->GetScrollPositionClampingScrollPortSize());
  } else {
    RefPtr<nsViewManager> viewManager = presShell->GetViewManager();
    if (viewManager) {
      viewManager->FlushDelayedResize(false);
    }

    aSize = CSSIntRect::FromAppUnitsRounded(
      presContext->GetVisibleArea().Size());
  }
  return NS_OK;
}

int32_t
nsGlobalWindowOuter::GetInnerWidthOuter(ErrorResult& aError)
{
  CSSIntSize size;
  aError = GetInnerSize(size);
  return size.width;
}

nsresult
nsGlobalWindowOuter::GetInnerWidth(int32_t* aInnerWidth)
{
  FORWARD_TO_INNER(GetInnerWidth, (aInnerWidth), NS_ERROR_UNEXPECTED);
}

void
nsGlobalWindowOuter::SetInnerWidthOuter(int32_t aInnerWidth,
                                        CallerType aCallerType,
                                        ErrorResult& aError)
{
  if (!mDocShell) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  CheckSecurityWidthAndHeight(&aInnerWidth, nullptr, aCallerType);

  RefPtr<nsIPresShell> presShell = mDocShell->GetPresShell();

  if (presShell && presShell->GetIsViewportOverridden())
  {
    nscoord height = 0;

    RefPtr<nsPresContext> presContext;
    presContext = presShell->GetPresContext();

    nsRect shellArea = presContext->GetVisibleArea();
    height = shellArea.Height();
    SetCSSViewportWidthAndHeight(nsPresContext::CSSPixelsToAppUnits(aInnerWidth),
                                 height);
    return;
  }

  int32_t height = 0;
  int32_t unused  = 0;

  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
  docShellAsWin->GetSize(&unused, &height);
  aError = SetDocShellWidthAndHeight(CSSToDevIntPixels(aInnerWidth), height);
}

int32_t
nsGlobalWindowOuter::GetInnerHeightOuter(ErrorResult& aError)
{
  CSSIntSize size;
  aError = GetInnerSize(size);
  return size.height;
}

nsresult
nsGlobalWindowOuter::GetInnerHeight(int32_t* aInnerHeight)
{
  FORWARD_TO_INNER(GetInnerHeight, (aInnerHeight), NS_ERROR_UNEXPECTED);
}

void
nsGlobalWindowOuter::SetInnerHeightOuter(int32_t aInnerHeight,
                                         CallerType aCallerType,
                                         ErrorResult& aError)
{
  if (!mDocShell) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  RefPtr<nsIPresShell> presShell = mDocShell->GetPresShell();

  if (presShell && presShell->GetIsViewportOverridden())
  {
    RefPtr<nsPresContext> presContext;
    presContext = presShell->GetPresContext();

    nsRect shellArea = presContext->GetVisibleArea();
    nscoord height = aInnerHeight;
    nscoord width = shellArea.Width();
    CheckSecurityWidthAndHeight(nullptr, &height, aCallerType);
    SetCSSViewportWidthAndHeight(width,
                                 nsPresContext::CSSPixelsToAppUnits(height));
    return;
  }

  int32_t height = 0;
  int32_t width  = 0;

  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
  docShellAsWin->GetSize(&width, &height);
  CheckSecurityWidthAndHeight(nullptr, &aInnerHeight, aCallerType);
  aError = SetDocShellWidthAndHeight(width, CSSToDevIntPixels(aInnerHeight));
}

nsIntSize
nsGlobalWindowOuter::GetOuterSize(CallerType aCallerType, ErrorResult& aError)
{
  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    CSSIntSize size;
    aError = GetInnerSize(size);
    return nsIntSize(size.width, size.height);
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return nsIntSize(0, 0);
  }

  nsIntSize sizeDevPixels;
  aError = treeOwnerAsWin->GetSize(&sizeDevPixels.width, &sizeDevPixels.height);
  if (aError.Failed()) {
    return nsIntSize();
  }

  return DevToCSSIntPixels(sizeDevPixels);
}

int32_t
nsGlobalWindowOuter::GetOuterWidthOuter(CallerType aCallerType, ErrorResult& aError)
{
  return GetOuterSize(aCallerType, aError).width;
}

int32_t
nsGlobalWindowOuter::GetOuterHeightOuter(CallerType aCallerType, ErrorResult& aError)
{
  return GetOuterSize(aCallerType, aError).height;
}

void
nsGlobalWindowOuter::SetOuterSize(int32_t aLengthCSSPixels, bool aIsWidth,
                                  CallerType aCallerType, ErrorResult& aError)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  CheckSecurityWidthAndHeight(aIsWidth ? &aLengthCSSPixels : nullptr,
                              aIsWidth ? nullptr : &aLengthCSSPixels,
                              aCallerType);

  int32_t width, height;
  aError = treeOwnerAsWin->GetSize(&width, &height);
  if (aError.Failed()) {
    return;
  }

  int32_t lengthDevPixels = CSSToDevIntPixels(aLengthCSSPixels);
  if (aIsWidth) {
    width = lengthDevPixels;
  } else {
    height = lengthDevPixels;
  }
  aError = treeOwnerAsWin->SetSize(width, height, true);

  CheckForDPIChange();
}

void
nsGlobalWindowOuter::SetOuterWidthOuter(int32_t aOuterWidth,
                                        CallerType aCallerType,
                                        ErrorResult& aError)
{
  SetOuterSize(aOuterWidth, true, aCallerType, aError);
}

void
nsGlobalWindowOuter::SetOuterHeightOuter(int32_t aOuterHeight,
                                         CallerType aCallerType,
                                         ErrorResult& aError)
{
  SetOuterSize(aOuterHeight, false, aCallerType, aError);
}

CSSIntPoint
nsGlobalWindowOuter::GetScreenXY(CallerType aCallerType, ErrorResult& aError)
{
  // When resisting fingerprinting, always return (0,0)
  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    return CSSIntPoint(0, 0);
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return CSSIntPoint(0, 0);
  }

  int32_t x = 0, y = 0;
  aError = treeOwnerAsWin->GetPosition(&x, &y); // LayoutDevice px values

  RefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext) {
    return CSSIntPoint(x, y);
  }

  // Find the global desktop coordinate of the top-left of the screen.
  // We'll use this as a "fake origin" when converting to CSS px units,
  // to avoid overlapping coordinates in cases such as a hi-dpi screen
  // placed to the right of a lo-dpi screen on Windows. (Instead, there
  // may be "gaps" in the resulting CSS px coordinates in some cases.)
  nsDeviceContext *dc = presContext->DeviceContext();
  nsRect screenRect;
  dc->GetRect(screenRect);
  LayoutDeviceRect screenRectDev =
    LayoutDevicePixel::FromAppUnits(screenRect, dc->AppUnitsPerDevPixel());

  DesktopToLayoutDeviceScale scale = dc->GetDesktopToDeviceScale();
  DesktopRect screenRectDesk = screenRectDev / scale;

  CSSPoint cssPt =
    LayoutDevicePoint(x - screenRectDev.x, y - screenRectDev.y) /
    presContext->CSSToDevPixelScale();
  cssPt.x += screenRectDesk.x;
  cssPt.y += screenRectDesk.y;

  return CSSIntPoint(NSToIntRound(cssPt.x), NSToIntRound(cssPt.y));
}

int32_t
nsGlobalWindowOuter::GetScreenXOuter(CallerType aCallerType, ErrorResult& aError)
{
  return GetScreenXY(aCallerType, aError).x;
}

nsRect
nsGlobalWindowOuter::GetInnerScreenRect()
{
  if (!mDocShell) {
    return nsRect();
  }

  EnsureSizeAndPositionUpToDate();

  if (!mDocShell) {
    return nsRect();
  }

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();
  if (!presShell) {
    return nsRect();
  }
  nsIFrame* rootFrame = presShell->GetRootFrame();
  if (!rootFrame) {
    return nsRect();
  }

  return rootFrame->GetScreenRectInAppUnits();
}

float
nsGlobalWindowOuter::GetMozInnerScreenXOuter(CallerType aCallerType)
{
  // When resisting fingerprinting, always return 0.
  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    return 0.0;
  }

  nsRect r = GetInnerScreenRect();
  return nsPresContext::AppUnitsToFloatCSSPixels(r.x);
}

float
nsGlobalWindowOuter::GetMozInnerScreenYOuter(CallerType aCallerType)
{
  // Return 0 to prevent fingerprinting.
  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    return 0.0;
  }

  nsRect r = GetInnerScreenRect();
  return nsPresContext::AppUnitsToFloatCSSPixels(r.y);
}

double
nsGlobalWindowOuter::GetDevicePixelRatioOuter(CallerType aCallerType)
{
  if (!mDocShell) {
    return 1.0;
  }

  RefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext) {
    return 1.0;
  }

  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    return 1.0;
  }

  float overrideDPPX = presContext->GetOverrideDPPX();

  if (overrideDPPX > 0) {
    return overrideDPPX;
  }

  return double(nsPresContext::AppUnitsPerCSSPixel()) /
         double(presContext->AppUnitsPerDevPixel());
}

float
nsPIDOMWindowOuter::GetDevicePixelRatio(CallerType aCallerType)
{
  return nsGlobalWindowOuter::Cast(this)->GetDevicePixelRatioOuter(aCallerType);
}

uint64_t
nsGlobalWindowOuter::GetMozPaintCountOuter()
{
  if (!mDocShell) {
    return 0;
  }

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();
  return presShell ? presShell->GetPaintCount() : 0;
}

already_AddRefed<MediaQueryList>
nsGlobalWindowOuter::MatchMediaOuter(const nsAString& aMediaQueryList,
                                     CallerType aCallerType)
{
  if (!mDoc) {
    return nullptr;
  }

  return mDoc->MatchMedia(aMediaQueryList, aCallerType);
}

void
nsGlobalWindowOuter::SetScreenXOuter(int32_t aScreenX,
                                     CallerType aCallerType,
                                     ErrorResult& aError)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  int32_t x, y;
  aError = treeOwnerAsWin->GetPosition(&x, &y);
  if (aError.Failed()) {
    return;
  }

  CheckSecurityLeftAndTop(&aScreenX, nullptr, aCallerType);
  x = CSSToDevIntPixels(aScreenX);

  aError = treeOwnerAsWin->SetPosition(x, y);

  CheckForDPIChange();
}

int32_t
nsGlobalWindowOuter::GetScreenYOuter(CallerType aCallerType, ErrorResult& aError)
{
  return GetScreenXY(aCallerType, aError).y;
}


void
nsGlobalWindowOuter::SetScreenYOuter(int32_t aScreenY,
                                     CallerType aCallerType,
                                     ErrorResult& aError)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  int32_t x, y;
  aError = treeOwnerAsWin->GetPosition(&x, &y);
  if (aError.Failed()) {
    return;
  }

  CheckSecurityLeftAndTop(nullptr, &aScreenY, aCallerType);
  y = CSSToDevIntPixels(aScreenY);

  aError = treeOwnerAsWin->SetPosition(x, y);

  CheckForDPIChange();
}

// NOTE: Arguments to this function should have values scaled to
// CSS pixels, not device pixels.
void
nsGlobalWindowOuter::CheckSecurityWidthAndHeight(int32_t* aWidth, int32_t* aHeight,
                                                 CallerType aCallerType)
{
#ifdef MOZ_XUL
  if (aCallerType != CallerType::System) {
    // if attempting to resize the window, hide any open popups
    nsContentUtils::HidePopupsInDocument(mDoc);
  }
#endif

  // This one is easy. Just ensure the variable is greater than 100;
  if ((aWidth && *aWidth < 100) || (aHeight && *aHeight < 100)) {
    // Check security state for use in determing window dimensions

    if (aCallerType != CallerType::System) {
      //sec check failed
      if (aWidth && *aWidth < 100) {
        *aWidth = 100;
      }
      if (aHeight && *aHeight < 100) {
        *aHeight = 100;
      }
    }
  }
}

// NOTE: Arguments to this function should have values in device pixels
nsresult
nsGlobalWindowOuter::SetDocShellWidthAndHeight(int32_t aInnerWidth, int32_t aInnerHeight)
{
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  mDocShell->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwner->SizeShellTo(mDocShell, aInnerWidth, aInnerHeight),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

// NOTE: Arguments to this function should have values in app units
void
nsGlobalWindowOuter::SetCSSViewportWidthAndHeight(nscoord aInnerWidth, nscoord aInnerHeight)
{
  RefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));

  nsRect shellArea = presContext->GetVisibleArea();
  shellArea.SetHeight(aInnerHeight);
  shellArea.SetWidth(aInnerWidth);

  presContext->SetVisibleArea(shellArea);
}

// NOTE: Arguments to this function should have values scaled to
// CSS pixels, not device pixels.
void
nsGlobalWindowOuter::CheckSecurityLeftAndTop(int32_t* aLeft, int32_t* aTop,
                                             CallerType aCallerType)
{
  // This one is harder. We have to get the screen size and window dimensions.

  // Check security state for use in determing window dimensions

  if (aCallerType != CallerType::System) {
#ifdef MOZ_XUL
    // if attempting to move the window, hide any open popups
    nsContentUtils::HidePopupsInDocument(mDoc);
#endif

    if (nsGlobalWindowOuter* rootWindow = nsGlobalWindowOuter::Cast(GetPrivateRoot())) {
      rootWindow->FlushPendingNotifications(FlushType::Layout);
    }

    nsCOMPtr<nsIBaseWindow> treeOwner = GetTreeOwnerWindow();

    RefPtr<nsScreen> screen = GetScreen();

    if (treeOwner && screen) {
      int32_t winLeft, winTop, winWidth, winHeight;

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
      int32_t screenLeft = screen->GetAvailLeft(IgnoreErrors());
      int32_t screenWidth = screen->GetAvailWidth(IgnoreErrors());
      int32_t screenHeight = screen->GetAvailHeight(IgnoreErrors());
#if defined(XP_MACOSX)
      /* The mac's coordinate system is different from the assumed Windows'
         system. It offsets by the height of the menubar so that a window
         placed at (0,0) will be entirely visible. Unfortunately that
         correction is made elsewhere (in Widget) and the meaning of
         the Avail... coordinates is overloaded. Here we allow a window
         to be placed at (0,0) because it does make sense to do so.
      */
      int32_t screenTop = screen->GetTop(IgnoreErrors());
#else
      int32_t screenTop = screen->GetAvailTop(IgnoreErrors());
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
}

int32_t
nsGlobalWindowOuter::GetScrollBoundaryOuter(Side aSide)
{
  FlushPendingNotifications(FlushType::Layout);
  if (nsIScrollableFrame *sf = GetScrollFrame()) {
    return nsPresContext::
      AppUnitsToIntCSSPixels(sf->GetScrollRange().Edge(aSide));
  }
  return 0;
}

CSSPoint
nsGlobalWindowOuter::GetScrollXY(bool aDoFlush)
{
  if (aDoFlush) {
    FlushPendingNotifications(FlushType::Layout);
  } else {
    EnsureSizeAndPositionUpToDate();
  }

  nsIScrollableFrame *sf = GetScrollFrame();
  if (!sf) {
    return CSSIntPoint(0, 0);
  }

  nsPoint scrollPos = sf->GetScrollPosition();
  if (scrollPos != nsPoint(0,0) && !aDoFlush) {
    // Oh, well.  This is the expensive case -- the window is scrolled and we
    // didn't actually flush yet.  Repeat, but with a flush, since the content
    // may get shorter and hence our scroll position may decrease.
    return GetScrollXY(true);
  }

  return CSSPoint::FromAppUnits(scrollPos);
}

double
nsGlobalWindowOuter::GetScrollXOuter()
{
  return GetScrollXY(false).x;
}

double
nsGlobalWindowOuter::GetScrollYOuter()
{
  return GetScrollXY(false).y;
}

uint32_t
nsGlobalWindowOuter::Length()
{
  nsDOMWindowList* windows = GetFrames();

  return windows ? windows->GetLength() : 0;
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindowOuter::GetTopOuter()
{
  nsCOMPtr<nsPIDOMWindowOuter> top = GetScriptableTop();
  return top.forget();
}

nsPIDOMWindowOuter*
nsGlobalWindowOuter::GetChildWindow(const nsAString& aName)
{
  nsCOMPtr<nsIDocShell> docShell(GetDocShell());
  NS_ENSURE_TRUE(docShell, nullptr);

  nsCOMPtr<nsIDocShellTreeItem> child;
  docShell->FindChildWithName(aName, false, true, nullptr, nullptr,
                              getter_AddRefs(child));

  return child ? child->GetWindow() : nullptr;
}

bool
nsGlobalWindowOuter::DispatchCustomEvent(const nsAString& aEventName)
{
  bool defaultActionEnabled = true;
  nsContentUtils::DispatchTrustedEvent(mDoc, ToSupports(this), aEventName,
                                       CanBubble::eYes, Cancelable::eYes,
                                       &defaultActionEnabled);

  return defaultActionEnabled;
}

bool
nsGlobalWindowOuter::DispatchResizeEvent(const CSSIntSize& aSize)
{
  ErrorResult res;
  RefPtr<Event> domEvent =
    mDoc->CreateEvent(NS_LITERAL_STRING("CustomEvent"), CallerType::System,
                      res);
  if (res.Failed()) {
    return false;
  }

  // We don't init the AutoJSAPI with ourselves because we don't want it
  // reporting errors to our onerror handlers.
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();
  JSAutoRealm ar(cx, GetWrapperPreserveColor());

  DOMWindowResizeEventDetail detail;
  detail.mWidth = aSize.width;
  detail.mHeight = aSize.height;
  JS::Rooted<JS::Value> detailValue(cx);
  if (!ToJSValue(cx, detail, &detailValue)) {
    return false;
  }

  CustomEvent* customEvent = static_cast<CustomEvent*>(domEvent.get());
  customEvent->InitCustomEvent(cx,
                               NS_LITERAL_STRING("DOMWindowResize"),
                               /* aCanBubble = */ true,
                               /* aCancelable = */ true,
                               detailValue);

  domEvent->SetTrusted(true);
  domEvent->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;

  nsCOMPtr<EventTarget> target = do_QueryInterface(GetOuterWindow());
  domEvent->SetTarget(target);

  return target->DispatchEvent(*domEvent, CallerType::System, IgnoreErrors());
}

static already_AddRefed<nsIDocShellTreeItem>
GetCallerDocShellTreeItem()
{
  nsCOMPtr<nsIWebNavigation> callerWebNav = do_GetInterface(GetEntryGlobal());
  nsCOMPtr<nsIDocShellTreeItem> callerItem = do_QueryInterface(callerWebNav);

  return callerItem.forget();
}

bool
nsGlobalWindowOuter::WindowExists(const nsAString& aName,
                                  bool aForceNoOpener,
                                  bool aLookForCallerOnJSStack)
{
  MOZ_ASSERT(mDocShell, "Must have docshell");

  if (aForceNoOpener) {
    return aName.LowerCaseEqualsLiteral("_self") ||
           aName.LowerCaseEqualsLiteral("_top") ||
           aName.LowerCaseEqualsLiteral("_parent");
  }

  nsCOMPtr<nsIDocShellTreeItem> caller;
  if (aLookForCallerOnJSStack) {
    caller = GetCallerDocShellTreeItem();
  }

  if (!caller) {
    caller = mDocShell;
  }

  nsCOMPtr<nsIDocShellTreeItem> namedItem;
  mDocShell->FindItemWithName(aName, nullptr, caller,
                              /* aSkipTabGroup = */ false,
                              getter_AddRefs(namedItem));
  return namedItem != nullptr;
}

already_AddRefed<nsIWidget>
nsGlobalWindowOuter::GetMainWidget()
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();

  nsCOMPtr<nsIWidget> widget;

  if (treeOwnerAsWin) {
    treeOwnerAsWin->GetMainWidget(getter_AddRefs(widget));
  }

  return widget.forget();
}

nsIWidget*
nsGlobalWindowOuter::GetNearestWidget() const
{
  nsIDocShell* docShell = GetDocShell();
  NS_ENSURE_TRUE(docShell, nullptr);
  nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
  NS_ENSURE_TRUE(presShell, nullptr);
  nsIFrame* rootFrame = presShell->GetRootFrame();
  NS_ENSURE_TRUE(rootFrame, nullptr);
  return rootFrame->GetView()->GetNearestWidget(nullptr);
}

void
nsGlobalWindowOuter::SetFullScreenOuter(bool aFullScreen, mozilla::ErrorResult& aError)
{
  aError = SetFullscreenInternal(FullscreenReason::ForFullscreenMode, aFullScreen);
}

nsresult
nsGlobalWindowOuter::SetFullScreen(bool aFullScreen)
{
  return SetFullscreenInternal(FullscreenReason::ForFullscreenMode, aFullScreen);
}

static void
FinishDOMFullscreenChange(nsIDocument* aDoc, bool aInDOMFullscreen)
{
  if (aInDOMFullscreen) {
    // Ask the document to handle any pending DOM fullscreen change.
    if (!nsIDocument::HandlePendingFullscreenRequests(aDoc)) {
      // If we don't end up having anything in fullscreen,
      // async request exiting fullscreen.
      nsIDocument::AsyncExitFullscreen(aDoc);
    }
  } else {
    // If the window is leaving fullscreen state, also ask the document
    // to exit from DOM Fullscreen.
    nsIDocument::ExitFullscreenInDocTree(aDoc);
  }
}

struct FullscreenTransitionDuration
{
  // The unit of the durations is millisecond
  uint16_t mFadeIn = 0;
  uint16_t mFadeOut = 0;
  bool IsSuppressed() const
  {
    return mFadeIn == 0 && mFadeOut == 0;
  }
};

static void
GetFullscreenTransitionDuration(bool aEnterFullscreen,
                                FullscreenTransitionDuration* aDuration)
{
  const char* pref = aEnterFullscreen ?
    "full-screen-api.transition-duration.enter" :
    "full-screen-api.transition-duration.leave";
  nsAutoCString prefValue;
  Preferences::GetCString(pref, prefValue);
  if (!prefValue.IsEmpty()) {
    sscanf(prefValue.get(), "%hu%hu",
           &aDuration->mFadeIn, &aDuration->mFadeOut);
  }
}

class FullscreenTransitionTask : public Runnable
{
public:
  FullscreenTransitionTask(const FullscreenTransitionDuration& aDuration,
                           nsGlobalWindowOuter* aWindow,
                           bool aFullscreen,
                           nsIWidget* aWidget,
                           nsIScreen* aScreen,
                           nsISupports* aTransitionData)
    : mozilla::Runnable("FullscreenTransitionTask")
    , mWindow(aWindow)
    , mWidget(aWidget)
    , mScreen(aScreen)
    , mTransitionData(aTransitionData)
    , mDuration(aDuration)
    , mStage(eBeforeToggle)
    , mFullscreen(aFullscreen)
  {
  }

  NS_IMETHOD Run() override;

private:
  ~FullscreenTransitionTask() override
  {
  }

  /**
   * The flow of fullscreen transition:
   *
   *         parent process         |         child process
   * ----------------------------------------------------------------
   *
   *                                    | request/exit fullscreen
   *                              <-----|
   *         BeforeToggle stage |
   *                            |
   *  ToggleFullscreen stage *1 |----->
   *                                    | HandleFullscreenRequests
   *                                    |
   *                              <-----| MozAfterPaint event
   *       AfterToggle stage *2 |
   *                            |
   *                  End stage |
   *
   * Note we also start a timer at *1 so that if we don't get MozAfterPaint
   * from the child process in time, we continue going to *2.
   */
  enum Stage {
    // BeforeToggle stage happens before we enter or leave fullscreen
    // state. In this stage, the task triggers the pre-toggle fullscreen
    // transition on the widget.
    eBeforeToggle,
    // ToggleFullscreen stage actually executes the fullscreen toggle,
    // and wait for the next paint on the content to continue.
    eToggleFullscreen,
    // AfterToggle stage happens after we toggle the fullscreen state.
    // In this stage, the task triggers the post-toggle fullscreen
    // transition on the widget.
    eAfterToggle,
    // End stage is triggered after the final transition finishes.
    eEnd
  };

  class Observer final : public nsIObserver
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    explicit Observer(FullscreenTransitionTask* aTask)
      : mTask(aTask) { }

  private:
    ~Observer() = default;

    RefPtr<FullscreenTransitionTask> mTask;
  };

  static const char* const kPaintedTopic;

  RefPtr<nsGlobalWindowOuter> mWindow;
  nsCOMPtr<nsIWidget> mWidget;
  nsCOMPtr<nsIScreen> mScreen;
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsISupports> mTransitionData;

  TimeStamp mFullscreenChangeStartTime;
  FullscreenTransitionDuration mDuration;
  Stage mStage;
  bool mFullscreen;
};

const char* const
FullscreenTransitionTask::kPaintedTopic = "fullscreen-painted";

NS_IMETHODIMP
FullscreenTransitionTask::Run()
{
  Stage stage = mStage;
  mStage = Stage(mStage + 1);
  if (MOZ_UNLIKELY(mWidget->Destroyed())) {
    // If the widget has been destroyed before we get here, don't try to
    // do anything more. Just let it go and release ourselves.
    NS_WARNING("The widget to fullscreen has been destroyed");
    return NS_OK;
  }
  if (stage == eBeforeToggle) {
    PROFILER_ADD_MARKER("Fullscreen transition start");
    mWidget->PerformFullscreenTransition(nsIWidget::eBeforeFullscreenToggle,
                                         mDuration.mFadeIn, mTransitionData,
                                         this);
  } else if (stage == eToggleFullscreen) {
    PROFILER_ADD_MARKER("Fullscreen toggle start");
    mFullscreenChangeStartTime = TimeStamp::Now();
    if (MOZ_UNLIKELY(mWindow->mFullScreen != mFullscreen)) {
      // This could happen in theory if several fullscreen requests in
      // different direction happen continuously in a short time. We
      // need to ensure the fullscreen state matches our target here,
      // otherwise the widget would change the window state as if we
      // toggle for Fullscreen Mode instead of Fullscreen API.
      NS_WARNING("The fullscreen state of the window does not match");
      mWindow->mFullScreen = mFullscreen;
    }
    // Toggle the fullscreen state on the widget
    if (!mWindow->SetWidgetFullscreen(FullscreenReason::ForFullscreenAPI,
                                      mFullscreen, mWidget, mScreen)) {
      // Fail to setup the widget, call FinishFullscreenChange to
      // complete fullscreen change directly.
      mWindow->FinishFullscreenChange(mFullscreen);
    }
    // Set observer for the next content paint.
    nsCOMPtr<nsIObserver> observer = new Observer(this);
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->AddObserver(observer, kPaintedTopic, false);
    // There are several edge cases where we may never get the paint
    // notification, including:
    // 1. the window/tab is closed before the next paint;
    // 2. the user has switched to another tab before we get here.
    // Completely fixing those cases seems to be tricky, and since they
    // should rarely happen, it probably isn't worth to fix. Hence we
    // simply add a timeout here to ensure we never hang forever.
    // In addition, if the page is complicated or the machine is less
    // powerful, layout could take a long time, in which case, staying
    // in black screen for that long could hurt user experience even
    // more than exposing an intermediate state.
    uint32_t timeout =
      Preferences::GetUint("full-screen-api.transition.timeout", 1000);
    NS_NewTimerWithObserver(getter_AddRefs(mTimer),
                            observer, timeout, nsITimer::TYPE_ONE_SHOT);
  } else if (stage == eAfterToggle) {
    Telemetry::AccumulateTimeDelta(Telemetry::FULLSCREEN_TRANSITION_BLACK_MS,
                                   mFullscreenChangeStartTime);
    mWidget->PerformFullscreenTransition(nsIWidget::eAfterFullscreenToggle,
                                         mDuration.mFadeOut, mTransitionData,
                                         this);
  } else if (stage == eEnd) {
    PROFILER_ADD_MARKER("Fullscreen transition end");
    mWidget->CleanupFullscreenTransition();
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(FullscreenTransitionTask::Observer, nsIObserver)

NS_IMETHODIMP
FullscreenTransitionTask::Observer::Observe(nsISupports* aSubject,
                                            const char* aTopic,
                                            const char16_t* aData)
{
  bool shouldContinue = false;
  if (strcmp(aTopic, FullscreenTransitionTask::kPaintedTopic) == 0) {
    nsCOMPtr<nsPIDOMWindowInner> win(do_QueryInterface(aSubject));
    nsCOMPtr<nsIWidget> widget = win ?
      nsGlobalWindowInner::Cast(win)->GetMainWidget() : nullptr;
    if (widget == mTask->mWidget) {
      // The paint notification arrives first. Cancel the timer.
      mTask->mTimer->Cancel();
      shouldContinue = true;
      PROFILER_ADD_MARKER("Fullscreen toggle end");
    }
  } else {
#ifdef DEBUG
    MOZ_ASSERT(strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC) == 0,
               "Should only get fullscreen-painted or timer-callback");
    nsCOMPtr<nsITimer> timer(do_QueryInterface(aSubject));
    MOZ_ASSERT(timer && timer == mTask->mTimer,
               "Should only trigger this with the timer the task created");
#endif
    shouldContinue = true;
    PROFILER_ADD_MARKER("Fullscreen toggle timeout");
  }
  if (shouldContinue) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->RemoveObserver(this, kPaintedTopic);
    mTask->mTimer = nullptr;
    mTask->Run();
  }
  return NS_OK;
}

static bool
MakeWidgetFullscreen(nsGlobalWindowOuter* aWindow, FullscreenReason aReason,
                     bool aFullscreen)
{
  nsCOMPtr<nsIWidget> widget = aWindow->GetMainWidget();
  if (!widget) {
    return false;
  }

  FullscreenTransitionDuration duration;
  bool performTransition = false;
  nsCOMPtr<nsISupports> transitionData;
  if (aReason == FullscreenReason::ForFullscreenAPI) {
    GetFullscreenTransitionDuration(aFullscreen, &duration);
    if (!duration.IsSuppressed()) {
      performTransition = widget->
        PrepareForFullscreenTransition(getter_AddRefs(transitionData));
    }
  }
  // We pass nullptr as the screen to SetWidgetFullscreen
  // and FullscreenTransitionTask, as we do not wish to override
  // the default screen selection behavior.  The screen containing
  // most of the widget will be selected.
  if (!performTransition) {
    return aWindow->SetWidgetFullscreen(aReason, aFullscreen, widget, nullptr);
  }
  nsCOMPtr<nsIRunnable> task =
    new FullscreenTransitionTask(duration, aWindow, aFullscreen,
                                 widget, nullptr, transitionData);
  task->Run();
  return true;
}

nsresult
nsGlobalWindowOuter::SetFullscreenInternal(FullscreenReason aReason,
                                           bool aFullScreen)
{
  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript(),
             "Requires safe to run script as it "
             "may call FinishDOMFullscreenChange");

  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  MOZ_ASSERT(aReason != FullscreenReason::ForForceExitFullscreen || !aFullScreen,
             "FullscreenReason::ForForceExitFullscreen can "
             "only be used with exiting fullscreen");

  // Only chrome can change our fullscreen mode. Otherwise, the state
  // can only be changed for DOM fullscreen.
  if (aReason == FullscreenReason::ForFullscreenMode &&
      !nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
    return NS_OK;
  }

  // SetFullScreen needs to be called on the root window, so get that
  // via the DocShell tree, and if we are not already the root,
  // call SetFullScreen on that window instead.
  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  mDocShell->GetRootTreeItem(getter_AddRefs(rootItem));
  nsCOMPtr<nsPIDOMWindowOuter> window = rootItem ? rootItem->GetWindow() : nullptr;
  if (!window)
    return NS_ERROR_FAILURE;
  if (rootItem != mDocShell)
    return window->SetFullscreenInternal(aReason, aFullScreen);

  // make sure we don't try to set full screen on a non-chrome window,
  // which might happen in embedding world
  if (mDocShell->ItemType() != nsIDocShellTreeItem::typeChrome)
    return NS_ERROR_FAILURE;

  // If we are already in full screen mode, just return.
  if (mFullScreen == aFullScreen)
    return NS_OK;

  // Note that although entering DOM fullscreen could also cause
  // consequential calls to this method, those calls will be skipped
  // at the condition above.
  if (aReason == FullscreenReason::ForFullscreenMode) {
    if (!aFullScreen && !mFullscreenMode) {
      // If we are exiting fullscreen mode, but we actually didn't
      // entered fullscreen mode, the fullscreen state was only for
      // the Fullscreen API. Change the reason here so that we can
      // perform transition for it.
      aReason = FullscreenReason::ForFullscreenAPI;
    } else {
      mFullscreenMode = aFullScreen;
    }
  } else {
    // If we are exiting from DOM fullscreen while we initially make
    // the window fullscreen because of fullscreen mode, don't restore
    // the window. But we still need to exit the DOM fullscreen state.
    if (!aFullScreen && mFullscreenMode) {
      FinishDOMFullscreenChange(mDoc, false);
      return NS_OK;
    }
  }

  // Prevent chrome documents which are still loading from resizing
  // the window after we set fullscreen mode.
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  nsCOMPtr<nsIXULWindow> xulWin(do_GetInterface(treeOwnerAsWin));
  if (aFullScreen && xulWin) {
    xulWin->SetIntrinsicallySized(false);
  }

  // Set this before so if widget sends an event indicating its
  // gone full screen, the state trap above works.
  mFullScreen = aFullScreen;

  // Sometimes we don't want the top-level widget to actually go fullscreen,
  // for example in the B2G desktop client, we don't want the emulated screen
  // dimensions to appear to increase when entering fullscreen mode; we just
  // want the content to fill the entire client area of the emulator window.
  if (!Preferences::GetBool("full-screen-api.ignore-widgets", false)) {
    if (MakeWidgetFullscreen(this, aReason, aFullScreen)) {
      // The rest of code for switching fullscreen is in nsGlobalWindowOuter::
      // FinishFullscreenChange() which will be called after sizemodechange
      // event is dispatched.
      return NS_OK;
    }
  }

  FinishFullscreenChange(aFullScreen);
  return NS_OK;
}

bool
nsGlobalWindowOuter::SetWidgetFullscreen(FullscreenReason aReason, bool aIsFullscreen,
                                         nsIWidget* aWidget, nsIScreen* aScreen)
{
  MOZ_ASSERT(this == GetTopInternal(), "Only topmost window should call this");
  MOZ_ASSERT(!GetFrameElementInternal(), "Content window should not call this");
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);

  if (!NS_WARN_IF(!IsChromeWindow())) {
    if (!NS_WARN_IF(mChromeFields.mFullscreenPresShell)) {
      if (nsIPresShell* shell = mDocShell->GetPresShell()) {
        if (nsRefreshDriver* rd = shell->GetRefreshDriver()) {
          mChromeFields.mFullscreenPresShell = do_GetWeakReference(shell);
          MOZ_ASSERT(mChromeFields.mFullscreenPresShell);
          rd->SetIsResizeSuppressed();
          rd->Freeze();
        }
      }
    }
  }
  nsresult rv = aReason == FullscreenReason::ForFullscreenMode ?
    // If we enter fullscreen for fullscreen mode, we want
    // the native system behavior.
    aWidget->MakeFullScreenWithNativeTransition(aIsFullscreen, aScreen) :
    aWidget->MakeFullScreen(aIsFullscreen, aScreen);
  return NS_SUCCEEDED(rv);
}

/* virtual */ void
nsGlobalWindowOuter::FullscreenWillChange(bool aIsFullscreen)
{
  if (aIsFullscreen) {
    DispatchCustomEvent(NS_LITERAL_STRING("willenterfullscreen"));
  } else {
    DispatchCustomEvent(NS_LITERAL_STRING("willexitfullscreen"));
  }
}

/* virtual */ void
nsGlobalWindowOuter::FinishFullscreenChange(bool aIsFullscreen)
{
  if (aIsFullscreen != mFullScreen) {
    NS_WARNING("Failed to toggle fullscreen state of the widget");
    // We failed to make the widget enter fullscreen.
    // Stop further changes and restore the state.
    if (!aIsFullscreen) {
      mFullScreen = false;
      mFullscreenMode = false;
    } else {
      MOZ_ASSERT_UNREACHABLE("Failed to exit fullscreen?");
      mFullScreen = true;
      // We don't know how code can reach here. Not sure
      // what value should be set for fullscreen mode.
      mFullscreenMode = false;
    }
    return;
  }

  // Note that we must call this to toggle the DOM fullscreen state
  // of the document before dispatching the "fullscreen" event, so
  // that the chrome can distinguish between browser fullscreen mode
  // and DOM fullscreen.
  FinishDOMFullscreenChange(mDoc, mFullScreen);

  // dispatch a "fullscreen" DOM event so that XUL apps can
  // respond visually if we are kicked into full screen mode
  DispatchCustomEvent(NS_LITERAL_STRING("fullscreen"));

  if (!NS_WARN_IF(!IsChromeWindow())) {
    if (nsCOMPtr<nsIPresShell> shell =
        do_QueryReferent(mChromeFields.mFullscreenPresShell)) {
      if (nsRefreshDriver* rd = shell->GetRefreshDriver()) {
        rd->Thaw();
      }
      mChromeFields.mFullscreenPresShell = nullptr;
    }
  }

  if (!mWakeLock && mFullScreen) {
    RefPtr<power::PowerManagerService> pmService =
      power::PowerManagerService::GetInstance();
    if (!pmService) {
      return;
    }

    // XXXkhuey using the inner here, do we need to do something if it changes?
    ErrorResult rv;
    mWakeLock = pmService->NewWakeLock(NS_LITERAL_STRING("DOM_Fullscreen"),
                                       GetCurrentInnerWindow(), rv);
    NS_WARNING_ASSERTION(!rv.Failed(), "Failed to lock the wakelock");
    rv.SuppressException();
  } else if (mWakeLock && !mFullScreen) {
    ErrorResult rv;
    mWakeLock->Unlock(rv);
    mWakeLock = nullptr;
    rv.SuppressException();
  }
}

bool
nsGlobalWindowOuter::FullScreen() const
{
  NS_ENSURE_TRUE(mDocShell, mFullScreen);

  // Get the fullscreen value of the root window, to always have the value
  // accurate, even when called from content.
  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  mDocShell->GetRootTreeItem(getter_AddRefs(rootItem));
  if (rootItem == mDocShell) {
    if (!XRE_IsContentProcess()) {
      // We are the root window. Return our internal value.
      return mFullScreen;
    }
    if (nsCOMPtr<nsIWidget> widget = GetNearestWidget()) {
      // We are in content process, figure out the value from
      // the sizemode of the puppet widget.
      return widget->SizeMode() == nsSizeMode_Fullscreen;
    }
    return false;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = rootItem->GetWindow();
  NS_ENSURE_TRUE(window, mFullScreen);

  return nsGlobalWindowOuter::Cast(window)->FullScreen();
}

bool
nsGlobalWindowOuter::GetFullScreenOuter()
{
  return FullScreen();
}

bool
nsGlobalWindowOuter::GetFullScreen()
{
  FORWARD_TO_INNER(GetFullScreen, (), false);
}

void
nsGlobalWindowOuter::EnsureReflowFlushAndPaint()
{
  NS_ASSERTION(mDocShell, "EnsureReflowFlushAndPaint() called with no "
               "docshell!");

  if (!mDocShell)
    return;

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();

  if (!presShell)
    return;

  // Flush pending reflows.
  if (mDoc) {
    mDoc->FlushPendingNotifications(FlushType::Layout);
  }

  // Unsuppress painting.
  presShell->UnsuppressPainting();
}

// static
void
nsGlobalWindowOuter::MakeScriptDialogTitle(nsAString& aOutTitle,
                                           nsIPrincipal* aSubjectPrincipal)
{
  MOZ_ASSERT(aSubjectPrincipal);

  aOutTitle.Truncate();

  // Try to get a host from the running principal -- this will do the
  // right thing for javascript: and data: documents.

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aSubjectPrincipal->GetURI(getter_AddRefs(uri));
  // Note - The check for the current JSContext here isn't necessarily sensical.
  // It's just designed to preserve existing behavior during a mass-conversion
  // patch.
  if (NS_SUCCEEDED(rv) && uri && nsContentUtils::GetCurrentJSContext()) {
    // remove user:pass for privacy and spoof prevention

    nsCOMPtr<nsIURIFixup> fixup(do_GetService(NS_URIFIXUP_CONTRACTID));
    if (fixup) {
      nsCOMPtr<nsIURI> fixedURI;
      rv = fixup->CreateExposableURI(uri, getter_AddRefs(fixedURI));
      if (NS_SUCCEEDED(rv) && fixedURI) {
        nsAutoCString host;
        fixedURI->GetHost(host);

        if (!host.IsEmpty()) {
          // if this URI has a host we'll show it. For other
          // schemes (e.g. file:) we fall back to the localized
          // generic string

          nsAutoCString prepath;
          fixedURI->GetDisplayPrePath(prepath);

          NS_ConvertUTF8toUTF16 ucsPrePath(prepath);
          const char16_t *formatStrings[] = { ucsPrePath.get() };
          nsContentUtils::FormatLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                                "ScriptDlgHeading",
                                                formatStrings,
                                                aOutTitle);
        }
      }
    }
  }

  if (aOutTitle.IsEmpty()) {
    // We didn't find a host so use the generic heading
    nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                       "ScriptDlgGenericHeading",
                                       aOutTitle);
  }

  // Just in case
  if (aOutTitle.IsEmpty()) {
    NS_WARNING("could not get ScriptDlgGenericHeading string from string bundle");
    aOutTitle.AssignLiteral("[Script]");
  }
}

bool
nsGlobalWindowOuter::CanMoveResizeWindows(CallerType aCallerType)
{
  // When called from chrome, we can avoid the following checks.
  if (aCallerType != CallerType::System) {
    // Don't allow scripts to move or resize windows that were not opened by a
    // script.
    if (!mHadOriginalOpener) {
      return false;
    }

    if (!CanSetProperty("dom.disable_window_move_resize")) {
      return false;
    }

    // Ignore the request if we have more than one tab in the window.
    uint32_t itemCount = 0;
    if (XRE_IsContentProcess()) {
      nsCOMPtr<nsIDocShell> docShell = GetDocShell();
      if (docShell) {
        nsCOMPtr<nsITabChild> child = docShell->GetTabChild();
        if (child) {
          child->SendGetTabCount(&itemCount);
        }
      }
    } else {
      nsCOMPtr<nsIDocShellTreeOwner> treeOwner = GetTreeOwner();
      if (!treeOwner || NS_FAILED(treeOwner->GetTabCount(&itemCount))) {
        itemCount = 0;
      }
    }
    if (itemCount > 1) {
      return false;
    }
  }

  if (mDocShell) {
    bool allow;
    nsresult rv = mDocShell->GetAllowWindowControl(&allow);
    if (NS_SUCCEEDED(rv) && !allow)
      return false;
  }

  if (nsGlobalWindowInner::sMouseDown && !nsGlobalWindowInner::sDragServiceDisabled) {
    nsCOMPtr<nsIDragService> ds =
      do_GetService("@mozilla.org/widget/dragservice;1");
    if (ds) {
      nsGlobalWindowInner::sDragServiceDisabled = true;
      ds->Suppress();
    }
  }
  return true;
}

bool
nsGlobalWindowOuter::AlertOrConfirm(bool aAlert,
                                    const nsAString& aMessage,
                                    nsIPrincipal& aSubjectPrincipal,
                                    ErrorResult& aError)
{
  // XXX This method is very similar to nsGlobalWindowOuter::Prompt, make
  // sure any modifications here don't need to happen over there!
  if (!AreDialogsEnabled()) {
    // Just silently return.  In the case of alert(), the return value is
    // ignored.  In the case of confirm(), returning false is the same thing as
    // would happen if the user cancels.
    return false;
  }

  // Reset popup state while opening a modal dialog, and firing events
  // about the dialog, to prevent the current state from being active
  // the whole time a modal dialog is open.
  nsAutoPopupStatePusher popupStatePusher(openAbused, true);

  // Before bringing up the window, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  nsAutoString title;
  MakeScriptDialogTitle(title, &aSubjectPrincipal);

  // Remove non-terminating null characters from the
  // string. See bug #310037.
  nsAutoString final;
  nsContentUtils::StripNullChars(aMessage, final);

  nsresult rv;
  nsCOMPtr<nsIPromptFactory> promptFac =
    do_GetService("@mozilla.org/prompter;1", &rv);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return false;
  }

  nsCOMPtr<nsIPrompt> prompt;
  aError = promptFac->GetPrompt(this, NS_GET_IID(nsIPrompt),
                                getter_AddRefs(prompt));
  if (aError.Failed()) {
    return false;
  }

  // Always allow tab modal prompts for alert and confirm.
  if (nsCOMPtr<nsIWritablePropertyBag2> promptBag = do_QueryInterface(prompt)) {
    promptBag->SetPropertyAsBool(NS_LITERAL_STRING("allowTabModal"), true);
  }

  bool result = false;
  nsAutoSyncOperation sync(mDoc);
  if (ShouldPromptToBlockDialogs()) {
    bool disallowDialog = false;
    nsAutoString label;
    nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                       "ScriptDialogLabel", label);

    aError = aAlert ?
               prompt->AlertCheck(title.get(), final.get(), label.get(),
                                  &disallowDialog) :
               prompt->ConfirmCheck(title.get(), final.get(), label.get(),
                                    &disallowDialog, &result);

    if (disallowDialog)
      DisableDialogs();
  } else {
    aError = aAlert ?
               prompt->Alert(title.get(), final.get()) :
               prompt->Confirm(title.get(), final.get(), &result);
  }

  return result;
}

void
nsGlobalWindowOuter::AlertOuter(const nsAString& aMessage,
                                nsIPrincipal& aSubjectPrincipal,
                                ErrorResult& aError)
{
  AlertOrConfirm(/* aAlert = */ true, aMessage, aSubjectPrincipal, aError);
}

bool
nsGlobalWindowOuter::ConfirmOuter(const nsAString& aMessage,
                                  nsIPrincipal& aSubjectPrincipal,
                                  ErrorResult& aError)
{
  return AlertOrConfirm(/* aAlert = */ false, aMessage, aSubjectPrincipal,
                        aError);
}

void
nsGlobalWindowOuter::PromptOuter(const nsAString& aMessage,
                                 const nsAString& aInitial,
                                 nsAString& aReturn,
                                 nsIPrincipal& aSubjectPrincipal,
                                 ErrorResult& aError)
{
  // XXX This method is very similar to nsGlobalWindowOuter::AlertOrConfirm, make
  // sure any modifications here don't need to happen over there!
  SetDOMStringToNull(aReturn);

  if (!AreDialogsEnabled()) {
    // Return null, as if the user just canceled the prompt.
    return;
  }

  // Reset popup state while opening a modal dialog, and firing events
  // about the dialog, to prevent the current state from being active
  // the whole time a modal dialog is open.
  nsAutoPopupStatePusher popupStatePusher(openAbused, true);

  // Before bringing up the window, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  nsAutoString title;
  MakeScriptDialogTitle(title, &aSubjectPrincipal);

  // Remove non-terminating null characters from the
  // string. See bug #310037.
  nsAutoString fixedMessage, fixedInitial;
  nsContentUtils::StripNullChars(aMessage, fixedMessage);
  nsContentUtils::StripNullChars(aInitial, fixedInitial);

  nsresult rv;
  nsCOMPtr<nsIPromptFactory> promptFac =
    do_GetService("@mozilla.org/prompter;1", &rv);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }

  nsCOMPtr<nsIPrompt> prompt;
  aError = promptFac->GetPrompt(this, NS_GET_IID(nsIPrompt),
                                getter_AddRefs(prompt));
  if (aError.Failed()) {
    return;
  }

  // Always allow tab modal prompts for prompt.
  if (nsCOMPtr<nsIWritablePropertyBag2> promptBag = do_QueryInterface(prompt)) {
    promptBag->SetPropertyAsBool(NS_LITERAL_STRING("allowTabModal"), true);
  }

  // Pass in the default value, if any.
  char16_t *inoutValue = ToNewUnicode(fixedInitial);
  bool disallowDialog = false;

  nsAutoString label;
  label.SetIsVoid(true);
  if (ShouldPromptToBlockDialogs()) {
    nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                       "ScriptDialogLabel", label);
  }

  nsAutoSyncOperation sync(mDoc);
  bool ok;
  aError = prompt->Prompt(title.get(), fixedMessage.get(), &inoutValue,
                          label.IsVoid() ? nullptr : label.get(),
                          &disallowDialog, &ok);

  if (disallowDialog) {
    DisableDialogs();
  }

  if (aError.Failed()) {
    return;
  }

  nsString outValue;
  outValue.Adopt(inoutValue);

  if (ok && inoutValue) {
    aReturn.Assign(outValue);
  }
}

void
nsGlobalWindowOuter::FocusOuter(ErrorResult& aError)
{
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return;
  }

  nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(mDocShell);

  bool isVisible = false;
  if (baseWin) {
    baseWin->GetVisibility(&isVisible);
  }

  if (!isVisible) {
    // A hidden tab is being focused, ignore this call.
    return;
  }

  nsCOMPtr<nsPIDOMWindowInner> caller = do_QueryInterface(GetEntryGlobal());
  nsPIDOMWindowOuter* callerOuter = caller ? caller->GetOuterWindow() : nullptr;
  nsCOMPtr<nsPIDOMWindowOuter> opener = GetOpener();

  // Enforce dom.disable_window_flip (for non-chrome), but still allow the
  // window which opened us to raise us at times when popups are allowed
  // (bugs 355482 and 369306).
  bool canFocus = CanSetProperty("dom.disable_window_flip") ||
                    (opener == callerOuter &&
                     RevisePopupAbuseLevel(GetPopupControlState()) < openBlocked);

  nsCOMPtr<mozIDOMWindowProxy> activeDOMWindow;
  fm->GetActiveWindow(getter_AddRefs(activeDOMWindow));

  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  mDocShell->GetRootTreeItem(getter_AddRefs(rootItem));
  nsCOMPtr<nsPIDOMWindowOuter> rootWin = rootItem ? rootItem->GetWindow() : nullptr;
  auto* activeWindow = nsPIDOMWindowOuter::From(activeDOMWindow);
  bool isActive = (rootWin == activeWindow);

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (treeOwnerAsWin && (canFocus || isActive)) {
    bool isEnabled = true;
    if (NS_SUCCEEDED(treeOwnerAsWin->GetEnabled(&isEnabled)) && !isEnabled) {
      NS_WARNING( "Should not try to set the focus on a disabled window" );
      return;
    }

    // XXXndeakin not sure what this is for or if it should go somewhere else
    nsCOMPtr<nsIEmbeddingSiteWindow> embeddingWin(do_GetInterface(treeOwnerAsWin));
    if (embeddingWin)
      embeddingWin->SetFocus();
  }

  if (!mDocShell) {
    return;
  }

  nsCOMPtr<nsIPresShell> presShell;
  // Don't look for a presshell if we're a root chrome window that's got
  // about:blank loaded.  We don't want to focus our widget in that case.
  // XXXbz should we really be checking for IsInitialDocument() instead?
  bool lookForPresShell = true;
  if (mDocShell->ItemType() == nsIDocShellTreeItem::typeChrome &&
      GetPrivateRoot() == this && mDoc) {
    nsIURI* ourURI = mDoc->GetDocumentURI();
    if (ourURI) {
      lookForPresShell = !NS_IsAboutBlank(ourURI);
    }
  }

  if (lookForPresShell) {
    mDocShell->GetEldestPresShell(getter_AddRefs(presShell));
  }

  nsCOMPtr<nsIDocShellTreeItem> parentDsti;
  mDocShell->GetParent(getter_AddRefs(parentDsti));

  // set the parent's current focus to the frame containing this window.
  nsCOMPtr<nsPIDOMWindowOuter> parent =
    parentDsti ? parentDsti->GetWindow() : nullptr;
  if (parent) {
    nsCOMPtr<nsIDocument> parentdoc = parent->GetDoc();
    if (!parentdoc) {
      return;
    }

    RefPtr<Element> frame = parentdoc->FindContentForSubDocument(mDoc);
    if (frame) {
      uint32_t flags = nsIFocusManager::FLAG_NOSCROLL;
      if (canFocus)
        flags |= nsIFocusManager::FLAG_RAISE;
      aError = fm->SetFocus(frame, flags);
    }
    return;
  }

  if (canFocus) {
    // if there is no parent, this must be a toplevel window, so raise the
    // window if canFocus is true. If this is a child process, the raise
    // window request will get forwarded to the parent by the puppet widget.
    aError = fm->SetActiveWindow(this);
  }
}

nsresult
nsGlobalWindowOuter::Focus()
{
  FORWARD_TO_INNER(Focus, (), NS_ERROR_UNEXPECTED);
}

void
nsGlobalWindowOuter::BlurOuter()
{
  // If dom.disable_window_flip == true, then content should not be allowed
  // to call this function (this would allow popunders, bug 369306)
  if (!CanSetProperty("dom.disable_window_flip")) {
    return;
  }

  // If embedding apps don't implement nsIEmbeddingSiteWindow, we
  // shouldn't throw exceptions to web content.

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner = GetTreeOwner();
  nsCOMPtr<nsIEmbeddingSiteWindow> siteWindow(do_GetInterface(treeOwner));
  if (siteWindow) {
    // This method call may cause mDocShell to become nullptr.
    siteWindow->Blur();

    // if the root is focused, clear the focus
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm && mDoc) {
      RefPtr<Element> element;
      fm->GetFocusedElementForWindow(this, false, nullptr, getter_AddRefs(element));
      if (element == mDoc->GetRootElement()) {
        fm->ClearFocus(this);
      }
    }
  }
}

void
nsGlobalWindowOuter::BackOuter(ErrorResult& aError)
{
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  if (!webNav) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  aError = webNav->GoBack();
}

void
nsGlobalWindowOuter::ForwardOuter(ErrorResult& aError)
{
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  if (!webNav) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  aError = webNav->GoForward();
}

void
nsGlobalWindowOuter::HomeOuter(nsIPrincipal& aSubjectPrincipal, ErrorResult& aError)
{
  if (!mDocShell) {
    return;
  }

  nsAutoString homeURL;
  Preferences::GetLocalizedString(PREF_BROWSER_STARTUP_HOMEPAGE, homeURL);

  if (homeURL.IsEmpty()) {
    // if all else fails, use this
#ifdef DEBUG_seth
    printf("all else failed.  using %s as the home page\n", DEFAULT_HOME_PAGE);
#endif
    homeURL = NS_LITERAL_STRING(DEFAULT_HOME_PAGE);
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
    int32_t firstPipe = homeURL.FindChar('|');

    if (firstPipe > 0) {
      homeURL.Truncate(firstPipe);
    }
  }
#endif

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  if (!webNav) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  aError = webNav->LoadURI(homeURL.get(),
                           nsIWebNavigation::LOAD_FLAGS_NONE,
                           nullptr,
                           nullptr,
                           nullptr,
                           &aSubjectPrincipal);
}

void
nsGlobalWindowOuter::StopOuter(ErrorResult& aError)
{
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  if (webNav) {
    aError = webNav->Stop(nsIWebNavigation::STOP_ALL);
  }
}

void
nsGlobalWindowOuter::PrintOuter(ErrorResult& aError)
{
#ifdef NS_PRINTING
  if (!AreDialogsEnabled()) {
    // We probably want to keep throwing here; silently doing nothing is a bit
    // weird given the typical use cases of print().
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  if (ShouldPromptToBlockDialogs() && !ConfirmDialogIfNeeded()) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint;
  if (NS_SUCCEEDED(GetInterface(NS_GET_IID(nsIWebBrowserPrint),
                                getter_AddRefs(webBrowserPrint)))) {
    nsAutoSyncOperation sync(GetCurrentInnerWindowInternal() ?
                               GetCurrentInnerWindowInternal()->mDoc.get() :
                               nullptr);

    nsCOMPtr<nsIPrintSettingsService> printSettingsService =
      do_GetService("@mozilla.org/gfx/printsettings-service;1");

    nsCOMPtr<nsIPrintSettings> printSettings;
    if (printSettingsService) {
      bool printSettingsAreGlobal =
        Preferences::GetBool("print.use_global_printsettings", false);

      if (printSettingsAreGlobal) {
        printSettingsService->GetGlobalPrintSettings(getter_AddRefs(printSettings));

        nsAutoString printerName;
        printSettings->GetPrinterName(printerName);

        bool shouldGetDefaultPrinterName = printerName.IsEmpty();
#ifdef MOZ_X11
        // In Linux, GTK backend does not support per printer settings.
        // Calling GetDefaultPrinterName causes a sandbox violation (see Bug 1329216).
        // The printer name is not needed anywhere else on Linux before it gets to the parent.
        // In the parent, we will then query the default printer name if no name is set.
        // Unless we are in the parent, we will skip this part.
        if (!XRE_IsParentProcess()) {
          shouldGetDefaultPrinterName = false;
        }
#endif
        if (shouldGetDefaultPrinterName) {
          printSettingsService->GetDefaultPrinterName(printerName);
          printSettings->SetPrinterName(printerName);
        }
        printSettingsService->InitPrintSettingsFromPrinter(printerName,
                                                           printSettings);
        printSettingsService->InitPrintSettingsFromPrefs(printSettings,
                                                         true,
                                                         nsIPrintSettings::kInitSaveAll);
      } else {
        printSettingsService->GetNewPrintSettings(getter_AddRefs(printSettings));
      }

      EnterModalState();
      webBrowserPrint->Print(printSettings, nullptr);
      LeaveModalState();

      bool savePrintSettings =
        Preferences::GetBool("print.save_print_settings", false);
      if (printSettingsAreGlobal && savePrintSettings) {
        printSettingsService->
          SavePrintSettingsToPrefs(printSettings,
                                   true,
                                   nsIPrintSettings::kInitSaveAll);
        printSettingsService->
          SavePrintSettingsToPrefs(printSettings,
                                   false,
                                   nsIPrintSettings::kInitSavePrinterName);
      }
    } else {
      webBrowserPrint->GetGlobalPrintSettings(getter_AddRefs(printSettings));
      webBrowserPrint->Print(printSettings, nullptr);
    }
  }
#endif //NS_PRINTING
}

void
nsGlobalWindowOuter::MoveToOuter(int32_t aXPos, int32_t aYPos,
                                 CallerType aCallerType, ErrorResult& aError)
{
  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.moveTo() by exiting early
   */

  if (!CanMoveResizeWindows(aCallerType) || IsFrame()) {
    return;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsIScreenManager> screenMgr =
    do_GetService("@mozilla.org/gfx/screenmanager;1");
  nsCOMPtr<nsIScreen> screen;
  if (screenMgr) {
    CSSIntSize size;
    GetInnerSize(size);
    screenMgr->ScreenForRect(aXPos, aYPos, size.width, size.height,
                             getter_AddRefs(screen));
  }

  if (screen) {
    // On secondary displays, the "CSS px" coordinates are offset so that they
    // share their origin with global desktop pixels, to avoid ambiguities in
    // the coordinate space when there are displays with different DPIs.
    // (See the corresponding code in GetScreenXY() above.)
    int32_t screenLeftDeskPx, screenTopDeskPx, w, h;
    screen->GetRectDisplayPix(&screenLeftDeskPx, &screenTopDeskPx, &w, &h);
    CSSIntPoint cssPos(aXPos - screenLeftDeskPx, aYPos - screenTopDeskPx);
    CheckSecurityLeftAndTop(&cssPos.x, &cssPos.y, aCallerType);

    double scale;
    screen->GetDefaultCSSScaleFactor(&scale);
    LayoutDevicePoint devPos = cssPos * CSSToLayoutDeviceScale(scale);

    screen->GetContentsScaleFactor(&scale);
    DesktopPoint deskPos = devPos / DesktopToLayoutDeviceScale(scale);
    aError = treeOwnerAsWin->SetPositionDesktopPix(screenLeftDeskPx + deskPos.x,
                                                   screenTopDeskPx + deskPos.y);
  } else {
    // We couldn't find a screen? Just assume a 1:1 mapping.
    CSSIntPoint cssPos(aXPos, aXPos);
    CheckSecurityLeftAndTop(&cssPos.x, &cssPos.y, aCallerType);
    LayoutDevicePoint devPos = cssPos * CSSToLayoutDeviceScale(1.0);
    aError = treeOwnerAsWin->SetPosition(devPos.x, devPos.y);
  }

  CheckForDPIChange();
}

void
nsGlobalWindowOuter::MoveByOuter(int32_t aXDif, int32_t aYDif,
                                 CallerType aCallerType, ErrorResult& aError)
{
  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.moveBy() by exiting early
   */

  if (!CanMoveResizeWindows(aCallerType) || IsFrame()) {
    return;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  // To do this correctly we have to convert what we get from GetPosition
  // into CSS pixels, add the arguments, do the security check, and
  // then convert back to device pixels for the call to SetPosition.

  int32_t x, y;
  aError = treeOwnerAsWin->GetPosition(&x, &y);
  if (aError.Failed()) {
    return;
  }

  // mild abuse of a "size" object so we don't need more helper functions
  nsIntSize cssPos(DevToCSSIntPixels(nsIntSize(x, y)));

  cssPos.width += aXDif;
  cssPos.height += aYDif;

  CheckSecurityLeftAndTop(&cssPos.width, &cssPos.height, aCallerType);

  nsIntSize newDevPos(CSSToDevIntPixels(cssPos));

  aError = treeOwnerAsWin->SetPosition(newDevPos.width, newDevPos.height);

  CheckForDPIChange();
}

nsresult
nsGlobalWindowOuter::MoveBy(int32_t aXDif, int32_t aYDif)
{
  ErrorResult rv;
  MoveByOuter(aXDif, aYDif, CallerType::System, rv);

  return rv.StealNSResult();
}

void
nsGlobalWindowOuter::ResizeToOuter(int32_t aWidth, int32_t aHeight,
                                   CallerType aCallerType,
                                   ErrorResult& aError)
{
  /*
   * If caller is a browser-element then dispatch a resize event to
   * the embedder.
   */
  if (mDocShell && mDocShell->GetIsMozBrowser()) {
    CSSIntSize size(aWidth, aHeight);
    if (!DispatchResizeEvent(size)) {
      // The embedder chose to prevent the default action for this
      // event, so let's not resize this window after all...
      return;
    }
  }

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.resizeTo() by exiting early
   */

  if (!CanMoveResizeWindows(aCallerType) || IsFrame()) {
    return;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsIntSize cssSize(aWidth, aHeight);
  CheckSecurityWidthAndHeight(&cssSize.width, &cssSize.height, aCallerType);

  nsIntSize devSz(CSSToDevIntPixels(cssSize));

  aError = treeOwnerAsWin->SetSize(devSz.width, devSz.height, true);

  CheckForDPIChange();
}

void
nsGlobalWindowOuter::ResizeByOuter(int32_t aWidthDif, int32_t aHeightDif,
                                   CallerType aCallerType, ErrorResult& aError)
{
  /*
   * If caller is a browser-element then dispatch a resize event to
   * parent.
   */
  if (mDocShell && mDocShell->GetIsMozBrowser()) {
    CSSIntSize size;
    if (NS_FAILED(GetInnerSize(size))) {
      return;
    }

    size.width += aWidthDif;
    size.height += aHeightDif;

    if (!DispatchResizeEvent(size)) {
      // The embedder chose to prevent the default action for this
      // event, so let's not resize this window after all...
      return;
    }
  }

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.resizeBy() by exiting early
   */

  if (!CanMoveResizeWindows(aCallerType) || IsFrame()) {
    return;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  int32_t width, height;
  aError = treeOwnerAsWin->GetSize(&width, &height);
  if (aError.Failed()) {
    return;
  }

  // To do this correctly we have to convert what we got from GetSize
  // into CSS pixels, add the arguments, do the security check, and
  // then convert back to device pixels for the call to SetSize.

  nsIntSize cssSize(DevToCSSIntPixels(nsIntSize(width, height)));

  cssSize.width += aWidthDif;
  cssSize.height += aHeightDif;

  CheckSecurityWidthAndHeight(&cssSize.width, &cssSize.height, aCallerType);

  nsIntSize newDevSize(CSSToDevIntPixels(cssSize));

  aError = treeOwnerAsWin->SetSize(newDevSize.width, newDevSize.height, true);

  CheckForDPIChange();
}

void
nsGlobalWindowOuter::SizeToContentOuter(CallerType aCallerType, ErrorResult& aError)
{
  if (!mDocShell) {
    return;
  }

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.sizeToContent() by exiting early
   */

  if (!CanMoveResizeWindows(aCallerType) || IsFrame()) {
    return;
  }

  // The content viewer does a check to make sure that it's a content
  // viewer for a toplevel docshell.
  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  if (!cv) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  int32_t width, height;
  aError = cv->GetContentSize(&width, &height);
  if (aError.Failed()) {
    return;
  }

  // Make sure the new size is following the CheckSecurityWidthAndHeight
  // rules.
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner = GetTreeOwner();
  if (!treeOwner) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsIntSize cssSize(DevToCSSIntPixels(nsIntSize(width, height)));
  CheckSecurityWidthAndHeight(&cssSize.width, &cssSize.height, aCallerType);

  nsIntSize newDevSize(CSSToDevIntPixels(cssSize));

  aError = treeOwner->SizeShellTo(mDocShell, newDevSize.width,
                                  newDevSize.height);
}

already_AddRefed<nsPIWindowRoot>
nsGlobalWindowOuter::GetTopWindowRoot()
{
  nsPIDOMWindowOuter* piWin = GetPrivateRoot();
  if (!piWin) {
    return nullptr;
  }

  nsCOMPtr<nsPIWindowRoot> window = do_QueryInterface(piWin->GetChromeEventHandler());
  return window.forget();
}

void
nsGlobalWindowOuter::FirePopupBlockedEvent(nsIDocument* aDoc,
                                           nsIURI* aPopupURI,
                                           const nsAString& aPopupWindowName,
                                           const nsAString& aPopupWindowFeatures)
{
  MOZ_ASSERT(aDoc);

  // Fire a "DOMPopupBlocked" event so that the UI can hear about
  // blocked popups.
  PopupBlockedEventInit init;
  init.mBubbles = true;
  init.mCancelable = true;
  // XXX: This is a different object, but webidl requires an inner window here
  // now.
  init.mRequestingWindow = GetCurrentInnerWindowInternal();
  init.mPopupWindowURI = aPopupURI;
  init.mPopupWindowName = aPopupWindowName;
  init.mPopupWindowFeatures = aPopupWindowFeatures;

  RefPtr<PopupBlockedEvent> event =
    PopupBlockedEvent::Constructor(aDoc,
                                   NS_LITERAL_STRING("DOMPopupBlocked"),
                                   init);

  event->SetTrusted(true);

  aDoc->DispatchEvent(*event);
}

void
nsGlobalWindowOuter::NotifyContentBlockingState(unsigned aState,
                                                nsIChannel* aChannel)
{
  nsCOMPtr<nsIDocShell> docShell = GetDocShell();
  if (!docShell) {
    return;
  }
  nsCOMPtr<nsIDocument> doc = docShell->GetDocument();
  NS_ENSURE_TRUE_VOID(doc);

  // This event might come after the user has navigated to another page.
  // To prevent showing the TrackingProtection UI on the wrong page, we need to
  // check that the loading URI for the channel is the same as the URI currently
  // loaded in the document.
  if (!SameLoadingURI(doc, aChannel)) {
    return;
  }

  // Notify nsIWebProgressListeners of this security event.
  // Can be used to change the UI state.
  nsresult rv = NS_OK;
  nsCOMPtr<nsISecurityEventSink> eventSink = do_QueryInterface(docShell, &rv);
  NS_ENSURE_SUCCESS_VOID(rv);
  uint32_t state = 0;
  nsCOMPtr<nsISecureBrowserUI> securityUI;
  docShell->GetSecurityUI(getter_AddRefs(securityUI));
  if (!securityUI) {
    return;
  }
  securityUI->GetState(&state);
  if (aState == nsIWebProgressListener::STATE_BLOCKED_TRACKING_CONTENT) {
    doc->SetHasTrackingContentBlocked(true);
  } else if (aState == nsIWebProgressListener::STATE_BLOCKED_TRACKING_COOKIES) {
    doc->SetHasTrackingCookiesBlocked(true);
  } else {
    // Ignore nsIWebProgressListener::STATE_BLOCKED_UNSAFE_CONTENT;
  }
  state |= aState;

  eventSink->OnSecurityChange(aChannel, state);
}

//static
bool
nsGlobalWindowOuter::SameLoadingURI(nsIDocument *aDoc, nsIChannel *aChannel)
{
  nsCOMPtr<nsIURI> docURI = aDoc->GetDocumentURI();
  nsCOMPtr<nsILoadInfo> channelLoadInfo = aChannel->GetLoadInfo();
  if (!channelLoadInfo || !docURI) {
    return false;
  }

  nsCOMPtr<nsIPrincipal> channelLoadingPrincipal = channelLoadInfo->LoadingPrincipal();
  if (!channelLoadingPrincipal) {
    // TYPE_DOCUMENT loads will not have a channelLoadingPrincipal. But top level
    // loads should not be blocked by Tracking Protection, so we will return
    // false
    return false;
  }
  nsCOMPtr<nsIURI> channelLoadingURI;
  channelLoadingPrincipal->GetURI(getter_AddRefs(channelLoadingURI));
  if (!channelLoadingURI) {
    return false;
  }
  bool equals = false;
  nsresult rv = docURI->EqualsExceptRef(channelLoadingURI, &equals);
  return NS_SUCCEEDED(rv) && equals;
}

// static
bool
nsGlobalWindowOuter::CanSetProperty(const char *aPrefName)
{
  // Chrome can set any property.
  if (nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
    return true;
  }

  // If the pref is set to true, we can not set the property
  // and vice versa.
  return !Preferences::GetBool(aPrefName, true);
}

bool
nsGlobalWindowOuter::PopupWhitelisted()
{
  if (mDoc && nsContentUtils::CanShowPopup(mDoc->NodePrincipal()))
  {
    return true;
  }

  nsCOMPtr<nsPIDOMWindowOuter> parent = GetParent();
  if (parent == this)
  {
    return false;
  }

  return nsGlobalWindowOuter::Cast(parent)->PopupWhitelisted();
}

/*
 * Examine the current document state to see if we're in a way that is
 * typically abused by web designers. The window.open code uses this
 * routine to determine whether to allow the new window.
 * Returns a value from the PopupControlState enum.
 */
PopupControlState
nsGlobalWindowOuter::RevisePopupAbuseLevel(PopupControlState aControl)
{
  NS_ASSERTION(mDocShell, "Must have docshell");

  if (mDocShell->ItemType() != nsIDocShellTreeItem::typeContent) {
    return openAllowed;
  }

  PopupControlState abuse = aControl;
  switch (abuse) {
  case openControlled:
  case openBlocked:
  case openOverridden:
    if (PopupWhitelisted())
      abuse = PopupControlState(abuse - 1);
    break;
  case openAbused:
    if (PopupWhitelisted())
      //Skip openBlocked
      abuse = openControlled;
    break;
  case openAllowed: break;
  default:
    NS_WARNING("Strange PopupControlState!");
  }

  // limit the number of simultaneously open popups
  if (abuse == openAbused ||
      abuse == openBlocked ||
      abuse == openControlled) {
    int32_t popupMax = Preferences::GetInt("dom.popup_maximum", -1);
    if (popupMax >= 0 && gOpenPopupSpamCount >= popupMax)
      abuse = openOverridden;
  }

  return abuse;
}

/* If a window open is blocked, fire the appropriate DOM events. */
void
nsGlobalWindowOuter::FireAbuseEvents(const nsAString &aPopupURL,
                                     const nsAString &aPopupWindowName,
                                     const nsAString &aPopupWindowFeatures)
{
  // fetch the URI of the window requesting the opened window

  nsCOMPtr<nsPIDOMWindowOuter> window = GetTop();
  if (!window) {
    return;
  }

  nsCOMPtr<nsIDocument> topDoc = window->GetDoc();
  nsCOMPtr<nsIURI> popupURI;

  // build the URI of the would-have-been popup window
  // (see nsWindowWatcher::URIfromURL)

  // first, fetch the opener's base URI

  nsIURI *baseURL = nullptr;

  nsCOMPtr<nsIDocument> doc = GetEntryDocument();
  if (doc)
    baseURL = doc->GetDocBaseURI();

  // use the base URI to build what would have been the popup's URI
  nsCOMPtr<nsIIOService> ios(do_GetService(NS_IOSERVICE_CONTRACTID));
  if (ios)
    ios->NewURI(NS_ConvertUTF16toUTF8(aPopupURL), nullptr, baseURL,
                getter_AddRefs(popupURI));

  // fire an event chock full of informative URIs
  FirePopupBlockedEvent(topDoc, popupURI, aPopupWindowName,
                        aPopupWindowFeatures);
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindowOuter::OpenOuter(const nsAString& aUrl, const nsAString& aName,
                               const nsAString& aOptions, ErrorResult& aError)
{
  nsCOMPtr<nsPIDOMWindowOuter> window;
  aError = OpenJS(aUrl, aName, aOptions, getter_AddRefs(window));
  return window.forget();
}

nsresult
nsGlobalWindowOuter::Open(const nsAString& aUrl, const nsAString& aName,
                          const nsAString& aOptions, nsDocShellLoadInfo* aLoadInfo,
                          bool aForceNoOpener, nsPIDOMWindowOuter **_retval)
{
  return OpenInternal(aUrl, aName, aOptions,
                      false,          // aDialog
                      false,          // aContentModal
                      true,           // aCalledNoScript
                      false,          // aDoJSFixups
                      true,           // aNavigate
                      nullptr, nullptr,  // No args
                      aLoadInfo,
                      aForceNoOpener,
                      _retval);
}

nsresult
nsGlobalWindowOuter::OpenJS(const nsAString& aUrl, const nsAString& aName,
                            const nsAString& aOptions, nsPIDOMWindowOuter **_retval)
{
  return OpenInternal(aUrl, aName, aOptions,
                      false,          // aDialog
                      false,          // aContentModal
                      false,          // aCalledNoScript
                      true,           // aDoJSFixups
                      true,           // aNavigate
                      nullptr, nullptr,  // No args
                      nullptr,        // aLoadInfo
                      false,          // aForceNoOpener
                      _retval);
}

// like Open, but attaches to the new window any extra parameters past
// [features] as a JS property named "arguments"
nsresult
nsGlobalWindowOuter::OpenDialog(const nsAString& aUrl, const nsAString& aName,
                                const nsAString& aOptions,
                                nsISupports* aExtraArgument,
                                nsPIDOMWindowOuter** _retval)
{
  return OpenInternal(aUrl, aName, aOptions,
                      true,                    // aDialog
                      false,                   // aContentModal
                      true,                    // aCalledNoScript
                      false,                   // aDoJSFixups
                      true,                    // aNavigate
                      nullptr, aExtraArgument, // Arguments
                      nullptr,                 // aLoadInfo
                      false,                   // aForceNoOpener
                      _retval);
}

// Like Open, but passes aNavigate=false.
/* virtual */ nsresult
nsGlobalWindowOuter::OpenNoNavigate(const nsAString& aUrl,
                                    const nsAString& aName,
                                    const nsAString& aOptions,
                                    nsPIDOMWindowOuter **_retval)
{
  return OpenInternal(aUrl, aName, aOptions,
                      false,          // aDialog
                      false,          // aContentModal
                      true,           // aCalledNoScript
                      false,          // aDoJSFixups
                      false,          // aNavigate
                      nullptr, nullptr,  // No args
                      nullptr,        // aLoadInfo
                      false,          // aForceNoOpener
                      _retval);

}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindowOuter::OpenDialogOuter(JSContext* aCx, const nsAString& aUrl,
                                     const nsAString& aName, const nsAString& aOptions,
                                     const Sequence<JS::Value>& aExtraArgument,
                                     ErrorResult& aError)
{
  nsCOMPtr<nsIJSArgArray> argvArray;
  aError = NS_CreateJSArgv(aCx, aExtraArgument.Length(),
                           aExtraArgument.Elements(),
                           getter_AddRefs(argvArray));
  if (aError.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> dialog;
  aError = OpenInternal(aUrl, aName, aOptions,
                        true,             // aDialog
                        false,            // aContentModal
                        false,            // aCalledNoScript
                        false,            // aDoJSFixups
                        true,                // aNavigate
                        argvArray, nullptr,  // Arguments
                        nullptr,          // aLoadInfo
                        false,            // aForceNoOpener
                        getter_AddRefs(dialog));
  return dialog.forget();
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindowOuter::GetFramesOuter()
{
  RefPtr<nsPIDOMWindowOuter> frames(this);
  FlushPendingNotifications(FlushType::ContentAndNotify);
  return frames.forget();
}

nsGlobalWindowInner*
nsGlobalWindowOuter::CallerInnerWindow()
{
  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  NS_ENSURE_TRUE(cx, nullptr);
  nsIGlobalObject* global = GetIncumbentGlobal();
  NS_ENSURE_TRUE(global, nullptr);
  JS::Rooted<JSObject*> scope(cx, global->GetGlobalJSObject());
  NS_ENSURE_TRUE(scope, nullptr);

  // When Jetpack runs content scripts inside a sandbox, it uses
  // sandboxPrototype to make them appear as though they're running in the
  // scope of the page. So when a content script invokes postMessage, it expects
  // the |source| of the received message to be the window set as the
  // sandboxPrototype. This used to work incidentally for unrelated reasons, but
  // now we need to do some special handling to support it.
  if (xpc::IsSandbox(scope)) {
    JSAutoRealm ar(cx, scope);
    JS::Rooted<JSObject*> scopeProto(cx);
    bool ok = JS_GetPrototype(cx, scope, &scopeProto);
    NS_ENSURE_TRUE(ok, nullptr);
    if (scopeProto && xpc::IsSandboxPrototypeProxy(scopeProto) &&
        (scopeProto = js::CheckedUnwrap(scopeProto, /* stopAtWindowProxy = */ false)))
    {
      global = xpc::NativeGlobal(scopeProto);
      NS_ENSURE_TRUE(global, nullptr);
    }
  }

  // The calling window must be holding a reference, so we can return a weak
  // pointer.
  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(global);
  return nsGlobalWindowInner::Cast(win);
}

void
nsGlobalWindowOuter::PostMessageMozOuter(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                                         const nsAString& aTargetOrigin,
                                         JS::Handle<JS::Value> aTransfer,
                                         nsIPrincipal& aSubjectPrincipal,
                                         ErrorResult& aError)
{
  //
  // Window.postMessage is an intentional subversion of the same-origin policy.
  // As such, this code must be particularly careful in the information it
  // exposes to calling code.
  //
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/section-crossDocumentMessages.html
  //

  // First, get the caller's window
  RefPtr<nsGlobalWindowInner> callerInnerWin = CallerInnerWindow();
  nsIPrincipal* callerPrin;
  if (callerInnerWin) {
    // Compute the caller's origin either from its principal or, in the case the
    // principal doesn't carry a URI (e.g. the system principal), the caller's
    // document.  We must get this now instead of when the event is created and
    // dispatched, because ultimately it is the identity of the calling window
    // *now* that determines who sent the message (and not an identity which might
    // have changed due to intervening navigations).
    callerPrin = callerInnerWin->GetPrincipal();
  }
  else {
    // In case the global is not a window, it can be a sandbox, and the sandbox's
    // principal can be used for the security check.
    nsIGlobalObject* global = GetIncumbentGlobal();
    NS_ASSERTION(global, "Why is there no global object?");
    callerPrin = global->PrincipalOrNull();
  }
  if (!callerPrin) {
    return;
  }

  nsCOMPtr<nsIURI> callerOuterURI;
  if (NS_FAILED(callerPrin->GetURI(getter_AddRefs(callerOuterURI)))) {
    return;
  }

  nsAutoString origin;
  if (callerOuterURI) {
    // if the principal has a URI, use that to generate the origin
    nsContentUtils::GetUTFOrigin(callerPrin, origin);
  }
  else if (callerInnerWin) {
    // otherwise use the URI of the document to generate origin
    nsCOMPtr<nsIDocument> doc = callerInnerWin->GetExtantDoc();
    if (!doc) {
      return;
    }
    callerOuterURI = doc->GetDocumentURI();
    // if the principal has a URI, use that to generate the origin
    nsContentUtils::GetUTFOrigin(callerOuterURI, origin);
  }
  else {
    // in case of a sandbox with a system principal origin can be empty
    if (!nsContentUtils::IsSystemPrincipal(callerPrin)) {
      return;
    }
  }

  // Convert the provided origin string into a URI for comparison purposes.
  nsCOMPtr<nsIPrincipal> providedPrincipal;

  if (aTargetOrigin.EqualsASCII("/")) {
    providedPrincipal = callerPrin;
  }
  // "*" indicates no specific origin is required.
  else if (!aTargetOrigin.EqualsASCII("*")) {
    nsCOMPtr<nsIURI> originURI;
    if (NS_FAILED(NS_NewURI(getter_AddRefs(originURI), aTargetOrigin))) {
      aError.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return;
    }

    nsresult rv = NS_MutateURI(originURI)
                    .SetUserPass(EmptyCString())
                    .SetPathQueryRef(EmptyCString())
                    .Finalize(originURI);
    if (NS_FAILED(rv)) {
      return;
    }

    OriginAttributes attrs = aSubjectPrincipal.OriginAttributesRef();
    if (aSubjectPrincipal.GetIsSystemPrincipal()) {
      auto principal = BasePrincipal::Cast(GetPrincipal());

      if (attrs != principal->OriginAttributesRef()) {
        nsCOMPtr<nsIURI> targetURI;
        nsAutoCString targetURL;
        nsAutoCString sourceOrigin;
        nsAutoCString targetOrigin;

        if (NS_FAILED(principal->GetURI(getter_AddRefs(targetURI))) ||
            NS_FAILED(targetURI->GetAsciiSpec(targetURL)) ||
            NS_FAILED(principal->GetOrigin(targetOrigin)) ||
            NS_FAILED(aSubjectPrincipal.GetOrigin(sourceOrigin))) {
          NS_WARNING("Failed to get source and target origins");
          return;
        }

        nsContentUtils::LogSimpleConsoleError(
          NS_ConvertUTF8toUTF16(nsPrintfCString(
            R"(Attempting to post a message to window with url "%s" and )"
            R"(origin "%s" from a system principal scope with mismatched )"
            R"(origin "%s".)",
            targetURL.get(), targetOrigin.get(), sourceOrigin.get())),
          "DOM",
          !!principal->PrivateBrowsingId());

        attrs = principal->OriginAttributesRef();
      }
    }

    // Create a nsIPrincipal inheriting the app/browser attributes from the
    // caller.
    providedPrincipal = BasePrincipal::CreateCodebasePrincipal(originURI, attrs);
    if (NS_WARN_IF(!providedPrincipal)) {
      return;
    }
  }

  // Create and asynchronously dispatch a runnable which will handle actual DOM
  // event creation and dispatch.
  RefPtr<PostMessageEvent> event =
    new PostMessageEvent(nsContentUtils::IsCallerChrome() || !callerInnerWin
                         ? nullptr
                         : callerInnerWin->GetOuterWindowInternal(),
                         origin,
                         this,
                         providedPrincipal,
                         callerInnerWin
                         ? callerInnerWin->GetDoc()
                         : nullptr,
                         nsContentUtils::IsCallerChrome());

  JS::Rooted<JS::Value> message(aCx, aMessage);
  JS::Rooted<JS::Value> transfer(aCx, aTransfer);

  event->Write(aCx, message, transfer, JS::CloneDataPolicy(), aError);
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  aError = Dispatch(TaskCategory::Other, event.forget());
}

class nsCloseEvent : public Runnable {

  RefPtr<nsGlobalWindowOuter> mWindow;
  bool mIndirect;

  nsCloseEvent(nsGlobalWindowOuter* aWindow, bool aIndirect)
    : mozilla::Runnable("nsCloseEvent")
    , mWindow(aWindow)
    , mIndirect(aIndirect)
  {}

public:

  static nsresult
  PostCloseEvent(nsGlobalWindowOuter* aWindow, bool aIndirect) {
    nsCOMPtr<nsIRunnable> ev = new nsCloseEvent(aWindow, aIndirect);
    nsresult rv =
      aWindow->Dispatch(TaskCategory::Other, ev.forget());
    if (NS_SUCCEEDED(rv))
      aWindow->MaybeForgiveSpamCount();
    return rv;
  }

  NS_IMETHOD Run() override {
    if (mWindow) {
      if (mIndirect) {
        return PostCloseEvent(mWindow, false);
      }
      mWindow->ReallyCloseWindow();
    }
    return NS_OK;
  }

};

bool
nsGlobalWindowOuter::CanClose()
{
  if (mIsChrome) {
    nsCOMPtr<nsIBrowserDOMWindow> bwin;
    GetBrowserDOMWindow(getter_AddRefs(bwin));

    bool canClose = true;
    if (bwin && NS_SUCCEEDED(bwin->CanClose(&canClose))) {
      return canClose;
    }
  }

  if (!mDocShell) {
    return true;
  }

  // Ask the content viewer whether the toplevel window can close.
  // If the content viewer returns false, it is responsible for calling
  // Close() as soon as it is possible for the window to close.
  // This allows us to not close the window while printing is happening.

  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  if (cv) {
    bool canClose;
    nsresult rv = cv->PermitUnload(&canClose);
    if (NS_SUCCEEDED(rv) && !canClose)
      return false;

    rv = cv->RequestWindowClose(&canClose);
    if (NS_SUCCEEDED(rv) && !canClose)
      return false;
  }

  return true;
}

void
nsGlobalWindowOuter::CloseOuter(bool aTrustedCaller)
{
  if (!mDocShell || IsInModalState() ||
      (IsFrame() && !mDocShell->GetIsMozBrowser())) {
    // window.close() is called on a frame in a frameset, on a window
    // that's already closed, or on a window for which there's
    // currently a modal dialog open. Ignore such calls.
    return;
  }

  if (mHavePendingClose) {
    // We're going to be closed anyway; do nothing since we don't want
    // to double-close
    return;
  }

  if (mBlockScriptedClosingFlag)
  {
    // A script's popup has been blocked and we don't want
    // the window to be closed directly after this event,
    // so the user can see that there was a blocked popup.
    return;
  }

  // Don't allow scripts from content to close non-neterror windows that
  // were not opened by script.
  if (mDoc) {
    nsAutoString url;
    nsresult rv = mDoc->GetURL(url);
    NS_ENSURE_SUCCESS_VOID(rv);

    if (!StringBeginsWith(url, NS_LITERAL_STRING("about:neterror")) &&
        !mHadOriginalOpener && !aTrustedCaller) {
      bool allowClose = mAllowScriptsToClose ||
        Preferences::GetBool("dom.allow_scripts_to_close_windows", true);
      if (!allowClose) {
        // We're blocking the close operation
        // report localized error msg in JS console
        nsContentUtils::ReportToConsole(
            nsIScriptError::warningFlag,
            NS_LITERAL_CSTRING("DOM Window"), mDoc,  // Better name for the category?
            nsContentUtils::eDOM_PROPERTIES,
            "WindowCloseBlockedWarning");

        return;
      }
    }
  }

  if (!mInClose && !mIsClosed && !CanClose()) {
    return;
  }

  // Fire a DOM event notifying listeners that this window is about to
  // be closed. The tab UI code may choose to cancel the default
  // action for this event, if so, we won't actually close the window
  // (since the tab UI code will close the tab in stead). Sure, this
  // could be abused by content code, but do we care? I don't think
  // so...

  bool wasInClose = mInClose;
  mInClose = true;

  if (!DispatchCustomEvent(NS_LITERAL_STRING("DOMWindowClose"))) {
    // Someone chose to prevent the default action for this event, if
    // so, let's not close this window after all...

    mInClose = wasInClose;
    return;
  }

  FinalClose();
}

nsresult
nsGlobalWindowOuter::Close()
{
  CloseOuter(/* aTrustedCaller = */ true);
  return NS_OK;
}

void
nsGlobalWindowOuter::ForceClose()
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);

  if (IsFrame() || !mDocShell) {
    // This may be a frame in a frameset, or a window that's already closed.
    // Ignore such calls.
    return;
  }

  if (mHavePendingClose) {
    // We're going to be closed anyway; do nothing since we don't want
    // to double-close
    return;
  }

  mInClose = true;

  DispatchCustomEvent(NS_LITERAL_STRING("DOMWindowClose"));

  FinalClose();
}

void
nsGlobalWindowOuter::FinalClose()
{
  // Flag that we were closed.
  mIsClosed = true;

  // If we get here from CloseOuter then it means that the parent process is
  // going to close our window for us. It's just important to set mIsClosed.
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    return;
  }

  // This stuff is non-sensical but incredibly fragile. The reasons for the
  // behavior here don't make sense today and may not have ever made sense,
  // but various bits of frontend code break when you change them. If you need
  // to fix up this behavior, feel free to. It's a righteous task, but involves
  // wrestling with various download manager tests, frontend code, and possible
  // broken addons. The chrome tests in toolkit/mozapps/downloads are a good
  // testing ground.
  //
  // In particular, if some inner of |win| is the entry global, we must
  // complete _two_ round-trips to the event loop before the call to
  // ReallyCloseWindow. This allows setTimeout handlers that are set after
  // FinalClose() is called to run before the window is torn down.
  nsCOMPtr<nsPIDOMWindowInner> entryWindow =
    do_QueryInterface(GetEntryGlobal());
  bool indirect =
    entryWindow && entryWindow->GetOuterWindow() == this;
  if (NS_FAILED(nsCloseEvent::PostCloseEvent(this, indirect))) {
    ReallyCloseWindow();
  } else {
    mHavePendingClose = true;
  }
}


void
nsGlobalWindowOuter::ReallyCloseWindow()
{
  // Make sure we never reenter this method.
  mHavePendingClose = true;

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();

  // If there's no treeOwnerAsWin, this window must already be closed.

  if (treeOwnerAsWin) {

    // but if we're a browser window we could be in some nasty
    // self-destroying cascade that we should mostly ignore

    if (mDocShell) {
      nsCOMPtr<nsIBrowserDOMWindow> bwin;
      nsCOMPtr<nsIDocShellTreeItem> rootItem;
      mDocShell->GetRootTreeItem(getter_AddRefs(rootItem));
      nsCOMPtr<nsPIDOMWindowOuter> rootWin =
       rootItem ? rootItem->GetWindow() : nullptr;
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
        bool isTab;
        if (rootWin == this ||
            !bwin ||
            (NS_SUCCEEDED(bwin->IsTabContentWindow(GetOuterWindowInternal(),
                                                   &isTab)) && isTab)) {
          treeOwnerAsWin->Destroy();
        }
      }
    }

    CleanUp();
  }
}

void
nsGlobalWindowOuter::EnterModalState()
{
  // GetScriptableTop, not GetTop, so that EnterModalState works properly with
  // <iframe mozbrowser>.
  nsGlobalWindowOuter* topWin = GetScriptableTopInternal();

  if (!topWin) {
    NS_ERROR("Uh, EnterModalState() called w/o a reachable top window?");
    return;
  }

  // If there is an active ESM in this window, clear it. Otherwise, this can
  // cause a problem if a modal state is entered during a mouseup event.
  EventStateManager* activeESM =
    static_cast<EventStateManager*>(
      EventStateManager::GetActiveEventStateManager());
  if (activeESM && activeESM->GetPresContext()) {
    nsIPresShell* activeShell = activeESM->GetPresContext()->GetPresShell();
    if (activeShell && (
        nsContentUtils::ContentIsCrossDocDescendantOf(activeShell->GetDocument(), mDoc) ||
        nsContentUtils::ContentIsCrossDocDescendantOf(mDoc, activeShell->GetDocument()))) {
      EventStateManager::ClearGlobalActiveContent(activeESM);

      nsIPresShell::SetCapturingContent(nullptr, 0);

      if (activeShell) {
        RefPtr<nsFrameSelection> frameSelection = activeShell->FrameSelection();
        frameSelection->SetDragState(false);
      }
    }
  }

  // If there are any drag and drop operations in flight, try to end them.
  nsCOMPtr<nsIDragService> ds =
    do_GetService("@mozilla.org/widget/dragservice;1");
  if (ds) {
    ds->EndDragSession(true, 0);
  }

  // Clear the capturing content if it is under topDoc.
  // Usually the activeESM check above does that, but there are cases when
  // we don't have activeESM, or it is for different document.
  nsIDocument* topDoc = topWin->GetExtantDoc();
  nsIContent* capturingContent = nsIPresShell::GetCapturingContent();
  if (capturingContent && topDoc &&
      nsContentUtils::ContentIsCrossDocDescendantOf(capturingContent, topDoc)) {
    nsIPresShell::SetCapturingContent(nullptr, 0);
  }

  if (topWin->mModalStateDepth == 0) {
    NS_ASSERTION(!topWin->mSuspendedDoc, "Shouldn't have mSuspendedDoc here!");

    topWin->mSuspendedDoc = topDoc;
    if (topDoc) {
      topDoc->SuppressEventHandling();
    }

    nsGlobalWindowInner* inner = topWin->GetCurrentInnerWindowInternal();
    if (inner) {
      topWin->GetCurrentInnerWindowInternal()->Suspend();
    }
  }
  topWin->mModalStateDepth++;
}

void
nsGlobalWindowOuter::LeaveModalState()
{
  nsGlobalWindowOuter* topWin = GetScriptableTopInternal();

  if (!topWin) {
    NS_ERROR("Uh, LeaveModalState() called w/o a reachable top window?");
    return;
  }

  MOZ_ASSERT(topWin->mModalStateDepth != 0);
  MOZ_ASSERT(IsSuspended());
  MOZ_ASSERT(topWin->IsSuspended());
  topWin->mModalStateDepth--;

  nsGlobalWindowInner* inner = topWin->GetCurrentInnerWindowInternal();

  if (topWin->mModalStateDepth == 0) {
    if (inner) {
      inner->Resume();
    }

    if (topWin->mSuspendedDoc) {
      nsCOMPtr<nsIDocument> currentDoc = topWin->GetExtantDoc();
      topWin->mSuspendedDoc->UnsuppressEventHandlingAndFireEvents(currentDoc == topWin->mSuspendedDoc);
      topWin->mSuspendedDoc = nullptr;
    }
  }

  // Remember the time of the last dialog quit.
  if (inner) {
    inner->mLastDialogQuitTime = TimeStamp::Now();
  }

  if (topWin->mModalStateDepth == 0) {
    RefPtr<Event> event = NS_NewDOMEvent(inner, nullptr, nullptr);
    event->InitEvent(NS_LITERAL_STRING("endmodalstate"), true, false);
    event->SetTrusted(true);
    event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;
    topWin->DispatchEvent(*event);
  }
}

bool
nsGlobalWindowOuter::IsInModalState()
{
  nsGlobalWindowOuter *topWin = GetScriptableTopInternal();

  if (!topWin) {
    // IsInModalState() getting called w/o a reachable top window is a bit
    // iffy, but valid enough not to make noise about it.  See bug 404828
    return false;
  }

  return topWin->mModalStateDepth != 0;
}

void
nsGlobalWindowOuter::NotifyWindowIDDestroyed(const char* aTopic)
{
  nsCOMPtr<nsIRunnable> runnable =
    new WindowDestroyedEvent(this, mWindowID, aTopic);
  Dispatch(TaskCategory::Other, runnable.forget());
}

Element*
nsGlobalWindowOuter::GetFrameElementOuter(nsIPrincipal& aSubjectPrincipal)
{
  if (!mDocShell || mDocShell->GetIsMozBrowser()) {
    return nullptr;
  }

  // Per HTML5, the frameElement getter returns null in cross-origin situations.
  Element* element = GetRealFrameElementOuter();
  if (!element) {
    return nullptr;
  }

  if (!aSubjectPrincipal.SubsumesConsideringDomain(element->NodePrincipal())) {
    return nullptr;
  }

  return element;
}

Element*
nsGlobalWindowOuter::GetRealFrameElementOuter()
{
  if (!mDocShell) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShell> parent;
  mDocShell->GetSameTypeParentIgnoreBrowserBoundaries(getter_AddRefs(parent));

  if (!parent || parent == mDocShell) {
    // We're at a chrome boundary, don't expose the chrome iframe
    // element to content code.
    return nullptr;
  }

  return mFrameElement;
}

/**
 * nsIGlobalWindow::GetFrameElement (when called from C++) is just a wrapper
 * around GetRealFrameElement.
 */
Element*
nsGlobalWindowOuter::GetFrameElement()
{
  FORWARD_TO_INNER(GetFrameElement, (), nullptr);
}

namespace {
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
} // anonymous namespace

void
nsGlobalWindowOuter::UpdateCommands(const nsAString& anAction,
                                    Selection* aSel,
                                    int16_t aReason)
{
  // If this is a child process, redirect to the parent process.
  if (nsIDocShell* docShell = GetDocShell()) {
    if (nsCOMPtr<nsITabChild> child = docShell->GetTabChild()) {
      nsCOMPtr<nsPIWindowRoot> root = GetTopWindowRoot();
      if (root) {
        nsContentUtils::AddScriptRunner(
          new ChildCommandDispatcher(root, child, anAction));
      }
      return;
    }
  }

  nsPIDOMWindowOuter *rootWindow = GetPrivateRoot();
  if (!rootWindow) {
    return;
  }

  nsIDocument* doc = rootWindow->GetExtantDoc();
  // See if we contain a XUL document.
  if (!doc || !doc->IsXULDocument()) {
    return;
  }
  // selectionchange action is only used for mozbrowser, not for XUL. So we bypass
  // XUL command dispatch if anAction is "selectionchange".
  if (!anAction.EqualsLiteral("selectionchange")) {
    // Retrieve the command dispatcher and call updateCommands on it.
    nsIDOMXULCommandDispatcher* xulCommandDispatcher =
      doc->AsXULDocument()->GetCommandDispatcher();
    if (xulCommandDispatcher) {
      nsContentUtils::AddScriptRunner(new CommandDispatcher(xulCommandDispatcher,
                                                            anAction));
    }
  }
}

Selection*
nsGlobalWindowOuter::GetSelectionOuter()
{
  if (!mDocShell) {
    return nullptr;
  }

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();
  if (!presShell) {
    return nullptr;
  }
  return presShell->GetCurrentSelection(SelectionType::eNormal);
}

already_AddRefed<Selection>
nsGlobalWindowOuter::GetSelection()
{
  RefPtr<Selection> selection = GetSelectionOuter();
  return selection.forget();
}

bool
nsGlobalWindowOuter::FindOuter(const nsAString& aString, bool aCaseSensitive,
                               bool aBackwards, bool aWrapAround, bool aWholeWord,
                               bool aSearchInFrames, bool aShowDialog,
                               ErrorResult& aError)
{
  Unused << aShowDialog;

  if (Preferences::GetBool("dom.disable_window_find", false)) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return false;
  }

  nsCOMPtr<nsIWebBrowserFind> finder(do_GetInterface(mDocShell));
  if (!finder) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return false;
  }

  // Set the options of the search
  aError = finder->SetSearchString(PromiseFlatString(aString).get());
  if (aError.Failed()) {
    return false;
  }
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

  if (aString.IsEmpty()) {
    return false;
  }

  // Launch the search with the passed in search string
  bool didFind = false;
  aError = finder->FindNext(&didFind);
  return didFind;
}

//*****************************************************************************
// EventTarget
//*****************************************************************************

nsPIDOMWindowOuter*
nsGlobalWindowOuter::GetOwnerGlobalForBindings()
{
  return this;
}

bool
nsGlobalWindowOuter::DispatchEvent(Event& aEvent,
                                   CallerType aCallerType,
                                   ErrorResult& aRv)
{
  FORWARD_TO_INNER(DispatchEvent, (aEvent, aCallerType, aRv), false);
}

bool
nsGlobalWindowOuter::ComputeDefaultWantsUntrusted(ErrorResult& aRv)
{
  // It's OK that we just return false here on failure to create an
  // inner.  GetOrCreateListenerManager() will likewise fail, and then
  // we won't be adding any listeners anyway.
  FORWARD_TO_INNER_CREATE(ComputeDefaultWantsUntrusted, (aRv), false);
}

EventListenerManager*
nsGlobalWindowOuter::GetOrCreateListenerManager()
{
  FORWARD_TO_INNER_CREATE(GetOrCreateListenerManager, (), nullptr);
}

EventListenerManager*
nsGlobalWindowOuter::GetExistingListenerManager() const
{
  FORWARD_TO_INNER(GetExistingListenerManager, (), nullptr);
}

//*****************************************************************************
// nsGlobalWindowOuter::nsPIDOMWindow
//*****************************************************************************

nsPIDOMWindowOuter*
nsGlobalWindowOuter::GetPrivateParent()
{
  nsCOMPtr<nsPIDOMWindowOuter> parent = GetParent();

  if (this == parent) {
    nsCOMPtr<nsIContent> chromeElement(do_QueryInterface(mChromeEventHandler));
    if (!chromeElement)
      return nullptr;             // This is ok, just means a null parent.

    nsIDocument* doc = chromeElement->GetComposedDoc();
    if (!doc)
      return nullptr;             // This is ok, just means a null parent.

    return doc->GetWindow();
  }

  return parent;
}

nsPIDOMWindowOuter*
nsGlobalWindowOuter::GetPrivateRoot()
{
  nsCOMPtr<nsPIDOMWindowOuter> top = GetTop();

  nsCOMPtr<nsIContent> chromeElement(do_QueryInterface(mChromeEventHandler));
  if (chromeElement) {
    nsIDocument* doc = chromeElement->GetComposedDoc();
    if (doc) {
      nsCOMPtr<nsPIDOMWindowOuter> parent = doc->GetWindow();
      if (parent) {
        top = parent->GetTop();
      }
    }
  }

  return top;
}

Location*
nsGlobalWindowOuter::GetLocation()
{
  // This method can be called on the outer window as well.
  FORWARD_TO_INNER(GetLocation, (), nullptr);
}

void
nsGlobalWindowOuter::ActivateOrDeactivate(bool aActivate)
{
  if (!mDoc) {
    return;
  }

  // Set / unset mIsActive on the top level window, which is used for the
  // :-moz-window-inactive pseudoclass, and its sheet (if any).
  nsCOMPtr<nsIWidget> mainWidget = GetMainWidget();
  nsCOMPtr<nsIWidget> topLevelWidget;
  if (mainWidget) {
    // Get the top level widget (if the main widget is a sheet, this will
    // be the sheet's top (non-sheet) parent).
    topLevelWidget = mainWidget->GetSheetWindowParent();
    if (!topLevelWidget) {
      topLevelWidget = mainWidget;
    }
  }

  SetActive(aActivate);

  if (mainWidget != topLevelWidget) {
    // This is a workaround for the following problem:
    // When a window with an open sheet gains or loses focus, only the sheet
    // window receives the NS_ACTIVATE/NS_DEACTIVATE event.  However the
    // styling of the containing top level window also needs to change.  We
    // get around this by calling nsPIDOMWindow::SetActive() on both windows.

    // Get the top level widget's nsGlobalWindowOuter
    nsCOMPtr<nsPIDOMWindowOuter> topLevelWindow;

    // widgetListener should be a nsXULWindow
    nsIWidgetListener* listener = topLevelWidget->GetWidgetListener();
    if (listener) {
      nsCOMPtr<nsIXULWindow> window = listener->GetXULWindow();
      nsCOMPtr<nsIInterfaceRequestor> req(do_QueryInterface(window));
      topLevelWindow = do_GetInterface(req);
    }

    if (topLevelWindow) {
      topLevelWindow->SetActive(aActivate);
    }
  }
}

static bool
NotifyDocumentTree(nsIDocument* aDocument, void* aData)
{
  aDocument->EnumerateSubDocuments(NotifyDocumentTree, nullptr);
  aDocument->DocumentStatesChanged(NS_DOCUMENT_STATE_WINDOW_INACTIVE);
  return true;
}

void
nsGlobalWindowOuter::SetActive(bool aActive)
{
  nsPIDOMWindowOuter::SetActive(aActive);
  if (mDoc) {
    NotifyDocumentTree(mDoc, nullptr);
  }
}

bool
nsGlobalWindowOuter::IsTopLevelWindowActive()
{
   nsCOMPtr<nsIDocShellTreeItem> treeItem(GetDocShell());
   if (!treeItem) {
     return false;
   }

   nsCOMPtr<nsIDocShellTreeItem> rootItem;
   treeItem->GetRootTreeItem(getter_AddRefs(rootItem));
   if (!rootItem) {
     return false;
   }

   nsCOMPtr<nsPIDOMWindowOuter> domWindow = rootItem->GetWindow();
   return domWindow && domWindow->IsActive();
}

void
nsGlobalWindowOuter::SetIsBackground(bool aIsBackground)
{
  bool changed = aIsBackground != IsBackground();
  SetIsBackgroundInternal(aIsBackground);

  nsGlobalWindowInner* inner = GetCurrentInnerWindowInternal();

  if (inner && changed) {
    inner->mTimeoutManager->UpdateBackgroundState();
  }

  if (aIsBackground) {
    // Notify gamepadManager we are at the background window,
    // we need to stop vibrate.
    // Stop the vr telemery time spent when it switches to
    // the background window.
    if (inner && changed) {
      inner->StopGamepadHaptics();
      // true is for asking to set the delta time to
      // the telemetry.
      inner->ResetVRTelemetry(true);
    }
    return;
  }

  if (inner) {
    // When switching to be as a top tab, restart the telemetry.
    // false is for only resetting the timestamp.
    inner->ResetVRTelemetry(false);
    inner->SyncGamepadState();
  }
}

void
nsGlobalWindowOuter::SetIsBackgroundInternal(bool aIsBackground)
{
  if (mIsBackground != aIsBackground) {
    TabGroup()->WindowChangedBackgroundStatus(aIsBackground);
  }
  mIsBackground = aIsBackground;
}

void
nsGlobalWindowOuter::MaybeUpdateTouchState()
{
  FORWARD_TO_INNER_VOID(MaybeUpdateTouchState, ());
}

void
nsGlobalWindowOuter::SetChromeEventHandler(EventTarget* aChromeEventHandler)
{
  SetChromeEventHandlerInternal(aChromeEventHandler);
  // update the chrome event handler on all our inner windows
  RefPtr<nsGlobalWindowInner> inner;
  for (PRCList* node = PR_LIST_HEAD(this);
       node != this;
       node = PR_NEXT_LINK(inner)) {
    // This cast is only safe if `node != this`, as nsGlobalWindowOuter is also
    // in the list.
    inner = static_cast<nsGlobalWindowInner*>(node);
    NS_ASSERTION(!inner->mOuterWindow || inner->mOuterWindow == this,
                 "bad outer window pointer");
    inner->SetChromeEventHandlerInternal(aChromeEventHandler);
  }
}

void
nsGlobalWindowOuter::SetFocusedElement(Element* aElement,
                                       uint32_t aFocusMethod,
                                       bool aNeedsFocus)
{
  FORWARD_TO_INNER_VOID(SetFocusedElement,
                        (aElement, aFocusMethod, aNeedsFocus));
}

uint32_t
nsGlobalWindowOuter::GetFocusMethod()
{
  FORWARD_TO_INNER(GetFocusMethod, (), 0);
}

bool
nsGlobalWindowOuter::ShouldShowFocusRing()
{
  FORWARD_TO_INNER(ShouldShowFocusRing, (), false);
}

void
nsGlobalWindowOuter::SetKeyboardIndicators(UIStateChangeType aShowAccelerators,
                                           UIStateChangeType aShowFocusRings)
{
  nsPIDOMWindowOuter* piWin = GetPrivateRoot();
  if (!piWin) {
    return;
  }

  MOZ_ASSERT(piWin == this);

  bool oldShouldShowFocusRing = ShouldShowFocusRing();

  // only change the flags that have been modified
  nsCOMPtr<nsPIWindowRoot> windowRoot = do_QueryInterface(mChromeEventHandler);
  if (!windowRoot) {
    return;
  }

  if (aShowAccelerators != UIStateChangeType_NoChange) {
    windowRoot->SetShowAccelerators(aShowAccelerators == UIStateChangeType_Set);
  }
  if (aShowFocusRings != UIStateChangeType_NoChange) {
    windowRoot->SetShowFocusRings(aShowFocusRings == UIStateChangeType_Set);
  }

  nsContentUtils::SetKeyboardIndicatorsOnRemoteChildren(GetOuterWindow(),
                                                        aShowAccelerators,
                                                        aShowFocusRings);

  bool newShouldShowFocusRing = ShouldShowFocusRing();
  if (mInnerWindow && nsGlobalWindowInner::Cast(mInnerWindow)->mHasFocus &&
      mInnerWindow->mFocusedElement &&
      oldShouldShowFocusRing != newShouldShowFocusRing) {
    // Update focusedNode's state.
    if (newShouldShowFocusRing) {
      mInnerWindow->mFocusedElement->AddStates(NS_EVENT_STATE_FOCUSRING);
    } else {
      mInnerWindow->mFocusedElement->RemoveStates(NS_EVENT_STATE_FOCUSRING);
    }
  }
}

bool
nsGlobalWindowOuter::TakeFocus(bool aFocus, uint32_t aFocusMethod)
{
  FORWARD_TO_INNER(TakeFocus, (aFocus, aFocusMethod), false);
}

void
nsGlobalWindowOuter::SetReadyForFocus()
{
  FORWARD_TO_INNER_VOID(SetReadyForFocus, ());
}

void
nsGlobalWindowOuter::PageHidden()
{
  FORWARD_TO_INNER_VOID(PageHidden, ());
}

already_AddRefed<nsICSSDeclaration>
nsGlobalWindowOuter::GetComputedStyleHelperOuter(Element& aElt,
                                                 const nsAString& aPseudoElt,
                                                 bool aDefaultStylesOnly)
{
  if (!mDoc) {
    return nullptr;
  }

  RefPtr<nsICSSDeclaration> compStyle =
    NS_NewComputedDOMStyle(&aElt, aPseudoElt, mDoc,
                           aDefaultStylesOnly ? nsComputedDOMStyle::eDefaultOnly :
                                                nsComputedDOMStyle::eAll);

  return compStyle.forget();
}

//*****************************************************************************
// nsGlobalWindowOuter::nsIInterfaceRequestor
//*****************************************************************************

nsresult
nsGlobalWindowOuter::GetInterfaceInternal(const nsIID& aIID, void** aSink)
{
  NS_ENSURE_ARG_POINTER(aSink);
  *aSink = nullptr;

  if (aIID.Equals(NS_GET_IID(nsIWebNavigation))) {
    nsGlobalWindowOuter* outer = GetOuterWindowInternal();
    NS_ENSURE_TRUE(outer, NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(outer->mDocShell));
    webNav.forget(aSink);
  }
  else if (aIID.Equals(NS_GET_IID(nsIDocShell))) {
    nsGlobalWindowOuter* outer = GetOuterWindowInternal();
    NS_ENSURE_TRUE(outer, NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsIDocShell> docShell = outer->mDocShell;
    docShell.forget(aSink);
  }
#ifdef NS_PRINTING
  else if (aIID.Equals(NS_GET_IID(nsIWebBrowserPrint))) {
    nsGlobalWindowOuter* outer = GetOuterWindowInternal();
    NS_ENSURE_TRUE(outer, NS_ERROR_NOT_INITIALIZED);

    if (outer->mDocShell) {
      nsCOMPtr<nsIContentViewer> viewer;
      outer->mDocShell->GetContentViewer(getter_AddRefs(viewer));
      if (viewer) {
        nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint(do_QueryInterface(viewer));
        webBrowserPrint.forget(aSink);
      }
    }
  }
#endif
  else if (aIID.Equals(NS_GET_IID(nsIDOMWindowUtils))) {
    nsGlobalWindowOuter* outer = GetOuterWindowInternal();
    NS_ENSURE_TRUE(outer, NS_ERROR_NOT_INITIALIZED);

    if (!mWindowUtils) {
      mWindowUtils = new nsDOMWindowUtils(outer);
    }

    *aSink = mWindowUtils;
    NS_ADDREF(((nsISupports *) *aSink));
  }
  else if (aIID.Equals(NS_GET_IID(nsILoadContext))) {
    nsGlobalWindowOuter* outer = GetOuterWindowInternal();
    NS_ENSURE_TRUE(outer, NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsILoadContext> loadContext(do_QueryInterface(outer->mDocShell));
    loadContext.forget(aSink);
  }

  return *aSink ? NS_OK : NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
nsGlobalWindowOuter::GetInterface(const nsIID & aIID, void **aSink)
{
  nsresult rv = GetInterfaceInternal(aIID, aSink);
  if (rv == NS_ERROR_NO_INTERFACE) {
    return QueryInterface(aIID, aSink);
  }
  return rv;
}

bool
nsGlobalWindowOuter::IsSuspended() const
{
  MOZ_ASSERT(NS_IsMainThread());
  // No inner means we are effectively suspended
  if (!mInnerWindow) {
    return true;
  }
  return mInnerWindow->IsSuspended();
}

bool
nsGlobalWindowOuter::IsFrozen() const
{
  MOZ_ASSERT(NS_IsMainThread());
  // No inner means we are effectively frozen
  if (!mInnerWindow) {
    return true;
  }
  return mInnerWindow->IsFrozen();
}

nsresult
nsGlobalWindowOuter::FireDelayedDOMEvents()
{
  FORWARD_TO_INNER(FireDelayedDOMEvents, (), NS_ERROR_UNEXPECTED);
}

//*****************************************************************************
// nsGlobalWindowOuter: Window Control Functions
//*****************************************************************************

nsPIDOMWindowOuter*
nsGlobalWindowOuter::GetParentInternal()
{
  nsCOMPtr<nsPIDOMWindowOuter> parent = GetParent();

  if (parent && parent != this) {
    return parent;
  }

  return nullptr;
}

void
nsGlobalWindowOuter::UnblockScriptedClosing()
{
  mBlockScriptedClosingFlag = false;
}

class AutoUnblockScriptClosing
{
private:
  RefPtr<nsGlobalWindowOuter> mWin;
public:
  explicit AutoUnblockScriptClosing(nsGlobalWindowOuter* aWin)
    : mWin(aWin)
  {
    MOZ_ASSERT(mWin);
  }
  ~AutoUnblockScriptClosing()
  {
    void (nsGlobalWindowOuter::*run)() = &nsGlobalWindowOuter::UnblockScriptedClosing;
    nsCOMPtr<nsIRunnable> caller = NewRunnableMethod(
      "AutoUnblockScriptClosing::~AutoUnblockScriptClosing", mWin, run);
    mWin->Dispatch(TaskCategory::Other, caller.forget());
  }
};

nsresult
nsGlobalWindowOuter::OpenInternal(const nsAString& aUrl, const nsAString& aName,
                                  const nsAString& aOptions, bool aDialog,
                                  bool aContentModal, bool aCalledNoScript,
                                  bool aDoJSFixups, bool aNavigate,
                                  nsIArray *argv,
                                  nsISupports *aExtraArgument,
                                  nsDocShellLoadInfo* aLoadInfo,
                                  bool aForceNoOpener,
                                  nsPIDOMWindowOuter **aReturn)
{
#ifdef DEBUG
  uint32_t argc = 0;
  if (argv)
      argv->GetLength(&argc);
#endif

  MOZ_ASSERT(!aExtraArgument || (!argv && argc == 0),
             "Can't pass in arguments both ways");
  MOZ_ASSERT(!aCalledNoScript || (!argv && argc == 0),
             "Can't pass JS args when called via the noscript methods");

  mozilla::Maybe<AutoUnblockScriptClosing> closeUnblocker;

  // Calls to window.open from script should navigate.
  MOZ_ASSERT(aCalledNoScript || aNavigate);

  *aReturn = nullptr;

  nsCOMPtr<nsIWebBrowserChrome> chrome = GetWebBrowserChrome();
  if (!chrome) {
    // No chrome means we don't want to go through with this open call
    // -- see nsIWindowWatcher.idl
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ASSERTION(mDocShell, "Must have docshell here");

  bool forceNoOpener = aForceNoOpener;
  if (!forceNoOpener) {
    // Unlike other window flags, "noopener" comes from splitting on commas with
    // HTML whitespace trimming...
    nsCharSeparatedTokenizerTemplate<nsContentUtils::IsHTMLWhitespace> tok(
      aOptions, ',');
    while (tok.hasMoreTokens()) {
      if (tok.nextToken().EqualsLiteral("noopener")) {
        forceNoOpener = true;
        break;
      }
    }
  }

  bool windowExists = WindowExists(aName, forceNoOpener, !aCalledNoScript);

  // XXXbz When this gets fixed to not use LegacyIsCallerNativeCode()
  // (indirectly) maybe we can nix the AutoJSAPI usage OnLinkClickEvent::Run.
  // But note that if you change this to GetEntryGlobal(), say, then
  // OnLinkClickEvent::Run will need a full-blown AutoEntryScript.
  const bool checkForPopup = !nsContentUtils::LegacyIsCallerChromeOrNativeCode() &&
    !aDialog && !windowExists;

  // Note: the Void handling here is very important, because the window watcher
  // expects a null URL string (not an empty string) if there is no URL to load.
  nsCString url;
  url.SetIsVoid(true);
  nsresult rv = NS_OK;

  nsCOMPtr<nsIURI> uri;

  // It's important to do this security check before determining whether this
  // window opening should be blocked, to ensure that we don't FireAbuseEvents
  // for a window opening that wouldn't have succeeded in the first place.
  if (!aUrl.IsEmpty()) {
    AppendUTF16toUTF8(aUrl, url);

    // It's safe to skip the security check below if we're not a dialog
    // because window.openDialog is not callable from content script.  See bug
    // 56851.
    //
    // If we're not navigating, we assume that whoever *does* navigate the
    // window will do a security check of their own.
    if (!url.IsVoid() && !aDialog && aNavigate)
      rv = SecurityCheckURL(url.get(), getter_AddRefs(uri));
  }

  if (NS_FAILED(rv))
    return rv;

  PopupControlState abuseLevel = GetPopupControlState();
  if (checkForPopup) {
    abuseLevel = RevisePopupAbuseLevel(abuseLevel);
    if (abuseLevel >= openBlocked) {
      if (!aCalledNoScript) {
        // If script in some other window is doing a window.open on us and
        // it's being blocked, then it's OK to close us afterwards, probably.
        // But if we're doing a window.open on ourselves and block the popup,
        // prevent this window from closing until after this script terminates
        // so that whatever popup blocker UI the app has will be visible.
        nsCOMPtr<nsPIDOMWindowInner> entryWindow =
          do_QueryInterface(GetEntryGlobal());
        // Note that entryWindow can be null here if some JS component was the
        // place where script was entered for this JS execution.
        if (entryWindow &&
            entryWindow->GetOuterWindow() == this) {
          mBlockScriptedClosingFlag = true;
          closeUnblocker.emplace(this);
        }
      }

      FireAbuseEvents(aUrl, aName, aOptions);
      return aDoJSFixups ? NS_OK : NS_ERROR_FAILURE;
    }
  }

  nsCOMPtr<mozIDOMWindowProxy> domReturn;

  nsCOMPtr<nsIWindowWatcher> wwatch =
    do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  NS_ENSURE_TRUE(wwatch, rv);

  NS_ConvertUTF16toUTF8 options(aOptions);
  NS_ConvertUTF16toUTF8 name(aName);

  const char *options_ptr = aOptions.IsEmpty() ? nullptr : options.get();
  const char *name_ptr = aName.IsEmpty() ? nullptr : name.get();

  nsCOMPtr<nsPIWindowWatcher> pwwatch(do_QueryInterface(wwatch));
  NS_ENSURE_STATE(pwwatch);

  MOZ_ASSERT_IF(checkForPopup, abuseLevel < openBlocked);
  // At this point we should know for a fact that if checkForPopup then
  // abuseLevel < openBlocked, so we could just check for abuseLevel ==
  // openControlled.  But let's be defensive just in case and treat anything
  // that fails the above assert as a spam popup too, if it ever happens.
  bool isPopupSpamWindow = checkForPopup && (abuseLevel >= openControlled);

  {
    // Reset popup state while opening a window to prevent the
    // current state from being active the whole time a modal
    // dialog is open.
    nsAutoPopupStatePusher popupStatePusher(openAbused, true);

    if (!aCalledNoScript) {
      // We asserted at the top of this function that aNavigate is true for
      // !aCalledNoScript.
      rv = pwwatch->OpenWindow2(this, url.IsVoid() ? nullptr : url.get(),
                                name_ptr,
                                options_ptr, /* aCalledFromScript = */ true,
                                aDialog, aNavigate, argv,
                                isPopupSpamWindow,
                                forceNoOpener,
                                aLoadInfo,
                                getter_AddRefs(domReturn));
    } else {
      // Force a system caller here so that the window watcher won't screw us
      // up.  We do NOT want this case looking at the JS context on the stack
      // when searching.  Compare comments on
      // nsIDOMWindow::OpenWindow and nsIWindowWatcher::OpenWindow.

      // Note: Because nsWindowWatcher is so broken, it's actually important
      // that we don't force a system caller here, because that screws it up
      // when it tries to compute the caller principal to associate with dialog
      // arguments. That whole setup just really needs to be rewritten. :-(
      Maybe<AutoNoJSAPI> nojsapi;
      if (!aContentModal) {
        nojsapi.emplace();
      }

      rv = pwwatch->OpenWindow2(this, url.IsVoid() ? nullptr : url.get(),
                                name_ptr,
                                options_ptr, /* aCalledFromScript = */ false,
                                aDialog, aNavigate, aExtraArgument,
                                isPopupSpamWindow,
                                forceNoOpener,
                                aLoadInfo,
                                getter_AddRefs(domReturn));

    }
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // success!

  if (!aCalledNoScript && !windowExists && uri) {
    MaybeAllowStorageForOpenedWindow(uri);
  }

  NS_ENSURE_TRUE(domReturn, NS_OK);
  nsCOMPtr<nsPIDOMWindowOuter> outerReturn =
    nsPIDOMWindowOuter::From(domReturn);
  outerReturn.swap(*aReturn);

  if (aDoJSFixups) {
    nsCOMPtr<nsIDOMChromeWindow> chrome_win(do_QueryInterface(*aReturn));
    if (!chrome_win) {
      // A new non-chrome window was created from a call to
      // window.open() from JavaScript, make sure there's a document in
      // the new window. We do this by simply asking the new window for
      // its document, this will synchronously create an empty document
      // if there is no document in the window.
      // XXXbz should this just use EnsureInnerWindow()?

      // Force document creation.
      nsCOMPtr<nsIDocument> doc = (*aReturn)->GetDoc();
      Unused << doc;
    }
  }

  return rv;
}

void
nsGlobalWindowOuter::MaybeAllowStorageForOpenedWindow(nsIURI* aURI)
{
  nsGlobalWindowInner *inner = GetCurrentInnerWindowInternal();
  if (NS_WARN_IF(!inner)) {
    return;
  }

  // No 3rd party or no tracking resource.
  if (!nsContentUtils::IsThirdPartyWindowOrChannel(inner, nullptr, nullptr) ||
      !nsContentUtils::IsTrackingResourceWindow(inner)) {
    return;
  }

  nsAutoString origin;
  nsresult rv = nsContentUtils::GetUTFOrigin(aURI, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  AntiTrackingCommon::AddFirstPartyStorageAccessGrantedFor(origin, inner);
}

//*****************************************************************************
// nsGlobalWindowOuter: Helper Functions
//*****************************************************************************

already_AddRefed<nsIDocShellTreeOwner>
nsGlobalWindowOuter::GetTreeOwner()
{
  // If there's no docShellAsItem, this window must have been closed,
  // in that case there is no tree owner.

  if (!mDocShell) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  mDocShell->GetTreeOwner(getter_AddRefs(treeOwner));
  return treeOwner.forget();
}

already_AddRefed<nsIBaseWindow>
nsGlobalWindowOuter::GetTreeOwnerWindow()
{
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;

  // If there's no mDocShell, this window must have been closed,
  // in that case there is no tree owner.

  if (mDocShell) {
    mDocShell->GetTreeOwner(getter_AddRefs(treeOwner));
  }

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(treeOwner);
  return baseWindow.forget();
}

already_AddRefed<nsIWebBrowserChrome>
nsGlobalWindowOuter::GetWebBrowserChrome()
{
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner = GetTreeOwner();

  nsCOMPtr<nsIWebBrowserChrome> browserChrome = do_GetInterface(treeOwner);
  return browserChrome.forget();
}

nsIScrollableFrame *
nsGlobalWindowOuter::GetScrollFrame()
{
  if (!mDocShell) {
    return nullptr;
  }

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();
  if (presShell) {
    return presShell->GetRootScrollFrameAsScrollable();
  }
  return nullptr;
}

nsresult
nsGlobalWindowOuter::SecurityCheckURL(const char *aURL, nsIURI** aURI)
{
  nsCOMPtr<nsPIDOMWindowInner> sourceWindow = do_QueryInterface(GetEntryGlobal());
  if (!sourceWindow) {
    sourceWindow = GetCurrentInnerWindow();
  }
  AutoJSContext cx;
  nsGlobalWindowInner* sourceWin = nsGlobalWindowInner::Cast(sourceWindow);
  JSAutoRealm ar(cx, sourceWin->GetGlobalJSObject());

  // Resolve the baseURI, which could be relative to the calling window.
  //
  // Note the algorithm to get the base URI should match the one
  // used to actually kick off the load in nsWindowWatcher.cpp.
  nsCOMPtr<nsIDocument> doc = sourceWindow->GetDoc();
  nsIURI* baseURI = nullptr;
  auto encoding = UTF_8_ENCODING; // default to utf-8
  if (doc) {
    baseURI = doc->GetDocBaseURI();
    encoding = doc->GetDocumentCharacterSet();
  }
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), nsDependentCString(aURL),
                          encoding, baseURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  if (NS_FAILED(nsContentUtils::GetSecurityManager()->
        CheckLoadURIFromScript(cx, uri))) {
    return NS_ERROR_FAILURE;
  }

  uri.forget(aURI);
  return NS_OK;
}

bool
nsGlobalWindowOuter::IsPrivateBrowsing()
{
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(GetDocShell());
  return loadContext && loadContext->UsePrivateBrowsing();
}

void
nsGlobalWindowOuter::FlushPendingNotifications(FlushType aType)
{
  if (mDoc) {
    mDoc->FlushPendingNotifications(aType);
  }
}

void
nsGlobalWindowOuter::EnsureSizeAndPositionUpToDate()
{
  // If we're a subframe, make sure our size is up to date.  It's OK that this
  // crosses the content/chrome boundary, since chrome can have pending reflows
  // too.
  nsGlobalWindowOuter *parent = nsGlobalWindowOuter::Cast(GetPrivateParent());
  if (parent) {
    parent->FlushPendingNotifications(FlushType::Layout);
  }
}

already_AddRefed<nsISupports>
nsGlobalWindowOuter::SaveWindowState()
{
  if (!mContext || !GetWrapperPreserveColor()) {
    // The window may be getting torn down; don't bother saving state.
    return nullptr;
  }

  nsGlobalWindowInner *inner = GetCurrentInnerWindowInternal();
  NS_ASSERTION(inner, "No inner window to save");

  // Don't do anything else to this inner window! After this point, all
  // calls to SetTimeoutOrInterval will create entries in the timeout
  // list that will only run after this window has come out of the bfcache.
  // Also, while we're frozen, we won't dispatch online/offline events
  // to the page.
  inner->Freeze();

  nsCOMPtr<nsISupports> state = new WindowStateHolder(inner);

#ifdef DEBUG_PAGE_CACHE
  printf("saving window state, state = %p\n", (void*)state);
#endif

  return state.forget();
}

nsresult
nsGlobalWindowOuter::RestoreWindowState(nsISupports *aState)
{
  if (!mContext || !GetWrapperPreserveColor()) {
    // The window may be getting torn down; don't bother restoring state.
    return NS_OK;
  }

  nsCOMPtr<WindowStateHolder> holder = do_QueryInterface(aState);
  NS_ENSURE_TRUE(holder, NS_ERROR_FAILURE);

#ifdef DEBUG_PAGE_CACHE
  printf("restoring window state, state = %p\n", (void*)holder);
#endif

  // And we're ready to go!
  nsGlobalWindowInner *inner = GetCurrentInnerWindowInternal();

  // if a link is focused, refocus with the FLAG_SHOWRING flag set. This makes
  // it easy to tell which link was last clicked when going back a page.
  Element* focusedElement = inner->GetFocusedElement();
  if (nsContentUtils::ContentIsLink(focusedElement)) {
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm) {
      // XXXbz Do we need the stack strong ref here?
      RefPtr<Element> kungFuDeathGrip(focusedElement);
      fm->SetFocus(kungFuDeathGrip, nsIFocusManager::FLAG_NOSCROLL |
                                    nsIFocusManager::FLAG_SHOWRING);
    }
  }

  inner->Thaw();

  holder->DidRestoreWindow();

  return NS_OK;
}

// XXX(nika): Can we remove these?
void
nsGlobalWindowOuter::EventListenerAdded(nsAtom* aType)
{
}

void
nsGlobalWindowOuter::EventListenerRemoved(nsAtom* aType)
{
}

void
nsGlobalWindowOuter::AddSizeOfIncludingThis(nsWindowSizes& aWindowSizes) const
{
  aWindowSizes.mDOMOtherSize += aWindowSizes.mState.mMallocSizeOf(this);
}

bool
nsGlobalWindowOuter::UpdateVRDisplays(nsTArray<RefPtr<mozilla::dom::VRDisplay>>& aDevices)
{
  FORWARD_TO_INNER(UpdateVRDisplays, (aDevices), false);
}

uint32_t
nsGlobalWindowOuter::GetAutoActivateVRDisplayID()
{
  uint32_t retVal = mAutoActivateVRDisplayID;
  mAutoActivateVRDisplayID = 0;
  return retVal;
}

void
nsGlobalWindowOuter::SetAutoActivateVRDisplayID(uint32_t aAutoActivateVRDisplayID)
{
  mAutoActivateVRDisplayID = aAutoActivateVRDisplayID;
}

already_AddRefed<nsWindowRoot>
nsGlobalWindowOuter::GetWindowRootOuter()
{
  nsCOMPtr<nsPIWindowRoot> root = GetTopWindowRoot();
  return root.forget().downcast<nsWindowRoot>();
}

//Note: This call will lock the cursor, it will not change as it moves.
//To unlock, the cursor must be set back to CURSOR_AUTO.
void
nsGlobalWindowOuter::SetCursorOuter(const nsAString& aCursor, ErrorResult& aError)
{
  int32_t cursor;

  if (aCursor.EqualsLiteral("auto"))
    cursor = NS_STYLE_CURSOR_AUTO;
  else {
    nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(aCursor);
    if (!nsCSSProps::FindKeyword(keyword, nsCSSProps::kCursorKTable, cursor)) {
      return;
    }
  }

  RefPtr<nsPresContext> presContext;
  if (mDocShell) {
    mDocShell->GetPresContext(getter_AddRefs(presContext));
  }

  if (presContext) {
    // Need root widget.
    nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();
    if (!presShell) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    nsViewManager* vm = presShell->GetViewManager();
    if (!vm) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    nsView* rootView = vm->GetRootView();
    if (!rootView) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    nsIWidget* widget = rootView->GetNearestWidget(nullptr);
    if (!widget) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    // Call esm and set cursor.
    aError = presContext->EventStateManager()->SetCursor(cursor, nullptr,
                                                         false, 0.0f, 0.0f,
                                                         widget, true);
  }
}

NS_IMETHODIMP
nsGlobalWindowOuter::GetBrowserDOMWindow(nsIBrowserDOMWindow **aBrowserWindow)
{
  MOZ_RELEASE_ASSERT(IsChromeWindow());
  FORWARD_TO_INNER(GetBrowserDOMWindow, (aBrowserWindow), NS_ERROR_UNEXPECTED);
}

nsIBrowserDOMWindow*
nsGlobalWindowOuter::GetBrowserDOMWindowOuter()
{
  MOZ_ASSERT(IsChromeWindow());
  return mChromeFields.mBrowserDOMWindow;
}

void
nsGlobalWindowOuter::SetBrowserDOMWindowOuter(nsIBrowserDOMWindow* aBrowserWindow)
{
  MOZ_ASSERT(IsChromeWindow());
  mChromeFields.mBrowserDOMWindow = aBrowserWindow;
}

ChromeMessageBroadcaster*
nsGlobalWindowOuter::GetMessageManager()
{
  if (!mInnerWindow) {
    NS_WARNING("No inner window available!");
    return nullptr;
  }
  return GetCurrentInnerWindowInternal()->MessageManager();
}

ChromeMessageBroadcaster*
nsGlobalWindowOuter::GetGroupMessageManager(const nsAString& aGroup)
{
  if (!mInnerWindow) {
    NS_WARNING("No inner window available!");
    return nullptr;
  }
  return GetCurrentInnerWindowInternal()->GetGroupMessageManager(aGroup);
}

void
nsPIDOMWindowOuter::SetOpenerForInitialContentBrowser(nsPIDOMWindowOuter* aOpenerWindow)
{
  MOZ_ASSERT(!mOpenerForInitialContentBrowser,
             "Don't set OpenerForInitialContentBrowser twice!");
  mOpenerForInitialContentBrowser = aOpenerWindow;
}

already_AddRefed<nsPIDOMWindowOuter>
nsPIDOMWindowOuter::TakeOpenerForInitialContentBrowser()
{
  // Intentionally forget our own member
  return mOpenerForInitialContentBrowser.forget();
}

void
nsGlobalWindowOuter::InitWasOffline()
{
  mWasOffline = NS_IsOffline();
}

#if defined(MOZ_WIDGET_ANDROID)
int16_t
nsGlobalWindowOuter::Orientation(CallerType aCallerType) const
{
  return nsContentUtils::ResistFingerprinting(aCallerType) ?
           0 : WindowOrientationObserver::OrientationAngle();
}
#endif

void
nsPIDOMWindowOuter::SetLargeAllocStatus(LargeAllocStatus aStatus)
{
  MOZ_ASSERT(mLargeAllocStatus == LargeAllocStatus::NONE);
  mLargeAllocStatus = aStatus;
}

bool
nsPIDOMWindowOuter::IsTopLevelWindow()
{
  return nsGlobalWindowOuter::Cast(this)->IsTopLevelWindow();
}

bool
nsPIDOMWindowOuter::HadOriginalOpener() const
{
  return nsGlobalWindowOuter::Cast(this)->HadOriginalOpener();
}

void
nsGlobalWindowOuter::ReportLargeAllocStatus()
{
  uint32_t errorFlags = nsIScriptError::warningFlag;
  const char* message = nullptr;

  switch (mLargeAllocStatus) {
    case LargeAllocStatus::SUCCESS:
      // Override the error flags such that the success message isn't reported
      // as a warning.
      errorFlags = nsIScriptError::infoFlag;
      message = "LargeAllocationSuccess";
      break;
    case LargeAllocStatus::NON_WIN32:
      errorFlags = nsIScriptError::infoFlag;
      message = "LargeAllocationNonWin32";
      break;
    case LargeAllocStatus::NON_GET:
      message = "LargeAllocationNonGetRequest";
      break;
    case LargeAllocStatus::NON_E10S:
      message = "LargeAllocationNonE10S";
      break;
    case LargeAllocStatus::NOT_ONLY_TOPLEVEL_IN_TABGROUP:
      message = "LargeAllocationNotOnlyToplevelInTabGroup";
      break;
    default: // LargeAllocStatus::NONE
      return; // Don't report a message to the console
  }

  nsContentUtils::ReportToConsole(errorFlags,
                                  NS_LITERAL_CSTRING("DOM"),
                                  mDoc,
                                  nsContentUtils::eDOM_PROPERTIES,
                                  message);
}

#ifdef _WINDOWS_
#error "Never include windows.h in this file!"
#endif

// Helper called by methods that move/resize the window,
// to ensure the presContext (if any) is aware of resolution
// change that may happen in multi-monitor configuration.
void
nsGlobalWindowOuter::CheckForDPIChange()
{
  if (mDocShell) {
    RefPtr<nsPresContext> presContext;
    mDocShell->GetPresContext(getter_AddRefs(presContext));
    if (presContext) {
      if (presContext->DeviceContext()->CheckDPIChange()) {
        presContext->UIResolutionChanged();
      }
    }
  }
}

mozilla::dom::TabGroup*
nsGlobalWindowOuter::TabGroupOuter()
{
  // Outer windows lazily join TabGroups when requested. This is usually done
  // because a document is getting its NodePrincipal, and asking for the
  // TabGroup to determine its DocGroup.
  if (!mTabGroup) {
    // Get mOpener ourselves, instead of relying on GetOpenerWindowOuter,
    // because that way we dodge the LegacyIsCallerChromeOrNativeCode() call
    // which we want to return false.
    nsCOMPtr<nsPIDOMWindowOuter> piOpener = do_QueryReferent(mOpener);
    nsPIDOMWindowOuter* opener = GetSanitizedOpener(piOpener);
    nsPIDOMWindowOuter* parent = GetScriptableParentOrNull();
    MOZ_ASSERT(!parent || !opener, "Only one of parent and opener may be provided");

    mozilla::dom::TabGroup* toJoin = nullptr;
    if (GetDocShell()->ItemType() == nsIDocShellTreeItem::typeChrome) {
      toJoin = TabGroup::GetChromeTabGroup();
    } else if (opener) {
      toJoin = opener->TabGroup();
    } else if (parent) {
      toJoin = parent->TabGroup();
    } else {
      toJoin = TabGroup::GetFromWindow(this);
    }

#ifdef DEBUG
    // Make sure that, if we have a tab group from the actor, it matches the one
    // we're planning to join.
    mozilla::dom::TabGroup* testGroup = TabGroup::GetFromWindow(this);
    MOZ_ASSERT_IF(testGroup, testGroup == toJoin);
#endif

    mTabGroup = mozilla::dom::TabGroup::Join(this, toJoin);
  }
  MOZ_ASSERT(mTabGroup);

#ifdef DEBUG
  // Ensure that we don't recurse forever
  if (!mIsValidatingTabGroup) {
    mIsValidatingTabGroup = true;
    // We only need to do this check if we aren't in the chrome tab group
    if (mIsChrome) {
      MOZ_ASSERT(mTabGroup == TabGroup::GetChromeTabGroup());
    } else {
      // Sanity check that our tabgroup matches our opener or parent.
      RefPtr<nsPIDOMWindowOuter> parent = GetScriptableParentOrNull();
      MOZ_ASSERT_IF(parent, parent->TabGroup() == mTabGroup);
      nsCOMPtr<nsPIDOMWindowOuter> piOpener = do_QueryReferent(mOpener);
      nsPIDOMWindowOuter* opener = GetSanitizedOpener(piOpener);
      MOZ_ASSERT_IF(opener && nsGlobalWindowOuter::Cast(opener) != this,
                    opener->TabGroup() == mTabGroup);
    }
    mIsValidatingTabGroup = false;
  }
#endif

  return mTabGroup;
}

nsresult
nsGlobalWindowOuter::Dispatch(TaskCategory aCategory,
                              already_AddRefed<nsIRunnable>&& aRunnable)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (GetDocGroup()) {
    return GetDocGroup()->Dispatch(aCategory, std::move(aRunnable));
  }
  return DispatcherTrait::Dispatch(aCategory, std::move(aRunnable));
}

nsISerialEventTarget*
nsGlobalWindowOuter::EventTargetFor(TaskCategory aCategory) const
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (GetDocGroup()) {
    return GetDocGroup()->EventTargetFor(aCategory);
  }
  return DispatcherTrait::EventTargetFor(aCategory);
}

AbstractThread*
nsGlobalWindowOuter::AbstractMainThreadFor(TaskCategory aCategory)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (GetDocGroup()) {
    return GetDocGroup()->AbstractMainThreadFor(aCategory);
  }
  return DispatcherTrait::AbstractMainThreadFor(aCategory);
}

nsGlobalWindowOuter::TemporarilyDisableDialogs::TemporarilyDisableDialogs(
  nsGlobalWindowOuter* aWindow MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : mSavedDialogsEnabled(false)
{
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;

  MOZ_ASSERT(aWindow);
  nsGlobalWindowOuter* topWindowOuter = aWindow->GetScriptableTopInternal();
  if (!topWindowOuter) {
    NS_ERROR("nsGlobalWindowOuter::TemporarilyDisableDialogs used without a top "
             "window?");
    return;
  }

  // TODO: Warn if no top window?
  nsGlobalWindowInner* topWindow =
    topWindowOuter->GetCurrentInnerWindowInternal();
  if (topWindow) {
    mTopWindow = topWindow;
    mSavedDialogsEnabled = mTopWindow->mAreDialogsEnabled;
    mTopWindow->mAreDialogsEnabled = false;
  }
}

nsGlobalWindowOuter::TemporarilyDisableDialogs::~TemporarilyDisableDialogs()
{
  if (mTopWindow) {
    mTopWindow->mAreDialogsEnabled = mSavedDialogsEnabled;
  }
}

mozilla::dom::TabGroup*
nsPIDOMWindowOuter::TabGroup()
{
  return nsGlobalWindowOuter::Cast(this)->TabGroupOuter();
}

/* static */ already_AddRefed<nsGlobalWindowOuter>
nsGlobalWindowOuter::Create(bool aIsChrome)
{
  RefPtr<nsGlobalWindowOuter> window = new nsGlobalWindowOuter();
  if (aIsChrome) {
    window->mIsChrome = true;
  }

  window->InitWasOffline();
  return window.forget();
}

nsIURI*
nsPIDOMWindowOuter::GetDocumentURI() const
{
  return mDoc ? mDoc->GetDocumentURI() : mDocumentURI.get();
}


nsIURI*
nsPIDOMWindowOuter::GetDocBaseURI() const
{
  return mDoc ? mDoc->GetDocBaseURI() : mDocBaseURI.get();
}

void
nsPIDOMWindowOuter::MaybeCreateDoc()
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

mozilla::dom::DocGroup*
nsPIDOMWindowOuter::GetDocGroup() const
{
  nsIDocument* doc = GetExtantDoc();
  if (doc) {
    return doc->GetDocGroup();
  }
  return nullptr;
}

// XXX: Can we define this in a header instead of here?
namespace mozilla {
namespace dom {
extern uint64_t
NextWindowID();
} // namespace dom
} // namespace mozilla

nsPIDOMWindowOuter::nsPIDOMWindowOuter()
  : mFrameElement(nullptr)
  , mDocShell(nullptr)
  , mModalStateDepth(0)
  , mIsActive(false)
  , mIsBackground(false)
  , mMediaSuspend(
      Preferences::GetBool("media.block-autoplay-until-in-foreground", true)
        ? nsISuspendedTypes::SUSPENDED_BLOCK
        : nsISuspendedTypes::NONE_SUSPENDED)
  , mAudioMuted(false)
  , mAudioVolume(1.0)
  , mDesktopModeViewport(false)
  , mIsRootOuterWindow(false)
  , mInnerWindow(nullptr)
  ,
  // Make sure no actual window ends up with mWindowID == 0
  mWindowID(NextWindowID())
  , mMarkedCCGeneration(0)
  , mServiceWorkersTestingEnabled(false)
  , mLargeAllocStatus(LargeAllocStatus::NONE)
{
}

nsPIDOMWindowOuter::~nsPIDOMWindowOuter() {}

nsAutoPopupStatePusherInternal::nsAutoPopupStatePusherInternal(PopupControlState aState, bool aForce)
  : mOldState(nsContentUtils::PushPopupControlState(aState, aForce))
{
}

nsAutoPopupStatePusherInternal::~nsAutoPopupStatePusherInternal()
{
  nsContentUtils::PopPopupControlState(mOldState);
}
