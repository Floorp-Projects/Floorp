/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowsMessageLoop.h"
#include "RPCChannel.h"

#include "nsAutoPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsStringGlue.h"
#include "nsIXULAppInfo.h"

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
 * Queued and "non-queued" messages will be processed during RPC calls if
 * modal UI related api calls block an RPC in-call in the child. To prevent
 * windows from freezing, and to allow concurrent processing of critical
 * events (such as painting), we spin a native event dispatch loop while
 * these in-calls are blocked.
 */

// pulled from widget's nsAppShell
extern const PRUnichar* kAppShellEventId;
#if defined(ACCESSIBILITY)
// pulled from accessibility's win utils
extern const PRUnichar* kPropNameTabContent;
#endif

namespace {

const wchar_t kOldWndProcProp[] = L"MozillaIPCOldWndProc";

// This isn't defined before Windows XP.
enum { WM_XP_THEMECHANGED = 0x031A };

PRUnichar gAppMessageWindowName[256] = { 0 };
PRInt32 gAppMessageWindowNameLength = 0;

nsTArray<HWND>* gNeuteredWindows = nsnull;

typedef nsTArray<nsAutoPtr<DeferredMessage> > DeferredMessageArray;
DeferredMessageArray* gDeferredMessages = nsnull;

HHOOK gDeferredGetMsgHook = NULL;
HHOOK gDeferredCallWndProcHook = NULL;

DWORD gUIThreadId = 0;
int gEventLoopDepth = 0;
static UINT sAppShellGeckoMsgId;

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
  //   3. We're not being called from the PeekMessage within the WaitForNotify
  //      function (indicated with SyncChannel::IsPumpingMessages). We really
  //      only want to run after returning to the main event loop.
  if (nCode >= 0 && gDeferredMessages && !SyncChannel::IsPumpingMessages()) {
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
    gDeferredMessages = nsnull;

    // Run all the deferred messages in order.
    PRUint32 count = messages->Length();
    for (PRUint32 index = 0; index < count; index++) {
      messages->ElementAt(index)->Run();
    }
  }

