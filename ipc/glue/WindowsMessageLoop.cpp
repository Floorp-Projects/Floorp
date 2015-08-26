/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "WindowsMessageLoop.h"
#include "Neutering.h"
#include "MessageChannel.h"

#include "nsAutoPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsIXULAppInfo.h"
#include "WinUtils.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/PaintTracker.h"

using namespace mozilla;
using namespace mozilla::ipc;
using namespace mozilla::ipc::windows;

/**
 * The Windows-only code below exists to solve a general problem with deadlocks
 * that we experience when sending synchronous IPC messages to processes that
 * contain native windows (i.e. HWNDs). Windows (the OS) sends synchronous
 * messages between parent and child HWNDs in multiple circumstances (e.g.
 * WM_PARENTNOTIFY, WM_NCACTIVATE, etc.), even when those HWNDs are controlled
 * by different threads or different processes. Thus we can very easily end up
 * in a deadlock by a call stack like the following:
 *
 * Process A:
 *   - CreateWindow(...) creates a "parent" HWND.
 *   - SendCreateChildWidget(HWND) is a sync IPC message that sends the "parent"
 *         HWND over to Process B. Process A blocks until a response is received
 *         from Process B.
 *
 * Process B:
 *   - RecvCreateWidget(HWND) gets the "parent" HWND from Process A.
 *   - CreateWindow(..., HWND) creates a "child" HWND with the parent from
 *         process A.
 *   - Windows (the OS) generates a WM_PARENTNOTIFY message that is sent
 *         synchronously to Process A. Process B blocks until a response is
 *         received from Process A. Process A, however, is blocked and cannot
 *         process the message. Both processes are deadlocked.
 *
 * The example above has a few different workarounds (e.g. setting the
 * WS_EX_NOPARENTNOTIFY style on the child window) but the general problem is
 * persists. Once two HWNDs are parented we must not block their owning
 * threads when manipulating either HWND.
 *
 * Windows requires any application that hosts native HWNDs to always process
 * messages or risk deadlock. Given our architecture the only way to meet
 * Windows' requirement and allow for synchronous IPC messages is to pump a
 * miniature message loop during a sync IPC call. We avoid processing any
 * queued messages during the loop (with one exception, see below), but 
 * "nonqueued" messages (see 
 * http://msdn.microsoft.com/en-us/library/ms644927(VS.85).aspx under the
 * section "Nonqueued messages") cannot be avoided. Those messages are trapped
 * in a special window procedure where we can either ignore the message or
 * process it in some fashion.
 *
 * Queued and "non-queued" messages will be processed during Interrupt calls if
 * modal UI related api calls block an Interrupt in-call in the child. To prevent
 * windows from freezing, and to allow concurrent processing of critical
 * events (such as painting), we spin a native event dispatch loop while
 * these in-calls are blocked.
 */

#if defined(ACCESSIBILITY)
// pulled from accessibility's win utils
extern const wchar_t* kPropNameTabContent;
#endif

// widget related message id constants we need to defer
namespace mozilla {
namespace widget {
extern UINT sAppShellGeckoMsgId;
}
}

