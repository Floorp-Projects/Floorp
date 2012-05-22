/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef nsPIDOMWindow_h__
#define nsPIDOMWindow_h__

#include "nsIDOMWindow.h"

#include "nsIDOMLocation.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMDocument.h"
#include "nsCOMPtr.h"
#include "nsEvent.h"
#include "nsIURI.h"

#define DOM_WINDOW_DESTROYED_TOPIC "dom-window-destroyed"
#define DOM_WINDOW_FROZEN_TOPIC "dom-window-frozen"
#define DOM_WINDOW_THAWED_TOPIC "dom-window-thawed"

class nsIPrincipal;

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

class nsIDocShell;
class nsIContent;
class nsIDocument;
class nsIScriptTimeoutHandler;
struct nsTimeout;
template <class> class nsScriptObjectHolder;
class nsXBLPrototypeHandler;
class nsIArray;
class nsPIWindowRoot;

#define NS_PIDOMWINDOW_IID \
{ 0xf0bafbe6, 0xe45c, 0x490e, \
  { 0xa2, 0x1c, 0xfe, 0x14, 0x2f, 0xb6, 0x34, 0xba } }

class nsPIDOMWindow : public nsIDOMWindowInternal
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_PIDOMWINDOW_IID)

  virtual nsPIDOMWindow* GetPrivateRoot() = 0;

  virtual void ActivateOrDeactivate(bool aActivate) = 0;

  // this is called GetTopWindowRoot to avoid conflicts with nsIDOMWindow::GetWindowRoot
  virtual already_AddRefed<nsPIWindowRoot> GetTopWindowRoot() = 0;

  virtual void SetActive(bool aActive)
  {
    NS_PRECONDITION(IsOuterWindow(),
                    "active state is only maintained on outer windows");
    mIsActive = aActive;
  }

  bool IsActive()
  {
    NS_PRECONDITION(IsOuterWindow(),
                    "active state is only maintained on outer windows");
    return mIsActive;
  }

  virtual void SetIsBackground(bool aIsBackground)
  {
    NS_PRECONDITION(IsOuterWindow(),
                    "background state is only maintained on outer windows");
    mIsBackground = aIsBackground;
  }

  bool IsBackground()
  {
    NS_PRECONDITION(IsOuterWindow(),
                    "background state is only maintained on outer windows");
    return mIsBackground;
  }

  nsIDOMEventTarget* GetChromeEventHandler() const
  {
    return mChromeEventHandler;
  }

  virtual void SetChromeEventHandler(nsIDOMEventTarget* aChromeEventHandler) = 0;

  nsIDOMEventTarget* GetParentTarget()
  {
    if (!mParentTarget) {
      UpdateParentTarget();
    }
    return mParentTarget;
  }

  bool HasMutationListeners(PRUint32 aMutationEventType) const
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
        NS_ERROR("HasMutationListeners() called on orphan inner window!");

        return false;
      }

      win = this;
    }

    return (win->mMutationBits & aMutationEventType) != 0;
  }

  void SetMutationListeners(PRUint32 aType)
  {
    nsPIDOMWindow *win;

    if (IsOuterWindow()) {
      win = GetCurrentInnerWindow();

      if (!win) {
        NS_ERROR("No inner window available to set mutation bits on!");

        return;
      }
    } else {
      if (!mOuterWindow) {
        NS_ERROR("HasMutationListeners() called on orphan inner window!");

        return;
      }

      win = this;
    }

    win->mMutationBits |= aType;
  }

  virtual void MaybeUpdateTouchState() {}
  virtual void UpdateTouchState() {}

  // GetExtantDocument provides a backdoor to the DOM GetDocument accessor
  nsIDOMDocument* GetExtantDocument() const
  {
    return mDocument;
  }
  nsIDocument* GetExtantDoc() const
  {
    return mDoc;
  }

  // Internal getter/setter for the frame element, this version of the
  // getter crosses chrome boundaries whereas the public scriptable
  // one doesn't for security reasons.
  nsIDOMElement* GetFrameElementInternal() const
  {
    if (mOuterWindow) {
      return mOuterWindow->GetFrameElementInternal();
    }

    NS_ASSERTION(!IsInnerWindow(),
                 "GetFrameElementInternal() called on orphan inner window");

    return mFrameElement;
  }

  void SetFrameElementInternal(nsIDOMElement *aFrameElement)
  {
    if (IsOuterWindow()) {
      mFrameElement = aFrameElement;

      return;
    }

    if (!mOuterWindow) {
      NS_ERROR("frameElement set on inner window with no outer!");

      return;
    }

    mOuterWindow->SetFrameElementInternal(aFrameElement);
  }

  bool IsLoadingOrRunningTimeout() const
  {
    const nsPIDOMWindow *win = GetCurrentInnerWindow();

    if (!win) {
      win = this;
    }

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

  // Tell this window who opened it.  This only has an effect if there is
  // either no document currently in the window or if the document is the
  // original document this window came with (an about:blank document either
  // preloaded into it when it was created, or created by
  // CreateAboutBlankContentViewer()).
  virtual void SetOpenerScriptPrincipal(nsIPrincipal* aPrincipal) = 0;
  // Ask this window who opened it.
  virtual nsIPrincipal* GetOpenerScriptPrincipal() = 0;

  virtual PopupControlState PushPopupControlState(PopupControlState aState,
                                                  bool aForce) const = 0;
  virtual void PopPopupControlState(PopupControlState state) const = 0;
  virtual PopupControlState GetPopupControlState() const = 0;

  // Returns an object containing the window's state.  This also suspends
  // all running timeouts in the window.
  virtual nsresult SaveWindowState(nsISupports **aState) = 0;

  // Restore the window state from aState.
  virtual nsresult RestoreWindowState(nsISupports *aState) = 0;

  // Suspend timeouts in this window and in child windows.
  virtual void SuspendTimeouts(PRUint32 aIncrease = 1,
                               bool aFreezeChildren = true) = 0;

  // Resume suspended timeouts in this window and in child windows.
  virtual nsresult ResumeTimeouts(bool aThawChildren = true) = 0;

  virtual PRUint32 TimeoutSuspendCount() = 0;

  // Fire any DOM notification events related to things that happened while
  // the window was frozen.
  virtual nsresult FireDelayedDOMEvents() = 0;

  virtual bool IsFrozen() const = 0;

  // Add a timeout to this window.
  virtual nsresult SetTimeoutOrInterval(nsIScriptTimeoutHandler *aHandler,
                                        PRInt32 interval,
                                        bool aIsInterval, PRInt32 *aReturn) = 0;

  // Clear a timeout from this window.
  virtual nsresult ClearTimeoutOrInterval(PRInt32 aTimerID) = 0;

  nsPIDOMWindow *GetOuterWindow()
  {
    return mIsInnerWindow ? mOuterWindow.get() : this;
  }

  nsPIDOMWindow *GetCurrentInnerWindow() const
  {
    return mInnerWindow;
  }

  nsPIDOMWindow *EnsureInnerWindow()
  {
    NS_ASSERTION(IsOuterWindow(), "EnsureInnerWindow called on inner window");
    // GetDocument forces inner window creation if there isn't one already
    nsCOMPtr<nsIDOMDocument> doc;
    GetDocument(getter_AddRefs(doc));
    return GetCurrentInnerWindow();
  }

  bool IsInnerWindow() const
  {
    return mIsInnerWindow;
  }

  bool IsOuterWindow() const
  {
    return !IsInnerWindow();
  }

  virtual bool WouldReuseInnerWindow(nsIDocument *aNewDocument) = 0;

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
   * Set or unset the docshell in the window.
   */
  virtual void SetDocShell(nsIDocShell *aDocShell) = 0;

  /**
   * Set a new document in the window. Calling this method will in
   * most cases create a new inner window. If this method is called on
   * an inner window the call will be forewarded to the outer window,
   * if the inner window is not the current inner window an
   * NS_ERROR_NOT_AVAILABLE error code will be returned. This may be
   * called with a pointer to the current document, in that case the
   * document remains unchanged, but a new inner window will be
   * created.
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
  virtual nsIDOMWindow *EnterModalState() = 0;
  virtual void LeaveModalState(nsIDOMWindow *) = 0;

  virtual bool CanClose() = 0;
  virtual nsresult ForceClose() = 0;

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
    mMayHaveTouchEventListener = true;
    MaybeUpdateTouchState();
  }

  bool HasTouchEventListeners()
  {
    return mMayHaveTouchEventListener;
  }

  /**
   * Moves the top-level window into fullscreen mode if aIsFullScreen is true,
   * otherwise exits fullscreen. If aRequireTrust is true, this method only
   * changes window state in a context trusted for write.
   */
  virtual nsresult SetFullScreenInternal(bool aIsFullScreen, bool aRequireTrust) = 0;

  /**
   * Call this to indicate that some node (this window, its document,
   * or content in that document) has a "MozAudioAvailable" event listener.
   */
  virtual void SetHasAudioAvailableEventListeners() = 0;

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

  virtual JSObject* GetCachedXBLPrototypeHandler(nsXBLPrototypeHandler* aKey) = 0;
  virtual void CacheXBLPrototypeHandler(nsXBLPrototypeHandler* aKey,
                                        nsScriptObjectHolder<JSObject>& aHandler) = 0;

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
      return mInnerWindow ? mInnerWindow->mFocusedNode.get() : nsnull;
    }
    return mFocusedNode;
  }
  virtual void SetFocusedNode(nsIContent* aNode,
                              PRUint32 aFocusMethod = 0,
                              bool aNeedsFocus = false) = 0;

  /**
   * Retrieves the method that was used to focus the current node.
   */
  virtual PRUint32 GetFocusMethod() = 0;

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
  virtual bool TakeFocus(bool aFocus, PRUint32 aFocusMethod) = 0;

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
   * Tell this window that it should listen for sensor changes of the given type.
   */
  virtual void EnableDeviceSensor(PRUint32 aType) = 0;

  /**
   * Tell this window that it should remove itself from sensor change notifications.
   */
  virtual void DisableDeviceSensor(PRUint32 aType) = 0;

  /**
   * Set a arguments for this window. This will be set on the window
   * right away (if there's an existing document) and it will also be
   * installed on the window when the next document is loaded. Each
   * language impl is responsible for converting to an array of args
   * as appropriate for that language.
   */
  virtual nsresult SetArguments(nsIArray *aArguments, nsIPrincipal *aOrigin) = 0;

  /**
   * NOTE! This function *will* be called on multiple threads so the
   * implementation must not do any AddRef/Release or other actions that will
   * mutate internal state.
   */
  virtual PRUint32 GetSerial() = 0;

  /**
   * Return the window id of this window
   */
  PRUint64 WindowID() const { return mWindowID; }

  /**
   * Dispatch a custom event with name aEventName targeted at this window.
   * Returns whether the default action should be performed.
   */
  virtual bool DispatchCustomEvent(const char *aEventName) = 0;

