/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPIDOMWindow_h__
#define nsPIDOMWindow_h__

#include "nsIDOMWindow.h"
#include "mozIDOMWindow.h"

#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "mozilla/dom/EventTarget.h"
#include "js/TypeDecls.h"
#include "nsRefPtrHashtable.h"

// Only fired for inner windows.
#define DOM_WINDOW_DESTROYED_TOPIC "dom-window-destroyed"
#define DOM_WINDOW_FROZEN_TOPIC "dom-window-frozen"
#define DOM_WINDOW_THAWED_TOPIC "dom-window-thawed"

class nsGlobalWindow;
class nsIArray;
class nsIContent;
class nsICSSDeclaration;
class nsIDocShell;
class nsIDocShellLoadInfo;
class nsIDocument;
class nsIIdleObserver;
class nsIPrincipal;
class nsIScriptTimeoutHandler;
class nsIURI;
class nsPIDOMWindowInner;
class nsPIDOMWindowOuter;
class nsPIWindowRoot;
class nsXBLPrototypeHandler;

typedef uint32_t SuspendTypes;

namespace mozilla {
class ThrottledEventQueue;
namespace dom {
class AudioContext;
class DocGroup;
class TabGroup;
class Element;
class Performance;
class ServiceWorkerRegistration;
class Timeout;
class CustomElementRegistry;
} // namespace dom
} // namespace mozilla

// Popup control state enum. The values in this enum must go from most
// permissive to least permissive so that it's safe to push state in
// all situations. Pushing popup state onto the stack never makes the
// current popup state less permissive (see
// nsGlobalWindow::PushPopupControlState()).
enum PopupControlState {
  openAllowed = 0,  // open that window without worries
  openControlled,   // it's a popup, but allow it
  openAbused,       // it's a popup. disallow it, but allow domain override.
  openOverridden    // disallow window open
};

enum UIStateChangeType
{
  UIStateChangeType_NoChange,
  UIStateChangeType_Set,
  UIStateChangeType_Clear,
  UIStateChangeType_Invalid // used for serialization only
};

enum class FullscreenReason
{
  // Toggling the fullscreen mode requires trusted context.
  ForFullscreenMode,
  // Fullscreen API is the API provided to untrusted content.
  ForFullscreenAPI,
  // This reason can only be used with exiting fullscreen.
  // It is otherwise identical to eForFullscreenAPI except it would
  // suppress the fullscreen transition.
  ForForceExitFullscreen
};


// nsPIDOMWindowInner and nsPIDOMWindowOuter are identical in all respects
// except for the type name. They *must* remain identical so that we can
// reinterpret_cast between them.
template<class T>
class nsPIDOMWindow : public T
{
public:
  nsPIDOMWindowInner* AsInner();
  const nsPIDOMWindowInner* AsInner() const;
  nsPIDOMWindowOuter* AsOuter();
  const nsPIDOMWindowOuter* AsOuter() const;

  virtual nsPIDOMWindowOuter* GetPrivateRoot() = 0;
  virtual mozilla::dom::CustomElementRegistry* CustomElements() = 0;
  // Outer windows only.
  virtual void ActivateOrDeactivate(bool aActivate) = 0;

  // this is called GetTopWindowRoot to avoid conflicts with nsIDOMWindow::GetWindowRoot
  /**
   * |top| gets the root of the window hierarchy.
   *
   * This function does not cross chrome-content boundaries, so if this
   * window's parent is of a different type, |top| will return this window.
   *
   * When script reads the top property, we run GetScriptableTop, which
   * will not cross an <iframe mozbrowser> boundary.
   *
   * In contrast, C++ calls to GetTop are forwarded to GetRealTop, which
   * ignores <iframe mozbrowser> boundaries.
   */

  virtual already_AddRefed<nsPIDOMWindowOuter> GetTop() = 0; // Outer only
  virtual already_AddRefed<nsPIDOMWindowOuter> GetParent() = 0;
  virtual nsPIDOMWindowOuter* GetScriptableTop() = 0;
  virtual nsPIDOMWindowOuter* GetScriptableParent() = 0;
  virtual already_AddRefed<nsPIWindowRoot> GetTopWindowRoot() = 0;