namespace {

const wchar_t kOldWndProcProp[] = L"MozillaIPCOldWndProc";
const wchar_t k3rdPartyWindowProp[] = L"Mozilla3rdPartyWindow";

// This isn't defined before Windows XP.
enum { WM_XP_THEMECHANGED = 0x031A };

char16_t gAppMessageWindowName[256] = { 0 };
int32_t gAppMessageWindowNameLength = 0;

nsTArray<HWND>* gNeuteredWindows = nullptr;

typedef nsTArray<nsAutoPtr<DeferredMessage> > DeferredMessageArray;
DeferredMessageArray* gDeferredMessages = nullptr;

HHOOK gDeferredGetMsgHook = nullptr;
HHOOK gDeferredCallWndProcHook = nullptr;

DWORD gUIThreadId = 0;
HWND gCOMWindow = 0;
// Once initialized, gWinEventHook is never unhooked. We save the handle so
// that we can check whether or not the hook is initialized.
HWINEVENTHOOK gWinEventHook = nullptr;
const wchar_t kCOMWindowClassName[] = L"OleMainThreadWndClass";

// WM_GETOBJECT id pulled from uia headers
#define MOZOBJID_UIAROOT -25

HWND
FindCOMWindow()
{
  MOZ_ASSERT(gUIThreadId);

  HWND last = 0;
  while ((last = FindWindowExW(HWND_MESSAGE, last, kCOMWindowClassName, NULL))) {
    if (GetWindowThreadProcessId(last, NULL) == gUIThreadId) {
      return last;
    }
  }

  return (HWND)0;
}

void CALLBACK
WinEventHook(HWINEVENTHOOK aWinEventHook, DWORD aEvent, HWND aHwnd,
             LONG aIdObject, LONG aIdChild, DWORD aEventThread,
             DWORD aMsEventTime)
{
  MOZ_ASSERT(aWinEventHook == gWinEventHook);
  MOZ_ASSERT(gUIThreadId == aEventThread);
  switch (aEvent) {
    case EVENT_OBJECT_CREATE: {
      if (aIdObject != OBJID_WINDOW || aIdChild != CHILDID_SELF) {
        // Not an event we're interested in
        return;
      }
      wchar_t classBuf[256] = {0};
      int result = ::GetClassNameW(aHwnd, classBuf,
                                   MOZ_ARRAY_LENGTH(classBuf));
      if (result != (MOZ_ARRAY_LENGTH(kCOMWindowClassName) - 1) ||
          wcsncmp(kCOMWindowClassName, classBuf, result)) {
        // Not a class we're interested in
        return;
      }
      MOZ_ASSERT(FindCOMWindow() == aHwnd);
      gCOMWindow = aHwnd;
      break;
    }
    case EVENT_OBJECT_DESTROY: {
      if (aHwnd == gCOMWindow && aIdObject == OBJID_WINDOW) {
        MOZ_ASSERT(aIdChild == CHILDID_SELF);
        gCOMWindow = 0;
      }
      break;
    }
    default: {
      return;
    }
  }
}

LRESULT CALLBACK
DeferredMessageHook(int nCode,
                    WPARAM wParam,
                    LPARAM lParam)
{
  // XXX This function is called for *both* the WH_CALLWNDPROC hook and the
  //     WH_GETMESSAGE hook, but they have different parameters. We don't
  //     use any of them except nCode which has the same meaning.

  // Only run deferred messages if all of these conditions are met:
  //   1. The |nCode| indicates that this hook should do something.
  //   2. We have deferred messages to run.
  //   3. We're not being called from the PeekMessage within the WaitFor*Notify
  //      function (indicated with MessageChannel::IsPumpingMessages). We really
  //      only want to run after returning to the main event loop.
  if (nCode >= 0 && gDeferredMessages && !MessageChannel::IsPumpingMessages()) {
    NS_ASSERTION(gDeferredGetMsgHook && gDeferredCallWndProcHook,
                 "These hooks must be set if we're being called!");
    NS_ASSERTION(gDeferredMessages->Length(), "No deferred messages?!");

    // Unset hooks first, in case we reenter below.
    UnhookWindowsHookEx(gDeferredGetMsgHook);
    UnhookWindowsHookEx(gDeferredCallWndProcHook);
    gDeferredGetMsgHook = 0;
    gDeferredCallWndProcHook = 0;

    // Unset the global and make sure we delete it when we're done here.
    nsAutoPtr<DeferredMessageArray> messages(gDeferredMessages);
    gDeferredMessages = nullptr;

    // Run all the deferred messages in order.
    uint32_t count = messages->Length();
    for (uint32_t index = 0; index < count; index++) {
      messages->ElementAt(index)->Run();
    }
  }

  // Always call the next hook.
  return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void
ScheduleDeferredMessageRun()
{
  if (gDeferredMessages &&
      !(gDeferredGetMsgHook && gDeferredCallWndProcHook)) {
    NS_ASSERTION(gDeferredMessages->Length(), "No deferred messages?!");

    gDeferredGetMsgHook = ::SetWindowsHookEx(WH_GETMESSAGE, DeferredMessageHook,
                                             nullptr, gUIThreadId);
    gDeferredCallWndProcHook = ::SetWindowsHookEx(WH_CALLWNDPROC,
                                                  DeferredMessageHook, nullptr,
                                                  gUIThreadId);
    NS_ASSERTION(gDeferredGetMsgHook && gDeferredCallWndProcHook,
                 "Failed to set hooks!");
  }
}

static void
DumpNeuteredMessage(HWND hwnd, UINT uMsg)
{
#ifdef DEBUG
  nsAutoCString log("Received \"nonqueued\" ");
  // classify messages
  if (uMsg < WM_USER) {
    int idx = 0;
    while (mozilla::widget::gAllEvents[idx].mId != (long)uMsg &&
           mozilla::widget::gAllEvents[idx].mStr != nullptr) {
      idx++;
    }
    if (mozilla::widget::gAllEvents[idx].mStr) {
      log.AppendPrintf("ui message \"%s\"", mozilla::widget::gAllEvents[idx].mStr);
    } else {
      log.AppendPrintf("ui message (0x%X)", uMsg);
    }
  } else if (uMsg >= WM_USER && uMsg < WM_APP) {
    log.AppendPrintf("WM_USER message (0x%X)", uMsg);
  } else if (uMsg >= WM_APP && uMsg < 0xC000) {
    log.AppendPrintf("WM_APP message (0x%X)", uMsg);
  } else if (uMsg >= 0xC000 && uMsg < 0x10000) {
    log.AppendPrintf("registered windows message (0x%X)", uMsg);
  } else {
    log.AppendPrintf("system message (0x%X)", uMsg);
  }

  log.AppendLiteral(" during a synchronous IPC message for window ");
  log.AppendPrintf("0x%X", hwnd);

  wchar_t className[256] = { 0 };
  if (GetClassNameW(hwnd, className, sizeof(className) - 1) > 0) {
    log.AppendLiteral(" (\"");
    log.Append(NS_ConvertUTF16toUTF8((char16_t*)className));
    log.AppendLiteral("\")");
  }

  log.AppendLiteral(", sending it to DefWindowProc instead of the normal "
                    "window procedure.");
  NS_ERROR(log.get());
#endif
}

LRESULT
ProcessOrDeferMessage(HWND hwnd,
                      UINT uMsg,
                      WPARAM wParam,
                      LPARAM lParam)
{
  DeferredMessage* deferred = nullptr;

  // Most messages ask for 0 to be returned if the message is processed.
  LRESULT res = 0;

  switch (uMsg) {
    // Messages that can be deferred as-is. These must not contain pointers in
    // their wParam or lParam arguments!
    case WM_ACTIVATE:
    case WM_ACTIVATEAPP:
    case WM_CANCELMODE:
    case WM_CAPTURECHANGED:
    case WM_CHILDACTIVATE:
    case WM_DESTROY:
    case WM_ENABLE:
    case WM_IME_NOTIFY:
    case WM_IME_SETCONTEXT:
    case WM_KILLFOCUS:
    case WM_MOUSEWHEEL:
    case WM_NCDESTROY:
    case WM_PARENTNOTIFY:
    case WM_SETFOCUS:
    case WM_SYSCOMMAND:
    case WM_DISPLAYCHANGE:
    case WM_SHOWWINDOW: // Intentional fall-through.
    case WM_XP_THEMECHANGED: {
      deferred = new DeferredSendMessage(hwnd, uMsg, wParam, lParam);
      break;
    }

    case WM_DEVICECHANGE:
    case WM_POWERBROADCAST:
    case WM_NCACTIVATE: // Intentional fall-through.
    case WM_SETCURSOR: {
      // Friggin unconventional return value...
      res = TRUE;
      deferred = new DeferredSendMessage(hwnd, uMsg, wParam, lParam);
      break;
    }

    case WM_MOUSEACTIVATE: {
      res = MA_NOACTIVATE;
      deferred = new DeferredSendMessage(hwnd, uMsg, wParam, lParam);
      break;
    }

    // These messages need to use the RedrawWindow function to generate the
    // right kind of message. We can't simply fake them as the MSDN docs say
    // explicitly that paint messages should not be sent by an application.
    case WM_ERASEBKGND: {
      UINT flags = RDW_INVALIDATE | RDW_ERASE | RDW_NOINTERNALPAINT |
                   RDW_NOFRAME | RDW_NOCHILDREN | RDW_ERASENOW;
      deferred = new DeferredRedrawMessage(hwnd, flags);
      break;
    }

    // This message will generate a WM_PAINT message if there are invalid
    // areas.
    case WM_PAINT: {
      deferred = new DeferredUpdateMessage(hwnd);
      break;
    }

    // This message holds a string in its lParam that we must copy.
    case WM_SETTINGCHANGE: {
      deferred = new DeferredSettingChangeMessage(hwnd, uMsg, wParam, lParam);
      break;
    }

    // These messages are faked via a call to SetWindowPos.
    case WM_WINDOWPOSCHANGED: {
      deferred = new DeferredWindowPosMessage(hwnd, lParam);
      break;
    }
    case WM_NCCALCSIZE: {
      deferred = new DeferredWindowPosMessage(hwnd, lParam, true, wParam);
      break;
    }

    case WM_COPYDATA: {
      deferred = new DeferredCopyDataMessage(hwnd, uMsg, wParam, lParam);
      res = TRUE;
      break;
    }

    case WM_STYLECHANGED: {
      deferred = new DeferredStyleChangeMessage(hwnd, wParam, lParam);
      break;
    }

    case WM_SETICON: {
      deferred = new DeferredSetIconMessage(hwnd, uMsg, wParam, lParam);
      break;
    }

    // Messages that are safe to pass to DefWindowProc go here.
    case WM_ENTERIDLE:
    case WM_GETICON:
    case WM_NCPAINT: // (never trap nc paint events)
    case WM_GETMINMAXINFO:
    case WM_GETTEXT:
    case WM_NCHITTEST:
    case WM_STYLECHANGING:  // Intentional fall-through.
    case WM_WINDOWPOSCHANGING:
    case WM_GETTEXTLENGTH: {
      return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    // Just return, prevents DefWindowProc from messaging the window
    // syncronously with other events, which may be deferred. Prevents 
    // random shutdown of aero composition on the window. 
    case WM_SYNCPAINT:
      return 0;

    // This message causes QuickTime to make re-entrant calls.
    // Simply discarding it doesn't seem to hurt anything.
    case WM_APP-1:
      return 0;

    // We only support a query for our IAccessible or UIA pointers.
    // This should be safe, and needs to be sync.
#if defined(ACCESSIBILITY)
   case WM_GETOBJECT: {
      if (!::GetPropW(hwnd, k3rdPartyWindowProp)) {
        DWORD objId = static_cast<DWORD>(lParam);
        if ((objId == OBJID_CLIENT || objId == MOZOBJID_UIAROOT)) {
          WNDPROC oldWndProc = (WNDPROC)GetProp(hwnd, kOldWndProcProp);
          if (oldWndProc) {
            return CallWindowProcW(oldWndProc, hwnd, uMsg, wParam, lParam);
          }
        }
      }
      return DefWindowProc(hwnd, uMsg, wParam, lParam);
   }
#endif // ACCESSIBILITY

    default: {
      // Unknown messages only are logged in debug builds and sent to
      // DefWindowProc.
      if (uMsg && uMsg == mozilla::widget::sAppShellGeckoMsgId) {
        // Widget's registered native event callback
        deferred = new DeferredSendMessage(hwnd, uMsg, wParam, lParam);
      }
    }
  }

  // No deferred message was created and we land here, this is an
  // unhandled message.
  if (!deferred) {
    DumpNeuteredMessage(hwnd, uMsg);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }

  // Create the deferred message array if it doesn't exist already.
  if (!gDeferredMessages) {
    gDeferredMessages = new nsTArray<nsAutoPtr<DeferredMessage> >(20);
    NS_ASSERTION(gDeferredMessages, "Out of memory!");
  }

  // Save for later. The array takes ownership of |deferred|.
  gDeferredMessages->AppendElement(deferred);
  return res;
}

} // namespace

// We need the pointer value of this in PluginInstanceChild.
LRESULT CALLBACK
NeuteredWindowProc(HWND hwnd,
                   UINT uMsg,
                   WPARAM wParam,
                   LPARAM lParam)
{
  WNDPROC oldWndProc = (WNDPROC)GetProp(hwnd, kOldWndProcProp);
  if (!oldWndProc) {
    // We should really never ever get here.
    NS_ERROR("No old wndproc!");
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }

  // See if we care about this message. We may either ignore it, send it to
  // DefWindowProc, or defer it for later.
  return ProcessOrDeferMessage(hwnd, uMsg, wParam, lParam);
}

namespace {

static bool
WindowIsDeferredWindow(HWND hWnd)
{
  if (!IsWindow(hWnd)) {
    NS_WARNING("Window has died!");
    return false;
  }

  char16_t buffer[256] = { 0 };
  int length = GetClassNameW(hWnd, (wchar_t*)buffer, sizeof(buffer) - 1);
  if (length <= 0) {
    NS_WARNING("Failed to get class name!");
    return false;
  }

#if defined(ACCESSIBILITY)
  // Tab content creates a window that responds to accessible WM_GETOBJECT
  // calls. This window can safely be ignored.
  if (::GetPropW(hWnd, kPropNameTabContent)) {
    return false;
  }
#endif

  // Common mozilla windows we must defer messages to.
  nsDependentString className(buffer, length);
  if (StringBeginsWith(className, NS_LITERAL_STRING("Mozilla")) ||
      StringBeginsWith(className, NS_LITERAL_STRING("Gecko")) ||
      className.EqualsLiteral("nsToolkitClass") ||
      className.EqualsLiteral("nsAppShell:EventWindowClass")) {
    return true;
  }

  // Plugin windows that can trigger ipc calls in child:
  // 'ShockwaveFlashFullScreen' - flash fullscreen window
  // 'QTNSHIDDEN' - QuickTime
  // 'AGFullScreenWinClass' - silverlight fullscreen window
  if (className.EqualsLiteral("ShockwaveFlashFullScreen") ||
      className.EqualsLiteral("QTNSHIDDEN") ||
      className.EqualsLiteral("AGFullScreenWinClass")) {
    SetPropW(hWnd, k3rdPartyWindowProp, (HANDLE)1);
    return true;
  }

  // Google Earth bridging msg window between the plugin instance and a separate
  // earth process. The earth process can trigger a plugin incall on the browser
  // at any time, which is badness if the instance is already making an incall.
  if (className.EqualsLiteral("__geplugin_bridge_window__")) {
    SetPropW(hWnd, k3rdPartyWindowProp, (HANDLE)1);
    return true;
  }

  // nsNativeAppSupport makes a window like "FirefoxMessageWindow" based on the
  // toolkit app's name. It's pretty expensive to calculate this so we only try
  // once.
  if (gAppMessageWindowNameLength == 0) {
    nsCOMPtr<nsIXULAppInfo> appInfo =
      do_GetService("@mozilla.org/xre/app-info;1");
    if (appInfo) {
      nsAutoCString appName;
      if (NS_SUCCEEDED(appInfo->GetName(appName))) {
        appName.AppendLiteral("MessageWindow");
        nsDependentString windowName(gAppMessageWindowName);
        CopyUTF8toUTF16(appName, windowName);
        gAppMessageWindowNameLength = windowName.Length();
      }
    }

    // Don't try again if that failed.
    if (gAppMessageWindowNameLength == 0) {
      gAppMessageWindowNameLength = -1;
    }
  }

  if (gAppMessageWindowNameLength != -1 &&
      className.Equals(nsDependentString(gAppMessageWindowName,
                                         gAppMessageWindowNameLength))) {
    return true;
  }

  return false;
}

bool
NeuterWindowProcedure(HWND hWnd)
{
  if (!WindowIsDeferredWindow(hWnd)) {
    // Some other kind of window, skip.
    return false;
  }

  NS_ASSERTION(!GetProp(hWnd, kOldWndProcProp), "This should always be null!");

  // It's possible to get nullptr out of SetWindowLongPtr, and the only way to
  // know if that's a valid old value is to use GetLastError. Clear the error
  // here so we can tell.
  SetLastError(ERROR_SUCCESS);

  LONG_PTR currentWndProc =
    SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)NeuteredWindowProc);
  if (!currentWndProc) {
    if (ERROR_SUCCESS == GetLastError()) {
      // No error, so we set something and must therefore reset it.
      SetWindowLongPtr(hWnd, GWLP_WNDPROC, currentWndProc);
    }
    return false;
  }

  NS_ASSERTION(currentWndProc != (LONG_PTR)NeuteredWindowProc,
               "This shouldn't be possible!");

  if (!SetProp(hWnd, kOldWndProcProp, (HANDLE)currentWndProc)) {
    // Cleanup
    NS_WARNING("SetProp failed!");
    SetWindowLongPtr(hWnd, GWLP_WNDPROC, currentWndProc);
    RemovePropW(hWnd, kOldWndProcProp);
    RemovePropW(hWnd, k3rdPartyWindowProp);
    return false;
  }

  return true;
}

void
RestoreWindowProcedure(HWND hWnd)
{
  NS_ASSERTION(WindowIsDeferredWindow(hWnd),
               "Not a deferred window, this shouldn't be in our list!");
  LONG_PTR oldWndProc = (LONG_PTR)GetProp(hWnd, kOldWndProcProp);
  if (oldWndProc) {
    NS_ASSERTION(oldWndProc != (LONG_PTR)NeuteredWindowProc,
                 "This shouldn't be possible!");

    DebugOnly<LONG_PTR> currentWndProc =
      SetWindowLongPtr(hWnd, GWLP_WNDPROC, oldWndProc);
    NS_ASSERTION(currentWndProc == (LONG_PTR)NeuteredWindowProc,
                 "This should never be switched out from under us!");
  }
  RemovePropW(hWnd, kOldWndProcProp);
  RemovePropW(hWnd, k3rdPartyWindowProp);
}

LRESULT CALLBACK
CallWindowProcedureHook(int nCode,
                        WPARAM wParam,
                        LPARAM lParam)
{
  if (nCode >= 0) {
    NS_ASSERTION(gNeuteredWindows, "This should never be null!");

    HWND hWnd = reinterpret_cast<CWPSTRUCT*>(lParam)->hwnd;

    if (!gNeuteredWindows->Contains(hWnd) && NeuterWindowProcedure(hWnd)) {
      if (!gNeuteredWindows->AppendElement(hWnd)) {
        NS_ERROR("Out of memory!");
        RestoreWindowProcedure(hWnd);
      }
    }
  }
  return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

inline void
AssertWindowIsNotNeutered(HWND hWnd)
{
#ifdef DEBUG
  // Make sure our neutered window hook isn't still in place.
  LONG_PTR wndproc = GetWindowLongPtr(hWnd, GWLP_WNDPROC);
  NS_ASSERTION(wndproc != (LONG_PTR)NeuteredWindowProc, "Window is neutered!");
#endif
}

void
UnhookNeuteredWindows()
{
  if (!gNeuteredWindows)
    return;
  uint32_t count = gNeuteredWindows->Length();
  for (uint32_t index = 0; index < count; index++) {
    RestoreWindowProcedure(gNeuteredWindows->ElementAt(index));
  }
  gNeuteredWindows->Clear();
}

// This timeout stuff assumes a sane value of mTimeoutMs (less than the overflow
// value for GetTickCount(), which is something like 50 days). It uses the
// cheapest (and least accurate) method supported by Windows 2000.

struct TimeoutData
{
  DWORD startTicks;
  DWORD targetTicks;
};

void
InitTimeoutData(TimeoutData* aData,
                int32_t aTimeoutMs)
{
  aData->startTicks = GetTickCount();
  if (!aData->startTicks) {
    // How unlikely is this!
    aData->startTicks++;
  }
  aData->targetTicks = aData->startTicks + aTimeoutMs;
}


bool
TimeoutHasExpired(const TimeoutData& aData)
{
  if (!aData.startTicks) {
    return false;
  }

  DWORD now = GetTickCount();

  if (aData.targetTicks < aData.startTicks) {
    // Overflow
    return now < aData.startTicks && now >= aData.targetTicks;
  }
  return now >= aData.targetTicks;
}

} // namespace

namespace mozilla {
namespace ipc {
namespace windows {

void
InitUIThread()
{
  // If we aren't setup before a call to NotifyWorkerThread, we'll hang
  // on startup.
  if (!gUIThreadId) {
    gUIThreadId = GetCurrentThreadId();
  }

  MOZ_ASSERT(gUIThreadId);
  MOZ_ASSERT(gUIThreadId == GetCurrentThreadId(),
             "Called InitUIThread multiple times on different threads!");

  if (!gWinEventHook) {
    gWinEventHook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY,
                                    NULL, &WinEventHook, GetCurrentProcessId(),
                                    gUIThreadId, WINEVENT_OUTOFCONTEXT);

    // We need to execute this after setting the hook in case the OLE window
    // already existed.
    gCOMWindow = FindCOMWindow();
  }
  MOZ_ASSERT(gWinEventHook);
}

} // namespace windows
} // namespace ipc
} // namespace mozilla

