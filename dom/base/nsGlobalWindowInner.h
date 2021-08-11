/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGlobalWindowInner_h___
#define nsGlobalWindowInner_h___

#include "nsPIDOMWindow.h"

#include "nsHashKeys.h"
#include "nsRefPtrHashtable.h"
#include "nsInterfaceHashtable.h"

// Local Includes
// Helper Classes
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsTHashMap.h"
#include "nsCycleCollectionParticipant.h"

// Interfaces Needed
#include "nsIBrowserDOMWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIDOMChromeWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "mozilla/EventListenerManager.h"
#include "nsIPrincipal.h"
#include "nsSize.h"
#include "mozilla/FlushType.h"
#include "prclist.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ChromeMessageBroadcaster.h"
#include "mozilla/dom/DebuggerNotificationManager.h"
#include "mozilla/dom/GamepadHandle.h"
#include "mozilla/dom/Location.h"
#include "mozilla/dom/NavigatorBinding.h"
#include "mozilla/dom/StorageEvent.h"
#include "mozilla/dom/StorageEventBinding.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/CallState.h"
#include "mozilla/Attributes.h"
#include "mozilla/LinkedList.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/StorageAccess.h"
#include "mozilla/TimeStamp.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/glean/bindings/Glean.h"
#include "mozilla/glean/bindings/GleanPings.h"
#include "Units.h"
#include "nsComponentManagerUtils.h"
#include "nsSize.h"
#include "nsCheapSets.h"
#include "mozilla/dom/ImageBitmapSource.h"
#include "mozilla/UniquePtr.h"
#include "nsRefreshObservers.h"
#include "nsThreadUtils.h"

class nsIArray;
class nsIBaseWindow;
class nsIContent;
class nsICSSDeclaration;
class nsIDocShellTreeOwner;
class nsIDOMWindowUtils;
class nsDOMOfflineResourceList;
class nsIScrollableFrame;
class nsIControllers;
class nsIScriptContext;
class nsIScriptTimeoutHandler;
class nsIBrowserChild;
class nsIPrintSettings;
class nsITimeoutHandler;
class nsIWebBrowserChrome;
class nsIWebProgressListener;
class mozIDOMWindowProxy;

class nsScreen;
class nsHistory;
class nsGlobalWindowObserver;
class nsGlobalWindowOuter;
class nsDOMWindowUtils;
class nsIUserIdleService;
struct nsRect;

class nsWindowSizes;

class IdleRequestExecutor;

class DialogValueHolder;

class PromiseDocumentFlushedResolver;

namespace mozilla {
class AbstractThread;
class ErrorResult;

namespace dom {
class BarProp;
class BrowsingContext;
struct ChannelPixelLayout;
class ClientSource;
class Console;
class Crypto;
class CustomElementRegistry;
class DocGroup;
class External;
class Function;
class Gamepad;
class ContentMediaController;
enum class ImageBitmapFormat : uint8_t;
class IdleRequest;
class IdleRequestCallback;
class IncrementalRunnable;
class InstallTriggerImpl;
class IntlUtils;
class MediaQueryList;
class OwningExternalOrWindowProxy;
class Promise;
class PostMessageEvent;
struct RequestInit;
class RequestOrUSVString;
class SharedWorker;
class Selection;
class SpeechSynthesis;
class Timeout;
class U2F;
class VisualViewport;
class VRDisplay;
enum class VRDisplayEventReason : uint8_t;
class VREventObserver;
class WakeLock;
#if defined(MOZ_WIDGET_ANDROID)
class WindowOrientationObserver;
#endif
struct WindowPostMessageOptions;
class Worklet;
namespace cache {
class CacheStorage;
}  // namespace cache
class IDBFactory;
}  // namespace dom
}  // namespace mozilla

extern already_AddRefed<nsIScriptTimeoutHandler> NS_CreateJSTimeoutHandler(
    JSContext* aCx, nsGlobalWindowInner* aWindow,
    mozilla::dom::Function& aFunction,
    const mozilla::dom::Sequence<JS::Value>& aArguments,
    mozilla::ErrorResult& aError);

extern already_AddRefed<nsIScriptTimeoutHandler> NS_CreateJSTimeoutHandler(
    JSContext* aCx, nsGlobalWindowInner* aWindow, const nsAString& aExpression,
    mozilla::ErrorResult& aError);

extern const JSClass OuterWindowProxyClass;

//*****************************************************************************
// nsGlobalWindowInner: Global Object for Scripting
//*****************************************************************************

// nsGlobalWindowInner inherits PRCList for maintaining a list of all inner
// windows still in memory for any given outer window. This list is needed to
// ensure that mOuterWindow doesn't end up dangling. The nature of PRCList means
// that the window itself is always in the list, and an outer window's list will
// also contain all inner window objects that are still in memory (and in
// reality all inner window object's lists also contain its outer and all other
// inner windows belonging to the same outer window, but that's an unimportant
// side effect of inheriting PRCList).

