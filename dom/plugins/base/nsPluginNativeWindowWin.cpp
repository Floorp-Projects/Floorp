/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BasicEvents.h"
#include "mozilla/DebugOnly.h"

#include "windows.h"
#include "windowsx.h"

// XXXbz windowsx.h defines GetFirstChild, GetNextSibling,
// GetPrevSibling are macros, apparently... Eeevil.  We have functions
// called that on some classes, so undef them.
#undef GetFirstChild
#undef GetNextSibling
#undef GetPrevSibling

#include "nsDebug.h"

#include "nsWindowsDllInterceptor.h"
#include "nsPluginNativeWindow.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"
#include "nsTWeakRef.h"
#include "nsCrashOnException.h"

using namespace mozilla;

#define NP_POPUP_API_VERSION 16

#define nsMajorVersion(v)       (((int32_t)(v) >> 16) & 0xffff)
#define nsMinorVersion(v)       ((int32_t)(v) & 0xffff)
#define versionOK(suppliedV, requiredV)                   \
  (nsMajorVersion(suppliedV) == nsMajorVersion(requiredV) \
   && nsMinorVersion(suppliedV) >= nsMinorVersion(requiredV))


#define NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION TEXT("MozillaPluginWindowPropertyAssociation")
#define NS_PLUGIN_CUSTOM_MSG_ID TEXT("MozFlashUserRelay")
#define WM_USER_FLASH WM_USER+1
static UINT sWM_FLASHBOUNCEMSG = 0;

typedef nsTWeakRef<class nsPluginNativeWindowWin> PluginWindowWeakRef;

/**
 *  PLEvent handling code
 */
class PluginWindowEvent : public nsRunnable {
public:
  PluginWindowEvent();
  void Init(const PluginWindowWeakRef &ref, HWND hWnd, UINT msg, WPARAM wParam,
            LPARAM lParam);
  void Clear();
  HWND   GetWnd()    { return mWnd; };
  UINT   GetMsg()    { return mMsg; };
  WPARAM GetWParam() { return mWParam; };
  LPARAM GetLParam() { return mLParam; };
  bool InUse()       { return mWnd != nullptr; };

  NS_DECL_NSIRUNNABLE

protected:
  PluginWindowWeakRef mPluginWindowRef;
  HWND   mWnd;
  UINT   mMsg;
  WPARAM mWParam;
  LPARAM mLParam;
};

PluginWindowEvent::PluginWindowEvent()
{
  Clear();
}

void PluginWindowEvent::Clear()
{
  mWnd    = nullptr;
  mMsg    = 0;
  mWParam = 0;
  mLParam = 0;
}

void PluginWindowEvent::Init(const PluginWindowWeakRef &ref, HWND aWnd,
                             UINT aMsg, WPARAM aWParam, LPARAM aLParam)
{
  NS_ASSERTION(aWnd != nullptr, "invalid plugin event value");
  NS_ASSERTION(mWnd == nullptr, "event already in use");
  mPluginWindowRef = ref;
  mWnd    = aWnd;
  mMsg    = aMsg;
  mWParam = aWParam;
  mLParam = aLParam;
}

/**
 *  nsPluginNativeWindow Windows specific class declaration
 */

class nsPluginNativeWindowWin : public nsPluginNativeWindow {
public:
  nsPluginNativeWindowWin();
  virtual ~nsPluginNativeWindowWin();

  virtual nsresult CallSetWindow(nsRefPtr<nsNPAPIPluginInstance> &aPluginInstance);

private:
  nsresult SubclassAndAssociateWindow();
  nsresult UndoSubclassAndAssociateWindow();

public:
  // locals
  WNDPROC GetPrevWindowProc();
  void SetPrevWindowProc(WNDPROC proc) { mPluginWinProc = proc; }
  WNDPROC GetWindowProc();
  PluginWindowEvent * GetPluginWindowEvent(HWND aWnd,
                                           UINT aMsg,
                                           WPARAM aWParam,
                                           LPARAM aLParam);

private:
  WNDPROC mPluginWinProc;
  WNDPROC mPrevWinProc;
  PluginWindowWeakRef mWeakRef;
  nsRefPtr<PluginWindowEvent> mCachedPluginWindowEvent;