// See SpinInternalEventLoop below
MessageChannel::SyncStackFrame::SyncStackFrame(MessageChannel* channel, bool interrupt)
  : mInterrupt(interrupt)
  , mSpinNestedEvents(false)
  , mListenerNotified(false)
  , mChannel(channel)
  , mPrev(mChannel->mTopFrame)
  , mStaticPrev(sStaticTopFrame)
{
  // Only track stack frames when Windows message deferral behavior
  // is request for the channel.
  if (!(mChannel->GetChannelFlags() & REQUIRE_DEFERRED_MESSAGE_PROTECTION)) {
    return;
  }

  mChannel->mTopFrame = this;
  sStaticTopFrame = this;

  if (!mStaticPrev) {
    NS_ASSERTION(!gNeuteredWindows, "Should only set this once!");
    gNeuteredWindows = new nsAutoTArray<HWND, 20>();
    NS_ASSERTION(gNeuteredWindows, "Out of memory!");
  }
}

MessageChannel::SyncStackFrame::~SyncStackFrame()
{
  if (!(mChannel->GetChannelFlags() & REQUIRE_DEFERRED_MESSAGE_PROTECTION)) {
    return;
  }

  NS_ASSERTION(this == mChannel->mTopFrame,
               "Mismatched interrupt stack frames");
  NS_ASSERTION(this == sStaticTopFrame,
               "Mismatched static Interrupt stack frames");

  mChannel->mTopFrame = mPrev;
  sStaticTopFrame = mStaticPrev;

  if (!mStaticPrev) {
    NS_ASSERTION(gNeuteredWindows, "Bad pointer!");
    delete gNeuteredWindows;
    gNeuteredWindows = nullptr;
  }
}