  bool IsRootOuterWindow()
  {
    MOZ_ASSERT(IsOuterWindow());
    return mIsRootOuterWindow;
  }

  /**
   * Behavies identically to GetScriptableParent extept that it returns null
   * if GetScriptableParent would return this window.
   */
  virtual nsPIDOMWindowOuter* GetScriptableParentOrNull() = 0;

  // Inner windows only.
  virtual nsresult RegisterIdleObserver(nsIIdleObserver* aIdleObserver) = 0;
  virtual nsresult UnregisterIdleObserver(nsIIdleObserver* aIdleObserver) = 0;

  virtual bool IsTopLevelWindowActive() = 0;

  // Outer windows only.
  virtual void SetActive(bool aActive)
  {
    MOZ_ASSERT(IsOuterWindow());
    mIsActive = aActive;
  }

  virtual void SetIsBackground(bool aIsBackground)
  {
    MOZ_ASSERT(IsOuterWindow());
    mIsBackground = aIsBackground;
  }

  mozilla::dom::EventTarget* GetChromeEventHandler() const
  {
    return mChromeEventHandler;
  }

  // Outer windows only.
  virtual void SetChromeEventHandler(mozilla::dom::EventTarget* aChromeEventHandler) = 0;

  mozilla::dom::EventTarget* GetParentTarget()
  {
    if (!mParentTarget) {
      UpdateParentTarget();
    }
    return mParentTarget;
  }

  virtual void MaybeUpdateTouchState() {}

  nsIDocument* GetExtantDoc() const
  {
    return mDoc;
  }
  nsIURI* GetDocumentURI() const;
  nsIURI* GetDocBaseURI() const;

  nsIDocument* GetDoc()
  {
    if (!mDoc) {
      MaybeCreateDoc();
    }
    return mDoc;
  }

  virtual bool IsRunningTimeout() = 0;

protected:
  // Lazily instantiate an about:blank document if necessary, and if
  // we have what it takes to do so.
  void MaybeCreateDoc();

public:
  // Check whether a document is currently loading
  inline bool IsLoading() const;
  inline bool IsHandlingResizeEvent() const;

  // Set the window up with an about:blank document with the current subject
  // principal.
  // Outer windows only.
  virtual void SetInitialPrincipalToSubject() = 0;

  virtual PopupControlState PushPopupControlState(PopupControlState aState,
                                                  bool aForce) const = 0;
  virtual void PopPopupControlState(PopupControlState state) const = 0;
  virtual PopupControlState GetPopupControlState() const = 0;

  // Returns an object containing the window's state.  This also suspends
  // all running timeouts in the window.
  virtual already_AddRefed<nsISupports> SaveWindowState() = 0;

  // Restore the window state from aState.
  virtual nsresult RestoreWindowState(nsISupports *aState) = 0;

  // Determine if the window is suspended or frozen.  Outer windows
  // will forward this call to the inner window for convenience.  If
  // there is no inner window then the outer window is considered
  // suspended and frozen by default.
  virtual bool IsSuspended() const = 0;
  virtual bool IsFrozen() const = 0;

  // Fire any DOM notification events related to things that happened while
  // the window was frozen.
  virtual nsresult FireDelayedDOMEvents() = 0;

  nsPIDOMWindowOuter* GetOuterWindow()
  {
    return mIsInnerWindow ? mOuterWindow.get() : AsOuter();
  }

  bool IsInnerWindow() const
  {
    return mIsInnerWindow;
  }

  bool IsOuterWindow() const
  {
    return !IsInnerWindow();
  }

  // Outer windows only.
  virtual bool WouldReuseInnerWindow(nsIDocument* aNewDocument) = 0;

  /**
   * Get the docshell in this window.
   */
  nsIDocShell *GetDocShell() const;

  /**
   * Set the docshell in the window.  Must not be called with a null docshell
   * (use DetachFromDocShell for that).
   */
  virtual void SetDocShell(nsIDocShell *aDocShell) = 0;

  /**
   * Detach an outer window from its docshell.
   */
  virtual void DetachFromDocShell() = 0;