class nsGlobalWindowInner final : public mozilla::dom::EventTarget,
                                  public nsPIDOMWindowInner,
                                  private nsIDOMWindow
    // NOTE: This interface is private, as it's only
    // implemented on chrome windows.
    ,
                                  private nsIDOMChromeWindow,
                                  public nsIScriptGlobalObject,
                                  public nsIScriptObjectPrincipal,
                                  public nsSupportsWeakReference,
                                  public nsIInterfaceRequestor,
                                  public PRCListStr {
 public:
  using RemoteProxy = mozilla::dom::BrowsingContext;

  using TimeStamp = mozilla::TimeStamp;
  using TimeDuration = mozilla::TimeDuration;

  using InnerWindowByIdTable =
      nsTHashMap<nsUint64HashKey, nsGlobalWindowInner*>;

  static void AssertIsOnMainThread()
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  static nsGlobalWindowInner* Cast(nsPIDOMWindowInner* aPIWin) {
    return static_cast<nsGlobalWindowInner*>(aPIWin);
  }
  static const nsGlobalWindowInner* Cast(const nsPIDOMWindowInner* aPIWin) {
    return static_cast<const nsGlobalWindowInner*>(aPIWin);
  }
  static nsGlobalWindowInner* Cast(mozIDOMWindow* aWin) {
    return Cast(nsPIDOMWindowInner::From(aWin));
  }

  static nsGlobalWindowInner* GetInnerWindowWithId(uint64_t aInnerWindowID) {
    AssertIsOnMainThread();

    if (!sInnerWindowsById) {
      return nullptr;
    }

    nsGlobalWindowInner* innerWindow = sInnerWindowsById->Get(aInnerWindowID);
    return innerWindow;
  }

  static InnerWindowByIdTable* GetWindowsTable() {
    AssertIsOnMainThread();

    return sInnerWindowsById;
  }

  static nsGlobalWindowInner* FromSupports(nsISupports* supports) {
    // Make sure this matches the casts we do in QueryInterface().
    return (nsGlobalWindowInner*)(mozilla::dom::EventTarget*)supports;
  }

  static already_AddRefed<nsGlobalWindowInner> Create(
      nsGlobalWindowOuter* aOuter, bool aIsChrome,
      mozilla::dom::WindowGlobalChild* aActor);

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> aGivenProto) override {
    return GetWrapper();
  }

  // nsIGlobalObject
  JSObject* GetGlobalJSObject() final { return GetWrapper(); }
  JSObject* GetGlobalJSObjectPreserveColor() const final {
    return GetWrapperPreserveColor();
  }
  // The HasJSGlobal on nsIGlobalObject ends up having to do a virtual
  // call to GetGlobalJSObjectPreserveColor(), because when it's
  // making the call it doesn't know it's doing it on an
  // nsGlobalWindowInner.  Add a version here that can be entirely
  // non-virtual.
  bool HasJSGlobal() const { return GetGlobalJSObjectPreserveColor(); }

  void TraceGlobalJSObject(JSTracer* aTrc);

  virtual nsresult EnsureScriptEnvironment() override;

  virtual nsIScriptContext* GetScriptContext() override;

  virtual bool IsBlackForCC(bool aTracingNeeded = true) override;

  // nsIScriptObjectPrincipal
  virtual nsIPrincipal* GetPrincipal() override;

  virtual nsIPrincipal* GetEffectiveStoragePrincipal() override;

  virtual nsIPrincipal* PartitionedPrincipal() override;

  // nsIDOMWindow
  NS_DECL_NSIDOMWINDOW

  // nsIDOMChromeWindow (only implemented on chrome windows)
  NS_DECL_NSIDOMCHROMEWINDOW

  void CaptureEvents();
  void ReleaseEvents();
  void Dump(const nsAString& aStr);
  void SetResizable(bool aResizable) const;

  virtual mozilla::EventListenerManager* GetExistingListenerManager()
      const override;

  virtual mozilla::EventListenerManager* GetOrCreateListenerManager() override;

  mozilla::Maybe<mozilla::dom::EventCallbackDebuggerNotificationType>
  GetDebuggerNotificationType() const override;

  bool ComputeDefaultWantsUntrusted(mozilla::ErrorResult& aRv) final;

  virtual nsPIDOMWindowOuter* GetOwnerGlobalForBindingsInternal() override;

  virtual nsIGlobalObject* GetOwnerGlobal() const override;

  EventTarget* GetTargetForDOMEvent() override;

  using mozilla::dom::EventTarget::DispatchEvent;
  bool DispatchEvent(mozilla::dom::Event& aEvent,
                     mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aRv) override;

  void GetEventTargetParent(mozilla::EventChainPreVisitor& aVisitor) override;

  nsresult PostHandleEvent(mozilla::EventChainPostVisitor& aVisitor) override;

  void Suspend(bool aIncludeSubWindows = true);
  void Resume(bool aIncludeSubWindows = true);
  virtual bool IsSuspended() const override;

  // Calling Freeze() on a window will automatically Suspend() it.  In
  // addition, the window and its children (if aIncludeSubWindows is true) are
  // further treated as no longer suitable for interaction with the user.  For
  // example, it may be marked non-visible, cannot be focused, etc.  All worker
  // threads are also frozen bringing them to a complete stop.  A window can
  // have Freeze() called multiple times and will only thaw after a matching
  // number of Thaw() calls.
  void Freeze(bool aIncludeSubWindows = true);
  void Thaw(bool aIncludeSubWindows = true);
  virtual bool IsFrozen() const override;
  void SyncStateFromParentWindow();

  mozilla::dom::DebuggerNotificationManager*
  GetOrCreateDebuggerNotificationManager() override;

  mozilla::dom::DebuggerNotificationManager*
  GetExistingDebuggerNotificationManager() override;

  mozilla::Maybe<mozilla::dom::ClientInfo> GetClientInfo() const override;
  mozilla::Maybe<mozilla::dom::ClientState> GetClientState() const;
  mozilla::Maybe<mozilla::dom::ServiceWorkerDescriptor> GetController()
      const override;

  void SetCsp(nsIContentSecurityPolicy* aCsp);
  void SetPreloadCsp(nsIContentSecurityPolicy* aPreloadCsp);
  nsIContentSecurityPolicy* GetCsp();

  virtual RefPtr<mozilla::dom::ServiceWorker> GetOrCreateServiceWorker(
      const mozilla::dom::ServiceWorkerDescriptor& aDescriptor) override;

  RefPtr<mozilla::dom::ServiceWorkerRegistration> GetServiceWorkerRegistration(
      const mozilla::dom::ServiceWorkerRegistrationDescriptor& aDescriptor)
      const override;

  RefPtr<mozilla::dom::ServiceWorkerRegistration>
  GetOrCreateServiceWorkerRegistration(
      const mozilla::dom::ServiceWorkerRegistrationDescriptor& aDescriptor)
      override;

  void NoteCalledRegisterForServiceWorkerScope(const nsACString& aScope);

  void NoteDOMContentLoaded();

  virtual nsresult FireDelayedDOMEvents(bool aIncludeSubWindows) override;

  virtual void MaybeUpdateTouchState() override;

  // Inner windows only.
  void RefreshRealmPrincipal();

  // For accessing protected field mFullscreen
  friend class FullscreenTransitionTask;

  // Inner windows only.
  virtual void SetHasGamepadEventListener(bool aHasGamepad = true) override;
  void NotifyHasXRSession();
  bool HasUsedVR() const;
  bool IsVRContentDetected() const;
  bool IsVRContentPresenting() const;
  void RequestXRPermission();
  void OnXRPermissionRequestAllow();
  void OnXRPermissionRequestCancel();

  using EventTarget::EventListenerAdded;
  virtual void EventListenerAdded(nsAtom* aType) override;
  using EventTarget::EventListenerRemoved;
  virtual void EventListenerRemoved(nsAtom* aType) override;

  // nsIInterfaceRequestor
  NS_DECL_NSIINTERFACEREQUESTOR

  // WebIDL interface.
  mozilla::dom::Nullable<mozilla::dom::WindowProxyHolder> IndexedGetter(
      uint32_t aIndex);

  static bool IsPrivilegedChromeWindow(JSContext*, JSObject* aObj);

  static bool IsRequestIdleCallbackEnabled(JSContext* aCx, JSObject*);

  static bool DeviceSensorsEnabled(JSContext*, JSObject*);

  static bool ContentPropertyEnabled(JSContext* aCx, JSObject*);

  bool DoResolve(
      JSContext* aCx, JS::Handle<JSObject*> aObj, JS::Handle<jsid> aId,
      JS::MutableHandle<mozilla::Maybe<JS::PropertyDescriptor>> aDesc);
  // The return value is whether DoResolve might end up resolving the given id.
  // If in doubt, return true.
  static bool MayResolve(jsid aId);

  void GetOwnPropertyNames(JSContext* aCx, JS::MutableHandleVector<jsid> aNames,
                           bool aEnumerableOnly, mozilla::ErrorResult& aRv);

  nsPIDOMWindowOuter* GetInProcessScriptableTop() override;
  inline nsGlobalWindowOuter* GetInProcessTopInternal();

  inline nsGlobalWindowOuter* GetInProcessScriptableTopInternal();

  already_AddRefed<mozilla::dom::BrowsingContext> GetChildWindow(
      const nsAString& aName);

  inline nsIBrowserChild* GetBrowserChild() { return mBrowserChild.get(); }

  nsIScriptContext* GetContextInternal();

  nsGlobalWindowOuter* GetOuterWindowInternal() const;

  bool IsChromeWindow() const { return mIsChrome; }

  // GetScrollFrame does not flush.  Callers should do it themselves as needed,
  // depending on which info they actually want off the scrollable frame.
  nsIScrollableFrame* GetScrollFrame();

  nsresult Observe(nsISupports* aSubject, const char* aTopic,
                   const char16_t* aData);

  void ObserveStorageNotification(mozilla::dom::StorageEvent* aEvent,
                                  const char16_t* aStorageType,
                                  bool aPrivateBrowsing);

  static void Init();
  static void ShutDown();
  static bool IsCallerChrome();

  friend class WindowStateHolder;

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(
      nsGlobalWindowInner, mozilla::dom::EventTarget)

#ifdef DEBUG
  // Call Unlink on this window. This may cause bad things to happen, so use
  // with caution.
  void RiskyUnlink();
#endif

  virtual bool TakeFocus(bool aFocus, uint32_t aFocusMethod) override;
  virtual void SetReadyForFocus() override;
  virtual void PageHidden() override;
  virtual nsresult DispatchAsyncHashchange(nsIURI* aOldURI,
                                           nsIURI* aNewURI) override;
  virtual nsresult DispatchSyncPopState() override;

  // Inner windows only.
  virtual void EnableDeviceSensor(uint32_t aType) override;
  virtual void DisableDeviceSensor(uint32_t aType) override;

#if defined(MOZ_WIDGET_ANDROID)
  virtual void EnableOrientationChangeListener() override;
  virtual void DisableOrientationChangeListener() override;
