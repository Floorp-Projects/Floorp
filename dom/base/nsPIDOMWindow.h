/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef nsPIDOMWindow_h__
#define nsPIDOMWindow_h__

#include "nsIDOMWindow.h"

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "mozilla/dom/EventTarget.h"
#include "js/TypeDecls.h"

#define DOM_WINDOW_DESTROYED_TOPIC "dom-window-destroyed"
#define DOM_WINDOW_FROZEN_TOPIC "dom-window-frozen"
#define DOM_WINDOW_THAWED_TOPIC "dom-window-thawed"

class nsIArray;
class nsIContent;
class nsIDocShell;
class nsIDocument;
class nsIIdleObserver;
class nsIScriptTimeoutHandler;
class nsIURI;
class nsPerformance;
class nsPIWindowRoot;
class nsXBLPrototypeHandler;
struct nsTimeout;

namespace mozilla {
namespace dom {
class AudioContext;
class Element;
}
namespace gfx {
class VRHMDInfo;
}
}

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
  UIStateChangeType_Clear
};

#define NS_PIDOMWINDOW_IID \
{ 0x2aebbbd7, 0x154b, 0x4341, \
  { 0x8d, 0x02, 0x7f, 0x70, 0xf8, 0x3e, 0xf7, 0xa1 } }