  /**
   * Set a new document in the window. Calling this method will in
   * most cases create a new inner window. If this method is called on
   * an inner window the call will be forewarded to the outer window,
   * if the inner window is not the current inner window an
   * NS_ERROR_NOT_AVAILABLE error code will be returned. This may be
   * called with a pointer to the current document, in that case the
   * document remains unchanged, but a new inner window will be
   * created.
   *
   * aDocument must not be null.
   */
  virtual nsresult SetNewDocument(nsIDocument *aDocument,
                                  nsISupports *aState,
                                  bool aForceReuseInnerWindow) = 0;

  /**
   * Set the opener window.  aOriginalOpener is true if and only if this is the
   * original opener for the window.  That is, it can only be true at most once
   * during the life cycle of a window, and then only the first time
   * SetOpenerWindow is called.  It might never be true, of course, if the
   * window does not have an opener when it's created.
   */
  virtual void SetOpenerWindow(nsPIDOMWindowOuter* aOpener,
                               bool aOriginalOpener) = 0;

  virtual void EnsureSizeUpToDate() = 0;

  /**
   * Callback for notifying a window about a modal dialog being
   * opened/closed with the window as a parent.
   */
  virtual void EnterModalState() = 0;
  virtual void LeaveModalState() = 0;

  // Outer windows only.
  virtual bool CanClose() = 0;
  virtual void ForceClose() = 0;

  bool IsModalContentWindow() const
  {
    return mIsModalContentWindow;
  }

  /**
   * Call this to indicate that some node (this window, its document,
   * or content in that document) has a paint event listener.
   */
  void SetHasPaintEventListeners()
  {
    mMayHavePaintEventListener = true;
  }

  /**
   * Call this to check whether some node (this window, its document,
   * or content in that document) has a paint event listener.
   */
  bool HasPaintEventListeners()
  {
    return mMayHavePaintEventListener;
  }

  /**
   * Call this to indicate that some node (this window, its document,
   * or content in that document) has a touch event listener.
   */
  void SetHasTouchEventListeners()
  {
    if (!mMayHaveTouchEventListener) {
      mMayHaveTouchEventListener = true;
      MaybeUpdateTouchState();
    }
  }

  /**
   * Moves the top-level window into fullscreen mode if aIsFullScreen is true,
   * otherwise exits fullscreen.
   *
   * Outer windows only.
   */
  virtual nsresult SetFullscreenInternal(
    FullscreenReason aReason, bool aIsFullscreen) = 0;

  /**
   * This function should be called when the fullscreen state is flipped.
   * If no widget is involved the fullscreen change, this method is called
   * by SetFullscreenInternal, otherwise, it is called when the widget
   * finishes its change to or from fullscreen.
   *
   * @param aIsFullscreen indicates whether the widget is in fullscreen.
   *
   * Outer windows only.
   */
  virtual void FinishFullscreenChange(bool aIsFullscreen) = 0;

  virtual JSObject* GetCachedXBLPrototypeHandler(nsXBLPrototypeHandler* aKey) = 0;
  virtual void CacheXBLPrototypeHandler(nsXBLPrototypeHandler* aKey,
                                        JS::Handle<JSObject*> aHandler) = 0;

  /*
   * Get and set the currently focused element within the document. If
   * aNeedsFocus is true, then set mNeedsFocus to true to indicate that a
   * document focus event is needed.
   *
   * DO NOT CALL EITHER OF THESE METHODS DIRECTLY. USE THE FOCUS MANAGER
   * INSTEAD.
   */
  nsIContent* GetFocusedNode() const;
  virtual void SetFocusedNode(nsIContent* aNode,
                              uint32_t aFocusMethod = 0,
                              bool aNeedsFocus = false) = 0;

  /**
   * Retrieves the method that was used to focus the current node.
   */
  virtual uint32_t GetFocusMethod() = 0;

  /*
   * Tells the window that it now has focus or has lost focus, based on the
   * state of aFocus. If this method returns true, then the document loaded
   * in the window has never received a focus event and expects to receive
   * one. If false is returned, the document has received a focus event before
   * and should only receive one if the window is being focused.
   *
   * aFocusMethod may be set to one of the focus method constants in
   * nsIFocusManager to indicate how focus was set.
   */
  virtual bool TakeFocus(bool aFocus, uint32_t aFocusMethod) = 0;

  /**
   * Indicates that the window may now accept a document focus event. This
   * should be called once a document has been loaded into the window.
   */
  virtual void SetReadyForFocus() = 0;

