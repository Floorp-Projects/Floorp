/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*****************************************************************************/

#define INCL_WIN
#include "os2.h"

#include "nsDebug.h"
#include "nsGUIEvent.h"
#include "nsPluginSafety.h"
#include "nsPluginNativeWindow.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"
#include "nsTWeakRef.h"

#define NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION "MozillaPluginWindowPropertyAssociation"
#define NS_PLUGIN_CUSTOM_MSG_ID "MozFlashUserRelay"
#define WM_USER_FLASH WM_USER+1
#ifndef WM_FOCUSCHANGED
#define WM_FOCUSCHANGED 0x000E
#endif

#define NP_POPUP_API_VERSION 16

#define nsMajorVersion(v)       (((PRInt32)(v) >> 16) & 0xffff)
#define nsMinorVersion(v)       ((PRInt32)(v) & 0xffff)
#define versionOK(suppliedV, requiredV)                   \
  (nsMajorVersion(suppliedV) == nsMajorVersion(requiredV) \
   && nsMinorVersion(suppliedV) >= nsMinorVersion(requiredV))

typedef nsTWeakRef<class nsPluginNativeWindowOS2> PluginWindowWeakRef;

extern "C" {
PVOID APIENTRY WinQueryProperty(HWND hwnd, PCSZ  pszNameOrAtom);

PVOID APIENTRY WinRemoveProperty(HWND hwnd, PCSZ  pszNameOrAtom);

BOOL  APIENTRY WinSetProperty(HWND hwnd, PCSZ  pszNameOrAtom,
                              PVOID pvData, ULONG ulFlags);
}

/*****************************************************************************/

static ULONG sWM_FLASHBOUNCEMSG = 0;

/*****************************************************************************/
/**
 *  PLEvent handling code
 */

class PluginWindowEvent : public nsRunnable
{
public:
  PluginWindowEvent();
  void Init(const PluginWindowWeakRef &ref, HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2);
  void Clear();
  HWND   GetWnd()    { return mWnd; };
  ULONG  GetMsg()    { return mMsg; };
  MPARAM GetWParam() { return mWParam; };
  MPARAM GetLParam() { return mLParam; };
  bool InUse()     { return (mWnd!=NULL); };
  
  NS_DECL_NSIRUNNABLE

protected:
  PluginWindowWeakRef mPluginWindowRef;
  HWND   mWnd;
  ULONG  mMsg;
  MPARAM mWParam;
  MPARAM mLParam;
};

PluginWindowEvent::PluginWindowEvent()
{
  Clear();
}

void PluginWindowEvent::Clear()
{
  mWnd    = NULL;
  mMsg    = 0;
  mWParam = 0;
  mLParam = 0;
}

void PluginWindowEvent::Init(const PluginWindowWeakRef &ref, HWND aWnd,
                             ULONG aMsg, MPARAM mp1, MPARAM mp2)
{
  NS_ASSERTION(aWnd != NULL, "invalid plugin event value");
  NS_ASSERTION(mWnd == NULL, "event already in use");
  mPluginWindowRef = ref;
  mWnd    = aWnd;
  mMsg    = aMsg;
  mWParam = mp1;
  mLParam = mp2;
}

/*****************************************************************************/

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

/*****************************************************************************/
/**
 *  nsPluginNativeWindow OS/2-specific class declaration
 */

typedef enum {
  nsPluginType_Unknown = 0,
  nsPluginType_Flash,
  nsPluginType_Java_vm,
  nsPluginType_Other
} nsPluginType;

class nsPluginNativeWindowOS2 : public nsPluginNativeWindow
{
public: 
  nsPluginNativeWindowOS2();
  virtual ~nsPluginNativeWindowOS2();

  virtual nsresult CallSetWindow(nsRefPtr<nsNPAPIPluginInstance> &aPluginInstance);

private:
  nsresult SubclassAndAssociateWindow();
  nsresult UndoSubclassAndAssociateWindow();

public:
  // locals
  PFNWP GetWindowProc();
  PluginWindowEvent* GetPluginWindowEvent(HWND aWnd,
                                          ULONG aMsg,
                                          MPARAM mp1, 
                                          MPARAM mp2);

private:
  PFNWP mPluginWinProc;
  PluginWindowWeakRef mWeakRef;
  nsRefPtr<PluginWindowEvent> mCachedPluginWindowEvent;

public:
  nsPluginType mPluginType;
};