  HWND mParentWnd;
  LONG_PTR mParentProc;
public:
  nsPluginHost::SpecialType mPluginType;
};

static bool sInMessageDispatch = false;
static bool sInPreviousMessageDispatch = false;
static UINT sLastMsg = 0;

static bool ProcessFlashMessageDelayed(nsPluginNativeWindowWin * aWin, nsNPAPIPluginInstance * aInst,
                                         HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  NS_ENSURE_TRUE(aWin, false);
  NS_ENSURE_TRUE(aInst, false);

  if (msg == sWM_FLASHBOUNCEMSG) {
    // See PluginWindowEvent::Run() below.
    NS_ASSERTION((sWM_FLASHBOUNCEMSG != 0), "RegisterWindowMessage failed in flash plugin WM_USER message handling!");
    ::CallWindowProc((WNDPROC)aWin->GetWindowProc(), hWnd, WM_USER_FLASH, wParam, lParam);
    return true;
  }

  if (msg != WM_USER_FLASH)
    return false; // no need to delay

  // do stuff
  nsCOMPtr<nsIRunnable> pwe = aWin->GetPluginWindowEvent(hWnd, msg, wParam, lParam);
  if (pwe) {
    NS_DispatchToCurrentThread(pwe);
    return true;
  }
  return false;
}

class nsDelayedPopupsEnabledEvent : public nsRunnable
{
public:
  nsDelayedPopupsEnabledEvent(nsNPAPIPluginInstance *inst)
    : mInst(inst)
  {}

  NS_DECL_NSIRUNNABLE

private:
  nsRefPtr<nsNPAPIPluginInstance> mInst;
};

NS_IMETHODIMP nsDelayedPopupsEnabledEvent::Run()
{
  mInst->PushPopupsEnabledState(false);
  return NS_OK;
}

static LRESULT CALLBACK PluginWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * New plugin window procedure
 *
 * e10s note - this subclass, and the hooks we set below using WindowsDllInterceptor
 * are currently not in use when running with e10s. (Utility calls like CallSetWindow
 * are still in use in the content process.) We would like to keep things this away,
 * essentially making all the hacks here obsolete. Some of the mitigation work here has
 * already been supplanted by code in PluginInstanceChild. The rest we eventually want
 * to rip out.
 */