  /**
   * Whether the focused content within the window should show a focus ring.
   */
  virtual bool ShouldShowFocusRing() = 0;

  /**
   * Set the keyboard indicator state for accelerators and focus rings.
   */
  virtual void SetKeyboardIndicators(UIStateChangeType aShowAccelerators,
                                     UIStateChangeType aShowFocusRings) = 0;

  /**
   * Indicates that the page in the window has been hidden. This is used to
   * reset the focus state.
   */
  virtual void PageHidden() = 0;

  /**
   * Instructs this window to asynchronously dispatch a hashchange event.  This
   * method must be called on an inner window.
   */
  virtual nsresult DispatchAsyncHashchange(nsIURI *aOldURI,
                                           nsIURI *aNewURI) = 0;

  /**
   * Instructs this window to synchronously dispatch a popState event.
   */
  virtual nsresult DispatchSyncPopState() = 0;

  /**
   * Tell this window that it should listen for sensor changes of the given
   * type.
   *
   * Inner windows only.
   */
  virtual void EnableDeviceSensor(uint32_t aType) = 0;

  /**
   * Tell this window that it should remove itself from sensor change
   * notifications.
   *
   * Inner windows only.
   */
  virtual void DisableDeviceSensor(uint32_t aType) = 0;

#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
  virtual void EnableOrientationChangeListener() = 0;
  virtual void DisableOrientationChangeListener() = 0;
#endif

  virtual void EnableTimeChangeNotifications() = 0;
  virtual void DisableTimeChangeNotifications() = 0;

#ifdef MOZ_B2G
  /**
   * Tell the window that it should start to listen to the network event of the
   * given aType.
   *
   * Inner windows only.
   */
  virtual void EnableNetworkEvent(mozilla::EventMessage aEventMessage) = 0;

  /**
   * Tell the window that it should stop to listen to the network event of the
   * given aType.
   *
   * Inner windows only.
   */
  virtual void DisableNetworkEvent(mozilla::EventMessage aEventMessage) = 0;
#endif // MOZ_B2G

  /**
   * Tell this window that there is an observer for gamepad input
   *
   * Inner windows only.
   */
  virtual void SetHasGamepadEventListener(bool aHasGamepad = true) = 0;

  /**
   * Set a arguments for this window. This will be set on the window
   * right away (if there's an existing document) and it will also be
   * installed on the window when the next document is loaded.
   *
   * This function serves double-duty for passing both |arguments| and
   * |dialogArguments| back from nsWindowWatcher to nsGlobalWindow. For the
   * latter, the array is an array of length 0 whose only element is a
   * DialogArgumentsHolder representing the JS value passed to showModalDialog.
   *
   * Outer windows only.
   */
  virtual nsresult SetArguments(nsIArray *aArguments) = 0;

  /**
   * NOTE! This function *will* be called on multiple threads so the
   * implementation must not do any AddRef/Release or other actions that will
   * mutate internal state.
   */
  virtual uint32_t GetSerial() = 0;

  /**
   * Return the window id of this window
   */
  uint64_t WindowID() const { return mWindowID; }

  /**
   * Dispatch a custom event with name aEventName targeted at this window.
   * Returns whether the default action should be performed.
   *
   * Outer windows only.
   */
  virtual bool DispatchCustomEvent(const nsAString& aEventName) = 0;

  /**
   * Like nsIDOMWindow::Open, except that we don't navigate to the given URL.
   *
   * Outer windows only.
   */
  virtual nsresult
  OpenNoNavigate(const nsAString& aUrl, const nsAString& aName,
                 const nsAString& aOptions, nsPIDOMWindowOuter **_retval) = 0;

  /**
   * Fire a popup blocked event on the document.
   */
  virtual void
  FirePopupBlockedEvent(nsIDocument* aDoc,
                        nsIURI* aPopupURI,
                        const nsAString& aPopupWindowName,
                        const nsAString& aPopupWindowFeatures) = 0;

  // WebIDL-ish APIs
  void MarkUncollectableForCCGeneration(uint32_t aGeneration)
  {
    mMarkedCCGeneration = aGeneration;
  }

  uint32_t GetMarkedCCGeneration()
  {
    return mMarkedCCGeneration;
  }

