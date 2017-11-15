/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGlobalWindowOuter_h___
#define nsGlobalWindowOuter_h___

#include "nsPIDOMWindow.h"

#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsRefPtrHashtable.h"
#include "nsInterfaceHashtable.h"

// Local Includes
// Helper Classes
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsWeakReference.h"
#include "nsDataHashtable.h"
#include "nsJSThingHashtable.h"
#include "nsCycleCollectionParticipant.h"

// Interfaces Needed
#include "nsIBrowserDOMWindow.h"
#include "nsIDOMEventTarget.h"
#include "nsIInterfaceRequestor.h"
#include "nsIDOMChromeWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsITimer.h"
#include "nsIDOMModalContentWindow.h"
#include "mozilla/EventListenerManager.h"
#include "nsIPrincipal.h"
#include "nsSize.h"
#include "mozilla/FlushType.h"
#include "prclist.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/StorageEvent.h"
#include "mozilla/dom/StorageEventBinding.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/ErrorResult.h"
#include "nsFrameMessageManager.h"
#include "mozilla/Attributes.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/LinkedList.h"
#include "mozilla/TimeStamp.h"
#include "nsWrapperCacheInlines.h"
#include "nsIIdleObserver.h"
#include "nsIDocument.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/WindowBinding.h"
#include "Units.h"
#include "nsComponentManagerUtils.h"
#include "nsSize.h"
#include "nsCheapSets.h"
#include "mozilla/dom/ImageBitmapSource.h"
#include "mozilla/UniquePtr.h"

class nsIArray;
class nsIBaseWindow;
class nsIContent;
class nsICSSDeclaration;
class nsIDocShellTreeOwner;
class nsIDOMOfflineResourceList;
class nsIScrollableFrame;
class nsIControllers;
class nsIJSID;
class nsIScriptContext;
class nsIScriptTimeoutHandler;
class nsITabChild;
class nsITimeoutHandler;
class nsIWebBrowserChrome;
class mozIDOMWindowProxy;

class nsDOMWindowList;
class nsScreen;
class nsHistory;
class nsGlobalWindowObserver;
class nsGlobalWindowInner;
class nsDOMWindowUtils;
class nsIIdleService;
struct nsRect;

class nsWindowSizes;

class IdleRequestExecutor;

struct IdleObserverHolder;

namespace mozilla {
class AbstractThread;
class DOMEventTargetHelper;
class ThrottledEventQueue;
namespace dom {
class BarProp;
struct ChannelPixelLayout;
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
class IntlUtils;
class Location;
class MediaQueryList;
class MozSelfSupport;
class Navigator;
class OwningExternalOrWindowProxy;
class Promise;
class PostMessageEvent;
struct RequestInit;
class RequestOrUSVString;
class Selection;
class SpeechSynthesis;
class TabGroup;
class Timeout;
class U2F;
class VRDisplay;
enum class VRDisplayEventReason : uint8_t;
class VREventObserver;
class WakeLock;
#if defined(MOZ_WIDGET_ANDROID)
class WindowOrientationObserver;
#endif
class Worklet;
namespace cache {
class CacheStorage;
} // namespace cache
class IDBFactory;
} // namespace dom
} // namespace mozilla

extern already_AddRefed<nsIScriptTimeoutHandler>
NS_CreateJSTimeoutHandler(JSContext* aCx, nsGlobalWindowInner *aWindow,
                          mozilla::dom::Function& aFunction,
                          const mozilla::dom::Sequence<JS::Value>& aArguments,
                          mozilla::ErrorResult& aError);

extern already_AddRefed<nsIScriptTimeoutHandler>
NS_CreateJSTimeoutHandler(JSContext* aCx, nsGlobalWindowInner *aWindow,
                          const nsAString& aExpression,
                          mozilla::ErrorResult& aError);

extern const js::Class OuterWindowProxyClass;

// Helper class to manage modal dialog arguments and all their quirks.
//
// Given our clunky embedding APIs, modal dialog arguments need to be passed
// as an nsISupports parameter to WindowWatcher, get stuck inside an array of
// length 1, and then passed back to the newly-created dialog.
//
// However, we need to track both the caller-passed value as well as the
// caller's, so that we can do an origin check (even for primitives) when the
// value is accessed. This class encapsulates that magic.
//
// We also use the same machinery for |returnValue|, which needs similar origin
// checks.
class DialogValueHolder final : public nsISupports
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DialogValueHolder)

  DialogValueHolder(nsIPrincipal* aSubject, nsIVariant* aValue)
    : mOrigin(aSubject)
    , mValue(aValue) {}
  nsresult Get(nsIPrincipal* aSubject, nsIVariant** aResult);
  void Get(JSContext* aCx, JS::Handle<JSObject*> aScope, nsIPrincipal* aSubject,
           JS::MutableHandle<JS::Value> aResult, mozilla::ErrorResult& aError);
private:
  virtual ~DialogValueHolder() {}

  nsCOMPtr<nsIPrincipal> mOrigin;
  nsCOMPtr<nsIVariant> mValue;
};


// NOTE: Currently this file, despite being named mozilla/dom/WindowProxy.h,
// exports the class nsGlobalWindowOuter. It will be renamed in the future to
// mozilla::dom::WindowProxy.

//*****************************************************************************
// nsGlobalWindow: Global Object for Scripting
//*****************************************************************************
// Beware that all scriptable interfaces implemented by
// nsGlobalWindow will be reachable from JS, if you make this class
// implement new interfaces you better know what you're
// doing. Security wise this is very sensitive code. --
// jst@netscape.com

// nsGlobalWindow inherits PRCList for maintaining a list of all inner
// windows still in memory for any given outer window. This list is
// needed to ensure that mOuterWindow doesn't end up dangling. The
// nature of PRCList means that the window itself is always in the
// list, and an outer window's list will also contain all inner window
// objects that are still in memory (and in reality all inner window
// object's lists also contain its outer and all other inner windows
// belonging to the same outer window, but that's an unimportant
// side effect of inheriting PRCList).