protected:
  // The nsPIDOMWindow constructor. The aOuterWindow argument should
  // be null if and only if the created window itself is an outer
  // window. In all other cases aOuterWindow should be the outer
  // window for the inner window that is being created.
  nsPIDOMWindow(nsPIDOMWindow *aOuterWindow);

  ~nsPIDOMWindow();

  void SetChromeEventHandlerInternal(nsIDOMEventTarget* aChromeEventHandler) {
    mChromeEventHandler = aChromeEventHandler;
    // mParentTarget will be set when the next event is dispatched.
    mParentTarget = nsnull;
  }

  virtual void UpdateParentTarget() = 0;

  // These two variables are special in that they're set to the same
  // value on both the outer window and the current inner window. Make
  // sure you keep them in sync!
  nsCOMPtr<nsIDOMEventTarget> mChromeEventHandler; // strong
  nsCOMPtr<nsIDOMDocument> mDocument; // strong
  nsCOMPtr<nsIDocument> mDoc; // strong, for fast access

  nsCOMPtr<nsIDOMEventTarget> mParentTarget; // strong

  // These members are only used on outer windows.
  nsCOMPtr<nsIDOMElement> mFrameElement;
  nsIDocShell           *mDocShell;  // Weak Reference

  PRUint32               mModalStateDepth;

  // These variables are only used on inner windows.
  nsTimeout             *mRunningTimeout;

  PRUint32               mMutationBits;

  bool                   mIsDocumentLoaded;
  bool                   mIsHandlingResizeEvent;
  bool                   mIsInnerWindow;
  bool                   mMayHavePaintEventListener;
  bool                   mMayHaveTouchEventListener;
  bool                   mMayHaveMouseEnterLeaveEventListener;

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

  // And these are the references between inner and outer windows.
  nsPIDOMWindow         *mInnerWindow;
  nsCOMPtr<nsPIDOMWindow> mOuterWindow;

  // the element within the document that is currently focused when this
  // window is active
  nsCOMPtr<nsIContent> mFocusedNode;

  // A unique (as long as our 64-bit counter doesn't roll over) id for
  // this window.
  PRUint64 mWindowID;

  // This is only used by the inner window. Set to true once we've sent
  // the (chrome|content)-document-global-created notification.
  bool mHasNotifiedGlobalCreated;
};


NS_DEFINE_STATIC_IID_ACCESSOR(nsPIDOMWindow, NS_PIDOMWINDOW_IID)

#ifdef _IMPL_NS_LAYOUT
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
#ifdef _IMPL_NS_LAYOUT
  NS_AUTO_POPUP_STATE_PUSHER(PopupControlState aState, bool aForce = false)
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
#ifndef _IMPL_NS_LAYOUT
  nsCOMPtr<nsPIDOMWindow> mWindow;
#endif
  PopupControlState mOldState;

private:
  // Hide so that this class can only be stack-allocated
  static void* operator new(size_t /*size*/) CPP_THROW_NEW { return nsnull; }
  static void operator delete(void* /*memory*/) {}
};

#define nsAutoPopupStatePusher NS_AUTO_POPUP_STATE_PUSHER

#endif // nsPIDOMWindow_h__