  virtual nsIDOMScreen* GetScreen() = 0;
  virtual nsIDOMNavigator* GetNavigator() = 0;
  virtual nsIDOMLocation* GetLocation() = 0;
  virtual nsresult GetPrompter(nsIPrompt** aPrompt) = 0;
  virtual nsresult GetControllers(nsIControllers** aControllers) = 0;
  virtual already_AddRefed<nsISelection> GetSelection() = 0;
  virtual already_AddRefed<nsPIDOMWindowOuter> GetOpener() = 0;
  virtual already_AddRefed<nsIDOMWindowCollection> GetFrames() = 0;
  // aLoadInfo will be passed on through to the windowwatcher.
  // aForceNoOpener will act just like a "noopener" feature in aOptions except
  //                will not affect any other window features.
  virtual nsresult Open(const nsAString& aUrl, const nsAString& aName,
                        const nsAString& aOptions,
                        nsIDocShellLoadInfo* aLoadInfo,
                        bool aForceNoOpener,
                        nsPIDOMWindowOuter **_retval) = 0;
  virtual nsresult OpenDialog(const nsAString& aUrl, const nsAString& aName,
                              const nsAString& aOptions,
                              nsISupports* aExtraArgument,
                              nsPIDOMWindowOuter** _retval) = 0;

  virtual nsresult GetDevicePixelRatio(float* aRatio) = 0;
  virtual nsresult GetInnerWidth(int32_t* aWidth) = 0;
  virtual nsresult GetInnerHeight(int32_t* aHeight) = 0;
  virtual already_AddRefed<nsICSSDeclaration>
    GetComputedStyle(mozilla::dom::Element& aElt, const nsAString& aPseudoElt,
                     mozilla::ErrorResult& aError) = 0;
  virtual already_AddRefed<nsIDOMElement> GetFrameElement() = 0;
  virtual already_AddRefed<nsIDOMOfflineResourceList> GetApplicationCache() = 0;
  virtual bool Closed() = 0;
  virtual bool GetFullScreen() = 0;
  virtual nsresult SetFullScreen(bool aFullScreen) = 0;

  virtual nsresult Focus() = 0;
  virtual nsresult Close() = 0;

  virtual nsresult MoveBy(int32_t aXDif, int32_t aYDif) = 0;
  virtual nsresult UpdateCommands(const nsAString& anAction, nsISelection* aSel, int16_t aReason) = 0;

  mozilla::dom::TabGroup* TabGroup();

  mozilla::dom::DocGroup* GetDocGroup();

  virtual mozilla::ThrottledEventQueue* GetThrottledEventQueue() = 0;

protected:
  // The nsPIDOMWindow constructor. The aOuterWindow argument should
  // be null if and only if the created window itself is an outer
  // window. In all other cases aOuterWindow should be the outer
  // window for the inner window that is being created.
  explicit nsPIDOMWindow<T>(nsPIDOMWindowOuter *aOuterWindow);

  ~nsPIDOMWindow<T>();

  void SetChromeEventHandlerInternal(mozilla::dom::EventTarget* aChromeEventHandler) {
    mChromeEventHandler = aChromeEventHandler;
    // mParentTarget will be set when the next event is dispatched.
    mParentTarget = nullptr;
  }

  virtual void UpdateParentTarget() = 0;

  // These two variables are special in that they're set to the same
  // value on both the outer window and the current inner window. Make
  // sure you keep them in sync!
  nsCOMPtr<mozilla::dom::EventTarget> mChromeEventHandler; // strong
  nsCOMPtr<nsIDocument> mDoc; // strong
  // Cache the URI when mDoc is cleared.
  nsCOMPtr<nsIURI> mDocumentURI; // strong
  nsCOMPtr<nsIURI> mDocBaseURI; // strong

  nsCOMPtr<mozilla::dom::EventTarget> mParentTarget; // strong

  // These members are only used on outer windows.
  nsCOMPtr<mozilla::dom::Element> mFrameElement;
  // This reference is used by the subclass nsGlobalWindow, and cleared in it's
  // DetachFromDocShell() method. This method is called by nsDocShell::Destroy(),
  // which is called before the nsDocShell is destroyed.
  nsIDocShell* MOZ_NON_OWNING_REF mDocShell;  // Weak Reference

