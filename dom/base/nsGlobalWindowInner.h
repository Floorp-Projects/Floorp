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
#include "nsDataHashtable.h"
#include "nsJSThingHashtable.h"
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
#include "mozilla/dom/NavigatorBinding.h"
#include "mozilla/dom/StorageEvent.h"
#include "mozilla/dom/StorageEventBinding.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/CallState.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Attributes.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/LinkedList.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/TimeStamp.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "Units.h"
#include "nsComponentManagerUtils.h"
#include "nsSize.h"
#include "nsCheapSets.h"
#include "mozilla/dom/ImageBitmapSource.h"
#include "mozilla/UniquePtr.h"
#include "nsRefreshDriver.h"
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
class nsITimeoutHandler;
class nsIWebBrowserChrome;
class mozIDOMWindowProxy;

class nsScreen;
class nsHistory;
class nsGlobalWindowObserver;
class nsGlobalWindowOuter;
class nsDOMWindowUtils;
class nsIIdleService;
struct nsRect;

class nsWindowSizes;

class IdleRequestExecutor;

class DialogValueHolder;

class PromiseDocumentFlushedResolver;

namespace mozilla {
class AbstractThread;
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
enum class ImageBitmapFormat : uint8_t;
class IdleRequest;
class IdleRequestCallback;
class IncrementalRunnable;
class InstallTriggerImpl;
class IntlUtils;
class Location;
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
                                  public PRCListStr,
                                  public nsAPostRefreshObserver {
 public:
  typedef mozilla::dom::BrowsingContext RemoteProxy;

  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  typedef nsDataHashtable<nsUint64HashKey, nsGlobalWindowInner*>
      InnerWindowByIdTable;

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

  virtual nsIPrincipal* IntrinsicStoragePrincipal() override;

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
  GetDebuggerNotificationType() const override {
    return mozilla::Some(
        mozilla::dom::EventCallbackDebuggerNotificationType::Global);
  }

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

  void ClearActiveStoragePrincipal();

  void Suspend();
  void Resume();
  virtual bool IsSuspended() const override;

  // Calling Freeze() on a window will automatically Suspend() it.  In
  // addition, the window and its children are further treated as no longer
  // suitable for interaction with the user.  For example, it may be marked
  // non-visible, cannot be focused, etc.  All worker threads are also frozen
  // bringing them to a complete stop.  A window can have Freeze() called
  // multiple times and will only thaw after a matching number of Thaw()
  // calls.
  void Freeze();
  void Thaw();
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

  virtual nsresult FireDelayedDOMEvents() override;

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

  bool DoResolve(JSContext* aCx, JS::Handle<JSObject*> aObj,
                 JS::Handle<jsid> aId,
                 JS::MutableHandle<JS::PropertyDescriptor> aDesc);
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

  // These return true if we've reached the state in this top level window
  // where we ask the user if further dialogs should be blocked.
  //
  // DialogsAreBeingAbused must be called on the scriptable top inner window.
  //
  // nsGlobalWindowOuter::ShouldPromptToBlockDialogs is implemented in terms of
  // nsGlobalWindowInner::DialogsAreBeingAbused, and will get the scriptable top
  // inner window automatically. Inner windows only.
  bool DialogsAreBeingAbused();

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

  enum SlowScriptResponse {
    ContinueSlowScript = 0,
    ContinueSlowScriptAndKeepNotifying,
    AlwaysContinueSlowScript,
    KillSlowScript,
    KillScriptGlobal
  };
  SlowScriptResponse ShowSlowScriptDialog(JSContext* aCx,
                                          const nsString& aAddonId);

  // Inner windows only.
  void AddGamepad(uint32_t aIndex, mozilla::dom::Gamepad* aGamepad);
  void RemoveGamepad(uint32_t aIndex);
  void GetGamepads(nsTArray<RefPtr<mozilla::dom::Gamepad>>& aGamepads);
  already_AddRefed<mozilla::dom::Gamepad> GetGamepad(uint32_t aIndex);
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
  void Blur(mozilla::ErrorResult& aError);
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
      const nsAString& aQuery, mozilla::dom::CallerType aCallerType,
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
      JSContext* aCx, const mozilla::dom::ImageBitmapSource& aImage,
      mozilla::ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise> CreateImageBitmap(
      JSContext* aCx, const mozilla::dom::ImageBitmapSource& aImage,
      int32_t aSx, int32_t aSy, int32_t aSw, int32_t aSh,
      mozilla::ErrorResult& aRv);

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

  void DidRefresh() override;

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

  virtual bool IsInSyncOperation() override {
    return GetExtantDoc() && GetExtantDoc()->IsInSyncOperation();
  }

  bool IsSharedMemoryAllowed() const override;

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
  typedef int32_t (nsGlobalWindowInner::*WindowCoordGetter)(
      mozilla::dom::CallerType aCallerType, mozilla::ErrorResult&);
  typedef void (nsGlobalWindowInner::*WindowCoordSetter)(
      int32_t, mozilla::dom::CallerType aCallerType, mozilla::ErrorResult&);
  void GetReplaceableWindowCoord(JSContext* aCx, WindowCoordGetter aGetter,
                                 JS::MutableHandle<JS::Value> aRetval,
                                 mozilla::dom::CallerType aCallerType,
                                 mozilla::ErrorResult& aError);
  void SetReplaceableWindowCoord(JSContext* aCx, WindowCoordSetter aSetter,
                                 JS::Handle<JS::Value> aValue,
                                 const char* aPropName,
                                 mozilla::dom::CallerType aCallerType,
                                 mozilla::ErrorResult& aError);
  // And the implementations of WindowCoordGetter/WindowCoordSetter.
 protected:
  int32_t GetInnerWidth(mozilla::dom::CallerType aCallerType,
                        mozilla::ErrorResult& aError);
  nsresult GetInnerWidth(int32_t* aWidth) override;
  void SetInnerWidth(int32_t aInnerWidth, mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);

 protected:
  int32_t GetInnerHeight(mozilla::dom::CallerType aCallerType,
                         mozilla::ErrorResult& aError);
  nsresult GetInnerHeight(int32_t* aHeight) override;
  void SetInnerHeight(int32_t aInnerHeight,
                      mozilla::dom::CallerType aCallerType,
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

 public:
  // popup tracking
  bool IsPopupSpamWindow();

 private:
  // Call the given method on the immediate children of this window.  The
  // CallState returned by the last child method invocation is returned or
  // CallState::Continue if the method returns void.
  template <typename Method, typename... Args>
  mozilla::CallState CallOnInProcessChildren(Method aMethod, Args&... aArgs);

  // Helper to convert a void returning child method into an implicit
  // CallState::Continue value.
  template <typename Return, typename Method, typename... Args>
  typename std::enable_if<std::is_void<Return>::value, mozilla::CallState>::type
  CallChild(nsGlobalWindowInner* aWindow, Method aMethod, Args&... aArgs) {
    (aWindow->*aMethod)(aArgs...);
    return mozilla::CallState::Continue;
  }

  // Helper that passes through the CallState value from a child method.
  template <typename Return, typename Method, typename... Args>
  typename std::enable_if<std::is_same<Return, mozilla::CallState>::value,
                          mozilla::CallState>::type
  CallChild(nsGlobalWindowInner* aWindow, Method aMethod, Args&... aArgs) {
    return (aWindow->*aMethod)(aArgs...);
  }

  void FreezeInternal();
  void ThawInternal();

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

  bool IsFrame();

  already_AddRefed<nsIWidget> GetMainWidget();
  nsIWidget* GetNearestWidget() const;

  bool IsInModalState();

  virtual void SetFocusedElement(mozilla::dom::Element* aElement,
                                 uint32_t aFocusMethod = 0,
                                 bool aNeedsFocus = false) override;

  virtual uint32_t GetFocusMethod() override;

  virtual bool ShouldShowFocusRing() override;

  // Inner windows only.
  void UpdateCanvasFocus(bool aFocusChanged, nsIContent* aNewContent);

 public:
  virtual already_AddRefed<nsPIWindowRoot> GetTopWindowRoot() override;

  // Get the toplevel principal, returns null if this is a toplevel window.
  nsIPrincipal* GetTopLevelAntiTrackingPrincipal();

  // Get the parent principal, returns null if this or the parent are not a
  // toplevel window. This is mainly used to determine the anti-tracking storage
  // area.
  nsIPrincipal* GetTopLevelStorageAreaPrincipal();

  // This method is called if this window loads a 3rd party tracking resource
  // and the storage is just been granted. The window can reset the partitioned
  // storage objects and switch to the first party cookie jar.
  void StorageAccessGranted();

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
  bool ResolveComponentsShim(JSContext* aCx, JS::Handle<JSObject*> aObj,
                             JS::MutableHandle<JS::PropertyDescriptor> aDesc);

  // nsPIDOMWindow{Inner,Outer} should be able to see these helper methods.
  friend class nsPIDOMWindowInner;
  friend class nsPIDOMWindowOuter;

  bool IsBackgroundInternal() const;

  // NOTE: Chrome Only
  void DisconnectAndClearGroupMessageManagers() {
    MOZ_RELEASE_ASSERT(IsChromeWindow());
    for (auto iter = mChromeFields.mGroupMessageManagers.Iter(); !iter.Done();
         iter.Next()) {
      mozilla::dom::ChromeMessageBroadcaster* mm = iter.UserData();
      if (mm) {
        mm->Disconnect();
      }
    }
    mChromeFields.mGroupMessageManagers.Clear();
  }

  // Call or Cancel mDocumentFlushedResolvers items, and perform MicroTask
  // checkpoint after that, and adds observer if new mDocumentFlushedResolvers
  // items are added while Promise callbacks inside the checkpoint.
  template <bool call>
  void CallOrCancelDocumentFlushedResolvers();

  void CallDocumentFlushedResolvers();
  void CancelDocumentFlushedResolvers();

  // Try to fire the "load" event on our content embedder if we're an iframe.
  void FireFrameLoadEvent();

 public:
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

  typedef mozilla::LinkedList<RefPtr<mozilla::dom::IdleRequest>> IdleRequests;
  void RemoveIdleCallback(mozilla::dom::IdleRequest* aRequest);

  void SetActiveLoadingState(bool aIsLoading) override;

  // Hint to the JS engine whether we are currently loading.
  void HintIsLoading(bool aIsLoading);

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

  // when true, show focus rings for the current focused content only.
  // This will be reset when another element is focused
  bool mShowFocusRingForContent : 1;

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

  nsCheapSet<nsUint32HashKey> mGamepadIndexSet;
  nsRefPtrHashtable<nsUint32HashKey, mozilla::dom::Gamepad> mGamepads;

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
  // We need to store an nsISupports pointer to this object because the
  // mozilla::dom::External class doesn't exist on b2g and using the type
  // forward declared here means that ~nsGlobalWindow wouldn't compile because
  // it wouldn't see the ~External function's declaration.
  nsCOMPtr<nsISupports> mExternal;
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
  nsCOMPtr<nsIPrincipal> mDocumentIntrinsicStoragePrincipal;
  nsCOMPtr<nsIContentSecurityPolicy> mDocumentCsp;

  RefPtr<mozilla::dom::DebuggerNotificationManager>
      mDebuggerNotificationManager;

  // mBrowserChild is only ever populated in the content process.
  nsCOMPtr<nsIBrowserChild> mBrowserChild;

  uint32_t mSuspendDepth;
  uint32_t mFreezeDepth;

#ifdef DEBUG
  uint32_t mSerial;
#endif

  // the method that was used to focus mFocusedNode
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

  // This counts the number of windows that have been opened in rapid succession
  // (i.e. within dom.successive_dialog_time_limit of each other). It is reset
  // to 0 once a dialog is opened after dom.successive_dialog_time_limit seconds
  // have elapsed without any other dialogs.
  uint32_t mDialogAbuseCount;

  // This holds the time when the last modal dialog was shown. If more than
  // MAX_DIALOG_LIMIT dialogs are shown within the time span defined by
  // dom.successive_dialog_time_limit, we show a checkbox or confirmation prompt
  // to allow disabling of further dialogs from this window.
  TimeStamp mLastDialogQuitTime;

  // This flag keeps track of whether dialogs are
  // currently enabled on this window.
  bool mAreDialogsEnabled;

  // This flag keeps track of whether this window is currently
  // observing DidRefresh notifications from the refresh driver.
  bool mObservingDidRefresh;
  // This flag keeps track of whether or not we're going through
  // promiseDocumentFlushed resolvers. When true, promiseDocumentFlushed
  // cannot be called.
  bool mIteratingDocumentFlushedResolvers;

  nsTArray<uint32_t> mEnabledSensors;

#if defined(MOZ_WIDGET_ANDROID)
  mozilla::UniquePtr<mozilla::dom::WindowOrientationObserver>
      mOrientationChangeObserver;
#endif

#ifdef MOZ_WEBSPEECH
  RefPtr<mozilla::dom::SpeechSynthesis> mSpeechSynthesis;
#endif

  // This is the CC generation the last time we called CanSkip.
  uint32_t mCanSkipCCGeneration;

  // The VR Displays for this window
  nsTArray<RefPtr<mozilla::dom::VRDisplay>> mVRDisplays;

  RefPtr<mozilla::dom::VREventObserver> mVREventObserver;

  int64_t mBeforeUnloadListenerCount;

  RefPtr<mozilla::dom::IntlUtils> mIntlUtils;

  mozilla::UniquePtr<mozilla::dom::ClientSource> mClientSource;

  nsTArray<RefPtr<mozilla::dom::Promise>> mPendingPromises;

  nsTArray<mozilla::UniquePtr<PromiseDocumentFlushedResolver>>
      mDocumentFlushedResolvers;

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

inline bool nsGlobalWindowInner::IsPopupSpamWindow() {
  if (!mOuterWindow) {
    return false;
  }

  return GetOuterWindowInternal()->mIsPopupSpam;
}

inline bool nsGlobalWindowInner::IsFrame() {
  return GetInProcessParentInternal() != nullptr;
}

#endif /* nsGlobalWindowInner_h___ */