static LRESULT CALLBACK PluginWndProcInternal(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  nsPluginNativeWindowWin * win = (nsPluginNativeWindowWin *)::GetProp(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION);
  if (!win)
    return TRUE;

  // The DispatchEvent(NS_PLUGIN_ACTIVATE) below can trigger a reentrant focus
  // event which might destroy us.  Hold a strong ref on the plugin instance
  // to prevent that, bug 374229.
  nsRefPtr<nsNPAPIPluginInstance> inst;
  win->GetPluginInstance(inst);

  // Real may go into a state where it recursivly dispatches the same event
  // when subclassed. If this is Real, lets examine the event and drop it
  // on the floor if we get into this recursive situation. See bug 192914.
  if (win->mPluginType == nsPluginHost::eSpecialType_RealPlayer) {
    if (sInMessageDispatch && msg == sLastMsg)
      return true;
    // Cache the last message sent
    sLastMsg = msg;
  }

  bool enablePopups = false;

  // Activate/deactivate mouse capture on the plugin widget
  // here, before we pass the Windows event to the plugin
  // because its possible our widget won't get paired events
  // (see bug 131007) and we'll look frozen. Note that this
  // is also done in ChildWindow::DispatchMouseEvent.
  switch (msg) {
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN: {
      nsCOMPtr<nsIWidget> widget;
      win->GetPluginWidget(getter_AddRefs(widget));
      if (widget)
        widget->CaptureMouse(true);
      break;
    }
    case WM_LBUTTONUP:
      enablePopups = true;

      // fall through
    case WM_MBUTTONUP:
    case WM_RBUTTONUP: {
      nsCOMPtr<nsIWidget> widget;
      win->GetPluginWidget(getter_AddRefs(widget));
      if (widget)
        widget->CaptureMouse(false);
      break;
    }
    case WM_KEYDOWN:
      // Ignore repeating keydown messages...
      if ((lParam & 0x40000000) != 0) {
        break;
      }

      // fall through
    case WM_KEYUP:
      enablePopups = true;

      break;

    case WM_MOUSEACTIVATE: {
      // If a child window of this plug-in is already focused,
      // don't focus the parent to avoid focus dance. We'll
      // receive a follow up WM_SETFOCUS which will notify
      // the appropriate window anyway.
      HWND focusedWnd = ::GetFocus();
      if (!::IsChild((HWND)win->window, focusedWnd)) {
        // Notify the dom / focus manager the plugin has focus when one of
        // it's child windows receives it. OOPP specific - this code is
        // critical in notifying the dom of focus changes when the plugin
        // window in the child process receives focus via a mouse click.
        // WM_MOUSEACTIVATE is sent by nsWindow via a custom window event
        // sent from PluginInstanceParent in response to focus events sent
        // from the child. (bug 540052) Note, this gui event could also be
        // sent directly from widget.
        nsCOMPtr<nsIWidget> widget;
        win->GetPluginWidget(getter_AddRefs(widget));
        if (widget) {
          WidgetGUIEvent event(true, NS_PLUGIN_ACTIVATE, widget);
          nsEventStatus status;
          widget->DispatchEvent(&event, status);
        }
      }
    }
    break;

    case WM_SETFOCUS:
    case WM_KILLFOCUS: {
      // RealPlayer can crash, don't process the message for those,
      // see bug 328675.
      if (win->mPluginType == nsPluginHost::eSpecialType_RealPlayer && msg == sLastMsg)
        return TRUE;
      // Make sure setfocus and killfocus get through to the widget procedure
      // even if they are eaten by the plugin. Also make sure we aren't calling
      // recursively.
      WNDPROC prevWndProc = win->GetPrevWindowProc();
      if (prevWndProc && !sInPreviousMessageDispatch) {
        sInPreviousMessageDispatch = true;
        ::CallWindowProc(prevWndProc, hWnd, msg, wParam, lParam);
        sInPreviousMessageDispatch = false;
      }
      break;
    }
  }

  // Macromedia Flash plugin may flood the message queue with some special messages
  // (WM_USER+1) causing 100% CPU consumption and GUI freeze, see mozilla bug 132759;
  // we can prevent this from happening by delaying the processing such messages;
  if (win->mPluginType == nsPluginHost::eSpecialType_Flash) {
    if (ProcessFlashMessageDelayed(win, inst, hWnd, msg, wParam, lParam))
      return TRUE;
  }

  if (enablePopups && inst) {
    uint16_t apiVersion;
    if (NS_SUCCEEDED(inst->GetPluginAPIVersion(&apiVersion)) &&
        !versionOK(apiVersion, NP_POPUP_API_VERSION)) {
      inst->PushPopupsEnabledState(true);
    }
  }

  sInMessageDispatch = true;
  LRESULT res;
  WNDPROC proc = (WNDPROC)win->GetWindowProc();
  if (PluginWndProc == proc) {
    NS_WARNING("Previous plugin window procedure references PluginWndProc! "
               "Report this bug!");
    res = CallWindowProc(DefWindowProc, hWnd, msg, wParam, lParam);
  } else {
    res = CallWindowProc(proc, hWnd, msg, wParam, lParam);
  }
  sInMessageDispatch = false;

  if (inst) {
    // Popups are enabled (were enabled before the call to
    // CallWindowProc()). Some plugins (at least the flash player)
    // post messages from their key handlers etc that delay the actual
    // processing, so we need to delay the disabling of popups so that
    // popups remain enabled when the flash player ends up processing
    // the actual key handlers. We do this by posting an event that
    // does the disabling, this way our disabling will happen after
    // the handlers in the plugin are done.

    // Note that it's not fatal if any of this fails (which won't
    // happen unless we're out of memory anyways) since the plugin
    // code will pop any popup state pushed by this plugin on
    // destruction.

    nsCOMPtr<nsIRunnable> event = new nsDelayedPopupsEnabledEvent(inst);
    if (event)
      NS_DispatchToCurrentThread(event);
  }

  return res;
}