  // mPerformance is only used on inner windows.
  RefPtr<mozilla::dom::Performance> mPerformance;

  typedef nsRefPtrHashtable<nsStringHashKey,
                            mozilla::dom::ServiceWorkerRegistration>
          ServiceWorkerRegistrationTable;
  ServiceWorkerRegistrationTable mServiceWorkerRegistrationTable;

  uint32_t               mModalStateDepth;

  // These variables are only used on inner windows.
  mozilla::dom::Timeout *mRunningTimeout;

  uint32_t               mMutationBits;

  bool                   mIsDocumentLoaded;
  bool                   mIsHandlingResizeEvent;
  bool                   mIsInnerWindow;
  bool                   mMayHavePaintEventListener;
  bool                   mMayHaveTouchEventListener;
  bool                   mMayHaveMouseEnterLeaveEventListener;
  bool                   mMayHavePointerEnterLeaveEventListener;

  // Used to detect whether we have called FreeInnerObjects() (e.g. to ensure
  // that a call to ResumeTimeouts() after FreeInnerObjects() does nothing).
  // This member is only used by inner windows.
  bool                   mInnerObjectsFreed;


  // This variable is used on both inner and outer windows (and they
  // should match).
  bool                   mIsModalContentWindow;

  // Tracks activation state that's used for :-moz-window-inactive.
  // Only used on outer windows.
  bool                   mIsActive;

  // Tracks whether our docshell is active.  If it is, mIsBackground
  // is false.  Too bad we have so many different concepts of
  // "active".  Only used on outer windows.
  bool                   mIsBackground;

  /**
   * The suspended types can be "disposable" or "permanent". This varable only
   * stores the value about permanent suspend.
   * - disposable
   * To pause all playing media in that window, but doesn't affect the media
   * which starts after that.
   *
   * - permanent
   * To pause all media in that window, and also affect the media which starts
   * after that.
   */
  SuspendTypes       mMediaSuspend;

  bool                   mAudioMuted;
  float                  mAudioVolume;

  bool                   mAudioCaptured;

  // current desktop mode flag.
  bool                   mDesktopModeViewport;

  bool                   mIsRootOuterWindow;

  // And these are the references between inner and outer windows.
  nsPIDOMWindowInner* MOZ_NON_OWNING_REF mInnerWindow;
  nsCOMPtr<nsPIDOMWindowOuter> mOuterWindow;

  // the element within the document that is currently focused when this
  // window is active
  nsCOMPtr<nsIContent> mFocusedNode;

  // The AudioContexts created for the current document, if any.
  nsTArray<mozilla::dom::AudioContext*> mAudioContexts; // Weak

  // This is present both on outer and inner windows.
  RefPtr<mozilla::dom::TabGroup> mTabGroup;

  // A unique (as long as our 64-bit counter doesn't roll over) id for
  // this window.
  uint64_t mWindowID;

  // This is only used by the inner window. Set to true once we've sent
  // the (chrome|content)-document-global-created notification.
  bool mHasNotifiedGlobalCreated;

  uint32_t mMarkedCCGeneration;

  // Let the service workers plumbing know that some feature are enabled while
  // testing.
  bool mServiceWorkersTestingEnabled;
};

#define NS_PIDOMWINDOWINNER_IID \
{ 0x775dabc9, 0x8f43, 0x4277, \
  { 0x9a, 0xdb, 0xf1, 0x99, 0x0d, 0x77, 0xcf, 0xfb } }

#define NS_PIDOMWINDOWOUTER_IID \
  { 0x769693d4, 0xb009, 0x4fe2, \
  { 0xaf, 0x18, 0x7d, 0xc8, 0xdf, 0x74, 0x96, 0xdf } }