MessageChannel::SyncStackFrame* MessageChannel::sStaticTopFrame;

// nsAppShell's notification that gecko events are being processed.
// If we are here and there is an Interrupt Incall active, we are spinning
// a nested gecko event loop. In which case the remote process needs
// to know about it.
void /* static */
MessageChannel::NotifyGeckoEventDispatch()
{
  // sStaticTopFrame is only valid for Interrupt channels
  if (!sStaticTopFrame || sStaticTopFrame->mListenerNotified)
    return;

  sStaticTopFrame->mListenerNotified = true;
  MessageChannel* channel = static_cast<MessageChannel*>(sStaticTopFrame->mChannel);
  channel->Listener()->ProcessRemoteNativeEventsInInterruptCall();
}

// invoked by the module that receives the spin event loop
// message.
void
MessageChannel::ProcessNativeEventsInInterruptCall()
{
  NS_ASSERTION(GetCurrentThreadId() == gUIThreadId,
               "Shouldn't be on a non-main thread in here!");
  if (!mTopFrame) {
    NS_ERROR("Spin logic error: no Interrupt frame");
    return;
  }

  mTopFrame->mSpinNestedEvents = true;
}

// Spin loop is called in place of WaitFor*Notify when modal ui is being shown
// in a child. There are some intricacies in using it however. Spin loop is
// enabled for a particular Interrupt frame by the client calling
// MessageChannel::ProcessNativeEventsInInterrupt().
// This call can be nested for multiple Interrupt frames in a single plugin or 
// multiple unrelated plugins.
void
MessageChannel::SpinInternalEventLoop()
{
  if (mozilla::PaintTracker::IsPainting()) {
    NS_RUNTIMEABORT("Don't spin an event loop while painting.");
  }

  NS_ASSERTION(mTopFrame && mTopFrame->mSpinNestedEvents,
               "Spinning incorrectly");

  // Nested windows event loop we trigger when the child enters into modal
  // event loops.
  
  // Note, when we return, we always reset the notify worker event. So there's
  // no need to reset it on return here.

  do {
    MSG msg = { 0 };

    // Don't get wrapped up in here if the child connection dies.
    {
      MonitorAutoLock lock(*mMonitor);
      if (!Connected()) {
        return;
      }
    }

    // Retrieve window or thread messages
    if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      // The child UI should have been destroyed before the app is closed, in
      // which case, we should never get this here.
      if (msg.message == WM_QUIT) {
          NS_ERROR("WM_QUIT received in SpinInternalEventLoop!");
      } else {
          TranslateMessage(&msg);
          ::DispatchMessageW(&msg);
          return;
      }
    }

    // Note, give dispatching windows events priority over checking if
    // mEvent is signaled, otherwise heavy ipc traffic can cause jittery
    // playback of video. We'll exit out on each disaptch above, so ipc
    // won't get starved.

    // Wait for UI events or a signal from the io thread.
    DWORD result = MsgWaitForMultipleObjects(1, &mEvent, FALSE, INFINITE,
                                             QS_ALLINPUT);
    if (result == WAIT_OBJECT_0) {
      // Our NotifyWorkerThread event was signaled
      return;
    }
  } while (true);
}