  // Always call the next hook.
  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void
ScheduleDeferredMessageRun()
{
  if (gDeferredMessages &&
      !(gDeferredGetMsgHook && gDeferredCallWndProcHook)) {
    NS_ASSERTION(gDeferredMessages->Length(), "No deferred messages?!");

    gDeferredGetMsgHook = ::SetWindowsHookEx(WH_GETMESSAGE, DeferredMessageHook,
                                             NULL, gUIThreadId);
    gDeferredCallWndProcHook = ::SetWindowsHookEx(WH_CALLWNDPROC,
                                                  DeferredMessageHook, NULL,
                                                  gUIThreadId);
    NS_ASSERTION(gDeferredGetMsgHook && gDeferredCallWndProcHook,
                 "Failed to set hooks!");
  }
}

LRESULT
ProcessOrDeferMessage(HWND hwnd,
                      UINT uMsg,
                      WPARAM wParam,
                      LPARAM lParam)
{
  DeferredMessage* deferred = nsnull;

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
    case WM_WINDOWPOSCHANGING: { 
      return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    // Just return, prevents DefWindowProc from messaging the window
    // syncronously with other events, which may be deferred. Prevents 
    // random shutdown of aero composition on the window. 
    case WM_SYNCPAINT:
      return 0;

    default: {
      if (uMsg && uMsg == sAppShellGeckoMsgId) {
        // Widget's registered native event callback
        deferred = new DeferredSendMessage(hwnd, uMsg, wParam, lParam);
      } else {
        // Unknown messages only
#ifdef DEBUG
        nsCAutoString log("Received \"nonqueued\" message ");
        log.AppendInt(uMsg);
        log.AppendLiteral(" during a synchronous IPC message for window ");
        log.AppendInt((PRInt64)hwnd);

        wchar_t className[256] = { 0 };
        if (GetClassNameW(hwnd, className, sizeof(className) - 1) > 0) {
          log.AppendLiteral(" (\"");
          log.Append(NS_ConvertUTF16toUTF8((PRUnichar*)className));
          log.AppendLiteral("\")");
        }

        log.AppendLiteral(", sending it to DefWindowProc instead of the normal "
                          "window procedure.");
        NS_ERROR(log.get());
#endif
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
      }
    }
  }

  NS_ASSERTION(deferred, "Must have a message here!");

  // Create the deferred message array if it doesn't exist already.
  if (!gDeferredMessages) {
    gDeferredMessages = new nsTArray<nsAutoPtr<DeferredMessage> >(20);
    NS_ASSERTION(gDeferredMessages, "Out of memory!");
  }

  // Save for later. The array takes ownership of |deferred|.
  gDeferredMessages->AppendElement(deferred);
  return res;
}

} // anonymous namespace

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

  PRUnichar buffer[256] = { 0 };
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
  // 'AGFullScreenWinClass' - silverlight fullscreen window
  if (className.EqualsLiteral("ShockwaveFlashFullScreen") ||
      className.EqualsLiteral("AGFullScreenWinClass")) {
    return true;
  }

  // Google Earth bridging msg window between the plugin instance and a separate
  // earth process. The earth process can trigger a plugin incall on the browser
  // at any time, which is badness if the instance is already making an incall.
  if (className.EqualsLiteral("__geplugin_bridge_window__")) {
    return true;
  }

  // nsNativeAppSupport makes a window like "FirefoxMessageWindow" based on the
  // toolkit app's name. It's pretty expensive to calculate this so we only try
  // once.
  if (gAppMessageWindowNameLength == 0) {
    nsCOMPtr<nsIXULAppInfo> appInfo =
      do_GetService("@mozilla.org/xre/app-info;1");
    if (appInfo) {
      nsCAutoString appName;
      if (NS_SUCCEEDED(appInfo->GetName(appName))) {
        appName.Append("MessageWindow");
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

  // It's possible to get NULL out of SetWindowLongPtr, and the only way to know
  // if that's a valid old value is to use GetLastError. Clear the error here so
  // we can tell.
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
    RemoveProp(hWnd, kOldWndProcProp);
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

    LONG_PTR currentWndProc =
      SetWindowLongPtr(hWnd, GWLP_WNDPROC, oldWndProc);
    NS_ASSERTION(currentWndProc == (LONG_PTR)NeuteredWindowProc,
                 "This should never be switched out from under us!");
  }
  RemoveProp(hWnd, kOldWndProcProp);
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
  return CallNextHookEx(NULL, nCode, wParam, lParam);
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
  PRUint32 count = gNeuteredWindows->Length();
  for (PRUint32 index = 0; index < count; index++) {
    RestoreWindowProcedure(gNeuteredWindows->ElementAt(index));
  }
  gNeuteredWindows->Clear();
}

void
Init()
{
  // If we aren't setup before a call to NotifyWorkerThread, we'll hang
  // on startup.
  if (!gUIThreadId) {
    gUIThreadId = GetCurrentThreadId();
  }
  NS_ASSERTION(gUIThreadId, "ThreadId should not be 0!");
  NS_ASSERTION(gUIThreadId == GetCurrentThreadId(),
               "Running on different threads!");
  sAppShellGeckoMsgId = RegisterWindowMessageW(kAppShellEventId);
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
                int32 aTimeoutMs)
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

} // anonymous namespace

RPCChannel::SyncStackFrame::SyncStackFrame(SyncChannel* channel, bool rpc)
  : mRPC(rpc)
  , mSpinNestedEvents(false)
  , mListenerNotified(false)
  , mChannel(channel)
  , mPrev(mChannel->mTopFrame)
  , mStaticPrev(sStaticTopFrame)
{
  mChannel->mTopFrame = this;
  sStaticTopFrame = this;

  if (!mStaticPrev) {
    NS_ASSERTION(!gNeuteredWindows, "Should only set this once!");
    gNeuteredWindows = new nsAutoTArray<HWND, 20>();
    NS_ASSERTION(gNeuteredWindows, "Out of memory!");
  }
}

RPCChannel::SyncStackFrame::~SyncStackFrame()
{
  NS_ASSERTION(this == mChannel->mTopFrame,
               "Mismatched RPC stack frames");
  NS_ASSERTION(this == sStaticTopFrame,
               "Mismatched static RPC stack frames");

  mChannel->mTopFrame = mPrev;
  sStaticTopFrame = mStaticPrev;

  if (!mStaticPrev) {
    NS_ASSERTION(gNeuteredWindows, "Bad pointer!");
    delete gNeuteredWindows;
    gNeuteredWindows = NULL;
  }
}