static LRESULT CALLBACK PluginWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  return mozilla::CallWindowProcCrashProtected(PluginWndProcInternal, hWnd, msg, wParam, lParam);
}

/*
 * Flash will reset the subclass of our widget at various times.
 * (Notably when entering and exiting full screen mode.) This
 * occurs independent of the main plugin window event procedure.
 * We trap these subclass calls to prevent our subclass hook from
 * getting dropped.
 * Note, ascii versions can be nixed once flash versions < 10.1
 * are considered obsolete.
 */
static WindowsDllInterceptor sUser32Intercept;

#ifdef _WIN64
typedef LONG_PTR
  (WINAPI *User32SetWindowLongPtrA)(HWND hWnd,
                                    int nIndex,
                                    LONG_PTR dwNewLong);
typedef LONG_PTR
  (WINAPI *User32SetWindowLongPtrW)(HWND hWnd,
                                    int nIndex,
                                    LONG_PTR dwNewLong);
static User32SetWindowLongPtrA sUser32SetWindowLongAHookStub = nullptr;
static User32SetWindowLongPtrW sUser32SetWindowLongWHookStub = nullptr;
#else
typedef LONG
(WINAPI *User32SetWindowLongA)(HWND hWnd,
                               int nIndex,
                               LONG dwNewLong);
typedef LONG
(WINAPI *User32SetWindowLongW)(HWND hWnd,
                               int nIndex,
                               LONG dwNewLong);
static User32SetWindowLongA sUser32SetWindowLongAHookStub = nullptr;
static User32SetWindowLongW sUser32SetWindowLongWHookStub = nullptr;
#endif
static inline bool
SetWindowLongHookCheck(HWND hWnd,
                       int nIndex,
                       LONG_PTR newLong)
{
  nsPluginNativeWindowWin * win =
    (nsPluginNativeWindowWin *)GetProp(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION);
  if (!win || (win && win->mPluginType != nsPluginHost::eSpecialType_Flash) ||
      (nIndex == GWLP_WNDPROC &&
       newLong == reinterpret_cast<LONG_PTR>(PluginWndProc)))
    return true;
  return false;
}

#ifdef _WIN64
LONG_PTR WINAPI
SetWindowLongPtrAHook(HWND hWnd,
                      int nIndex,
                      LONG_PTR newLong)
#else
LONG WINAPI
SetWindowLongAHook(HWND hWnd,
                   int nIndex,
                   LONG newLong)
#endif
{
  if (SetWindowLongHookCheck(hWnd, nIndex, newLong))
      return sUser32SetWindowLongAHookStub(hWnd, nIndex, newLong);

  // Set flash's new subclass to get the result.
  LONG_PTR proc = sUser32SetWindowLongAHookStub(hWnd, nIndex, newLong);

  // We already checked this in SetWindowLongHookCheck
  nsPluginNativeWindowWin * win =
    (nsPluginNativeWindowWin *)GetProp(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION);

  // Hook our subclass back up, just like we do on setwindow.
  win->SetPrevWindowProc(
    reinterpret_cast<WNDPROC>(sUser32SetWindowLongWHookStub(hWnd, nIndex,
      reinterpret_cast<LONG_PTR>(PluginWndProc))));
  return proc;
}