static inline bool
IsTimeoutExpired(PRIntervalTime aStart, PRIntervalTime aTimeout)
{
  return (aTimeout != PR_INTERVAL_NO_TIMEOUT) &&
    (aTimeout <= (PR_IntervalNow() - aStart));
}

static HHOOK gWindowHook;

static inline void
StartNeutering()
{
  MOZ_ASSERT(gUIThreadId);
  MOZ_ASSERT(!gWindowHook);
  NS_ASSERTION(!MessageChannel::IsPumpingMessages(),
               "Shouldn't be pumping already!");
  MessageChannel::SetIsPumpingMessages(true);
  gWindowHook = ::SetWindowsHookEx(WH_CALLWNDPROC, CallWindowProcedureHook,
                                   nullptr, gUIThreadId);
  NS_ASSERTION(gWindowHook, "Failed to set hook!");
}

static void
StopNeutering()
{
  MOZ_ASSERT(MessageChannel::IsPumpingMessages());
  ::UnhookWindowsHookEx(gWindowHook);
  gWindowHook = NULL;
  ::UnhookNeuteredWindows();
  // Before returning we need to set a hook to run any deferred messages that
  // we received during the IPC call. The hook will unset itself as soon as
  // someone else calls GetMessage, PeekMessage, or runs code that generates
  // a "nonqueued" message.
  ::ScheduleDeferredMessageRun();
  MessageChannel::SetIsPumpingMessages(false);
}