SyncChannel::SyncStackFrame* SyncChannel::sStaticTopFrame;

// nsAppShell's notification that gecko events are being processed.
// If we are here and there is an RPC Incall active, we are spinning
// a nested gecko event loop. In which case the remote process needs
// to know about it.
void /* static */
RPCChannel::NotifyGeckoEventDispatch()
{
  // sStaticTopFrame is only valid for RPC channels
  if (!sStaticTopFrame || sStaticTopFrame->mListenerNotified)
    return;

  sStaticTopFrame->mListenerNotified = true;
  RPCChannel* channel = static_cast<RPCChannel*>(sStaticTopFrame->mChannel);
  channel->Listener()->ProcessRemoteNativeEventsInRPCCall();
}

// invoked by the module that receives the spin event loop
// message.
void
RPCChannel::ProcessNativeEventsInRPCCall()
{
  if (!mTopFrame) {
    NS_ERROR("Spin logic error: no RPC frame");
    return;
  }

  mTopFrame->mSpinNestedEvents = true;
}

// Spin loop is called in place of WaitForNotify when modal ui is being shown
// in a child. There are some intricacies in using it however. Spin loop is
// enabled for a particular RPC frame by the client calling
// RPCChannel::ProcessNativeEventsInRPCCall().
// This call can be nested for multiple RPC frames in a single plugin or 
// multiple unrelated plugins.
void
RPCChannel::SpinInternalEventLoop()
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
    if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
      // The child UI should have been destroyed before the app is closed, in
      // which case, we should never get this here.
      if (msg.message == WM_QUIT) {
          NS_ERROR("WM_QUIT received in SpinInternalEventLoop!");
      } else {
          TranslateMessage(&msg);
          DispatchMessageW(&msg);
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

bool
SyncChannel::WaitForNotify()
{
  mMonitor->AssertCurrentThreadOwns();

  // Initialize global objects used in deferred messaging.
  Init();

  NS_ASSERTION(mTopFrame && !mTopFrame->mRPC,
               "Top frame is not a sync frame!");

  MonitorAutoUnlock unlock(*mMonitor);

  bool timedout = false;

  UINT_PTR timerId = NULL;
  TimeoutData timeoutData = { 0 };

  if (mTimeoutMs != kNoTimeout) {
    InitTimeoutData(&timeoutData, mTimeoutMs);

    // We only do this to ensure that we won't get stuck in
    // MsgWaitForMultipleObjects below.
    timerId = SetTimer(NULL, 0, mTimeoutMs, NULL);
    NS_ASSERTION(timerId, "SetTimer failed!");
  }

  // Setup deferred processing of native events while we wait for a response.
  NS_ASSERTION(!SyncChannel::IsPumpingMessages(),
               "Shouldn't be pumping already!");

  SyncChannel::SetIsPumpingMessages(true);
  HHOOK windowHook = SetWindowsHookEx(WH_CALLWNDPROC, CallWindowProcedureHook,
                                      NULL, gUIThreadId);
  NS_ASSERTION(windowHook, "Failed to set hook!");

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

      // This PeekMessage call will actually process all "nonqueued" messages
      // that are pending before returning. If we have "nonqueued" messages
      // pending then we should have switched out all the window procedures
      // above. In that case this PeekMessage call won't actually cause any
      // mozilla code (or plugin code) to run.

      // If the following PeekMessage call fails to return a message for us (and
      // returns false) and we didn't run any "nonqueued" messages then we must
      // have woken up for a message designated for a window in another thread.
      // If we loop immediately then we could enter a tight loop, so we'll give
      // up our time slice here to let the child process its message.
      if (!PeekMessageW(&msg, NULL, 0, 0, PM_NOREMOVE) &&
          !haveSentMessagesPending) {
        // Message was for child, we should wait a bit.
        SwitchToThread();
      }
    }
  }

  // Unhook the neutered window procedure hook.
  UnhookWindowsHookEx(windowHook);

  // Unhook any neutered windows procedures so messages can be delivered
  // normally.
  UnhookNeuteredWindows();

  // Before returning we need to set a hook to run any deferred messages that
  // we received during the IPC call. The hook will unset itself as soon as
  // someone else calls GetMessage, PeekMessage, or runs code that generates
  // a "nonqueued" message.
  ScheduleDeferredMessageRun();

  if (timerId) {
    KillTimer(NULL, timerId);
  }

  SyncChannel::SetIsPumpingMessages(false);

  return WaitResponse(timedout);
}