#ifdef _WIN64
LONG_PTR WINAPI
SetWindowLongPtrWHook(HWND hWnd,
                      int nIndex,
                      LONG_PTR newLong)
#else
LONG WINAPI
SetWindowLongWHook(HWND hWnd,
                   int nIndex,
                   LONG newLong)
#endif
{
  if (SetWindowLongHookCheck(hWnd, nIndex, newLong))
      return sUser32SetWindowLongWHookStub(hWnd, nIndex, newLong);

  // Set flash's new subclass to get the result.
  LONG_PTR proc = sUser32SetWindowLongWHookStub(hWnd, nIndex, newLong);

  // We already checked this in SetWindowLongHookCheck
  nsPluginNativeWindowWin * win =
    (nsPluginNativeWindowWin *)GetProp(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION);

  // Hook our subclass back up, just like we do on setwindow.
  win->SetPrevWindowProc(
    reinterpret_cast<WNDPROC>(sUser32SetWindowLongWHookStub(hWnd, nIndex,
      reinterpret_cast<LONG_PTR>(PluginWndProc))));
  return proc;
}

static void
HookSetWindowLongPtr()
{
  sUser32Intercept.Init("user32.dll");
#ifdef _WIN64
  if (!sUser32SetWindowLongAHookStub)
    sUser32Intercept.AddHook("SetWindowLongPtrA",
                             reinterpret_cast<intptr_t>(SetWindowLongPtrAHook),
                             (void**) &sUser32SetWindowLongAHookStub);
  if (!sUser32SetWindowLongWHookStub)
    sUser32Intercept.AddHook("SetWindowLongPtrW",
                             reinterpret_cast<intptr_t>(SetWindowLongPtrWHook),
                             (void**) &sUser32SetWindowLongWHookStub);
#else
  if (!sUser32SetWindowLongAHookStub)
    sUser32Intercept.AddHook("SetWindowLongA",
                             reinterpret_cast<intptr_t>(SetWindowLongAHook),
                             (void**) &sUser32SetWindowLongAHookStub);
  if (!sUser32SetWindowLongWHookStub)
    sUser32Intercept.AddHook("SetWindowLongW",
                             reinterpret_cast<intptr_t>(SetWindowLongWHook),
                             (void**) &sUser32SetWindowLongWHookStub);
#endif
}

/**
 *   nsPluginNativeWindowWin implementation
 */
nsPluginNativeWindowWin::nsPluginNativeWindowWin() : nsPluginNativeWindow()
{
  // initialize the struct fields
  window = nullptr;
  x = 0;
  y = 0;
  width = 0;
  height = 0;

  mPrevWinProc = nullptr;
  mPluginWinProc = nullptr;
  mPluginType = nsPluginHost::eSpecialType_None;

  mParentWnd = nullptr;
  mParentProc = 0;

  if (!sWM_FLASHBOUNCEMSG) {
    sWM_FLASHBOUNCEMSG = ::RegisterWindowMessage(NS_PLUGIN_CUSTOM_MSG_ID);
  }
}

nsPluginNativeWindowWin::~nsPluginNativeWindowWin()
{
  // clear weak reference to self to prevent any pending events from
  // dereferencing this.
  mWeakRef.forget();
}

WNDPROC nsPluginNativeWindowWin::GetPrevWindowProc()
{
  return mPrevWinProc;
}

WNDPROC nsPluginNativeWindowWin::GetWindowProc()
{
  return mPluginWinProc;
}