class nsPIDOMWindow : public nsIDOMWindowInternal
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_PIDOMWINDOW_IID)

  virtual nsPIDOMWindow* GetPrivateRoot() = 0;

  // Outer windows only.
  virtual void ActivateOrDeactivate(bool aActivate) = 0;

  // this is called GetTopWindowRoot to avoid conflicts with nsIDOMWindow::GetWindowRoot
  virtual already_AddRefed<nsPIWindowRoot> GetTopWindowRoot() = 0;

  // Inner windows only.
  virtual nsresult RegisterIdleObserver(nsIIdleObserver* aIdleObserver) = 0;
  virtual nsresult UnregisterIdleObserver(nsIIdleObserver* aIdleObserver) = 0;

  // Outer windows only.
  virtual void SetActive(bool aActive)
  {
    MOZ_ASSERT(IsOuterWindow());
    mIsActive = aActive;
  }
  bool IsActive()
  {
    MOZ_ASSERT(IsOuterWindow());
    return mIsActive;
  }

  // Outer windows only.
  void SetDesktopModeViewport(bool aDesktopModeViewport)
  {
    MOZ_ASSERT(IsOuterWindow());
    mDesktopModeViewport = aDesktopModeViewport;
  }
  bool IsDesktopModeViewport() const
  {
    MOZ_ASSERT(IsOuterWindow());
    return mDesktopModeViewport;
  }

  virtual void SetIsBackground(bool aIsBackground)
  {
    MOZ_ASSERT(IsOuterWindow());
    mIsBackground = aIsBackground;
  }
  bool IsBackground()
  {
    MOZ_ASSERT(IsOuterWindow());
    return mIsBackground;
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

  bool HasMutationListeners(uint32_t aMutationEventType) const
  {
    MOZ_ASSERT(IsInnerWindow());

    if (!mOuterWindow) {
      NS_ERROR("HasMutationListeners() called on orphan inner window!");

      return false;
    }

    return (mMutationBits & aMutationEventType) != 0;
  }

  void SetMutationListeners(uint32_t aType)
  {
    MOZ_ASSERT(IsInnerWindow());

    if (!mOuterWindow) {
      NS_ERROR("HasMutationListeners() called on orphan inner window!");

      return;
    }

    mMutationBits |= aType;
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

  // Audio API
  bool GetAudioMuted() const;
  void SetAudioMuted(bool aMuted);

  float GetAudioVolume() const;
  nsresult SetAudioVolume(float aVolume);

  float GetAudioGlobalVolume();

  virtual void SetServiceWorkersTestingEnabled(bool aEnabled)
  {
    MOZ_ASSERT(IsOuterWindow());
    mServiceWorkersTestingEnabled = aEnabled;
  }

  bool GetServiceWorkersTestingEnabled()
  {
    MOZ_ASSERT(IsOuterWindow());
    return mServiceWorkersTestingEnabled;
  }

protected:
  // Lazily instantiate an about:blank document if necessary, and if
  // we have what it takes to do so.
  void MaybeCreateDoc();

  float GetAudioGlobalVolumeInternal(float aVolume);
  void RefreshMediaElements();

public:
  // Internal getter/setter for the frame element, this version of the
  // getter crosses chrome boundaries whereas the public scriptable
  // one doesn't for security reasons.
  mozilla::dom::Element* GetFrameElementInternal() const;
  void SetFrameElementInternal(mozilla::dom::Element* aFrameElement);

  bool IsLoadingOrRunningTimeout() const
  {
    const nsPIDOMWindow* win = IsInnerWindow() ? this : GetCurrentInnerWindow();
    return !win->mIsDocumentLoaded || win->mRunningTimeout;
  }

  // Check whether a document is currently loading
  bool IsLoading() const
  {
    const nsPIDOMWindow *win;

    if (IsOuterWindow()) {
      win = GetCurrentInnerWindow();

      if (!win) {
        NS_ERROR("No current inner window available!");

        return false;
      }
    } else {
      if (!mOuterWindow) {
        NS_ERROR("IsLoading() called on orphan inner window!");

        return false;
      }

      win = this;
    }

    return !win->mIsDocumentLoaded;
  }

  bool IsHandlingResizeEvent() const
  {
    const nsPIDOMWindow *win;

    if (IsOuterWindow()) {
      win = GetCurrentInnerWindow();

      if (!win) {
        NS_ERROR("No current inner window available!");

        return false;
      }
    } else {
      if (!mOuterWindow) {
        NS_ERROR("IsHandlingResizeEvent() called on orphan inner window!");

        return false;
      }

      win = this;
    }

    return win->mIsHandlingResizeEvent;
  }

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

  // Suspend timeouts in this window and in child windows.
  virtual void SuspendTimeouts(uint32_t aIncrease = 1,
                               bool aFreezeChildren = true) = 0;

  // Resume suspended timeouts in this window and in child windows.
  virtual nsresult ResumeTimeouts(bool aThawChildren = true) = 0;

  virtual uint32_t TimeoutSuspendCount() = 0;

  // Fire any DOM notification events related to things that happened while
  // the window was frozen.
  virtual nsresult FireDelayedDOMEvents() = 0;

  virtual bool IsFrozen() const = 0;

  // Add a timeout to this window.
  virtual nsresult SetTimeoutOrInterval(nsIScriptTimeoutHandler *aHandler,
                                        int32_t interval,
                                        bool aIsInterval, int32_t *aReturn) = 0;

  // Clear a timeout from this window.
  virtual nsresult ClearTimeoutOrInterval(int32_t aTimerID) = 0;

  nsPIDOMWindow *GetOuterWindow()
  {
    return mIsInnerWindow ? mOuterWindow.get() : this;
  }

  nsPIDOMWindow *GetCurrentInnerWindow() const
  {
    MOZ_ASSERT(IsOuterWindow());
    return mInnerWindow;
  }

  nsPIDOMWindow *EnsureInnerWindow()
  {
    NS_ASSERTION(IsOuterWindow(), "EnsureInnerWindow called on inner window");
    // GetDoc forces inner window creation if there isn't one already
    GetDoc();
    return GetCurrentInnerWindow();
  }

  bool IsInnerWindow() const
  {
    return mIsInnerWindow;
  }

  // Returns true if this object has an outer window and it is the current inner
  // window of that outer. Only call this on inner windows.
  bool IsCurrentInnerWindow() const
  {
    MOZ_ASSERT(IsInnerWindow(),
               "It doesn't make sense to call this on outer windows.");
    return mOuterWindow && mOuterWindow->GetCurrentInnerWindow() == this;
  }

  // Returns true if the document of this window is the active document.  This
  // is not identical to IsCurrentInnerWindow() because document.open() will
  // keep the same document active but create a new window.
  bool HasActiveDocument()
  {
    MOZ_ASSERT(IsInnerWindow());
    return IsCurrentInnerWindow() ||
      (mOuterWindow &&
       mOuterWindow->GetCurrentInnerWindow() &&
       mOuterWindow->GetCurrentInnerWindow()->GetDoc() == mDoc);
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
  nsIDocShell *GetDocShell()
  {
    if (mOuterWindow) {
      return mOuterWindow->mDocShell;
    }

    return mDocShell;
  }

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
  virtual void SetOpenerWindow(nsIDOMWindow* aOpener,
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

  enum FullscreenReason
  {
    // Toggling the fullscreen mode requires trusted context.
    eForFullscreenMode,
    // Fullscreen API is the API provided to untrusted content.
    eForFullscreenAPI
  };

  /**
   * Moves the top-level window into fullscreen mode if aIsFullScreen is true,
   * otherwise exits fullscreen.
   *
   * If aHMD is not null, the window is made full screen on the given VR HMD
   * device instead of its currrent display.
   *
   * Outer windows only.
   */
  virtual nsresult SetFullscreenInternal(
    FullscreenReason aReason, bool aIsFullscreen,
    mozilla::gfx::VRHMDInfo *aHMD = nullptr) = 0;

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
  nsIContent* GetFocusedNode()
  {
    if (IsOuterWindow()) {
      return mInnerWindow ? mInnerWindow->mFocusedNode.get() : nullptr;
    }
    return mFocusedNode;
  }
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
   * Get the keyboard indicator state for accelerators and focus rings.
   */
  virtual void GetKeyboardIndicators(bool* aShowAccelerators,
                                     bool* aShowFocusRings) = 0;

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

  virtual void EnableTimeChangeNotifications() = 0;
  virtual void DisableTimeChangeNotifications() = 0;

#ifdef MOZ_B2G
  /**
   * Tell the window that it should start to listen to the network event of the
   * given aType.
   *
   * Inner windows only.
   */
  virtual void EnableNetworkEvent(uint32_t aType) = 0;

  /**
   * Tell the window that it should stop to listen to the network event of the
   * given aType.
   *
   * Inner windows only.
   */
  virtual void DisableNetworkEvent(uint32_t aType) = 0;
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
   * Call when the document principal may have changed and the compartment
   * principal needs to be updated.
   *
   * Inner windows only.
   */
  virtual void RefreshCompartmentPrincipal() = 0;

  /**
   * Like nsIDOMWindow::Open, except that we don't navigate to the given URL.
   *
   * Outer windows only.
   */
  virtual nsresult
  OpenNoNavigate(const nsAString& aUrl, const nsAString& aName,
                 const nsAString& aOptions, nsIDOMWindow **_retval) = 0;

  /**
   * Fire a popup blocked event on the document.
   */
  virtual void
  FirePopupBlockedEvent(nsIDocument* aDoc,
                        nsIURI* aPopupURI,
                        const nsAString& aPopupWindowName,
                        const nsAString& aPopupWindowFeatures) = 0;

  // Inner windows only.
  void AddAudioContext(mozilla::dom::AudioContext* aAudioContext);
  void RemoveAudioContext(mozilla::dom::AudioContext* aAudioContext);
  void MuteAudioContexts();
  void UnmuteAudioContexts();

  // Given an inner window, return its outer if the inner is the current inner.
  // Otherwise (argument null or not an inner or not current) return null.
  static nsPIDOMWindow* GetOuterFromCurrentInner(nsPIDOMWindow* aInner)
  {
    if (!aInner) {
      return nullptr;
    }

    nsPIDOMWindow* outer = aInner->GetOuterWindow();
    if (!outer || outer->GetCurrentInnerWindow() != aInner) {
      return nullptr;
    }

    return outer;
  }

  // WebIDL-ish APIs
  nsPerformance* GetPerformance();

  void MarkUncollectableForCCGeneration(uint32_t aGeneration)
  {
    mMarkedCCGeneration = aGeneration;
  }

  uint32_t GetMarkedCCGeneration()
  {
    return mMarkedCCGeneration;
  }

protected:
  // The nsPIDOMWindow constructor. The aOuterWindow argument should
  // be null if and only if the created window itself is an outer
  // window. In all other cases aOuterWindow should be the outer
  // window for the inner window that is being created.
  explicit nsPIDOMWindow(nsPIDOMWindow *aOuterWindow);

  ~nsPIDOMWindow();

  void SetChromeEventHandlerInternal(mozilla::dom::EventTarget* aChromeEventHandler) {
    mChromeEventHandler = aChromeEventHandler;
    // mParentTarget will be set when the next event is dispatched.
    mParentTarget = nullptr;
  }

  virtual void UpdateParentTarget() = 0;

  // Helper for creating performance objects.
  // Inner windows only.
  void CreatePerformanceObjectIfNeeded();

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
  nsRefPtr<nsPerformance>       mPerformance;

  uint32_t               mModalStateDepth;

  // These variables are only used on inner windows.
  nsTimeout             *mRunningTimeout;

  uint32_t               mMutationBits;

  bool                   mIsDocumentLoaded;
  bool                   mIsHandlingResizeEvent;
  bool                   mIsInnerWindow;
  bool                   mMayHavePaintEventListener;
  bool                   mMayHaveTouchEventListener;
  bool                   mMayHaveMouseEnterLeaveEventListener;
  bool                   mMayHavePointerEnterLeaveEventListener;

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

  bool                   mAudioMuted;
  float                  mAudioVolume;

  // current desktop mode flag.
  bool                   mDesktopModeViewport;

  // And these are the references between inner and outer windows.
  nsPIDOMWindow* MOZ_NON_OWNING_REF mInnerWindow;
  nsCOMPtr<nsPIDOMWindow> mOuterWindow;

  // the element within the document that is currently focused when this
  // window is active
  nsCOMPtr<nsIContent> mFocusedNode;

  // The AudioContexts created for the current document, if any.
  nsTArray<mozilla::dom::AudioContext*> mAudioContexts; // Weak

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


NS_DEFINE_STATIC_IID_ACCESSOR(nsPIDOMWindow, NS_PIDOMWINDOW_IID)

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
  NS_AUTO_POPUP_STATE_PUSHER(nsPIDOMWindow *aWindow, PopupControlState aState)
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
  nsCOMPtr<nsPIDOMWindow> mWindow;
#endif
  PopupControlState mOldState;

private:
  // Hide so that this class can only be stack-allocated
  static void* operator new(size_t /*size*/) CPP_THROW_NEW { return nullptr; }
  static void operator delete(void* /*memory*/) {}
};

#define nsAutoPopupStatePusher NS_AUTO_POPUP_STATE_PUSHER

#endif // nsPIDOMWindow_h__