// NB: It's very very important that these two classes have identical vtables
// and memory layout!
class nsPIDOMWindowInner : public nsPIDOMWindow<mozIDOMWindow>
{
  friend nsGlobalWindow;

public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_PIDOMWINDOWINNER_IID)

  static nsPIDOMWindowInner* From(mozIDOMWindow* aFrom) {
    return static_cast<nsPIDOMWindowInner*>(aFrom);
  }

  // Returns true if this object has an outer window and it is the current inner
  // window of that outer.
  inline bool IsCurrentInnerWindow() const;

  // Returns true if the document of this window is the active document.  This
  // is not identical to IsCurrentInnerWindow() because document.open() will
  // keep the same document active but create a new window.
  inline bool HasActiveDocument();

  bool AddAudioContext(mozilla::dom::AudioContext* aAudioContext);
  void RemoveAudioContext(mozilla::dom::AudioContext* aAudioContext);
  void MuteAudioContexts();
  void UnmuteAudioContexts();

  bool GetAudioCaptured() const;
  nsresult SetAudioCapture(bool aCapture);

  already_AddRefed<mozilla::dom::ServiceWorkerRegistration>
    GetServiceWorkerRegistration(const nsAString& aScope);
  void InvalidateServiceWorkerRegistration(const nsAString& aScope);

  mozilla::dom::Performance* GetPerformance();

  bool HasMutationListeners(uint32_t aMutationEventType) const
  {
    if (!mOuterWindow) {
      NS_ERROR("HasMutationListeners() called on orphan inner window!");

      return false;
    }

    return (mMutationBits & aMutationEventType) != 0;
  }

  void SetMutationListeners(uint32_t aType)
  {
    if (!mOuterWindow) {
      NS_ERROR("HasMutationListeners() called on orphan inner window!");

      return;
    }

    mMutationBits |= aType;
  }

  /**
   * Call this to check whether some node (this window, its document,
   * or content in that document) has a mouseenter/leave event listener.
   */
  bool HasMouseEnterLeaveEventListeners()
  {
    return mMayHaveMouseEnterLeaveEventListener;
  }

  /**
   * Call this to indicate that some node (this window, its document,
   * or content in that document) has a mouseenter/leave event listener.
   */
  void SetHasMouseEnterLeaveEventListeners()
  {
    mMayHaveMouseEnterLeaveEventListener = true;
  }

  /**
   * Call this to check whether some node (this window, its document,
   * or content in that document) has a Pointerenter/leave event listener.
   */
  bool HasPointerEnterLeaveEventListeners()
  {
    return mMayHavePointerEnterLeaveEventListener;
  }

  /**
   * Call this to indicate that some node (this window, its document,
   * or content in that document) has a Pointerenter/leave event listener.
   */
  void SetHasPointerEnterLeaveEventListeners()
  {
    mMayHavePointerEnterLeaveEventListener = true;
  }

  /**
   * Check whether this has had inner objects freed.
   */
  bool InnerObjectsFreed() const
  {
    return mInnerObjectsFreed;
  }

  /**
   * Check whether this window is a secure context.
   */
  bool IsSecureContext() const;

  // Calling suspend should prevent any asynchronous tasks from
  // executing javascript for this window.  This means setTimeout,
  // requestAnimationFrame, and events should not be fired. Suspending
  // a window also suspends its children and workers.  Workers may
  // continue to perform computations in the background.  A window
  // can have Suspend() called multiple times and will only resume after
  // a matching number of Resume() calls.
  void Suspend();
  void Resume();

  // Calling Freeze() on a window will automatically Suspend() it.  In
  // addition, the window and its children are further treated as no longer
  // suitable for interaction with the user.  For example, it may be marked
  // non-visible, cannot be focused, etc.  All worker threads are also frozen
  // bringing them to a complete stop.  A window can have Freeze() called
  // multiple times and will only thaw after a matching number of Thaw()
  // calls.
  void Freeze();
  void Thaw();

  // Apply the parent window's suspend, freeze, and modal state to the current
  // window.
  void SyncStateFromParentWindow();

protected:
  void CreatePerformanceObjectIfNeeded();
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPIDOMWindowInner, NS_PIDOMWINDOWINNER_IID)