#endif

  void AddSizeOfIncludingThis(nsWindowSizes& aWindowSizes) const;

  void CollectDOMSizesForDataDocuments(nsWindowSizes&) const;
  void RegisterDataDocumentForMemoryReporting(Document*);
  void UnregisterDataDocumentForMemoryReporting(Document*);

  enum SlowScriptResponse {
    ContinueSlowScript = 0,
    ContinueSlowScriptAndKeepNotifying,
    AlwaysContinueSlowScript,
    KillSlowScript,
    KillScriptGlobal
  };
  SlowScriptResponse ShowSlowScriptDialog(JSContext* aCx,
                                          const nsString& aAddonId,
                                          const double aDuration);

  // Inner windows only.
  void AddGamepad(mozilla::dom::GamepadHandle aHandle,
                  mozilla::dom::Gamepad* aGamepad);
  void RemoveGamepad(mozilla::dom::GamepadHandle aHandle);
  void GetGamepads(nsTArray<RefPtr<mozilla::dom::Gamepad>>& aGamepads);
  already_AddRefed<mozilla::dom::Gamepad> GetGamepad(
      mozilla::dom::GamepadHandle aHandle);
  void SetHasSeenGamepadInput(bool aHasSeen);
  bool HasSeenGamepadInput();
  void SyncGamepadState();
  void StopGamepadHaptics();

  // Inner windows only.
  // Enable/disable updates for gamepad input.
  void EnableGamepadUpdates();
  void DisableGamepadUpdates();

  // Inner windows only.
  // Enable/disable updates for VR
  void EnableVRUpdates();
  void DisableVRUpdates();
  // Reset telemetry data when switching windows.
  // aUpdate, true for accumulating the result to the histogram.
  // false for only resetting the timestamp.
  void ResetVRTelemetry(bool aUpdate);

  void StartVRActivity();
  void StopVRActivity();

  // Update the VR displays for this window
  bool UpdateVRDisplays(nsTArray<RefPtr<mozilla::dom::VRDisplay>>& aDisplays);

  // Inner windows only.
  // Called to inform that the set of active VR displays has changed.
  void NotifyActiveVRDisplaysChanged();
  void NotifyDetectXRRuntimesCompleted();
  void NotifyPresentationGenerationChanged(uint32_t aDisplayID);

  void DispatchVRDisplayActivate(uint32_t aDisplayID,
                                 mozilla::dom::VRDisplayEventReason aReason);
  void DispatchVRDisplayDeactivate(uint32_t aDisplayID,
                                   mozilla::dom::VRDisplayEventReason aReason);
  void DispatchVRDisplayConnect(uint32_t aDisplayID);
  void DispatchVRDisplayDisconnect(uint32_t aDisplayID);
  void DispatchVRDisplayPresentChange(uint32_t aDisplayID);

#define EVENT(name_, id_, type_, struct_)                              \
  mozilla::dom::EventHandlerNonNull* GetOn##name_() {                  \
    mozilla::EventListenerManager* elm = GetExistingListenerManager(); \
    return elm ? elm->GetEventHandler(nsGkAtoms::on##name_) : nullptr; \
  }                                                                    \
  void SetOn##name_(mozilla::dom::EventHandlerNonNull* handler) {      \
    mozilla::EventListenerManager* elm = GetOrCreateListenerManager(); \
    if (elm) {                                                         \
      elm->SetEventHandler(nsGkAtoms::on##name_, handler);             \
    }                                                                  \
  }
#define ERROR_EVENT(name_, id_, type_, struct_)                          \
  mozilla::dom::OnErrorEventHandlerNonNull* GetOn##name_() {             \
    mozilla::EventListenerManager* elm = GetExistingListenerManager();   \
    return elm ? elm->GetOnErrorEventHandler() : nullptr;                \
  }                                                                      \
  void SetOn##name_(mozilla::dom::OnErrorEventHandlerNonNull* handler) { \
    mozilla::EventListenerManager* elm = GetOrCreateListenerManager();   \
    if (elm) {                                                           \
      elm->SetEventHandler(handler);                                     \
    }                                                                    \
  }
#define BEFOREUNLOAD_EVENT(name_, id_, type_, struct_)                 \
  mozilla::dom::OnBeforeUnloadEventHandlerNonNull* GetOn##name_() {    \
    mozilla::EventListenerManager* elm = GetExistingListenerManager(); \
    return elm ? elm->GetOnBeforeUnloadEventHandler() : nullptr;       \
  }                                                                    \
  void SetOn##name_(                                                   \
      mozilla::dom::OnBeforeUnloadEventHandlerNonNull* handler) {      \
    mozilla::EventListenerManager* elm = GetOrCreateListenerManager(); \
    if (elm) {                                                         \
      elm->SetEventHandler(handler);                                   \
    }                                                                  \
  }
#define WINDOW_ONLY_EVENT EVENT
#define TOUCH_EVENT EVENT
#include "mozilla/EventNameList.h"
#undef TOUCH_EVENT
#undef WINDOW_ONLY_EVENT
#undef BEFOREUNLOAD_EVENT
#undef ERROR_EVENT
#undef EVENT

  nsISupports* GetParentObject() { return nullptr; }

  static JSObject* CreateNamedPropertiesObject(JSContext* aCx,
                                               JS::Handle<JSObject*> aProto);

  mozilla::dom::WindowProxyHolder Window();
  mozilla::dom::WindowProxyHolder Self() { return Window(); }
  Document* GetDocument() { return GetDoc(); }
  void GetName(nsAString& aName, mozilla::ErrorResult& aError);
  void SetName(const nsAString& aName, mozilla::ErrorResult& aError);
  mozilla::dom::Location* Location() override;
  nsHistory* GetHistory(mozilla::ErrorResult& aError);
  mozilla::dom::CustomElementRegistry* CustomElements() override;
  mozilla::dom::CustomElementRegistry* GetExistingCustomElements();
  mozilla::dom::BarProp* GetLocationbar(mozilla::ErrorResult& aError);
  mozilla::dom::BarProp* GetMenubar(mozilla::ErrorResult& aError);
  mozilla::dom::BarProp* GetPersonalbar(mozilla::ErrorResult& aError);
  mozilla::dom::BarProp* GetScrollbars(mozilla::ErrorResult& aError);
  mozilla::dom::BarProp* GetStatusbar(mozilla::ErrorResult& aError);
  mozilla::dom::BarProp* GetToolbar(mozilla::ErrorResult& aError);
  void GetStatus(nsAString& aStatus, mozilla::ErrorResult& aError);
  void SetStatus(const nsAString& aStatus, mozilla::ErrorResult& aError);
  void Close(mozilla::dom::CallerType aCallerType,
             mozilla::ErrorResult& aError);
  nsresult Close() override;
  bool GetClosed(mozilla::ErrorResult& aError);
  void Stop(mozilla::ErrorResult& aError);
  void Focus(mozilla::dom::CallerType aCallerType,
             mozilla::ErrorResult& aError);
  nsresult Focus(mozilla::dom::CallerType aCallerType) override;
  void Blur(mozilla::dom::CallerType aCallerType, mozilla::ErrorResult& aError);
  mozilla::dom::WindowProxyHolder GetFrames(mozilla::ErrorResult& aError);
  uint32_t Length();
  mozilla::dom::Nullable<mozilla::dom::WindowProxyHolder> GetTop(
      mozilla::ErrorResult& aError);

 protected:
  explicit nsGlobalWindowInner(nsGlobalWindowOuter* aOuterWindow,
                               mozilla::dom::WindowGlobalChild* aActor);
  // Initializes the mWasOffline member variable
  void InitWasOffline();

 public:
  mozilla::dom::Nullable<mozilla::dom::WindowProxyHolder> GetOpenerWindow(
      mozilla::ErrorResult& aError);
  void GetOpener(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval,
                 mozilla::ErrorResult& aError);
  void SetOpener(JSContext* aCx, JS::Handle<JS::Value> aOpener,
                 mozilla::ErrorResult& aError);
  void GetEvent(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval);
  mozilla::dom::Nullable<mozilla::dom::WindowProxyHolder> GetParent(
      mozilla::ErrorResult& aError);
  nsPIDOMWindowOuter* GetInProcessScriptableParent() override;
  mozilla::dom::Element* GetFrameElement(nsIPrincipal& aSubjectPrincipal,
                                         mozilla::ErrorResult& aError);
  mozilla::dom::Nullable<mozilla::dom::WindowProxyHolder> Open(
      const nsAString& aUrl, const nsAString& aName, const nsAString& aOptions,
      mozilla::ErrorResult& aError);
  nsDOMOfflineResourceList* GetApplicationCache(mozilla::ErrorResult& aError);
  nsDOMOfflineResourceList* GetApplicationCache() override;

#if defined(MOZ_WIDGET_ANDROID)
  int16_t Orientation(mozilla::dom::CallerType aCallerType) const;