/*****************************************************************************/

static bool ProcessFlashMessageDelayed(nsPluginNativeWindowOS2 * aWin,
                                         nsNPAPIPluginInstance * aInst,
                                         HWND hWnd, ULONG msg,
                                         MPARAM mp1, MPARAM mp2)
{
  NS_ENSURE_TRUE(aWin, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aInst, NS_ERROR_NULL_POINTER);

  if (msg == sWM_FLASHBOUNCEMSG) {
    // See PluginWindowEvent::Run() below.
    NS_TRY_SAFE_CALL_VOID((aWin->GetWindowProc())(hWnd, WM_USER_FLASH, mp1, mp2),
                           inst);
    return TRUE;
  }

  if (msg != WM_USER_FLASH)
    return false; // no need to delay

  // do stuff
  nsCOMPtr<nsIRunnable> pwe = aWin->GetPluginWindowEvent(hWnd, msg, mp1, mp2);
  if (pwe) {
    NS_DispatchToCurrentThread(pwe);
    return true;  
  }
  return false;
}

/*****************************************************************************/
/**
 *   New plugin window procedure
 */

static MRESULT EXPENTRY PluginWndProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  nsPluginNativeWindowOS2 * win = (nsPluginNativeWindowOS2 *)
            ::WinQueryProperty(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION);
  if (!win)
    return (MRESULT)TRUE;

  // The DispatchEvent(NS_PLUGIN_ACTIVATE) below can trigger a reentrant focus
  // event which might destroy us.  Hold a strong ref on the plugin instance
  // to prevent that, bug 374229.
  nsRefPtr<nsNPAPIPluginInstance> inst;
  win->GetPluginInstance(inst);

  // check plugin mime type and cache whether it is Flash or java-vm or not;
  // flash and java-vm will need special treatment later
  if (win->mPluginType == nsPluginType_Unknown) {
    if (inst) {
      const char* mimetype = nullptr;
      inst->GetMIMEType(&mimetype);
      if (mimetype) {
        if (!strcmp(mimetype, "application/x-shockwave-flash"))
          win->mPluginType = nsPluginType_Flash;
        else if (!strcmp(mimetype, "application/x-java-vm"))
          win->mPluginType = nsPluginType_Java_vm;
        else
          win->mPluginType = nsPluginType_Other;
      }
    }
  }

  bool enablePopups = false;

  // Activate/deactivate mouse capture on the plugin widget
  // here, before we pass the Windows event to the plugin
  // because its possible our widget won't get paired events
  // (see bug 131007) and we'll look frozen. Note that this
  // is also done in ChildWindow::DispatchMouseEvent.
  switch (msg) {
    case WM_BUTTON1DOWN:
    case WM_BUTTON2DOWN:
    case WM_BUTTON3DOWN: {
      nsCOMPtr<nsIWidget> widget;
      win->GetPluginWidget(getter_AddRefs(widget));
      if (widget)
        widget->CaptureMouse(true);
      break;
    }

    case WM_BUTTON1UP:
    case WM_BUTTON2UP:
    case WM_BUTTON3UP: {
      if (msg == WM_BUTTON1UP)
        enablePopups = true;

      nsCOMPtr<nsIWidget> widget;
      win->GetPluginWidget(getter_AddRefs(widget));
      if (widget)
        widget->CaptureMouse(false);
      break;
    }

    case WM_CHAR:
      // Ignore repeating keydown messages...
      if (SHORT1FROMMP(mp1) & KC_PREVDOWN)
        break;
      enablePopups = true;
      break;

    // When the child of a plugin gets the focus, nsWindow doesn't get
    // a WM_FOCUSCHANGED msg, so plugin and window activation events
    // don't happen.  This fixes the problem by synthesizing a msg
    // that makes it look like the plugin widget just got the focus.
    case WM_FOCUSCHANGE: {

      // Some plugins don't pass this msg on.  If the default window proc
      // doesn't receive it, window activation/deactivation won't happen.
      WinDefWindowProc(hWnd, msg, mp1, mp2);

      // If focus is being gained, and the plugin widget neither lost nor
      // gained the focus, then a child just got it from some other window.
      // If that other window was neither a child of the widget nor owned
      // by a child of the widget (e.g. a popup menu), post a WM_FOCUSCHANGED
      // msg that identifies the child as the window losing focus.  After
      // nsWindow:: ActivatePlugin() activates the plugin, it will restore
      // the focus to the child.
      if (SHORT1FROMMP(mp2) && (HWND)mp1 != hWnd) {
        HWND hFocus = WinQueryFocus(HWND_DESKTOP);
        if (hFocus != hWnd &&
            WinIsChild(hFocus, hWnd) &&
            !WinIsChild((HWND)mp1, hWnd) &&
            !WinIsChild(WinQueryWindow((HWND)mp1, QW_OWNER), hWnd)) {
          WinPostMsg(hWnd, WM_FOCUSCHANGED, (MPARAM)hFocus, mp2);
        }
      }
      break;
    }
  }

  // Macromedia Flash plugin may flood the message queue with some special messages
  // (WM_USER+1) causing 100% CPU consumption and GUI freeze, see mozilla bug 132759;
  // we can prevent this from happening by delaying the processing such messages;
  if (win->mPluginType == nsPluginType_Flash) {
    if (ProcessFlashMessageDelayed(win, inst, hWnd, msg, mp1, mp2))
      return (MRESULT)TRUE;
  }

  if (enablePopups && inst) {
    PRUint16 apiVersion;
    if (NS_SUCCEEDED(inst->GetPluginAPIVersion(&apiVersion)) &&
        !versionOK(apiVersion, NP_POPUP_API_VERSION))
      inst->PushPopupsEnabledState(true);
  }

  MRESULT res = (MRESULT)TRUE;
  if (win->mPluginType == nsPluginType_Java_vm)
    NS_TRY_SAFE_CALL_RETURN(res, ::WinDefWindowProc(hWnd, msg, mp1, mp2), inst);
  else
    NS_TRY_SAFE_CALL_RETURN(res, (win->GetWindowProc())(hWnd, msg, mp1, mp2), inst);

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