NeuteredWindowRegion::NeuteredWindowRegion(bool aDoNeuter MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : mNeuteredByThis(!gWindowHook && aDoNeuter)
{
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  if (mNeuteredByThis) {
    StartNeutering();
  }
}

NeuteredWindowRegion::~NeuteredWindowRegion()
{
  if (gWindowHook && mNeuteredByThis) {
    StopNeutering();
  }
}

void
NeuteredWindowRegion::PumpOnce()
{
  if (!gWindowHook) {
    // This should be a no-op if nothing has been neutered.
    return;
  }

  MSG msg = {0};
  // Pump any COM messages so that we don't hang due to STA marshaling.
  if (gCOMWindow && ::PeekMessageW(&msg, gCOMWindow, 0, 0, PM_REMOVE)) {
      ::TranslateMessage(&msg);
      ::DispatchMessageW(&msg);
  }
  // Expunge any nonqueued messages on the current thread.
  ::PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE);
}

DeneuteredWindowRegion::DeneuteredWindowRegion(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_IN_IMPL)
  : mReneuter(gWindowHook != NULL)
{
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  if (mReneuter) {
    StopNeutering();
  }
}

DeneuteredWindowRegion::~DeneuteredWindowRegion()
{
  if (mReneuter) {
    StartNeutering();
  }
}

bool
MessageChannel::WaitForSyncNotify()
{
  mMonitor->AssertCurrentThreadOwns();

  MOZ_ASSERT(gUIThreadId, "InitUIThread was not called!");

  // Use a blocking wait if this channel does not require
  // Windows message deferral behavior.
  if (!(mFlags & REQUIRE_DEFERRED_MESSAGE_PROTECTION)) {
    PRIntervalTime timeout = (kNoTimeout == mTimeoutMs) ?
                             PR_INTERVAL_NO_TIMEOUT :
                             PR_MillisecondsToInterval(mTimeoutMs);
    PRIntervalTime waitStart = 0;

    if (timeout != PR_INTERVAL_NO_TIMEOUT) {
      waitStart = PR_IntervalNow();
    }

    MOZ_ASSERT(!mIsSyncWaitingOnNonMainThread);
    mIsSyncWaitingOnNonMainThread = true;

    mMonitor->Wait(timeout);

    MOZ_ASSERT(mIsSyncWaitingOnNonMainThread);
    mIsSyncWaitingOnNonMainThread = false;

    // If the timeout didn't expire, we know we received an event. The
    // converse is not true.
    return WaitResponse(timeout == PR_INTERVAL_NO_TIMEOUT ?
                        false : IsTimeoutExpired(waitStart, timeout));
  }

  NS_ASSERTION(mFlags & REQUIRE_DEFERRED_MESSAGE_PROTECTION,
               "Shouldn't be here for channels that don't use message deferral!");
  NS_ASSERTION(mTopFrame && !mTopFrame->mInterrupt,
               "Top frame is not a sync frame!");

  MonitorAutoUnlock unlock(*mMonitor);

  bool timedout = false;

  UINT_PTR timerId = 0;
  TimeoutData timeoutData = { 0 };

  if (mTimeoutMs != kNoTimeout) {
    InitTimeoutData(&timeoutData, mTimeoutMs);

    // We only do this to ensure that we won't get stuck in
    // MsgWaitForMultipleObjects below.
    timerId = SetTimer(nullptr, 0, mTimeoutMs, nullptr);
    NS_ASSERTION(timerId, "SetTimer failed!");
  }

  {
    while (1) {
      MSG msg = { 0 };
      // Don't get wrapped up in here if the child connection dies.
      {
        MonitorAutoLock lock(*mMonitor);
        if (!Connected()) {
          break;
        }
      }

      // Wait until we have a message in the queue. MSDN docs are a bit unclear
      // but it seems that windows from two different threads (and it should be
      // noted that a thread in another process counts as a "different thread")
      // will implicitly have their message queues attached if they are parented
      // to one another. This wait call, then, will return for a message
      // delivered to *either* thread.
      DWORD result = MsgWaitForMultipleObjects(1, &mEvent, FALSE, INFINITE,
                                               QS_ALLINPUT);
      if (result == WAIT_OBJECT_0) {
        // Our NotifyWorkerThread event was signaled
        ResetEvent(mEvent);
        break;
      } else
      if (result != (WAIT_OBJECT_0 + 1)) {
        NS_ERROR("Wait failed!");
        break;
      }

      if (TimeoutHasExpired(timeoutData)) {
        // A timeout was specified and we've passed it. Break out.
        timedout = true;
        break;
      }

      // The only way to know on which thread the message was delivered is to
      // use some logic on the return values of GetQueueStatus and PeekMessage.
      // PeekMessage will return false if there are no "queued" messages, but it
      // will run all "nonqueued" messages before returning. So if PeekMessage
      // returns false and there are no "nonqueued" messages that were run then
      // we know that the message we woke for was intended for a window on
      // another thread.
      bool haveSentMessagesPending =
        (HIWORD(GetQueueStatus(QS_SENDMESSAGE)) & QS_SENDMESSAGE) != 0;

      // Either of the PeekMessage calls below will actually process all
      // "nonqueued" messages that are pending before returning. If we have
      // "nonqueued" messages pending then we should have switched out all the
      // window procedures above. In that case this PeekMessage call won't
      // actually cause any mozilla code (or plugin code) to run.

      // We have to manually pump all COM messages *after* looking at the queue
      // queue status but before yielding our thread below.
      if (gCOMWindow) {
        if (PeekMessageW(&msg, gCOMWindow, 0, 0, PM_REMOVE)) {
          TranslateMessage(&msg);
          ::DispatchMessageW(&msg);
        }
      }

      // If the following PeekMessage call fails to return a message for us (and
      // returns false) and we didn't run any "nonqueued" messages then we must
      // have woken up for a message designated for a window in another thread.
      // If we loop immediately then we could enter a tight loop, so we'll give
      // up our time slice here to let the child process its message.
      if (!PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE) &&
          !haveSentMessagesPending) {
        // Message was for child, we should wait a bit.
        SwitchToThread();
      }
    }
  }

  if (timerId) {
    KillTimer(nullptr, timerId);
    timerId = 0;
  }

  return WaitResponse(timedout);
}