class nsGlobalWindowOuter : public mozilla::dom::EventTarget,
                            public nsPIDOMWindowOuter,
                            private nsIDOMWindow,
                            // NOTE: This interface is private, as it's only
                            // implemented on chrome windows.
                            private nsIDOMChromeWindow,
                            public nsIScriptGlobalObject,
                            public nsIScriptObjectPrincipal,
                            public nsSupportsWeakReference,
                            public nsIInterfaceRequestor,
                            public PRCListStr
{
public:
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  typedef nsDataHashtable<nsUint64HashKey, nsGlobalWindowOuter*> OuterWindowByIdTable;

  static void
  AssertIsOnMainThread()
#ifdef DEBUG
  ;
#else
  { }
#endif

  static nsGlobalWindowOuter* Cast(nsPIDOMWindowOuter* aPIWin) {
    return static_cast<nsGlobalWindowOuter*>(aPIWin);
  }
  static const nsGlobalWindowOuter* Cast(const nsPIDOMWindowOuter* aPIWin) {
    return static_cast<const nsGlobalWindowOuter*>(aPIWin);
  }
  static nsGlobalWindowOuter* Cast(mozIDOMWindowProxy* aWin) {
    return Cast(nsPIDOMWindowOuter::From(aWin));
  }

  static nsGlobalWindowOuter*
  GetOuterWindowWithId(uint64_t aWindowID)
  {
    AssertIsOnMainThread();

    if (!sOuterWindowsById) {
      return nullptr;
    }

    nsGlobalWindowOuter* outerWindow = sOuterWindowsById->Get(aWindowID);
    MOZ_ASSERT(!outerWindow || outerWindow->IsOuterWindow(),
                "Inner window in sOuterWindowsById?");
    return outerWindow;
  }

  static OuterWindowByIdTable* GetWindowsTable() {
    AssertIsOnMainThread();

    return sOuterWindowsById;
  }

  static nsGlobalWindowOuter *FromSupports(nsISupports *supports)
  {
    // Make sure this matches the casts we do in QueryInterface().
    return (nsGlobalWindowOuter *)(mozilla::dom::EventTarget *)supports;
  }

  static already_AddRefed<nsGlobalWindowOuter> Create(bool aIsChrome);

  // public methods
  nsPIDOMWindowOuter* GetPrivateParent();

  // callback for close event
  void ReallyCloseWindow();

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsWrapperCache
  virtual JSObject *WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override
  {
    return EnsureInnerWindow() ? GetWrapper() : nullptr;
  }

  // nsIGlobalJSObjectHolder
  virtual JSObject* GetGlobalJSObject() override;

  // nsIScriptGlobalObject
  JSObject *FastGetGlobalJSObject() const
  {
    return GetWrapperPreserveColor();
  }

  void TraceGlobalJSObject(JSTracer* aTrc);

  virtual nsresult EnsureScriptEnvironment() override;

  virtual nsIScriptContext *GetScriptContext() override;

  void PoisonOuterWindowProxy(JSObject *aObject);

  virtual bool IsBlackForCC(bool aTracingNeeded = true) override;

  // nsIScriptObjectPrincipal
  virtual nsIPrincipal* GetPrincipal() override;

  // nsIDOMWindow
  NS_DECL_NSIDOMWINDOW

  // nsIDOMChromeWindow (only implemented on chrome windows)
  NS_DECL_NSIDOMCHROMEWINDOW

  nsresult
  OpenJS(const nsAString& aUrl, const nsAString& aName,
         const nsAString& aOptions, nsPIDOMWindowOuter **_retval);

  // nsIDOMEventTarget
  NS_DECL_NSIDOMEVENTTARGET

  virtual mozilla::EventListenerManager*
    GetExistingListenerManager() const override;

  virtual mozilla::EventListenerManager*
    GetOrCreateListenerManager() override;

  using mozilla::dom::EventTarget::RemoveEventListener;
  virtual void AddEventListener(const nsAString& aType,
                                mozilla::dom::EventListener* aListener,
                                const mozilla::dom::AddEventListenerOptionsOrBoolean& aOptions,
                                const mozilla::dom::Nullable<bool>& aWantsUntrusted,
                                mozilla::ErrorResult& aRv) override;
  virtual nsPIDOMWindowOuter* GetOwnerGlobalForBindings() override;

  virtual nsIGlobalObject* GetOwnerGlobal() const override;

  // nsPIDOMWindow
  virtual nsPIDOMWindowOuter* GetPrivateRoot() override;

  // Outer windows only.
  virtual void ActivateOrDeactivate(bool aActivate) override;
  virtual void SetActive(bool aActive) override;
  virtual bool IsTopLevelWindowActive() override;
  virtual void SetIsBackground(bool aIsBackground) override;
  virtual void SetChromeEventHandler(mozilla::dom::EventTarget* aChromeEventHandler) override;

  // Outer windows only.
  virtual void SetInitialPrincipalToSubject() override;

  virtual PopupControlState PushPopupControlState(PopupControlState state, bool aForce) const override;
  virtual void PopPopupControlState(PopupControlState state) const override;
  virtual PopupControlState GetPopupControlState() const override;

  virtual already_AddRefed<nsISupports> SaveWindowState() override;
  virtual nsresult RestoreWindowState(nsISupports *aState) override;

  virtual bool IsSuspended() const override;
  virtual bool IsFrozen() const override;

  virtual nsresult FireDelayedDOMEvents() override;

  // Outer windows only.
  bool WouldReuseInnerWindow(nsIDocument* aNewDocument);

  void SetDocShell(nsIDocShell* aDocShell);
  void DetachFromDocShell();

  virtual nsresult SetNewDocument(nsIDocument *aDocument,
                                  nsISupports *aState,
                                  bool aForceReuseInnerWindow) override;

  // Outer windows only.
  void DispatchDOMWindowCreated();

  virtual void SetOpenerWindow(nsPIDOMWindowOuter* aOpener,
                               bool aOriginalOpener) override;

  // Outer windows only.
  virtual void EnsureSizeAndPositionUpToDate() override;

  virtual void EnterModalState() override;
  virtual void LeaveModalState() override;

  // Outer windows only.
  virtual bool CanClose() override;
  virtual void ForceClose() override;

  virtual void MaybeUpdateTouchState() override;

  // Outer windows only.
  virtual bool DispatchCustomEvent(const nsAString& aEventName) override;
  bool DispatchResizeEvent(const mozilla::CSSIntSize& aSize);

  // For accessing protected field mFullScreen
  friend class FullscreenTransitionTask;

  // Outer windows only.
  virtual nsresult SetFullscreenInternal(
    FullscreenReason aReason, bool aIsFullscreen) override final;
  virtual void FullscreenWillChange(bool aIsFullscreen) override final;
  virtual void FinishFullscreenChange(bool aIsFullscreen) override final;
  bool SetWidgetFullscreen(FullscreenReason aReason, bool aIsFullscreen,
                           nsIWidget* aWidget, nsIScreen* aScreen);
  bool FullScreen() const;

  // Inner windows only.
  virtual void SetHasGamepadEventListener(bool aHasGamepad = true) override;

  using EventTarget::EventListenerAdded;
  virtual void EventListenerAdded(nsAtom* aType) override;
  using EventTarget::EventListenerRemoved;
  virtual void EventListenerRemoved(nsAtom* aType) override;

  // nsIInterfaceRequestor
  NS_DECL_NSIINTERFACEREQUESTOR

  already_AddRefed<nsPIDOMWindowOuter> IndexedGetterOuter(uint32_t aIndex);

  already_AddRefed<nsPIDOMWindowOuter> GetTop() override;
  nsPIDOMWindowOuter* GetScriptableTop() override;
  inline nsGlobalWindowOuter *GetTopInternal();

  inline nsGlobalWindowOuter* GetScriptableTopInternal();

  nsPIDOMWindowOuter* GetChildWindow(const nsAString& aName);

  // These return true if we've reached the state in this top level window
  // where we ask the user if further dialogs should be blocked.
  //
  // DialogsAreBeingAbused must be called on the scriptable top inner window.
  //
  // ShouldPromptToBlockDialogs is implemented in terms of
  // DialogsAreBeingAbused, and will get the scriptable top inner window
  // automatically.
  // Outer windows only.
  bool ShouldPromptToBlockDialogs();

  // These functions are used for controlling and determining whether dialogs
  // (alert, prompt, confirm) are currently allowed in this window.  If you want
  // to temporarily disable dialogs, please use TemporarilyDisableDialogs, not
  // EnableDialogs/DisableDialogs, because correctly determining whether to
  // re-enable dialogs is actually quite difficult.
  void EnableDialogs();
  void DisableDialogs();
  // Outer windows only.
  bool AreDialogsEnabled();

  class MOZ_RAII TemporarilyDisableDialogs
  {
  public:
    explicit TemporarilyDisableDialogs(nsGlobalWindowOuter* aWindow
                                       MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
    ~TemporarilyDisableDialogs();

  private:
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

    // Always an inner window; this is the window whose dialog state we messed
    // with.  We just want to keep it alive, because we plan to poke at its
    // members in our destructor.
    RefPtr<nsGlobalWindowInner> mTopWindow;
    // This is not a AutoRestore<bool> because that would require careful
    // member destructor ordering, which is a bit fragile.  This way we can
    // explicitly restore things before we drop our ref to mTopWindow.
    bool mSavedDialogsEnabled;
  };
  friend class TemporarilyDisableDialogs;

  nsIScriptContext *GetContextInternal();

  nsGlobalWindowOuter *GetOuterWindowInternal();

  nsGlobalWindowInner* GetCurrentInnerWindowInternal() const;

  nsGlobalWindowInner* EnsureInnerWindowInternal();

  bool IsCreatingInnerWindow() const
  {
    return mCreatingInnerWindow;
  }

  bool IsChromeWindow() const
  {
    return mIsChrome;
  }

  // GetScrollFrame does not flush.  Callers should do it themselves as needed,
  // depending on which info they actually want off the scrollable frame.
  nsIScrollableFrame *GetScrollFrame();

  // Outer windows only.
  void UnblockScriptedClosing();

  static void Init();
  static void ShutDown();
  static bool IsCallerChrome();

  void CleanupCachedXBLHandlers();

  friend class WindowStateHolder;

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsGlobalWindowOuter,
                                                                   nsIDOMEventTarget)

#ifdef DEBUG
  // Call Unlink on this window. This may cause bad things to happen, so use
  // with caution.
  void RiskyUnlink();
#endif

  virtual JSObject*
    GetCachedXBLPrototypeHandler(nsXBLPrototypeHandler* aKey) override;

  virtual void
    CacheXBLPrototypeHandler(nsXBLPrototypeHandler* aKey,
                             JS::Handle<JSObject*> aHandler) override;

  virtual bool TakeFocus(bool aFocus, uint32_t aFocusMethod) override;
  virtual void SetReadyForFocus() override;
  virtual void PageHidden() override;
  virtual nsresult DispatchAsyncHashchange(nsIURI *aOldURI, nsIURI *aNewURI) override;
  virtual nsresult DispatchSyncPopState() override;

  // Inner windows only.
  virtual void EnableDeviceSensor(uint32_t aType) override;
  virtual void DisableDeviceSensor(uint32_t aType) override;

#if defined(MOZ_WIDGET_ANDROID)
  virtual void EnableOrientationChangeListener() override;
  virtual void DisableOrientationChangeListener() override;
#endif

  virtual void EnableTimeChangeNotifications() override;
  virtual void DisableTimeChangeNotifications() override;

  virtual nsresult SetArguments(nsIArray* aArguments) override;

  void MaybeForgiveSpamCount();
  bool IsClosedOrClosing() {
    return (mIsClosed ||
            mInClose ||
            mHavePendingClose ||
            mCleanedUp);
  }

  bool
  IsCleanedUp() const
  {
    return mCleanedUp;
  }

  bool
  HadOriginalOpener() const
  {
    MOZ_ASSERT(IsOuterWindow());
    return mHadOriginalOpener;
  }

  bool IsTopLevelWindow();

  virtual void
  FirePopupBlockedEvent(nsIDocument* aDoc,
                        nsIURI* aPopupURI,
                        const nsAString& aPopupWindowName,
                        const nsAString& aPopupWindowFeatures) override;

  virtual uint32_t GetSerial() override {
    return mSerial;
  }

  void AddSizeOfIncludingThis(nsWindowSizes& aWindowSizes) const;

  void AllowScriptsToClose()
  {
    mAllowScriptsToClose = true;
  }

  // Update the VR displays for this window
  bool UpdateVRDisplays(nsTArray<RefPtr<mozilla::dom::VRDisplay>>& aDisplays);

  // Outer windows only.
  uint32_t GetAutoActivateVRDisplayID();
  // Outer windows only.
  void SetAutoActivateVRDisplayID(uint32_t aAutoActivateVRDisplayID);

#define EVENT(name_, id_, type_, struct_)                                     \
  mozilla::dom::EventHandlerNonNull* GetOn##name_()                           \
  {                                                                           \
    mozilla::EventListenerManager* elm = GetExistingListenerManager();        \
    return elm ? elm->GetEventHandler(nsGkAtoms::on##name_, EmptyString())    \
               : nullptr;                                                     \
  }                                                                           \
  void SetOn##name_(mozilla::dom::EventHandlerNonNull* handler)               \
  {                                                                           \
    mozilla::EventListenerManager* elm = GetOrCreateListenerManager();        \
    if (elm) {                                                                \
      elm->SetEventHandler(nsGkAtoms::on##name_, EmptyString(), handler);     \
    }                                                                         \
  }
#define ERROR_EVENT(name_, id_, type_, struct_)                               \
  mozilla::dom::OnErrorEventHandlerNonNull* GetOn##name_()                    \
  {                                                                           \
    mozilla::EventListenerManager* elm = GetExistingListenerManager();        \
    return elm ? elm->GetOnErrorEventHandler() : nullptr;                     \
  }                                                                           \
  void SetOn##name_(mozilla::dom::OnErrorEventHandlerNonNull* handler)        \
  {                                                                           \
    mozilla::EventListenerManager* elm = GetOrCreateListenerManager();        \
    if (elm) {                                                                \
      elm->SetEventHandler(handler);                                          \
    }                                                                         \
  }
#define BEFOREUNLOAD_EVENT(name_, id_, type_, struct_)                        \
  mozilla::dom::OnBeforeUnloadEventHandlerNonNull* GetOn##name_()             \
  {                                                                           \
    mozilla::EventListenerManager* elm = GetExistingListenerManager();        \
    return elm ? elm->GetOnBeforeUnloadEventHandler() : nullptr;              \
  }                                                                           \
  void SetOn##name_(mozilla::dom::OnBeforeUnloadEventHandlerNonNull* handler) \
  {                                                                           \
    mozilla::EventListenerManager* elm = GetOrCreateListenerManager();        \
    if (elm) {                                                                \
      elm->SetEventHandler(handler);                                          \
    }                                                                         \
  }
#define WINDOW_ONLY_EVENT EVENT
#define TOUCH_EVENT EVENT
#include "mozilla/EventNameList.h"
#undef TOUCH_EVENT
#undef WINDOW_ONLY_EVENT
#undef BEFOREUNLOAD_EVENT
#undef ERROR_EVENT
#undef EVENT

  nsISupports* GetParentObject()
  {
    return nullptr;
  }

  nsIDocument* GetDocument()
  {
    return GetDoc();
  }
  void GetNameOuter(nsAString& aName);
  void SetNameOuter(const nsAString& aName, mozilla::ErrorResult& aError);
  mozilla::dom::Location* GetLocation() override;
  mozilla::dom::CustomElementRegistry* CustomElements() override;
  void GetStatusOuter(nsAString& aStatus);
  void SetStatusOuter(const nsAString& aStatus);
  void CloseOuter(bool aTrustedCaller);
  nsresult Close() override;
  bool GetClosedOuter();
  bool Closed() override;
  void StopOuter(mozilla::ErrorResult& aError);
  void FocusOuter(mozilla::ErrorResult& aError);
  nsresult Focus() override;
  void BlurOuter();
  already_AddRefed<nsPIDOMWindowOuter> GetFramesOuter();
  already_AddRefed<nsIDOMWindowCollection> GetFrames() override;
  uint32_t Length();
  already_AddRefed<nsPIDOMWindowOuter> GetTopOuter();

  nsresult GetPrompter(nsIPrompt** aPrompt) override;
protected:
  explicit nsGlobalWindowOuter();
  nsPIDOMWindowOuter* GetOpenerWindowOuter();
  // Initializes the mWasOffline member variable
  void InitWasOffline();
public:
  nsPIDOMWindowOuter*
  GetSanitizedOpener(nsPIDOMWindowOuter* aOpener);

  already_AddRefed<nsPIDOMWindowOuter> GetOpener() override;
  already_AddRefed<nsPIDOMWindowOuter> GetParentOuter();
  already_AddRefed<nsPIDOMWindowOuter> GetParent() override;
  nsPIDOMWindowOuter* GetScriptableParent() override;
  nsPIDOMWindowOuter* GetScriptableParentOrNull() override;
  mozilla::dom::Element*
  GetFrameElementOuter(nsIPrincipal& aSubjectPrincipal);
  already_AddRefed<nsIDOMElement> GetFrameElement() override;
  already_AddRefed<nsPIDOMWindowOuter>
  OpenOuter(const nsAString& aUrl,
            const nsAString& aName,
            const nsAString& aOptions,
            mozilla::ErrorResult& aError);
  nsresult Open(const nsAString& aUrl, const nsAString& aName,
                const nsAString& aOptions,
                nsIDocShellLoadInfo* aLoadInfo,
                bool aForceNoOpener,
                nsPIDOMWindowOuter **_retval) override;
  nsIDOMNavigator* GetNavigator() override;
  already_AddRefed<nsIDOMOfflineResourceList> GetApplicationCache() override;

#if defined(MOZ_WIDGET_ANDROID)
  int16_t Orientation(mozilla::dom::CallerType aCallerType) const;
#endif

protected:
  bool AlertOrConfirm(bool aAlert, const nsAString& aMessage,
                      nsIPrincipal& aSubjectPrincipal,
                      mozilla::ErrorResult& aError);

public:
  void AlertOuter(const nsAString& aMessage,
                  nsIPrincipal& aSubjectPrincipal,
                  mozilla::ErrorResult& aError);
  bool ConfirmOuter(const nsAString& aMessage,
                    nsIPrincipal& aSubjectPrincipal,
                    mozilla::ErrorResult& aError);
  void PromptOuter(const nsAString& aMessage, const nsAString& aInitial,
                   nsAString& aReturn,
                   nsIPrincipal& aSubjectPrincipal,
                   mozilla::ErrorResult& aError);
  void PrintOuter(mozilla::ErrorResult& aError);
  mozilla::dom::Selection* GetSelectionOuter();
  already_AddRefed<nsISelection> GetSelection() override;
  already_AddRefed<nsICSSDeclaration>
    GetComputedStyle(mozilla::dom::Element& aElt, const nsAString& aPseudoElt,
                     mozilla::ErrorResult& aError) override;
  already_AddRefed<mozilla::dom::MediaQueryList> MatchMediaOuter(
    const nsAString& aQuery,
    mozilla::dom::CallerType aCallerType);
  nsIDOMScreen* GetScreen() override;
  void MoveToOuter(int32_t aXPos, int32_t aYPos,
                   mozilla::dom::CallerType aCallerType,
                   mozilla::ErrorResult& aError);
  void MoveByOuter(int32_t aXDif, int32_t aYDif,
                   mozilla::dom::CallerType aCallerType,
                   mozilla::ErrorResult& aError);
  nsresult MoveBy(int32_t aXDif, int32_t aYDif) override;
  void ResizeToOuter(int32_t aWidth, int32_t aHeight,
                     mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  void ResizeByOuter(int32_t aWidthDif, int32_t aHeightDif,
                     mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  double GetScrollXOuter();
  double GetScrollYOuter();

  void SizeToContentOuter(mozilla::dom::CallerType aCallerType,
                          mozilla::ErrorResult& aError);
  nsIControllers* GetControllersOuter(mozilla::ErrorResult& aError);
  nsresult GetControllers(nsIControllers** aControllers) override;
  mozilla::dom::Element* GetRealFrameElementOuter();
  float GetMozInnerScreenXOuter(mozilla::dom::CallerType aCallerType);
  float GetMozInnerScreenYOuter(mozilla::dom::CallerType aCallerType);
  double GetDevicePixelRatioOuter(mozilla::dom::CallerType aCallerType);
  bool GetFullScreenOuter();
  bool GetFullScreen() override;
  void SetFullScreenOuter(bool aFullScreen, mozilla::ErrorResult& aError);
  nsresult SetFullScreen(bool aFullScreen) override;
  void BackOuter(mozilla::ErrorResult& aError);
  void ForwardOuter(mozilla::ErrorResult& aError);
  void HomeOuter(nsIPrincipal& aSubjectPrincipal, mozilla::ErrorResult& aError);
  bool FindOuter(const nsAString& aString, bool aCaseSensitive, bool aBackwards,
                 bool aWrapAround, bool aWholeWord, bool aSearchInFrames,
                 bool aShowDialog, mozilla::ErrorResult& aError);
  uint64_t GetMozPaintCountOuter();

  bool ShouldResistFingerprinting();

  already_AddRefed<nsPIDOMWindowOuter>
  OpenDialogOuter(JSContext* aCx,
                  const nsAString& aUrl,
                  const nsAString& aName,
                  const nsAString& aOptions,
                  const mozilla::dom::Sequence<JS::Value>& aExtraArgument,
                  mozilla::ErrorResult& aError);
  nsresult OpenDialog(const nsAString& aUrl, const nsAString& aName,
                      const nsAString& aOptions,
                      nsISupports* aExtraArgument,
                      nsPIDOMWindowOuter** _retval) override;
  nsresult UpdateCommands(const nsAString& anAction, nsISelection* aSel, int16_t aReason) override;

  already_AddRefed<nsPIDOMWindowOuter>
  GetContentInternal(mozilla::ErrorResult& aError,
                     mozilla::dom::CallerType aCallerType);
  void GetContentOuter(JSContext* aCx,
                       JS::MutableHandle<JSObject*> aRetval,
                       mozilla::dom::CallerType aCallerType,
                       mozilla::ErrorResult& aError);
  already_AddRefed<nsPIDOMWindowOuter> GetContent()
  {
    MOZ_ASSERT(IsOuterWindow());
    mozilla::IgnoredErrorResult ignored;
    nsCOMPtr<nsPIDOMWindowOuter> win =
      GetContentInternal(ignored, mozilla::dom::CallerType::System);
    return win.forget();
  }

  // ChromeWindow bits.  Do NOT call these unless your window is in
  // fact chrome.
  nsIBrowserDOMWindow* GetBrowserDOMWindowOuter();
  void SetBrowserDOMWindowOuter(nsIBrowserDOMWindow* aBrowserWindow);
  void SetCursorOuter(const nsAString& aCursor, mozilla::ErrorResult& aError);

  void GetDialogArgumentsOuter(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval,
                               nsIPrincipal& aSubjectPrincipal,
                               mozilla::ErrorResult& aError);
  void GetDialogArguments(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval,
                          nsIPrincipal& aSubjectPrincipal,
                          mozilla::ErrorResult& aError);
  void GetReturnValueOuter(JSContext* aCx, JS::MutableHandle<JS::Value> aReturnValue,
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

  already_AddRefed<nsWindowRoot> GetWindowRootOuter();

  virtual bool IsInSyncOperation() override
  {
    return GetExtantDoc() && GetExtantDoc()->IsInSyncOperation();
  }

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
  typedef int32_t
    (nsGlobalWindowOuter::*WindowCoordGetter)(mozilla::dom::CallerType aCallerType,
                                              mozilla::ErrorResult&);
  typedef void
    (nsGlobalWindowOuter::*WindowCoordSetter)(int32_t,
                                              mozilla::dom::CallerType aCallerType,
                                              mozilla::ErrorResult&);
  // And the implementations of WindowCoordGetter/WindowCoordSetter.
public:
  int32_t GetInnerWidthOuter(mozilla::ErrorResult& aError);
protected:
  nsresult GetInnerWidth(int32_t* aWidth) override;
  void SetInnerWidthOuter(int32_t aInnerWidth,
                          mozilla::dom::CallerType aCallerType,
                          mozilla::ErrorResult& aError);
public:
  int32_t GetInnerHeightOuter(mozilla::ErrorResult& aError);
protected:
  nsresult GetInnerHeight(int32_t* aHeight) override;
  void SetInnerHeightOuter(int32_t aInnerHeight,
                           mozilla::dom::CallerType aCallerType,
                           mozilla::ErrorResult& aError);
  int32_t GetScreenXOuter(mozilla::dom::CallerType aCallerType,
                          mozilla::ErrorResult& aError);
  void SetScreenXOuter(int32_t aScreenX,
                       mozilla::dom::CallerType aCallerType,
                       mozilla::ErrorResult& aError);
  int32_t GetScreenYOuter(mozilla::dom::CallerType aCallerType,
                          mozilla::ErrorResult& aError);
  void SetScreenYOuter(int32_t aScreenY,
                       mozilla::dom::CallerType aCallerType,
                       mozilla::ErrorResult& aError);
  int32_t GetOuterWidthOuter(mozilla::dom::CallerType aCallerType,
                             mozilla::ErrorResult& aError);
  void SetOuterWidthOuter(int32_t aOuterWidth,
                          mozilla::dom::CallerType aCallerType,
                          mozilla::ErrorResult& aError);
  int32_t GetOuterHeightOuter(mozilla::dom::CallerType aCallerType,
                              mozilla::ErrorResult& aError);
  void SetOuterHeightOuter(int32_t aOuterHeight,
                           mozilla::dom::CallerType aCallerType,
                           mozilla::ErrorResult& aError);

  // Array of idle observers that are notified of idle events.
  nsTObserverArray<IdleObserverHolder> mIdleObservers;

  // Idle timer used for function callbacks to notify idle observers.
  nsCOMPtr<nsITimer> mIdleTimer;

  // Idle fuzz time added to idle timer callbacks.
  uint32_t mIdleFuzzFactor;

  // Index in mArrayIdleObservers
  // Next idle observer to notify user idle status
  int32_t mIdleCallbackIndex;

  // If false then the topic is "active"
  // If true then the topic is "idle"
  bool mCurrentlyIdle;

  // Set to true when a fuzz time needs to be applied
  // to active notifications to the idle observer.
  bool mAddActiveEventFuzzTime;

  nsCOMPtr <nsIIdleService> mIdleService;

  RefPtr<mozilla::dom::WakeLock> mWakeLock;

  friend class HashchangeCallback;
  friend class mozilla::dom::BarProp;

  // Object Management
  virtual ~nsGlobalWindowOuter();
  void DropOuterWindowDocs();
  void CleanUp();
  void ClearControllers();
  // Outer windows only.
  void FinalClose();

  inline void MaybeClearInnerWindow(nsGlobalWindowInner* aExpectedInner);

  nsGlobalWindowInner *CallerInnerWindow();

  // Get the parent, returns null if this is a toplevel window
  nsPIDOMWindowOuter* GetParentInternal();

public:
  // popup tracking
  bool IsPopupSpamWindow();

  // Outer windows only.
  void SetIsPopupSpamWindow(bool aIsPopupSpam);

protected:
  // Window Control Functions

  // Outer windows only.
  virtual nsresult
  OpenNoNavigate(const nsAString& aUrl,
                 const nsAString& aName,
                 const nsAString& aOptions,
                 nsPIDOMWindowOuter** _retval) override;

private:
  /**
   * @param aUrl the URL we intend to load into the window.  If aNavigate is
   *        true, we'll actually load this URL into the window. Otherwise,
   *        aUrl is advisory; OpenInternal will not load the URL into the
   *        new window.
   *
   * @param aName the name to use for the new window
   *
   * @param aOptions the window options to use for the new window
   *
   * @param aDialog true when called from variants of OpenDialog.  If this is
   *        true, this method will skip popup blocking checks.  The aDialog
   *        argument is passed on to the window watcher.
   *
   * @param aCalledNoScript true when called via the [noscript] open()
   *        and openDialog() methods.  When this is true, we do NOT want to use
   *        the JS stack for things like caller determination.
   *
   * @param aDoJSFixups true when this is the content-accessible JS version of
   *        window opening.  When true, popups do not cause us to throw, we save
   *        the caller's principal in the new window for later consumption, and
   *        we make sure that there is a document in the newly-opened window.
   *        Note that this last will only be done if the newly-opened window is
   *        non-chrome.
   *
   * @param aNavigate true if we should navigate to the provided URL, false
   *        otherwise.  When aNavigate is false, we also skip our can-load
   *        security check, on the assumption that whoever *actually* loads this
   *        page will do their own security check.
   *
   * @param argv The arguments to pass to the new window.  The first
   *        three args, if present, will be aUrl, aName, and aOptions.  So this
   *        param only matters if there are more than 3 arguments.
   *
   * @param aExtraArgument Another way to pass arguments in.  This is mutually
   *        exclusive with the argv approach.
   *
   * @param aLoadInfo to be passed on along to the windowwatcher.
   *
   * @param aForceNoOpener if true, will act as if "noopener" were passed in
   *                       aOptions, but without affecting any other window
   *                       features.
   *
   * @param aReturn [out] The window that was opened, if any.  Will be null if
   *                      aForceNoOpener is true of if aOptions contains
   *                      "noopener".
   *
   * Outer windows only.
   */
  nsresult OpenInternal(const nsAString& aUrl,
                        const nsAString& aName,
                        const nsAString& aOptions,
                        bool aDialog,
                        bool aContentModal,
                        bool aCalledNoScript,
                        bool aDoJSFixups,
                        bool aNavigate,
                        nsIArray *argv,
                        nsISupports *aExtraArgument,
                        nsIDocShellLoadInfo* aLoadInfo,
                        bool aForceNoOpener,
                        nsPIDOMWindowOuter **aReturn);

public:
  // Helper Functions
  already_AddRefed<nsIDocShellTreeOwner> GetTreeOwner();
  already_AddRefed<nsIBaseWindow> GetTreeOwnerWindow();
  already_AddRefed<nsIWebBrowserChrome> GetWebBrowserChrome();
  nsresult SecurityCheckURL(const char *aURL);
  bool IsPrivateBrowsing();

  bool PopupWhitelisted();
  PopupControlState RevisePopupAbuseLevel(PopupControlState);
  void     FireAbuseEvents(const nsAString &aPopupURL,
                           const nsAString &aPopupWindowName,
                           const nsAString &aPopupWindowFeatures);

  bool GetIsPrerendered();

private:
  void ReportLargeAllocStatus();

public:
  virtual nsresult RegisterIdleObserver(nsIIdleObserver* aIdleObserverPtr) override;
  virtual nsresult UnregisterIdleObserver(nsIIdleObserver* aIdleObserverPtr) override;

  void FlushPendingNotifications(mozilla::FlushType aType);

  // Outer windows only.
  void EnsureReflowFlushAndPaint();
  void CheckSecurityWidthAndHeight(int32_t* width, int32_t* height,
                                   mozilla::dom::CallerType aCallerType);
  void CheckSecurityLeftAndTop(int32_t* left, int32_t* top,
                               mozilla::dom::CallerType aCallerType);

  // Outer windows only.
  // Arguments to this function should have values in app units
  void SetCSSViewportWidthAndHeight(nscoord width, nscoord height);
  // Arguments to this function should have values in device pixels
  nsresult SetDocShellWidthAndHeight(int32_t width, int32_t height);

  static bool CanSetProperty(const char *aPrefName);

  static void MakeScriptDialogTitle(nsAString& aOutTitle,
                                    nsIPrincipal* aSubjectPrincipal);

  // Outer windows only.
  bool CanMoveResizeWindows(mozilla::dom::CallerType aCallerType);

  // If aDoFlush is true, we'll flush our own layout; otherwise we'll try to
  // just flush our parent and only flush ourselves if we think we need to.
  // Outer windows only.
  mozilla::CSSPoint GetScrollXY(bool aDoFlush);

  int32_t GetScrollBoundaryOuter(mozilla::Side aSide);

  // Outer windows only.
  nsresult GetInnerSize(mozilla::CSSIntSize& aSize);
  nsIntSize GetOuterSize(mozilla::dom::CallerType aCallerType,
                         mozilla::ErrorResult& aError);
  void SetOuterSize(int32_t aLengthCSSPixels, bool aIsWidth,
                    mozilla::dom::CallerType aCallerType,
                    mozilla::ErrorResult& aError);
  nsRect GetInnerScreenRect();

  bool IsFrame();

  // Outer windows only.
  // If aLookForCallerOnJSStack is true, this method will look at the JS stack
  // to determine who the caller is.  If it's false, it'll use |this| as the
  // caller.
  bool WindowExists(const nsAString& aName, bool aForceNoOpener,
                    bool aLookForCallerOnJSStack);

  already_AddRefed<nsIWidget> GetMainWidget();
  nsIWidget* GetNearestWidget() const;

  bool IsInModalState();

  // Convenience functions for the many methods that need to scale
  // from device to CSS pixels or vice versa.  Note: if a presentation
  // context is not available, they will assume a 1:1 ratio.
  int32_t DevToCSSIntPixels(int32_t px);
  int32_t CSSToDevIntPixels(int32_t px);
  nsIntSize DevToCSSIntPixels(nsIntSize px);
  nsIntSize CSSToDevIntPixels(nsIntSize px);

  virtual void SetFocusedNode(nsIContent* aNode,
                              uint32_t aFocusMethod = 0,
                              bool aNeedsFocus = false) override;

  virtual uint32_t GetFocusMethod() override;

  virtual bool ShouldShowFocusRing() override;

  virtual void SetKeyboardIndicators(UIStateChangeType aShowAccelerators,
                                     UIStateChangeType aShowFocusRings) override;

public:
  virtual already_AddRefed<nsPIWindowRoot> GetTopWindowRoot() override;

protected:
  void NotifyWindowIDDestroyed(const char* aTopic);

  void ClearStatus();

  virtual void UpdateParentTarget() override;

  void InitializeShowFocusRings();

public:
  // Outer windows only.
  nsDOMWindowList* GetWindowList();
protected:
  // Helper for getComputedStyle and getDefaultComputedStyle
  already_AddRefed<nsICSSDeclaration>
    GetComputedStyleHelperOuter(mozilla::dom::Element& aElt,
                                const nsAString& aPseudoElt,
                                bool aDefaultStylesOnly);

  // Outer windows only.
  void PreloadLocalStorage();

  // Returns CSS pixels based on primary screen.  Outer windows only.
  mozilla::CSSIntPoint GetScreenXY(mozilla::dom::CallerType aCallerType,
                                   mozilla::ErrorResult& aError);

  void PostMessageMozOuter(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                           const nsAString& aTargetOrigin,
                           JS::Handle<JS::Value> aTransfer,
                           nsIPrincipal& aSubjectPrincipal,
                           mozilla::ErrorResult& aError);

  // Ask the user if further dialogs should be blocked, if dialogs are currently
  // being abused. This is used in the cases where we have no modifiable UI to
  // show, in that case we show a separate dialog to ask this question.
  bool ConfirmDialogIfNeeded();

  // Helper called after moving/resizing, to update docShell's presContext
  // if we have caused a resolution change by moving across monitors.
  void CheckForDPIChange();

private:
  enum class SecureContextFlags {
    eDefault,
    eIgnoreOpener
  };
  // Called only on outer windows to compute the value that will be returned by
  // IsSecureContext() for the inner window that corresponds to aDocument.
  bool ComputeIsSecureContext(nsIDocument* aDocument,
                              SecureContextFlags aFlags =
                                SecureContextFlags::eDefault);

  // nsPIDOMWindow<T> should be able to see these helper methods.
  friend class nsPIDOMWindow<mozIDOMWindowProxy>;
  friend class nsPIDOMWindow<mozIDOMWindow>;
  friend class nsPIDOMWindow<nsISupports>;
  friend class nsPIDOMWindowInner;
  friend class nsPIDOMWindowOuter;

  mozilla::dom::TabGroup* TabGroupOuter();

  bool IsBackgroundInternal() const;

  void SetIsBackgroundInternal(bool aIsBackground);

  // NOTE: Chrome Only
  void DisconnectAndClearGroupMessageManagers()
  {
    MOZ_RELEASE_ASSERT(IsChromeWindow());
    for (auto iter = mChromeFields.mGroupMessageManagers.Iter(); !iter.Done(); iter.Next()) {
      nsIMessageBroadcaster* mm = iter.UserData();
      if (mm) {
        static_cast<nsFrameMessageManager*>(mm)->Disconnect();
      }
    }
    mChromeFields.mGroupMessageManagers.Clear();
  }

public:
  // Dispatch a runnable related to the global.
  virtual nsresult Dispatch(mozilla::TaskCategory aCategory,
                            already_AddRefed<nsIRunnable>&& aRunnable) override;

  virtual nsISerialEventTarget*
  EventTargetFor(mozilla::TaskCategory aCategory) const override;

  virtual mozilla::AbstractThread*
  AbstractMainThreadFor(mozilla::TaskCategory aCategory) override;

  uint32_t LastIdleRequestHandle() const { return mIdleRequestCallbackCounter - 1; }
  nsresult RunIdleRequest(mozilla::dom::IdleRequest* aRequest,
                          DOMHighResTimeStamp aDeadline, bool aDidTimeout);

  typedef mozilla::LinkedList<mozilla::dom::IdleRequest> IdleRequests;

  void RemoveIdleCallback(mozilla::dom::IdleRequest* aRequest);

protected:
  // These members are only used on outer window objects. Make sure
  // you never set any of these on an inner object!
  bool                          mFullScreen : 1;
  bool                          mFullscreenMode : 1;
  bool                          mIsClosed : 1;
  bool                          mInClose : 1;
  // mHavePendingClose means we've got a termination function set to
  // close us when the JS stops executing or that we have a close
  // event posted.  If this is set, just ignore window.close() calls.
  bool                          mHavePendingClose : 1;
  bool                          mHadOriginalOpener : 1;
  bool                          mOriginalOpenerWasSecureContext : 1;
  bool                          mIsSecureContextIfOpenerIgnored : 1;
  bool                          mIsPopupSpam : 1;

  // Indicates whether scripts are allowed to close this window.
  bool                          mBlockScriptedClosingFlag : 1;

  // Window offline status. Checked to see if we need to fire offline event
  bool                          mWasOffline : 1;

  // Represents whether the inner window's page has had a slow script notice.
  // Only used by inner windows; will always be false for outer windows.
  // This is used to implement Telemetry measures such as SLOW_SCRIPT_PAGE_COUNT.
  bool                          mHasHadSlowScript : 1;

  // Track what sorts of events we need to fire when thawed
  bool                          mNotifyIdleObserversIdleOnThaw : 1;
  bool                          mNotifyIdleObserversActiveOnThaw : 1;

  // Indicates whether we're in the middle of creating an initializing
  // a new inner window object.
  bool                          mCreatingInnerWindow : 1;

  // Fast way to tell if this is a chrome window (without having to QI).
  bool                          mIsChrome : 1;

  // Hack to indicate whether a chrome window needs its message manager
  // to be disconnected, since clean up code is shared in the global
  // window superclass.
  bool                          mCleanMessageManager : 1;

  // Indicates that the current document has never received a document focus
  // event.
  bool                   mNeedsFocus : 1;
  bool                   mHasFocus : 1;

  // when true, show focus rings for the current focused content only.
  // This will be reset when another element is focused
  bool                   mShowFocusRingForContent : 1;

  // true if tab navigation has occurred for this window. Focus rings
  // should be displayed.
  bool                   mFocusByKeyOccurred : 1;

  // Inner windows only.
  // Indicates whether this window wants gamepad input events
  bool                   mHasGamepad : 1;

  // Inner windows only.
  // Indicates whether this window wants VR events
  bool                   mHasVREvents : 1;

  // Inner windows only.
  // Indicates whether this window wants VRDisplayActivate events
  bool                   mHasVRDisplayActivateEvents : 1;
  nsCheapSet<nsUint32HashKey> mGamepadIndexSet;
  nsRefPtrHashtable<nsUint32HashKey, mozilla::dom::Gamepad> mGamepads;
  bool mHasSeenGamepadInput;

  // whether we've sent the destroy notification for our window id
  bool                   mNotifiedIDDestroyed : 1;
  // whether scripts may close the window,
  // even if "dom.allow_scripts_to_close_windows" is false.
  bool                   mAllowScriptsToClose : 1;

  bool mTopLevelOuterContentWindow : 1;

  nsCOMPtr<nsIScriptContext>    mContext;
  nsWeakPtr                     mOpener;
  nsCOMPtr<nsIControllers>      mControllers;

  // For |window.arguments|, via |openDialog|.
  nsCOMPtr<nsIArray>            mArguments;

  // Only used in the outer.
  RefPtr<DialogValueHolder> mReturnValue;

  RefPtr<mozilla::dom::Navigator> mNavigator;
  RefPtr<nsScreen>            mScreen;
  RefPtr<nsDOMWindowList>     mFrames;
  // All BarProps are inner window only.
  RefPtr<mozilla::dom::BarProp> mMenubar;
  RefPtr<mozilla::dom::BarProp> mToolbar;
  RefPtr<mozilla::dom::BarProp> mLocationbar;
  RefPtr<mozilla::dom::BarProp> mPersonalbar;
  RefPtr<mozilla::dom::BarProp> mStatusbar;
  RefPtr<mozilla::dom::BarProp> mScrollbars;
  RefPtr<nsDOMWindowUtils>      mWindowUtils;
  nsString                      mStatus;
  nsString                      mDefaultStatus;
  RefPtr<nsGlobalWindowObserver> mObserver; // Inner windows only.
  RefPtr<mozilla::dom::Crypto>  mCrypto;
  RefPtr<mozilla::dom::U2F> mU2F;
  RefPtr<mozilla::dom::cache::CacheStorage> mCacheStorage;
  RefPtr<mozilla::dom::Console> mConsole;
  RefPtr<mozilla::dom::Worklet> mAudioWorklet;
  RefPtr<mozilla::dom::Worklet> mPaintWorklet;
  // We need to store an nsISupports pointer to this object because the
  // mozilla::dom::External class doesn't exist on b2g and using the type
  // forward declared here means that ~nsGlobalWindow wouldn't compile because
  // it wouldn't see the ~External function's declaration.
  nsCOMPtr<nsISupports>         mExternal;

  RefPtr<mozilla::dom::MozSelfSupport> mMozSelfSupport;

  RefPtr<mozilla::dom::Storage> mLocalStorage;
  RefPtr<mozilla::dom::Storage> mSessionStorage;

  // These member variable are used only on inner windows.
  RefPtr<mozilla::EventListenerManager> mListenerManager;
  RefPtr<mozilla::dom::Location> mLocation;
  RefPtr<nsHistory>           mHistory;
  RefPtr<mozilla::dom::CustomElementRegistry> mCustomElements;

  // These member variables are used on both inner and the outer windows.
  nsCOMPtr<nsIPrincipal> mDocumentPrincipal;
  // mTabChild is only ever populated in the content process.
  nsCOMPtr<nsITabChild>  mTabChild;

  uint32_t mSuspendDepth;
  uint32_t mFreezeDepth;

  // the method that was used to focus mFocusedNode
  uint32_t mFocusMethod;

  uint32_t mSerial;

  // The current idle request callback handle
  uint32_t mIdleRequestCallbackCounter;
  IdleRequests mIdleRequestCallbacks;
  RefPtr<IdleRequestExecutor> mIdleRequestExecutor;

#ifdef DEBUG
  bool mSetOpenerWindowCalled;
  nsCOMPtr<nsIURI> mLastOpenedURI;
#endif

  bool mCleanedUp;

  nsCOMPtr<nsIDOMOfflineResourceList> mApplicationCache;

  using XBLPrototypeHandlerTable = nsJSThingHashtable<nsPtrHashKey<nsXBLPrototypeHandler>, JSObject*>;
  mozilla::UniquePtr<XBLPrototypeHandlerTable> mCachedXBLPrototypeHandlers;

  // mSuspendedDoc is only set on outer windows. It's useful when we get matched
  // EnterModalState/LeaveModalState calls, in which case the outer window is
  // responsible for unsuspending events on the document. If we don't (for
  // example, if the outer window is closed before the LeaveModalState call),
  // then the inner window whose mDoc is our mSuspendedDoc is responsible for
  // unsuspending it.
  nsCOMPtr<nsIDocument> mSuspendedDoc;

  RefPtr<mozilla::dom::IDBFactory> mIndexedDB;

  // This counts the number of windows that have been opened in rapid succession
  // (i.e. within dom.successive_dialog_time_limit of each other). It is reset
  // to 0 once a dialog is opened after dom.successive_dialog_time_limit seconds
  // have elapsed without any other dialogs.
  uint32_t                      mDialogAbuseCount;

  // This holds the time when the last modal dialog was shown. If more than
  // MAX_DIALOG_LIMIT dialogs are shown within the time span defined by
  // dom.successive_dialog_time_limit, we show a checkbox or confirmation prompt
  // to allow disabling of further dialogs from this window.
  TimeStamp                     mLastDialogQuitTime;

  // This flag keeps track of whether dialogs are
  // currently enabled on this window.
  bool                          mAreDialogsEnabled;

  nsTHashtable<nsPtrHashKey<mozilla::DOMEventTargetHelper> > mEventTargetObjects;

  nsTArray<uint32_t> mEnabledSensors;

#if defined(MOZ_WIDGET_ANDROID)
  mozilla::UniquePtr<mozilla::dom::WindowOrientationObserver> mOrientationChangeObserver;
#endif

#ifdef MOZ_WEBSPEECH
  // mSpeechSynthesis is only used on inner windows.
  RefPtr<mozilla::dom::SpeechSynthesis> mSpeechSynthesis;
#endif

#ifdef DEBUG
  // This member is used in the debug only assertions in TabGroup()
  // to catch cyclic parent/opener trees and not overflow the stack.
  bool mIsValidatingTabGroup;
#endif

  // This is the CC generation the last time we called CanSkip.
  uint32_t mCanSkipCCGeneration;

  // The VR Displays for this window
  nsTArray<RefPtr<mozilla::dom::VRDisplay>> mVRDisplays;

  RefPtr<mozilla::dom::VREventObserver> mVREventObserver;

  // When non-zero, the document should receive a vrdisplayactivate event
  // after loading.  The value is the ID of the VRDisplay that content should
  // begin presentation on.
  uint32_t mAutoActivateVRDisplayID; // Outer windows only
  int64_t mBeforeUnloadListenerCount; // Inner windows only

  RefPtr<mozilla::dom::IntlUtils> mIntlUtils;

  static OuterWindowByIdTable* sOuterWindowsById;

  // Members in the mChromeFields member should only be used in chrome windows.
  // All accesses to this field should be guarded by a check of mIsChrome.
  struct ChromeFields {
    ChromeFields()
      : mGroupMessageManagers(1)
    {}

    nsCOMPtr<nsIBrowserDOMWindow> mBrowserDOMWindow;
    nsCOMPtr<nsIMessageBroadcaster> mMessageManager;
    nsInterfaceHashtable<nsStringHashKey, nsIMessageBroadcaster> mGroupMessageManagers;
    // A weak pointer to the nsPresShell that we are doing fullscreen for.
    // The pointer being set indicates we've set the IsInFullscreenChange
    // flag on this pres shell.
    nsWeakPtr mFullscreenPresShell;
    nsCOMPtr<mozIDOMWindowProxy> mOpenerForInitialContentBrowser;
  } mChromeFields;

  friend class nsDOMScriptableHelper;
  friend class nsDOMWindowUtils;
  friend class mozilla::dom::PostMessageEvent;
  friend class DesktopNotification;
  friend class mozilla::dom::TimeoutManager;
  friend class IdleRequestExecutor;
  friend class nsGlobalWindowInner;
};

// XXX: EWW - This is an awful hack - let's not do this
#include "nsGlobalWindowInner.h"

inline nsISupports*
ToSupports(nsGlobalWindowOuter *p)
{
    return static_cast<nsIDOMEventTarget*>(p);
}

inline nsISupports*
ToCanonicalSupports(nsGlobalWindowOuter *p)
{
    return static_cast<nsIDOMEventTarget*>(p);
}

inline nsIGlobalObject*
nsGlobalWindowOuter::GetOwnerGlobal() const
{
  return GetCurrentInnerWindowInternal();
}

inline nsGlobalWindowOuter*
nsGlobalWindowOuter::GetTopInternal()
{
  nsCOMPtr<nsPIDOMWindowOuter> top = GetTop();
  if (top) {
    return nsGlobalWindowOuter::Cast(top);
  }
  return nullptr;
}

inline nsGlobalWindowOuter*
nsGlobalWindowOuter::GetScriptableTopInternal()
{
  nsPIDOMWindowOuter* top = GetScriptableTop();
  return nsGlobalWindowOuter::Cast(top);
}

inline nsIScriptContext*
nsGlobalWindowOuter::GetContextInternal()
{
  if (mOuterWindow) {
    return GetOuterWindowInternal()->mContext;
  }

  return mContext;
}

inline nsGlobalWindowOuter*
nsGlobalWindowOuter::GetOuterWindowInternal()
{
  return nsGlobalWindowOuter::Cast(GetOuterWindow());
}

inline nsGlobalWindowInner*
nsGlobalWindowOuter::GetCurrentInnerWindowInternal() const
{
  MOZ_ASSERT(IsOuterWindow());
  return nsGlobalWindowInner::Cast(mInnerWindow);
}

inline nsGlobalWindowInner*
nsGlobalWindowOuter::EnsureInnerWindowInternal()
{
  return nsGlobalWindowInner::Cast(AsOuter()->EnsureInnerWindow());
}

inline bool
nsGlobalWindowOuter::IsTopLevelWindow()
{
  MOZ_ASSERT(IsOuterWindow());
  nsPIDOMWindowOuter* parentWindow = GetScriptableTop();
  return parentWindow == this->AsOuter();
}

inline bool
nsGlobalWindowOuter::IsPopupSpamWindow()
{
  return mIsPopupSpam;
}

inline bool
nsGlobalWindowOuter::IsFrame()
{
  return GetParentInternal() != nullptr;
}

inline void
nsGlobalWindowOuter::MaybeClearInnerWindow(nsGlobalWindowInner* aExpectedInner)
{
  if(mInnerWindow == aExpectedInner->AsInner()) {
    mInnerWindow = nullptr;
  }
}

/* factory function */
inline already_AddRefed<nsGlobalWindowOuter>
NS_NewScriptGlobalObject(bool aIsChrome)
{
  return nsGlobalWindowOuter::Create(aIsChrome);
}

#endif /* nsGlobalWindowOuter_h___ */