/*****************************************************************************/
/**
 *   nsPluginNativeWindowOS2 implementation
 */

nsPluginNativeWindowOS2::nsPluginNativeWindowOS2() : nsPluginNativeWindow()
{
  // initialize the struct fields
  window = nullptr; 
  x = 0; 
  y = 0; 
  width = 0; 
  height = 0; 

  mPluginWinProc = NULL;
  mPluginType = nsPluginType_Unknown;

  // once the atom has been added, it won't be deleted
  if (!sWM_FLASHBOUNCEMSG) {
    sWM_FLASHBOUNCEMSG = ::WinFindAtom(WinQuerySystemAtomTable(),
                                       NS_PLUGIN_CUSTOM_MSG_ID);
    if (!sWM_FLASHBOUNCEMSG)
      sWM_FLASHBOUNCEMSG = ::WinAddAtom(WinQuerySystemAtomTable(),
                                        NS_PLUGIN_CUSTOM_MSG_ID);
  }
}

nsPluginNativeWindowOS2::~nsPluginNativeWindowOS2()
{
  // clear weak reference to self to prevent any pending events from
  // dereferencing this.
  mWeakRef.forget();
}

PFNWP nsPluginNativeWindowOS2::GetWindowProc()
{
  return mPluginWinProc;
}