bool
MessageChannel::WaitForInterruptNotify()
{
  mMonitor->AssertCurrentThreadOwns();

  MOZ_ASSERT(gUIThreadId, "InitUIThread was not called!");

  // Re-use sync notification wait code if this channel does not require
  // Windows message deferral behavior. 
  if (!(mFlags & REQUIRE_DEFERRED_MESSAGE_PROTECTION)) {
    return WaitForSyncNotify();
  }

  if (!InterruptStackDepth() && !AwaitingIncomingMessage()) {
    // There is currently no way to recover from this condition.
    NS_RUNTIMEABORT("StackDepth() is 0 in call to MessageChannel::WaitForNotify!");
  }

  NS_ASSERTION(mFlags & REQUIRE_DEFERRED_MESSAGE_PROTECTION,
               "Shouldn't be here for channels that don't use message deferral!");
  NS_ASSERTION(mTopFrame && mTopFrame->mInterrupt,
               "Top frame is not a sync frame!");

  MonitorAutoUnlock unlock(*mMonitor);

  bool timedout = false;

  UINT_PTR timerId = 0;
  TimeoutData timeoutData = { 0 };

  // gWindowHook is used as a flag variable for the loop below: if it is set
  // and we start to spin a nested event loop, we need to clear the hook and
  // process deferred/pending messages.
  while (1) {
    NS_ASSERTION((!!gWindowHook) == MessageChannel::IsPumpingMessages(),
                 "gWindowHook out of sync with reality");

    if (mTopFrame->mSpinNestedEvents) {
      if (gWindowHook && timerId) {
        KillTimer(nullptr, timerId);
        timerId = 0;
      }
      DeneuteredWindowRegion deneuteredRgn;
      SpinInternalEventLoop();
      ResetEvent(mEvent);
      return true;
    }

    if (mTimeoutMs != kNoTimeout && !timerId) {
      InitTimeoutData(&timeoutData, mTimeoutMs);
      timerId = SetTimer(nullptr, 0, mTimeoutMs, nullptr);
      NS_ASSERTION(timerId, "SetTimer failed!");
    }

    MSG msg = { 0 };

    // Don't get wrapped up in here if the child connection dies.
    {
      MonitorAutoLock lock(*mMonitor);
      if (!Connected()) {
        break;
      }
    }

    DWORD result = MsgWaitForMultipleObjects(1, &mEvent, FALSE, INFINITE,
                                             QS_ALLINPUT);
    if (result == WAIT_OBJECT_0) {
      // Our NotifyWorkerThread event was signaled
      ResetEvent(mEvent);
      break;
    } else
    if (result != (WAIT_OBJECT_0 + 1)) {
      NS_ERROR("Wait failed!");
      break;
    }

    if (TimeoutHasExpired(timeoutData)) {
      // A timeout was specified and we've passed it. Break out.
      timedout = true;
      break;
    }

    // See MessageChannel's WaitFor*Notify for details.
    bool haveSentMessagesPending =
      (HIWORD(GetQueueStatus(QS_SENDMESSAGE)) & QS_SENDMESSAGE) != 0;

    // Run all COM messages *after* looking at the queue status.
    if (gCOMWindow) {
        if (PeekMessageW(&msg, gCOMWindow, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
    }

    // PeekMessage markes the messages as "old" so that they don't wake up
    // MsgWaitForMultipleObjects every time.
    if (!PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE) &&
        !haveSentMessagesPending) {
      // Message was for child, we should wait a bit.
      SwitchToThread();
    }
  }

  if (timerId) {
    KillTimer(nullptr, timerId);
    timerId = 0;
  }

  return WaitResponse(timedout);
}

void
MessageChannel::NotifyWorkerThread()
{
  mMonitor->AssertCurrentThreadOwns();

  if (mIsSyncWaitingOnNonMainThread) {
    mMonitor->Notify();
    return;
  }

  NS_ASSERTION(mEvent, "No signal event to set, this is really bad!");
  if (!SetEvent(mEvent)) {
    NS_WARNING("Failed to set NotifyWorkerThread event!");
  }
}

void
DeferredSendMessage::Run()
{
  AssertWindowIsNotNeutered(hWnd);
  if (!IsWindow(hWnd)) {
    NS_ERROR("Invalid window!");
    return;
  }

  WNDPROC wndproc =
    reinterpret_cast<WNDPROC>(GetWindowLongPtr(hWnd, GWLP_WNDPROC));
  if (!wndproc) {
    NS_ERROR("Invalid window procedure!");
    return;
  }

  CallWindowProc(wndproc, hWnd, message, wParam, lParam);
}

void
DeferredRedrawMessage::Run()
{
  AssertWindowIsNotNeutered(hWnd);
  if (!IsWindow(hWnd)) {
    NS_ERROR("Invalid window!");
    return;
  }

#ifdef DEBUG
  BOOL ret =
#endif
  RedrawWindow(hWnd, nullptr, nullptr, flags);
  NS_ASSERTION(ret, "RedrawWindow failed!");
}

DeferredUpdateMessage::DeferredUpdateMessage(HWND aHWnd)
{
  mWnd = aHWnd;
  if (!GetUpdateRect(mWnd, &mUpdateRect, FALSE)) {
    memset(&mUpdateRect, 0, sizeof(RECT));
    return;
  }
  ValidateRect(mWnd, &mUpdateRect);
}

void
DeferredUpdateMessage::Run()
{
  AssertWindowIsNotNeutered(mWnd);
  if (!IsWindow(mWnd)) {
    NS_ERROR("Invalid window!");
    return;
  }

  InvalidateRect(mWnd, &mUpdateRect, FALSE);
#ifdef DEBUG
  BOOL ret =
#endif
  UpdateWindow(mWnd);
  NS_ASSERTION(ret, "UpdateWindow failed!");
}

DeferredSettingChangeMessage::DeferredSettingChangeMessage(HWND aHWnd,
                                                           UINT aMessage,
                                                           WPARAM aWParam,
                                                           LPARAM aLParam)
: DeferredSendMessage(aHWnd, aMessage, aWParam, aLParam)
{
  NS_ASSERTION(aMessage == WM_SETTINGCHANGE, "Wrong message type!");
  if (aLParam) {
    lParamString = _wcsdup(reinterpret_cast<const wchar_t*>(aLParam));
    lParam = reinterpret_cast<LPARAM>(lParamString);
  }
  else {
    lParamString = nullptr;
    lParam = 0;
  }
}

DeferredSettingChangeMessage::~DeferredSettingChangeMessage()
{
  free(lParamString);
}

DeferredWindowPosMessage::DeferredWindowPosMessage(HWND aHWnd,
                                                   LPARAM aLParam,
                                                   bool aForCalcSize,
                                                   WPARAM aWParam)
{
  if (aForCalcSize) {
    if (aWParam) {
      NCCALCSIZE_PARAMS* arg = reinterpret_cast<NCCALCSIZE_PARAMS*>(aLParam);
      memcpy(&windowPos, arg->lppos, sizeof(windowPos));

      NS_ASSERTION(aHWnd == windowPos.hwnd, "Mismatched hwnds!");
    }
    else {
      RECT* arg = reinterpret_cast<RECT*>(aLParam);
      windowPos.hwnd = aHWnd;
      windowPos.hwndInsertAfter = nullptr;
      windowPos.x = arg->left;
      windowPos.y = arg->top;
      windowPos.cx = arg->right - arg->left;
      windowPos.cy = arg->bottom - arg->top;

      NS_ASSERTION(arg->right >= arg->left && arg->bottom >= arg->top,
                   "Negative width or height!");
    }
    windowPos.flags = SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOOWNERZORDER |
                      SWP_NOZORDER | SWP_DEFERERASE | SWP_NOSENDCHANGING;
  }
  else {
    // Not for WM_NCCALCSIZE
    WINDOWPOS* arg = reinterpret_cast<WINDOWPOS*>(aLParam);
    memcpy(&windowPos, arg, sizeof(windowPos));

    NS_ASSERTION(aHWnd == windowPos.hwnd, "Mismatched hwnds!");

    // Windows sends in some private flags sometimes that we can't simply copy.
    // Filter here.
    UINT mask = SWP_ASYNCWINDOWPOS | SWP_DEFERERASE | SWP_DRAWFRAME |
                SWP_FRAMECHANGED | SWP_HIDEWINDOW | SWP_NOACTIVATE |
                SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREDRAW |
                SWP_NOREPOSITION | SWP_NOSENDCHANGING | SWP_NOSIZE |
                SWP_NOZORDER | SWP_SHOWWINDOW;
    windowPos.flags &= mask;
  }
}

void
DeferredWindowPosMessage::Run()
{
  AssertWindowIsNotNeutered(windowPos.hwnd);
  if (!IsWindow(windowPos.hwnd)) {
    NS_ERROR("Invalid window!");
    return;
  }

  if (!IsWindow(windowPos.hwndInsertAfter)) {
    NS_WARNING("ZOrder change cannot be honored");
    windowPos.hwndInsertAfter = 0;
    windowPos.flags |= SWP_NOZORDER;
  }

#ifdef DEBUG
  BOOL ret = 
#endif
  SetWindowPos(windowPos.hwnd, windowPos.hwndInsertAfter, windowPos.x,
               windowPos.y, windowPos.cx, windowPos.cy, windowPos.flags);
  NS_ASSERTION(ret, "SetWindowPos failed!");
}

DeferredCopyDataMessage::DeferredCopyDataMessage(HWND aHWnd,
                                                 UINT aMessage,
                                                 WPARAM aWParam,
                                                 LPARAM aLParam)
: DeferredSendMessage(aHWnd, aMessage, aWParam, aLParam)
{
  NS_ASSERTION(IsWindow(reinterpret_cast<HWND>(aWParam)), "Bad window!");

  COPYDATASTRUCT* source = reinterpret_cast<COPYDATASTRUCT*>(aLParam);
  NS_ASSERTION(source, "Should never be null!");

  copyData.dwData = source->dwData;
  copyData.cbData = source->cbData;

  if (source->cbData) {
    copyData.lpData = malloc(source->cbData);
    if (copyData.lpData) {
      memcpy(copyData.lpData, source->lpData, source->cbData);
    }
    else {
      NS_ERROR("Out of memory?!");
      copyData.cbData = 0;
    }
  }
  else {
    copyData.lpData = nullptr;
  }

  lParam = reinterpret_cast<LPARAM>(&copyData);
}

DeferredCopyDataMessage::~DeferredCopyDataMessage()
{
  free(copyData.lpData);
}

DeferredStyleChangeMessage::DeferredStyleChangeMessage(HWND aHWnd,
                                                       WPARAM aWParam,
                                                       LPARAM aLParam)
: hWnd(aHWnd)
{
  index = static_cast<int>(aWParam);
  style = reinterpret_cast<STYLESTRUCT*>(aLParam)->styleNew;
}

void
DeferredStyleChangeMessage::Run()
{
  SetWindowLongPtr(hWnd, index, style);
}

DeferredSetIconMessage::DeferredSetIconMessage(HWND aHWnd,
                                               UINT aMessage,
                                               WPARAM aWParam,
                                               LPARAM aLParam)
: DeferredSendMessage(aHWnd, aMessage, aWParam, aLParam)
{
  NS_ASSERTION(aMessage == WM_SETICON, "Wrong message type!");
}

void
DeferredSetIconMessage::Run()
{
  AssertWindowIsNotNeutered(hWnd);
  if (!IsWindow(hWnd)) {
    NS_ERROR("Invalid window!");
    return;
  }

  WNDPROC wndproc =
    reinterpret_cast<WNDPROC>(GetWindowLongPtr(hWnd, GWLP_WNDPROC));
  if (!wndproc) {
    NS_ERROR("Invalid window procedure!");
    return;
  }

  HICON hOld = reinterpret_cast<HICON>(
    CallWindowProc(wndproc, hWnd, message, wParam, lParam));
  if (hOld) {
    DestroyIcon(hOld);
  }
}