#endif

  already_AddRefed<mozilla::dom::Console> GetConsole(JSContext* aCx,
                                                     mozilla::ErrorResult& aRv);

  // https://w3c.github.io/webappsec-secure-contexts/#dom-window-issecurecontext
  bool IsSecureContext() const;

  void GetSidebar(mozilla::dom::OwningExternalOrWindowProxy& aResult,
                  mozilla::ErrorResult& aRv);
  mozilla::dom::External* GetExternal(mozilla::ErrorResult& aRv);

  mozilla::dom::Worklet* GetPaintWorklet(mozilla::ErrorResult& aRv);

  void GetRegionalPrefsLocales(nsTArray<nsString>& aLocales);

  void GetWebExposedLocales(nsTArray<nsString>& aLocales);

  mozilla::dom::IntlUtils* GetIntlUtils(mozilla::ErrorResult& aRv);

  void StoreSharedWorker(mozilla::dom::SharedWorker* aSharedWorker);

  void ForgetSharedWorker(mozilla::dom::SharedWorker* aSharedWorker);

 public:
  void Alert(nsIPrincipal& aSubjectPrincipal, mozilla::ErrorResult& aError);
  void Alert(const nsAString& aMessage, nsIPrincipal& aSubjectPrincipal,
             mozilla::ErrorResult& aError);
  bool Confirm(const nsAString& aMessage, nsIPrincipal& aSubjectPrincipal,
               mozilla::ErrorResult& aError);
  void Prompt(const nsAString& aMessage, const nsAString& aInitial,
              nsAString& aReturn, nsIPrincipal& aSubjectPrincipal,
              mozilla::ErrorResult& aError);
  already_AddRefed<mozilla::dom::cache::CacheStorage> GetCaches(
      mozilla::ErrorResult& aRv);
  already_AddRefed<mozilla::dom::Promise> Fetch(
      const mozilla::dom::RequestOrUSVString& aInput,
      const mozilla::dom::RequestInit& aInit,
      mozilla::dom::CallerType aCallerType, mozilla::ErrorResult& aRv);
  void Print(mozilla::ErrorResult& aError);
  mozilla::dom::Nullable<mozilla::dom::WindowProxyHolder> PrintPreview(
      nsIPrintSettings*, nsIWebProgressListener*, nsIDocShell*,
      mozilla::ErrorResult&);
  void PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                      const nsAString& aTargetOrigin,
                      const mozilla::dom::Sequence<JSObject*>& aTransfer,
                      nsIPrincipal& aSubjectPrincipal,
                      mozilla::ErrorResult& aError);
  void PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                      const mozilla::dom::WindowPostMessageOptions& aOptions,
                      nsIPrincipal& aSubjectPrincipal,
                      mozilla::ErrorResult& aError);

  MOZ_CAN_RUN_SCRIPT
  int32_t SetTimeout(JSContext* aCx, mozilla::dom::Function& aFunction,
                     int32_t aTimeout,
                     const mozilla::dom::Sequence<JS::Value>& aArguments,
                     mozilla::ErrorResult& aError);

  MOZ_CAN_RUN_SCRIPT
  int32_t SetTimeout(JSContext* aCx, const nsAString& aHandler,
                     int32_t aTimeout,
                     const mozilla::dom::Sequence<JS::Value>& /* unused */,
                     mozilla::ErrorResult& aError);

  MOZ_CAN_RUN_SCRIPT
  void ClearTimeout(int32_t aHandle);

  MOZ_CAN_RUN_SCRIPT
  int32_t SetInterval(JSContext* aCx, mozilla::dom::Function& aFunction,
                      const int32_t aTimeout,
                      const mozilla::dom::Sequence<JS::Value>& aArguments,
                      mozilla::ErrorResult& aError);

  MOZ_CAN_RUN_SCRIPT
  int32_t SetInterval(JSContext* aCx, const nsAString& aHandler,
                      const int32_t aTimeout,
                      const mozilla::dom::Sequence<JS::Value>& /* unused */,
                      mozilla::ErrorResult& aError);

  MOZ_CAN_RUN_SCRIPT
  void ClearInterval(int32_t aHandle);
  void GetOrigin(nsAString& aOrigin);

  MOZ_CAN_RUN_SCRIPT
  void ReportError(JSContext* aCx, JS::Handle<JS::Value> aError,
                   mozilla::dom::CallerType aCallerType,
                   mozilla::ErrorResult& aRv);

  void Atob(const nsAString& aAsciiBase64String, nsAString& aBinaryData,
            mozilla::ErrorResult& aError);
  void Btoa(const nsAString& aBinaryData, nsAString& aAsciiBase64String,
            mozilla::ErrorResult& aError);
  mozilla::dom::Storage* GetSessionStorage(mozilla::ErrorResult& aError);
  mozilla::dom::Storage* GetLocalStorage(mozilla::ErrorResult& aError);
  mozilla::dom::Selection* GetSelection(mozilla::ErrorResult& aError);
  mozilla::dom::IDBFactory* GetIndexedDB(mozilla::ErrorResult& aError);
  already_AddRefed<nsICSSDeclaration> GetComputedStyle(
      mozilla::dom::Element& aElt, const nsAString& aPseudoElt,
      mozilla::ErrorResult& aError) override;
  mozilla::dom::VisualViewport* VisualViewport();
  already_AddRefed<mozilla::dom::MediaQueryList> MatchMedia(
      const nsACString& aQuery, mozilla::dom::CallerType aCallerType,
      mozilla::ErrorResult& aError);
  nsScreen* GetScreen(mozilla::ErrorResult& aError);
  void MoveTo(int32_t aXPos, int32_t aYPos,
              mozilla::dom::CallerType aCallerType,
              mozilla::ErrorResult& aError);
  void MoveBy(int32_t aXDif, int32_t aYDif,
              mozilla::dom::CallerType aCallerType,
              mozilla::ErrorResult& aError);
  void ResizeTo(int32_t aWidth, int32_t aHeight,
                mozilla::dom::CallerType aCallerType,
                mozilla::ErrorResult& aError);
  void ResizeBy(int32_t aWidthDif, int32_t aHeightDif,
                mozilla::dom::CallerType aCallerType,
                mozilla::ErrorResult& aError);
  void Scroll(double aXScroll, double aYScroll);
  void Scroll(const mozilla::dom::ScrollToOptions& aOptions);
  void ScrollTo(double aXScroll, double aYScroll);
  void ScrollTo(const mozilla::dom::ScrollToOptions& aOptions);
  void ScrollBy(double aXScrollDif, double aYScrollDif);
  void ScrollBy(const mozilla::dom::ScrollToOptions& aOptions);
  void ScrollByLines(int32_t numLines,
                     const mozilla::dom::ScrollOptions& aOptions);
  void ScrollByPages(int32_t numPages,
                     const mozilla::dom::ScrollOptions& aOptions);
  void MozScrollSnap();
  void GetInnerWidth(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
                     mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  void SetInnerWidth(JSContext* aCx, JS::Handle<JS::Value> aValue,
                     mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  void GetInnerHeight(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
                      mozilla::dom::CallerType aCallerType,
                      mozilla::ErrorResult& aError);
  void SetInnerHeight(JSContext* aCx, JS::Handle<JS::Value> aValue,
                      mozilla::dom::CallerType aCallerType,
                      mozilla::ErrorResult& aError);
  double GetScrollX(mozilla::ErrorResult& aError);
  double GetPageXOffset(mozilla::ErrorResult& aError) {
    return GetScrollX(aError);
  }
  double GetScrollY(mozilla::ErrorResult& aError);
  double GetPageYOffset(mozilla::ErrorResult& aError) {
    return GetScrollY(aError);
  }

  int32_t GetScreenLeft(mozilla::dom::CallerType aCallerType,
                        mozilla::ErrorResult& aError) {
    return GetScreenX(aCallerType, aError);
  }

  int32_t GetScreenTop(mozilla::dom::CallerType aCallerType,
                       mozilla::ErrorResult& aError) {
    return GetScreenY(aCallerType, aError);
  }

  void GetScreenX(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
                  mozilla::dom::CallerType aCallerType,
                  mozilla::ErrorResult& aError);
  void SetScreenX(JSContext* aCx, JS::Handle<JS::Value> aValue,
                  mozilla::dom::CallerType aCallerType,
                  mozilla::ErrorResult& aError);
  void GetScreenY(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
                  mozilla::dom::CallerType aCallerType,
                  mozilla::ErrorResult& aError);
  void SetScreenY(JSContext* aCx, JS::Handle<JS::Value> aValue,
                  mozilla::dom::CallerType aCallerType,
                  mozilla::ErrorResult& aError);
  void GetOuterWidth(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
                     mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  void SetOuterWidth(JSContext* aCx, JS::Handle<JS::Value> aValue,
                     mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  void GetOuterHeight(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
                      mozilla::dom::CallerType aCallerType,
                      mozilla::ErrorResult& aError);
  void SetOuterHeight(JSContext* aCx, JS::Handle<JS::Value> aValue,
                      mozilla::dom::CallerType aCallerType,
                      mozilla::ErrorResult& aError);

  MOZ_CAN_RUN_SCRIPT
  int32_t RequestAnimationFrame(mozilla::dom::FrameRequestCallback& aCallback,
                                mozilla::ErrorResult& aError);

  MOZ_CAN_RUN_SCRIPT
  void CancelAnimationFrame(int32_t aHandle, mozilla::ErrorResult& aError);

  uint32_t RequestIdleCallback(JSContext* aCx,
                               mozilla::dom::IdleRequestCallback& aCallback,
                               const mozilla::dom::IdleRequestOptions& aOptions,
                               mozilla::ErrorResult& aError);
  void CancelIdleCallback(uint32_t aHandle);

#ifdef MOZ_WEBSPEECH
  mozilla::dom::SpeechSynthesis* GetSpeechSynthesis(
      mozilla::ErrorResult& aError);
  bool HasActiveSpeechSynthesis();
#endif

  mozilla::glean::Glean* Glean();
  mozilla::glean::GleanPings* GleanPings();

  already_AddRefed<nsICSSDeclaration> GetDefaultComputedStyle(
      mozilla::dom::Element& aElt, const nsAString& aPseudoElt,
      mozilla::ErrorResult& aError);
  void SizeToContent(mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  mozilla::dom::Crypto* GetCrypto(mozilla::ErrorResult& aError);
  mozilla::dom::U2F* GetU2f(mozilla::ErrorResult& aError);
  nsIControllers* GetControllers(mozilla::ErrorResult& aError);
  nsresult GetControllers(nsIControllers** aControllers) override;
  mozilla::dom::Element* GetRealFrameElement(mozilla::ErrorResult& aError);
  float GetMozInnerScreenX(mozilla::dom::CallerType aCallerType,
                           mozilla::ErrorResult& aError);
  float GetMozInnerScreenY(mozilla::dom::CallerType aCallerType,
                           mozilla::ErrorResult& aError);
  double GetDevicePixelRatio(mozilla::dom::CallerType aCallerType,
                             mozilla::ErrorResult& aError);
  int32_t GetScrollMinX(mozilla::ErrorResult& aError);
  int32_t GetScrollMinY(mozilla::ErrorResult& aError);
  int32_t GetScrollMaxX(mozilla::ErrorResult& aError);
  int32_t GetScrollMaxY(mozilla::ErrorResult& aError);
  bool GetFullScreen(mozilla::ErrorResult& aError);
  bool GetFullScreen() override;
  void SetFullScreen(bool aFullscreen, mozilla::ErrorResult& aError);
  bool Find(const nsAString& aString, bool aCaseSensitive, bool aBackwards,
            bool aWrapAround, bool aWholeWord, bool aSearchInFrames,
            bool aShowDialog, mozilla::ErrorResult& aError);
  uint64_t GetMozPaintCount(mozilla::ErrorResult& aError);

  bool ShouldResistFingerprinting();

  bool DidFireDocElemInserted() const { return mDidFireDocElemInserted; }
  void SetDidFireDocElemInserted() { mDidFireDocElemInserted = true; }

  mozilla::dom::Nullable<mozilla::dom::WindowProxyHolder> OpenDialog(
      JSContext* aCx, const nsAString& aUrl, const nsAString& aName,
      const nsAString& aOptions,
      const mozilla::dom::Sequence<JS::Value>& aExtraArgument,
      mozilla::ErrorResult& aError);
  void UpdateCommands(const nsAString& anAction, mozilla::dom::Selection* aSel,
                      int16_t aReason);

  void GetContent(JSContext* aCx, JS::MutableHandle<JSObject*> aRetval,
                  mozilla::dom::CallerType aCallerType,
                  mozilla::ErrorResult& aError);

  already_AddRefed<mozilla::dom::Promise> CreateImageBitmap(
      const mozilla::dom::ImageBitmapSource& aImage, mozilla::ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> CreateImageBitmap(
      const mozilla::dom::ImageBitmapSource& aImage, int32_t aSx, int32_t aSy,
      int32_t aSw, int32_t aSh, mozilla::ErrorResult& aRv);

  // ChromeWindow bits.  Do NOT call these unless your window is in
  // fact chrome.
  uint16_t WindowState();
  bool IsFullyOccluded();
  nsIBrowserDOMWindow* GetBrowserDOMWindow(mozilla::ErrorResult& aError);
  void SetBrowserDOMWindow(nsIBrowserDOMWindow* aBrowserWindow,
                           mozilla::ErrorResult& aError);
  void GetAttention(mozilla::ErrorResult& aError);
  void GetAttentionWithCycleCount(int32_t aCycleCount,
                                  mozilla::ErrorResult& aError);
  void SetCursor(const nsACString& aCursor, mozilla::ErrorResult& aError);
  void Maximize();
  void Minimize();
  void Restore();
  void GetWorkspaceID(nsAString& workspaceID);
  void MoveToWorkspace(const nsAString& workspaceID);
  void NotifyDefaultButtonLoaded(mozilla::dom::Element& aDefaultButton,
                                 mozilla::ErrorResult& aError);
  mozilla::dom::ChromeMessageBroadcaster* MessageManager();
  mozilla::dom::ChromeMessageBroadcaster* GetGroupMessageManager(
      const nsAString& aGroup);

  already_AddRefed<mozilla::dom::Promise> PromiseDocumentFlushed(
      mozilla::dom::PromiseDocumentFlushedCallback& aCallback,
      mozilla::ErrorResult& aError);

  void GetReturnValueOuter(JSContext* aCx,
                           JS::MutableHandle<JS::Value> aReturnValue,
                           nsIPrincipal& aSubjectPrincipal,
                           mozilla::ErrorResult& aError);
  void GetReturnValue(JSContext* aCx, JS::MutableHandle<JS::Value> aReturnValue,
                      nsIPrincipal& aSubjectPrincipal,
                      mozilla::ErrorResult& aError);
  void SetReturnValueOuter(JSContext* aCx, JS::Handle<JS::Value> aReturnValue,
                           nsIPrincipal& aSubjectPrincipal,
                           mozilla::ErrorResult& aError);
  void SetReturnValue(JSContext* aCx, JS::Handle<JS::Value> aReturnValue,
                      nsIPrincipal& aSubjectPrincipal,
                      mozilla::ErrorResult& aError);

  void GetInterface(JSContext* aCx, JS::Handle<JS::Value> aIID,
                    JS::MutableHandle<JS::Value> aRetval,
                    mozilla::ErrorResult& aError);

  already_AddRefed<nsWindowRoot> GetWindowRoot(mozilla::ErrorResult& aError);

  bool ShouldReportForServiceWorkerScope(const nsAString& aScope);

  mozilla::dom::InstallTriggerImpl* GetInstallTrigger();

  nsIDOMWindowUtils* GetWindowUtils(mozilla::ErrorResult& aRv);

  void UpdateTopInnerWindow();

  virtual bool IsInSyncOperation() override;

  // Early during inner window creation, `IsSharedMemoryAllowedInternal`
  // is called before the `mDoc` field has been initialized in order to
  // determine whether to expose the `SharedArrayBuffer` constructor on the
  // JS global. We still want to consider the document's principal to see if
  // it is a privileged extension which should be exposed to
  // `SharedArrayBuffer`, however the inner window doesn't know the document's
  // principal yet. `aPrincipalOverride` is used in that situation to provide
  // the principal for the to-be-loaded document.
  bool IsSharedMemoryAllowed() const override {
    return IsSharedMemoryAllowedInternal(
        const_cast<nsGlobalWindowInner*>(this)->GetPrincipal());
  }

  bool IsSharedMemoryAllowedInternal(nsIPrincipal* aPrincipal = nullptr) const;

  // https://whatpr.org/html/4734/structured-data.html#cross-origin-isolated
  bool CrossOriginIsolated() const override;

 protected:
  // Web IDL helpers

  // Redefine the property called aPropName on this window object to be a value
  // property with the value aValue, much like we would do for a [Replaceable]
  // property in IDL.
  void RedefineProperty(JSContext* aCx, const char* aPropName,
                        JS::Handle<JS::Value> aValue,
                        mozilla::ErrorResult& aError);

  // Implementation guts for our writable IDL attributes that are really
  // supposed to be readonly replaceable.
  template <typename T>
  using WindowCoordGetter = T (nsGlobalWindowInner::*)(
      mozilla::dom::CallerType aCallerType, mozilla::ErrorResult&);
  template <typename T>
  using WindowCoordSetter = void (nsGlobalWindowInner::*)(
      T, mozilla::dom::CallerType aCallerType, mozilla::ErrorResult&);

  template <typename T>
  void GetReplaceableWindowCoord(JSContext* aCx, WindowCoordGetter<T> aGetter,
                                 JS::MutableHandle<JS::Value> aRetval,
                                 mozilla::dom::CallerType aCallerType,
                                 mozilla::ErrorResult& aError);

  template <typename T>
  void SetReplaceableWindowCoord(JSContext* aCx, WindowCoordSetter<T> aSetter,
                                 JS::Handle<JS::Value> aValue,
                                 const char* aPropName,
                                 mozilla::dom::CallerType aCallerType,
                                 mozilla::ErrorResult& aError);
  // And the implementations of WindowCoordGetter/WindowCoordSetter.
 protected:
  double GetInnerWidth(mozilla::dom::CallerType aCallerType,
                       mozilla::ErrorResult& aError);
  nsresult GetInnerWidth(double* aWidth) override;
  void SetInnerWidth(double aInnerWidth, mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  double GetInnerHeight(mozilla::dom::CallerType aCallerType,
                        mozilla::ErrorResult& aError);
  nsresult GetInnerHeight(double* aHeight) override;
  void SetInnerHeight(double aInnerHeight, mozilla::dom::CallerType aCallerType,
                      mozilla::ErrorResult& aError);
  int32_t GetScreenX(mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  void SetScreenX(int32_t aScreenX, mozilla::dom::CallerType aCallerType,
                  mozilla::ErrorResult& aError);
  int32_t GetScreenY(mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  void SetScreenY(int32_t aScreenY, mozilla::dom::CallerType aCallerType,
                  mozilla::ErrorResult& aError);
  int32_t GetOuterWidth(mozilla::dom::CallerType aCallerType,
                        mozilla::ErrorResult& aError);
  void SetOuterWidth(int32_t aOuterWidth, mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  int32_t GetOuterHeight(mozilla::dom::CallerType aCallerType,
                         mozilla::ErrorResult& aError);
  void SetOuterHeight(int32_t aOuterHeight,
                      mozilla::dom::CallerType aCallerType,
                      mozilla::ErrorResult& aError);

  RefPtr<mozilla::dom::WakeLock> mWakeLock;

  friend class HashchangeCallback;
  friend class mozilla::dom::BarProp;

  // Object Management
  virtual ~nsGlobalWindowInner();

  void FreeInnerObjects();

  // Initialize state that depends on the document.  By this point, mDoc should
  // be set correctly and have us set as its script global object.
  void InitDocumentDependentState(JSContext* aCx);

  nsresult EnsureClientSource();
  nsresult ExecutionReady();

  // Inner windows only.
  nsresult DefineArgumentsProperty(nsIArray* aArguments);

  // Get the parent, returns null if this is a toplevel window
  nsPIDOMWindowOuter* GetInProcessParentInternal();

 private:
  template <typename Method, typename... Args>
  mozilla::CallState CallOnInProcessDescendantsInternal(
      mozilla::dom::BrowsingContext* aBrowsingContext, bool aChildOnly,
      Method aMethod, Args&&... aArgs);

  // Call the given method on the immediate children of this window.  The
  // CallState returned by the last child method invocation is returned or
  // CallState::Continue if the method returns void.
  template <typename Method, typename... Args>
  mozilla::CallState CallOnInProcessChildren(Method aMethod, Args&&... aArgs) {
    MOZ_ASSERT(IsCurrentInnerWindow());
    return CallOnInProcessDescendantsInternal(GetBrowsingContext(), true,
                                              aMethod, aArgs...);
  }

  // Call the given method on the descendant of this window.  The CallState
  // returned by the last descendant method invocation is returned or
  // CallState::Continue if the method returns void.
  template <typename Method, typename... Args>
  mozilla::CallState CallOnInProcessDescendants(Method aMethod,
                                                Args&&... aArgs) {
    MOZ_ASSERT(IsCurrentInnerWindow());
    return CallOnInProcessDescendantsInternal(GetBrowsingContext(), false,
                                              aMethod, aArgs...);
  }

  // Helper to convert a void returning child method into an implicit
  // CallState::Continue value.
  template <typename Return, typename Method, typename... Args>
  typename std::enable_if<std::is_void<Return>::value, mozilla::CallState>::type
  CallDescendant(nsGlobalWindowInner* aWindow, Method aMethod,
                 Args&&... aArgs) {
    (aWindow->*aMethod)(aArgs...);
    return mozilla::CallState::Continue;
  }

  // Helper that passes through the CallState value from a child method.
  template <typename Return, typename Method, typename... Args>
  typename std::enable_if<std::is_same<Return, mozilla::CallState>::value,
                          mozilla::CallState>::type
  CallDescendant(nsGlobalWindowInner* aWindow, Method aMethod,
                 Args&&... aArgs) {
    return (aWindow->*aMethod)(aArgs...);
  }

  void FreezeInternal(bool aIncludeSubWindows);
  void ThawInternal(bool aIncludeSubWindows);

  mozilla::CallState ShouldReportForServiceWorkerScopeInternal(
      const nsACString& aScope, bool* aResultOut);

 public:
  // Timeout Functions
  // |interval| is in milliseconds.
  MOZ_CAN_RUN_SCRIPT
  int32_t SetTimeoutOrInterval(
      JSContext* aCx, mozilla::dom::Function& aFunction, int32_t aTimeout,
      const mozilla::dom::Sequence<JS::Value>& aArguments, bool aIsInterval,
      mozilla::ErrorResult& aError);

  MOZ_CAN_RUN_SCRIPT
  int32_t SetTimeoutOrInterval(JSContext* aCx, const nsAString& aHandler,
                               int32_t aTimeout, bool aIsInterval,
                               mozilla::ErrorResult& aError);

  // Return true if |aTimeout| was cleared while its handler ran.
  MOZ_CAN_RUN_SCRIPT
  bool RunTimeoutHandler(mozilla::dom::Timeout* aTimeout,
                         nsIScriptContext* aScx);

  // Helper Functions
  already_AddRefed<nsIDocShellTreeOwner> GetTreeOwner();
  already_AddRefed<nsIWebBrowserChrome> GetWebBrowserChrome();
  bool IsPrivateBrowsing();

  void FireOfflineStatusEventIfChanged();

 public:
  // Inner windows only.
  nsresult FireHashchange(const nsAString& aOldURL, const nsAString& aNewURL);

  void FlushPendingNotifications(mozilla::FlushType aType);

  void ScrollTo(const mozilla::CSSIntPoint& aScroll,
                const mozilla::dom::ScrollOptions& aOptions);

  already_AddRefed<nsIWidget> GetMainWidget();
  nsIWidget* GetNearestWidget() const;

  bool IsInModalState();

  void SetFocusedElement(mozilla::dom::Element* aElement,
                         uint32_t aFocusMethod = 0,
                         bool aNeedsFocus = false) override;

  uint32_t GetFocusMethod() override;

  bool ShouldShowFocusRing() override;

  // Inner windows only.
  void UpdateCanvasFocus(bool aFocusChanged, nsIContent* aNewContent);

 public:
  virtual already_AddRefed<nsPIWindowRoot> GetTopWindowRoot() override;

  // Get the toplevel principal, returns null if this is a toplevel window.
  nsIPrincipal* GetTopLevelAntiTrackingPrincipal();

  // This method is called if this window loads a 3rd party tracking resource
  // and the storage is just been granted. The window can reset the partitioned
  // storage objects and switch to the first party cookie jar.
  void StorageAccessPermissionGranted();

 protected:
  static void NotifyDOMWindowDestroyed(nsGlobalWindowInner* aWindow);
  void NotifyWindowIDDestroyed(const char* aTopic);

  static void NotifyDOMWindowFrozen(nsGlobalWindowInner* aWindow);
  static void NotifyDOMWindowThawed(nsGlobalWindowInner* aWindow);

  virtual void UpdateParentTarget() override;

  void InitializeShowFocusRings();

  // Clear the document-dependent slots on our JS wrapper.  Inner windows only.
  void ClearDocumentDependentSlots(JSContext* aCx);

  // Inner windows only.
  already_AddRefed<mozilla::dom::StorageEvent> CloneStorageEvent(
      const nsAString& aType, const RefPtr<mozilla::dom::StorageEvent>& aEvent,
      mozilla::ErrorResult& aRv);

 protected:
  already_AddRefed<nsICSSDeclaration> GetComputedStyleHelper(
      mozilla::dom::Element& aElt, const nsAString& aPseudoElt,
      bool aDefaultStylesOnly, mozilla::ErrorResult& aError);

  nsGlobalWindowInner* InnerForSetTimeoutOrInterval(
      mozilla::ErrorResult& aError);

  void PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                      const nsAString& aTargetOrigin,
                      JS::Handle<JS::Value> aTransfer,
                      nsIPrincipal& aSubjectPrincipal,
                      mozilla::ErrorResult& aError);

 private:
  // Fire the JS engine's onNewGlobalObject hook.  Only used on inner windows.
  void FireOnNewGlobalObject();

  // Helper for resolving the components shim.
  bool ResolveComponentsShim(
      JSContext* aCx, JS::Handle<JSObject*> aObj,
      JS::MutableHandle<mozilla::Maybe<JS::PropertyDescriptor>> aDesc);

  // nsPIDOMWindow{Inner,Outer} should be able to see these helper methods.
  friend class nsPIDOMWindowInner;
  friend class nsPIDOMWindowOuter;

  bool IsBackgroundInternal() const;

  // NOTE: Chrome Only
  void DisconnectAndClearGroupMessageManagers() {
    MOZ_RELEASE_ASSERT(IsChromeWindow());
    for (const auto& entry : mChromeFields.mGroupMessageManagers) {
      mozilla::dom::ChromeMessageBroadcaster* mm = entry.GetWeak();
      if (mm) {
        mm->Disconnect();
      }
    }
    mChromeFields.mGroupMessageManagers.Clear();
  }

  // Call mDocumentFlushedResolvers items, and perform MicroTask checkpoint
  // after that.
  //
  // If aUntilExhaustion is true, then we call resolvers that get added as a
  // result synchronously, otherwise we wait until the next refresh driver tick.
  void CallDocumentFlushedResolvers(bool aUntilExhaustion);

  // Called after a refresh driver tick. See documentation of
  // CallDocumentFlushedResolvers for the meaning of aUntilExhaustion.
  //
  // Returns whether we need to keep observing the refresh driver or not.
  bool MaybeCallDocumentFlushedResolvers(bool aUntilExhaustion);

  // Try to fire the "load" event on our content embedder if we're an iframe.
  void FireFrameLoadEvent();

  void UpdateAutoplayPermission();
  void UpdateShortcutsPermission();
  void UpdatePopupPermission();

  void UpdatePermissions();

 public:
  static uint32_t GetShortcutsPermission(nsIPrincipal* aPrincipal);

  // Dispatch a runnable related to the global.
  virtual nsresult Dispatch(mozilla::TaskCategory aCategory,
                            already_AddRefed<nsIRunnable>&& aRunnable) override;

  virtual nsISerialEventTarget* EventTargetFor(
      mozilla::TaskCategory aCategory) const override;

  virtual mozilla::AbstractThread* AbstractMainThreadFor(
      mozilla::TaskCategory aCategory) override;

  void DisableIdleCallbackRequests();
  uint32_t LastIdleRequestHandle() const {
    return mIdleRequestCallbackCounter - 1;
  }
  MOZ_CAN_RUN_SCRIPT
  void RunIdleRequest(mozilla::dom::IdleRequest* aRequest,
                      DOMHighResTimeStamp aDeadline, bool aDidTimeout);
  MOZ_CAN_RUN_SCRIPT
  void ExecuteIdleRequest(TimeStamp aDeadline);
  void ScheduleIdleRequestDispatch();
  void SuspendIdleRequests();
  void ResumeIdleRequests();

  using IdleRequests = mozilla::LinkedList<RefPtr<mozilla::dom::IdleRequest>>;
  void RemoveIdleCallback(mozilla::dom::IdleRequest* aRequest);

  void SetActiveLoadingState(bool aIsLoading) override;

  // Hint to the JS engine whether we are currently loading.
  void HintIsLoading(bool aIsLoading);

  mozilla::dom::ContentMediaController* GetContentMediaController();

  bool TryOpenExternalProtocolIframe() {
    if (mHasOpenedExternalProtocolFrame) {
      return false;
    }
    mHasOpenedExternalProtocolFrame = true;
    return true;
  }

  nsTArray<uint32_t>& GetScrollMarks() { return mScrollMarks; }
  void SetScrollMarks(const nsTArray<uint32_t>& aScrollMarks);

  // Don't use this value directly, call StorageAccess::StorageAllowedForWindow
  // instead.
  mozilla::Maybe<mozilla::StorageAccess> GetStorageAllowedCache(
      uint32_t& aRejectedReason) {
    if (mStorageAllowedCache.isSome()) {
      aRejectedReason = mStorageAllowedReasonCache;
    }
    return mStorageAllowedCache;
  }
  void SetStorageAllowedCache(const mozilla::StorageAccess& storageAllowed,
                              uint32_t aRejectedReason) {
    mStorageAllowedCache = Some(storageAllowed);
    mStorageAllowedReasonCache = aRejectedReason;
  }
  void ClearStorageAllowedCache() {
    mStorageAllowedCache = mozilla::Nothing();
    mStorageAllowedReasonCache = 0;
  }

 private:
  RefPtr<mozilla::dom::ContentMediaController> mContentMediaController;

 protected:
  // Window offline status. Checked to see if we need to fire offline event
  bool mWasOffline : 1;

  // Represents whether the inner window's page has had a slow script notice.
  // Only used by inner windows; will always be false for outer windows.
  // This is used to implement Telemetry measures such as
  // SLOW_SCRIPT_PAGE_COUNT.
  bool mHasHadSlowScript : 1;

  // Fast way to tell if this is a chrome window (without having to QI).
  bool mIsChrome : 1;

  // Hack to indicate whether a chrome window needs its message manager
  // to be disconnected, since clean up code is shared in the global
  // window superclass.
  bool mCleanMessageManager : 1;

  // Indicates that the current document has never received a document focus
  // event.
  bool mNeedsFocus : 1;
  bool mHasFocus : 1;

  // true if tab navigation has occurred for this window. Focus rings
  // should be displayed.
  bool mFocusByKeyOccurred : 1;

  // True if we have notified document-element-inserted observers for this
  // document.
  bool mDidFireDocElemInserted : 1;

  // Indicates whether this window wants gamepad input events
  bool mHasGamepad : 1;

  // Indicates whether this window has content that has an XR session
  // An XR session results in enumeration and activation of XR devices.
  bool mHasXRSession : 1;

  // Indicates whether this window wants VRDisplayActivate events
  bool mHasVRDisplayActivateEvents : 1;

  // Indicates that a request for XR runtime detection has been
  // requested, but has not yet been resolved
  bool mXRRuntimeDetectionInFlight : 1;

  // Indicates that an XR permission request has been requested
  // but has not yet been resolved.
  bool mXRPermissionRequestInFlight : 1;

  // Indicates that an XR permission request has been granted.
  // The page should not request permission multiple times.
  bool mXRPermissionGranted : 1;

  // True if this was the currently-active inner window for a BrowsingContext at
  // the time it was discarded.
  bool mWasCurrentInnerWindow : 1;
  void SetWasCurrentInnerWindow() { mWasCurrentInnerWindow = true; }
  bool WasCurrentInnerWindow() const override { return mWasCurrentInnerWindow; }

  bool mHasSeenGamepadInput : 1;

  // Whether we told the JS engine that we were in pageload.
  bool mHintedWasLoading : 1;

  // Whether this window has opened an external-protocol iframe without user
  // activation once already. Only relevant for top windows.
  bool mHasOpenedExternalProtocolFrame : 1;

  nsCheapSet<nsUint32HashKey> mGamepadIndexSet;
  nsRefPtrHashtable<nsGenericHashKey<mozilla::dom::GamepadHandle>,
                    mozilla::dom::Gamepad>
      mGamepads;

  RefPtr<nsScreen> mScreen;

  RefPtr<mozilla::dom::BarProp> mMenubar;
  RefPtr<mozilla::dom::BarProp> mToolbar;
  RefPtr<mozilla::dom::BarProp> mLocationbar;
  RefPtr<mozilla::dom::BarProp> mPersonalbar;
  RefPtr<mozilla::dom::BarProp> mStatusbar;
  RefPtr<mozilla::dom::BarProp> mScrollbars;

  RefPtr<nsGlobalWindowObserver> mObserver;
  RefPtr<mozilla::dom::Crypto> mCrypto;
  RefPtr<mozilla::dom::U2F> mU2F;
  RefPtr<mozilla::dom::cache::CacheStorage> mCacheStorage;
  RefPtr<mozilla::dom::Console> mConsole;
  RefPtr<mozilla::dom::Worklet> mPaintWorklet;
  RefPtr<mozilla::dom::External> mExternal;
  RefPtr<mozilla::dom::InstallTriggerImpl> mInstallTrigger;

  RefPtr<mozilla::dom::Storage> mLocalStorage;
  RefPtr<mozilla::dom::Storage> mSessionStorage;

  RefPtr<mozilla::EventListenerManager> mListenerManager;
  RefPtr<mozilla::dom::Location> mLocation;
  RefPtr<nsHistory> mHistory;
  RefPtr<mozilla::dom::CustomElementRegistry> mCustomElements;

  nsTObserverArray<RefPtr<mozilla::dom::SharedWorker>> mSharedWorkers;

  RefPtr<mozilla::dom::VisualViewport> mVisualViewport;

  // The document's principals and CSP are only stored if
  // FreeInnerObjects has been called.
  nsCOMPtr<nsIPrincipal> mDocumentPrincipal;
  nsCOMPtr<nsIPrincipal> mDocumentStoragePrincipal;
  nsCOMPtr<nsIPrincipal> mDocumentPartitionedPrincipal;
  nsCOMPtr<nsIContentSecurityPolicy> mDocumentCsp;

  // Used to cache the result of StorageAccess::StorageAllowedForWindow.
  // Don't use this field directly, use StorageAccess::StorageAllowedForWindow
  // instead.
  mozilla::Maybe<mozilla::StorageAccess> mStorageAllowedCache;
  uint32_t mStorageAllowedReasonCache;

  RefPtr<mozilla::dom::DebuggerNotificationManager>
      mDebuggerNotificationManager;

  // mBrowserChild is only ever populated in the content process.
  nsCOMPtr<nsIBrowserChild> mBrowserChild;

  uint32_t mSuspendDepth;
  uint32_t mFreezeDepth;

#ifdef DEBUG
  uint32_t mSerial;
#endif

  // the method that was used to focus mFocusedElement
  uint32_t mFocusMethod;

  // The current idle request callback handle
  uint32_t mIdleRequestCallbackCounter;
  IdleRequests mIdleRequestCallbacks;
  RefPtr<IdleRequestExecutor> mIdleRequestExecutor;

#ifdef DEBUG
  nsCOMPtr<nsIURI> mLastOpenedURI;
#endif

  RefPtr<nsDOMOfflineResourceList> mApplicationCache;

  RefPtr<mozilla::dom::IDBFactory> mIndexedDB;

  // This flag keeps track of whether this window is currently
  // observing refresh notifications from the refresh driver.
  bool mObservingRefresh;

  bool mIteratingDocumentFlushedResolvers;

  bool TryToObserveRefresh();

  nsTArray<uint32_t> mEnabledSensors;

#if defined(MOZ_WIDGET_ANDROID)
  mozilla::UniquePtr<mozilla::dom::WindowOrientationObserver>
      mOrientationChangeObserver;
#endif

#ifdef MOZ_WEBSPEECH
  RefPtr<mozilla::dom::SpeechSynthesis> mSpeechSynthesis;
#endif

  RefPtr<mozilla::glean::Glean> mGlean;
  RefPtr<mozilla::glean::GleanPings> mGleanPings;

  // This is the CC generation the last time we called CanSkip.
  uint32_t mCanSkipCCGeneration;

  // The VR Displays for this window
  nsTArray<RefPtr<mozilla::dom::VRDisplay>> mVRDisplays;

  RefPtr<mozilla::dom::VREventObserver> mVREventObserver;

  // The number of unload and beforeunload even listeners registered on this
  // window.
  uint64_t mUnloadOrBeforeUnloadListenerCount = 0;

  RefPtr<mozilla::dom::IntlUtils> mIntlUtils;

  mozilla::UniquePtr<mozilla::dom::ClientSource> mClientSource;

  nsTArray<mozilla::UniquePtr<PromiseDocumentFlushedResolver>>
      mDocumentFlushedResolvers;

  nsTArray<uint32_t> mScrollMarks;

  nsTArray<nsWeakPtr> mDataDocumentsForMemoryReporting;

  static InnerWindowByIdTable* sInnerWindowsById;

  // Members in the mChromeFields member should only be used in chrome windows.
  // All accesses to this field should be guarded by a check of mIsChrome.
  struct ChromeFields {
    RefPtr<mozilla::dom::ChromeMessageBroadcaster> mMessageManager;
    nsRefPtrHashtable<nsStringHashKey, mozilla::dom::ChromeMessageBroadcaster>
        mGroupMessageManagers{1};
  } mChromeFields;

  // These fields are used by the inner and outer windows to prevent
  // programatically moving the window while the mouse is down.
  static bool sMouseDown;
  static bool sDragServiceDisabled;

  friend class nsDOMScriptableHelper;
  friend class nsDOMWindowUtils;
  friend class mozilla::dom::PostMessageEvent;
  friend class DesktopNotification;
  friend class mozilla::dom::TimeoutManager;
  friend class IdleRequestExecutor;
  friend class nsGlobalWindowOuter;
};

inline nsISupports* ToSupports(nsGlobalWindowInner* p) {
  return static_cast<mozilla::dom::EventTarget*>(p);
}

inline nsISupports* ToCanonicalSupports(nsGlobalWindowInner* p) {
  return static_cast<mozilla::dom::EventTarget*>(p);
}

// XXX: EWW - This is an awful hack - let's not do this
#include "nsGlobalWindowOuter.h"

inline nsIGlobalObject* nsGlobalWindowInner::GetOwnerGlobal() const {
  return const_cast<nsGlobalWindowInner*>(this);
}

inline nsGlobalWindowOuter* nsGlobalWindowInner::GetInProcessTopInternal() {
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
  nsCOMPtr<nsPIDOMWindowOuter> top = outer ? outer->GetInProcessTop() : nullptr;
  if (top) {
    return nsGlobalWindowOuter::Cast(top);
  }
  return nullptr;
}

inline nsGlobalWindowOuter*
nsGlobalWindowInner::GetInProcessScriptableTopInternal() {
  nsPIDOMWindowOuter* top = GetInProcessScriptableTop();
  return nsGlobalWindowOuter::Cast(top);
}

inline nsIScriptContext* nsGlobalWindowInner::GetContextInternal() {
  if (mOuterWindow) {
    return GetOuterWindowInternal()->mContext;
  }

  return nullptr;
}

inline nsGlobalWindowOuter* nsGlobalWindowInner::GetOuterWindowInternal()
    const {
  return nsGlobalWindowOuter::Cast(GetOuterWindow());
}

#endif /* nsGlobalWindowInner_h___ */