NS_IMETHODIMP PluginWindowEvent::Run()
{
  nsPluginNativeWindowWin *win = mPluginWindowRef.get();
  if (!win)
    return NS_OK;

  HWND hWnd = GetWnd();
  if (!hWnd)
    return NS_OK;

  nsRefPtr<nsNPAPIPluginInstance> inst;
  win->GetPluginInstance(inst);

  if (GetMsg() == WM_USER_FLASH) {
    // XXX Unwind issues related to runnable event callback depth for this
    // event and destruction of the plugin. (Bug 493601)
    ::PostMessage(hWnd, sWM_FLASHBOUNCEMSG, GetWParam(), GetLParam());
  }
  else {
    // Currently not used, but added so that processing events here
    // is more generic.
    ::CallWindowProc(win->GetWindowProc(),
                     hWnd,
                     GetMsg(),
                     GetWParam(),
                     GetLParam());
  }

  Clear();
  return NS_OK;
}

PluginWindowEvent *
nsPluginNativeWindowWin::GetPluginWindowEvent(HWND aWnd, UINT aMsg, WPARAM aWParam, LPARAM aLParam)
{
  if (!mWeakRef) {
    mWeakRef = this;
    if (!mWeakRef)
      return nullptr;
  }

  PluginWindowEvent *event;

  // We have the ability to alloc if needed in case in the future some plugin
  // should post multiple PostMessages. However, this could lead to many
  // alloc's per second which could become a performance issue. See bug 169247.
  if (!mCachedPluginWindowEvent)
  {
    event = new PluginWindowEvent();
    if (!event) return nullptr;
    mCachedPluginWindowEvent = event;
  }
  else if (mCachedPluginWindowEvent->InUse())
  {
    event = new PluginWindowEvent();
    if (!event) return nullptr;
  }
  else
  {
    event = mCachedPluginWindowEvent;
  }

  event->Init(mWeakRef, aWnd, aMsg, aWParam, aLParam);
  return event;
}

nsresult nsPluginNativeWindowWin::CallSetWindow(nsRefPtr<nsNPAPIPluginInstance> &aPluginInstance)
{
  // Note, 'window' can be null

  // check the incoming instance, null indicates that window is going away and we are
  // not interested in subclassing business any more, undo and don't subclass
  if (!aPluginInstance) {
    UndoSubclassAndAssociateWindow();
    // release plugin instance
    SetPluginInstance(nullptr);
    nsPluginNativeWindow::CallSetWindow(aPluginInstance);
    return NS_OK;
  }

  // check plugin mime type and cache it if it will need special treatment later
  if (mPluginType == nsPluginHost::eSpecialType_None) {
    const char* mimetype = nullptr;
    if (NS_SUCCEEDED(aPluginInstance->GetMIMEType(&mimetype)) && mimetype) {
      mPluginType = nsPluginHost::GetSpecialType(nsDependentCString(mimetype));
    }
  }

  // With e10s we execute in the content process and as such we don't
  // have access to native widgets. CallSetWindow and skip native widget
  // subclassing.
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    nsPluginNativeWindow::CallSetWindow(aPluginInstance);
    return NS_OK;
  }

  if (window) {
    // grab the widget procedure before the plug-in does a subclass in
    // setwindow. We'll use this in PluginWndProc for forwarding focus
    // events to the widget.
    WNDPROC currentWndProc =
      (WNDPROC)::GetWindowLongPtr((HWND)window, GWLP_WNDPROC);
    if (!mPrevWinProc && currentWndProc != PluginWndProc)
      mPrevWinProc = currentWndProc;

    // PDF plugin v7.0.9, v8.1.3, and v9.0 subclass parent window, bug 531551
    // V8.2.2 and V9.1 don't have such problem.
    if (mPluginType == nsPluginHost::eSpecialType_PDF) {
      HWND parent = ::GetParent((HWND)window);
      if (mParentWnd != parent) {
        NS_ASSERTION(!mParentWnd, "Plugin's parent window changed");
        mParentWnd = parent;
        mParentProc = ::GetWindowLongPtr(mParentWnd, GWLP_WNDPROC);
      }
    }
  }

  nsPluginNativeWindow::CallSetWindow(aPluginInstance);

  SubclassAndAssociateWindow();

  if (window && mPluginType == nsPluginHost::eSpecialType_Flash &&
      !GetPropW((HWND)window, L"PluginInstanceParentProperty")) {
    HookSetWindowLongPtr();
  }

  return NS_OK;
}