bool
RPCChannel::WaitForNotify()
{
  mMonitor->AssertCurrentThreadOwns();

  if (!StackDepth() && !mBlockedOnParent) {
    // There is currently no way to recover from this condition.
    NS_RUNTIMEABORT("StackDepth() is 0 in call to RPCChannel::WaitForNotify!");
  }

  // Initialize global objects used in deferred messaging.
  Init();

  NS_ASSERTION(mTopFrame && mTopFrame->mRPC,
               "Top frame is not a sync frame!");

  MonitorAutoUnlock unlock(*mMonitor);

  bool timedout = false;

  UINT_PTR timerId = NULL;
  TimeoutData timeoutData = { 0 };

  // windowHook is used as a flag variable for the loop below: if it is set
  // and we start to spin a nested event loop, we need to clear the hook and
  // process deferred/pending messages.
  // If windowHook is NULL, SyncChannel::IsPumpingMessages should be false.
  HHOOK windowHook = NULL;

  while (1) {
    NS_ASSERTION((!!windowHook) == SyncChannel::IsPumpingMessages(),
                 "windowHook out of sync with reality");

    if (mTopFrame->mSpinNestedEvents) {
      if (windowHook) {
        UnhookWindowsHookEx(windowHook);
        windowHook = NULL;

        if (timerId) {
          KillTimer(NULL, timerId);
          timerId = NULL;
        }

        // Used by widget to assert on incoming native events
        SyncChannel::SetIsPumpingMessages(false);

        // Unhook any neutered windows procedures so messages can be delievered
        // normally.
        UnhookNeuteredWindows();

        // Send all deferred "nonqueued" message to the intended receiver.
        // We're dropping into SpinInternalEventLoop so we should be fairly
        // certain these will get delivered soohn.
        ScheduleDeferredMessageRun();
      }
      SpinInternalEventLoop();
      ResetEvent(mEvent);
      return true;
    }

    if (!windowHook) {
      SyncChannel::SetIsPumpingMessages(true);
      windowHook = SetWindowsHookEx(WH_CALLWNDPROC, CallWindowProcedureHook,
                                    NULL, gUIThreadId);
      NS_ASSERTION(windowHook, "Failed to set hook!");

      NS_ASSERTION(!timerId, "Timer already initialized?");

      if (mTimeoutMs != kNoTimeout) {
        InitTimeoutData(&timeoutData, mTimeoutMs);
        timerId = SetTimer(NULL, 0, mTimeoutMs, NULL);
        NS_ASSERTION(timerId, "SetTimer failed!");
      }
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

    // See SyncChannel's WaitForNotify for details.
    bool haveSentMessagesPending =
      (HIWORD(GetQueueStatus(QS_SENDMESSAGE)) & QS_SENDMESSAGE) != 0;

    // PeekMessage markes the messages as "old" so that they don't wake up
    // MsgWaitForMultipleObjects every time.
    if (!PeekMessageW(&msg, NULL, 0, 0, PM_NOREMOVE) &&
        !haveSentMessagesPending) {
      // Message was for child, we should wait a bit.
      SwitchToThread();
    }
  }

  if (windowHook) {
    // Unhook the neutered window procedure hook.
    UnhookWindowsHookEx(windowHook);

    // Unhook any neutered windows procedures so messages can be delivered
    // normally.
    UnhookNeuteredWindows();

    // Before returning we need to set a hook to run any deferred messages that
    // we received during the IPC call. The hook will unset itself as soon as
    // someone else calls GetMessage, PeekMessage, or runs code that generates
    // a "nonqueued" message.
    ScheduleDeferredMessageRun();

    if (timerId) {
      KillTimer(NULL, timerId);
    }
  }

  SyncChannel::SetIsPumpingMessages(false);

  return WaitResponse(timedout);
}

void
SyncChannel::NotifyWorkerThread()
{
  mMonitor->AssertCurrentThreadOwns();
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
  RedrawWindow(hWnd, NULL, NULL, flags);
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
    lParamString = NULL;
    lParam = NULL;
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
      windowPos.hwndInsertAfter = NULL;
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
    copyData.lpData = NULL;
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