// NB: It's very very important that these two classes have identical vtables
// and memory layout!
class nsPIDOMWindowOuter : public nsPIDOMWindow<mozIDOMWindowProxy>
{
protected:
  void RefreshMediaElementsVolume();
  void RefreshMediaElementsSuspend(SuspendTypes aSuspend);
  bool IsDisposableSuspend(SuspendTypes aSuspend) const;

public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_PIDOMWINDOWOUTER_IID)

  static nsPIDOMWindowOuter* From(mozIDOMWindowProxy* aFrom) {
    return static_cast<nsPIDOMWindowOuter*>(aFrom);
  }

  // Given an inner window, return its outer if the inner is the current inner.
  // Otherwise (argument null or not an inner or not current) return null.
  static nsPIDOMWindowOuter* GetFromCurrentInner(nsPIDOMWindowInner* aInner);

  nsPIDOMWindowInner* GetCurrentInnerWindow() const
  {
    return mInnerWindow;
  }

  nsPIDOMWindowInner* EnsureInnerWindow()
  {
    MOZ_ASSERT(IsOuterWindow());
    // GetDoc forces inner window creation if there isn't one already
    GetDoc();
    return GetCurrentInnerWindow();
  }

  /**
   * Set initial keyboard indicator state for accelerators and focus rings.
   */
  void SetInitialKeyboardIndicators(UIStateChangeType aShowAccelerators,
                                    UIStateChangeType aShowFocusRings);

  // Internal getter/setter for the frame element, this version of the
  // getter crosses chrome boundaries whereas the public scriptable
  // one doesn't for security reasons.
  mozilla::dom::Element* GetFrameElementInternal() const;
  void SetFrameElementInternal(mozilla::dom::Element* aFrameElement);

  bool IsActive()
  {
    return mIsActive;
  }

  void SetDesktopModeViewport(bool aDesktopModeViewport)
  {
    mDesktopModeViewport = aDesktopModeViewport;
  }
  bool IsDesktopModeViewport() const
  {
    return mDesktopModeViewport;
  }
  bool IsBackground()
  {
    return mIsBackground;
  }

  // Audio API
  SuspendTypes GetMediaSuspend() const;
  void SetMediaSuspend(SuspendTypes aSuspend);

  bool GetAudioMuted() const;
  void SetAudioMuted(bool aMuted);

  float GetAudioVolume() const;
  nsresult SetAudioVolume(float aVolume);

  void SetServiceWorkersTestingEnabled(bool aEnabled);
  bool GetServiceWorkersTestingEnabled();
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPIDOMWindowOuter, NS_PIDOMWINDOWOUTER_IID)

#include "nsPIDOMWindowInlines.h"

#ifdef MOZILLA_INTERNAL_API
PopupControlState
PushPopupControlState(PopupControlState aState, bool aForce);

void
PopPopupControlState(PopupControlState aState);

#define NS_AUTO_POPUP_STATE_PUSHER nsAutoPopupStatePusherInternal
#else
#define NS_AUTO_POPUP_STATE_PUSHER nsAutoPopupStatePusherExternal
#endif

// Helper class that helps with pushing and popping popup control
// state. Note that this class looks different from within code that's
// part of the layout library than it does in code outside the layout
// library.  We give the two object layouts different names so the symbols
// don't conflict, but code should always use the name
// |nsAutoPopupStatePusher|.
class NS_AUTO_POPUP_STATE_PUSHER
{
public:
#ifdef MOZILLA_INTERNAL_API
  explicit NS_AUTO_POPUP_STATE_PUSHER(PopupControlState aState, bool aForce = false)
    : mOldState(::PushPopupControlState(aState, aForce))
  {
  }

  ~NS_AUTO_POPUP_STATE_PUSHER()
  {
    PopPopupControlState(mOldState);
  }
#else
  NS_AUTO_POPUP_STATE_PUSHER(nsPIDOMWindowOuter *aWindow, PopupControlState aState)
    : mWindow(aWindow), mOldState(openAbused)
  {
    if (aWindow) {
      mOldState = aWindow->PushPopupControlState(aState, false);
    }
  }

  ~NS_AUTO_POPUP_STATE_PUSHER()
  {
    if (mWindow) {
      mWindow->PopPopupControlState(mOldState);
    }
  }
#endif

protected:
#ifndef MOZILLA_INTERNAL_API
  nsCOMPtr<nsPIDOMWindowOuter> mWindow;
#endif
  PopupControlState mOldState;

private:
  // Hide so that this class can only be stack-allocated
  static void* operator new(size_t /*size*/) CPP_THROW_NEW { return nullptr; }
  static void operator delete(void* /*memory*/) {}
};

#define nsAutoPopupStatePusher NS_AUTO_POPUP_STATE_PUSHER

#endif // nsPIDOMWindow_h__