nsresult nsPluginNativeWindowWin::SubclassAndAssociateWindow()
{
  if (type != NPWindowTypeWindow || !window)
    return NS_ERROR_FAILURE;

  HWND hWnd = (HWND)window;

  // check if we need to subclass
  WNDPROC currentWndProc = (WNDPROC)::GetWindowLongPtr(hWnd, GWLP_WNDPROC);
  if (currentWndProc == PluginWndProc)
    return NS_OK;

  // If the plugin reset the subclass, set it back.
  if (mPluginWinProc) {
#ifdef DEBUG
    NS_WARNING("A plugin cleared our subclass - resetting.");
    if (currentWndProc != mPluginWinProc) {
      NS_WARNING("Procedures do not match up, discarding old subclass value.");
    }
    if (mPrevWinProc && currentWndProc == mPrevWinProc) {
      NS_WARNING("The new procedure is our widget procedure?");
    }
#endif
    SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)PluginWndProc);
    return NS_OK;
  }

  LONG_PTR style = GetWindowLongPtr(hWnd, GWL_STYLE);
  // Out of process plugins must not have the WS_CLIPCHILDREN style set on their
  // parent windows or else synchronous paints (via UpdateWindow() and others)
  // will cause deadlocks.
  if (::GetPropW(hWnd, L"PluginInstanceParentProperty"))
    style &= ~WS_CLIPCHILDREN;
  else
    style |= WS_CLIPCHILDREN;
  SetWindowLongPtr(hWnd, GWL_STYLE, style);

  mPluginWinProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)PluginWndProc);
  if (!mPluginWinProc)
    return NS_ERROR_FAILURE;

  DebugOnly<nsPluginNativeWindowWin *> win = (nsPluginNativeWindowWin *)::GetProp(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION);
  NS_ASSERTION(!win || (win == this), "plugin window already has property and this is not us");

  if (!::SetProp(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION, (HANDLE)this))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult nsPluginNativeWindowWin::UndoSubclassAndAssociateWindow()
{
  // remove window property
  HWND hWnd = (HWND)window;
  if (IsWindow(hWnd))
    ::RemoveProp(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION);

  // restore the original win proc
  // but only do this if this were us last time
  if (mPluginWinProc) {
    WNDPROC currentWndProc = (WNDPROC)::GetWindowLongPtr(hWnd, GWLP_WNDPROC);
    if (currentWndProc == PluginWndProc)
      SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)mPluginWinProc);
    mPluginWinProc = nullptr;

    LONG_PTR style = GetWindowLongPtr(hWnd, GWL_STYLE);
    style &= ~WS_CLIPCHILDREN;
    SetWindowLongPtr(hWnd, GWL_STYLE, style);
  }

  if (mPluginType == nsPluginHost::eSpecialType_Flash && mParentWnd) {
    ::SetWindowLongPtr(mParentWnd, GWLP_WNDPROC, mParentProc);
    mParentWnd = nullptr;
    mParentProc = 0;
  }

  return NS_OK;
}

nsresult PLUG_NewPluginNativeWindow(nsPluginNativeWindow ** aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);

  *aPluginNativeWindow = new nsPluginNativeWindowWin();

  return *aPluginNativeWindow ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult PLUG_DeletePluginNativeWindow(nsPluginNativeWindow * aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);
  nsPluginNativeWindowWin *p = (nsPluginNativeWindowWin *)aPluginNativeWindow;
  delete p;
  return NS_OK;
}