NS_IMETHODIMP PluginWindowEvent::Run()
{
  nsPluginNativeWindowOS2 *win = mPluginWindowRef.get();
  if (!win)
    return NS_OK;

  HWND hWnd = GetWnd();
  if (!hWnd)
    return NS_OK;

  nsRefPtr<nsNPAPIPluginInstance> inst;
  win->GetPluginInstance(inst);

  if (GetMsg() == WM_USER_FLASH)
    // XXX Unwind issues related to runnable event callback depth for this
    // event and destruction of the plugin. (Bug 493601)
    ::WinPostMsg(hWnd, sWM_FLASHBOUNCEMSG, GetWParam(), GetLParam());
  else
    // Currently not used, but added so that processing events here
    // is more generic.
    NS_TRY_SAFE_CALL_VOID((win->GetWindowProc()) 
                          (hWnd, GetMsg(), GetWParam(), GetLParam()),
                          inst);

  Clear();
  return NS_OK;
}

PluginWindowEvent*
nsPluginNativeWindowOS2::GetPluginWindowEvent(HWND aWnd, ULONG aMsg, MPARAM aMp1, MPARAM aMp2)
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
  if (!mCachedPluginWindowEvent) {
    event = new PluginWindowEvent();
    if (!event)
      return nullptr;
    mCachedPluginWindowEvent = event;
  }
  else
  if (mCachedPluginWindowEvent->InUse()) {
    event = new PluginWindowEvent();
    if (!event)
      return nullptr;
  }
  else
    event = mCachedPluginWindowEvent;

  event->Init(mWeakRef, aWnd, aMsg, aMp1, aMp2);
  return event;
}

nsresult nsPluginNativeWindowOS2::CallSetWindow(nsRefPtr<nsNPAPIPluginInstance> &aPluginInstance)
{
  // check the incoming instance, null indicates that window is going away and we are
  // not interested in subclassing business any more, undo and don't subclass
  if (!aPluginInstance) {
    UndoSubclassAndAssociateWindow();
  }

  nsPluginNativeWindow::CallSetWindow(aPluginInstance);

  if (aPluginInstance)
    SubclassAndAssociateWindow();

  return NS_OK;
}

nsresult nsPluginNativeWindowOS2::SubclassAndAssociateWindow()
{
  if (type != NPWindowTypeWindow)
    return NS_ERROR_FAILURE;

  HWND hWnd = (HWND)window;
  if (!hWnd)
    return NS_ERROR_FAILURE;

  // check if we need to re-subclass
  PFNWP currentWndProc = (PFNWP)::WinQueryWindowPtr(hWnd, QWP_PFNWP);
  if (PluginWndProc == currentWndProc)
    return NS_OK;

  mPluginWinProc = ::WinSubclassWindow(hWnd, PluginWndProc);
  if (!mPluginWinProc)
    return NS_ERROR_FAILURE;

#ifdef DEBUG
  nsPluginNativeWindowOS2 * win = (nsPluginNativeWindowOS2 *)
            ::WinQueryProperty(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION);
  NS_ASSERTION(!win || (win == this), "plugin window already has property and this is not us");
#endif

  if (!::WinSetProperty(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION, (PVOID)this, 0))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult nsPluginNativeWindowOS2::UndoSubclassAndAssociateWindow()
{
  // release plugin instance
  SetPluginInstance(nullptr);

  // remove window property
  HWND hWnd = (HWND)window;
  if (::WinIsWindow(/*HAB*/0, hWnd))
    ::WinRemoveProperty(hWnd, NS_PLUGIN_WINDOW_PROPERTY_ASSOCIATION);

  // restore the original win proc
  // but only do this if this were us last time
  if (mPluginWinProc) {
    PFNWP currentWndProc = (PFNWP)::WinQueryWindowPtr(hWnd, QWP_PFNWP);
    if (currentWndProc == PluginWndProc)
      ::WinSubclassWindow(hWnd, mPluginWinProc);
  }

  return NS_OK;
}

nsresult PLUG_NewPluginNativeWindow(nsPluginNativeWindow ** aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);

  *aPluginNativeWindow = new nsPluginNativeWindowOS2();

  return *aPluginNativeWindow ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult PLUG_DeletePluginNativeWindow(nsPluginNativeWindow * aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);
  nsPluginNativeWindowOS2 *p = (nsPluginNativeWindowOS2 *)aPluginNativeWindow;
  delete p;
  return NS_OK;
}
