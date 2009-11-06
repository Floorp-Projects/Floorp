/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Chris Jones <jones.chris.g@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/ipc/SyncChannel.h"
#include "mozilla/ipc/GeckoThread.h"

#include "nsDebug.h"

#ifdef OS_WIN
#include "nsServiceManagerUtils.h"
#include "nsIXULAppInfo.h"
#endif

using mozilla::MutexAutoLock;

template<>
struct RunnableMethodTraits<mozilla::ipc::SyncChannel>
{
    static void RetainCallee(mozilla::ipc::SyncChannel* obj) { }
    static void ReleaseCallee(mozilla::ipc::SyncChannel* obj) { }
};

namespace mozilla {
namespace ipc {

bool
SyncChannel::Send(Message* msg, Message* reply)
{
    AssertWorkerThread();
    NS_ABORT_IF_FALSE(!ProcessingSyncMessage(),
                      "violation of sync handler invariant");
    NS_ABORT_IF_FALSE(msg->is_sync(), "can only Send() sync messages here");

    MutexAutoLock lock(mMutex);

    if (!Connected()) {
        ReportConnectionError("SyncChannel");
        return false;
    }

    mPendingReply = msg->type() + 1;
    mIOLoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &SyncChannel::OnSend, msg));

    // wait for the next sync message to arrive
    WaitForNotify();

    if (!Connected()) {
        ReportConnectionError("SyncChannel");
        return false;
    }

    // we just received a synchronous message from the other side.
    // If it's not the reply we were awaiting, there's a serious
    // error: either a mistimed/malformed message or a sync in-message
    // that raced with our sync out-message.
    // (NB: IPDL prevents the latter from occuring in actor code)

    // FIXME/cjones: real error handling
    NS_ABORT_IF_FALSE(mRecvd.is_sync() && mRecvd.is_reply() &&
                      (mPendingReply == mRecvd.type() || mRecvd.is_reply_error()),
                      "unexpected sync message");

    mPendingReply = 0;
    *reply = mRecvd;

    return true;
}

void
SyncChannel::OnDispatchMessage(const Message& msg)
{
    AssertWorkerThread();
    NS_ABORT_IF_FALSE(msg.is_sync(), "only sync messages here");
    NS_ABORT_IF_FALSE(!msg.is_reply(), "wasn't awaiting reply");

    Message* reply = 0;

    mProcessingSyncMessage = true;
    Result rv =
        static_cast<SyncListener*>(mListener)->OnMessageReceived(msg, reply);
    mProcessingSyncMessage = false;

    if (!MaybeHandleError(rv, "SyncChannel")) {
        // FIXME/cjones: error handling; OnError()?
        delete reply;
        reply = new Message();
        reply->set_sync();
        reply->set_reply();
        reply->set_reply_error();
    }

    mIOLoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &SyncChannel::OnSend, reply));
}

//
// The methods below run in the context of the IO thread, and can proxy
// back to the methods above
//

void
SyncChannel::OnMessageReceived(const Message& msg)
{
    AssertIOThread();
    if (!msg.is_sync()) {
        return AsyncChannel::OnMessageReceived(msg);
    }

    MutexAutoLock lock(mMutex);

    if (!AwaitingSyncReply()) {
        // wake up the worker, there's work to do
        mWorkerLoop->PostTask(
            FROM_HERE,
            NewRunnableMethod(this, &SyncChannel::OnDispatchMessage, msg));
    }
    else {
        // let the worker know a new sync message has arrived
        mRecvd = msg;
        NotifyWorkerThread();
    }
}

void
SyncChannel::OnChannelError()
{
    AssertIOThread();
    {
        MutexAutoLock lock(mMutex);

        mChannelState = ChannelError;

        if (AwaitingSyncReply()) {
            NotifyWorkerThread();
        }
    }

    return AsyncChannel::OnChannelError();
}

//
// Synchronization between worker and IO threads
//

namespace {
bool gPumpingMessages = false;
} // anonymous namespace

// static
bool
SyncChannel::IsPumpingMessages()
{
    return gPumpingMessages;
}

#ifdef OS_WIN

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
 * queued messages during the loop, but "nonqueued" messages (see
 * http://msdn.microsoft.com/en-us/library/ms644927(VS.85).aspx under the
 * section "Nonqueued messages") cannot be avoided. Those messages are trapped
 * in a special window procedure where we can either ignore the message or
 * process it in some fashion.
 */

namespace {

UINT gEventLoopMessage =
    RegisterWindowMessage(L"SyncChannel::RunWindowsEventLoop Message");

const wchar_t kOldWndProcProp[] = L"MozillaIPCOldWndProc";

PRUnichar gAppMessageWindowName[256] = { 0 };
PRInt32 gAppMessageWindowNameLength = 0;

nsTArray<HWND>* gNeuteredWindows = nsnull;

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

    // XXX We may need more elaborate ways of faking some kinds of responses
    //     here. For now we will let DefWindowProc handle the message, but in
    //     the future we could allow the message to go to the intended wndproc
    //     (via CallWindowProc) or cheat (via some combination of ReplyMessage
    //     and PostMessage).

#ifdef DEBUG
    {
        printf("WARNING: Received nonqueued message 0x%x during a sync IPC "
               "message for window 0x%d", uMsg, hwnd);

        wchar_t className[256] = { 0 };
        if (GetClassNameW(hwnd, className, sizeof(className) - 1) > 0) {
            printf(" (\"%S\")", className);
        }

        printf(", sending it to DefWindowProc instead of the normal "
               "window procedure.\n");
    }
#endif
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static bool
WindowIsMozillaWindow(HWND hWnd)
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

    nsDependentString className(buffer, length);
    if (StringBeginsWith(className, NS_LITERAL_STRING("Mozilla")) ||
        StringBeginsWith(className, NS_LITERAL_STRING("Gecko")) ||
        className.EqualsLiteral("nsToolkitClass") ||
        className.EqualsLiteral("nsAppShell:EventWindowClass")) {
        return true;
    }

    // nsNativeAppSupport makes a window like "FirefoxMessageWindow" based on
    // the toolkit app's name. It's pretty expensive to calculate this so we
    // only try once.
    if (gAppMessageWindowNameLength == 0) {
        // This will only happen once.
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
    if (!WindowIsMozillaWindow(hWnd)) {
        // Some other kind of window, skip.
        return false;
    }

    NS_ASSERTION(!GetProp(hWnd, kOldWndProcProp),
                 "This should always be null!");

    // It's possible to get NULL out of SetWindowLongPtr, and the only way to
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
        RemoveProp(hWnd, kOldWndProcProp);
        return false;
    }

    return true;
}

void
RestoreWindowProcedure(HWND hWnd)
{
    NS_ASSERTION(WindowIsMozillaWindow(hWnd), "Not a mozilla window!");

    LONG_PTR oldWndProc = (LONG_PTR)RemoveProp(hWnd, kOldWndProcProp);
    if (oldWndProc) {
        NS_ASSERTION(oldWndProc != (LONG_PTR)NeuteredWindowProc,
                     "This shouldn't be possible!");

        LONG_PTR currentWndProc =
            SetWindowLongPtr(hWnd, GWLP_WNDPROC, oldWndProc);
        NS_ASSERTION(currentWndProc == (LONG_PTR)NeuteredWindowProc,
                     "This should never be switched out from under us!");
    }
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

} // anonymous namespace

void
SyncChannel::RunWindowsEventLoop()
{
    mMutex.AssertCurrentThreadOwns();

    NS_ASSERTION(mEventLoopDepth >= 0, "Event loop depth mismatch!");

    HHOOK windowHook = NULL;

    nsAutoTArray<HWND, 20> neuteredWindows;

    if (++mEventLoopDepth == 1) {
        NS_ASSERTION(!gPumpingMessages, "Shouldn't be pumping already!");
        gPumpingMessages = true;

        if (!mUIThreadId) {
            mUIThreadId = GetCurrentThreadId();
        }
        NS_ASSERTION(mUIThreadId, "ThreadId should not be 0!");

        NS_ASSERTION(!gNeuteredWindows, "Should only set this once!");
        gNeuteredWindows = &neuteredWindows;

        windowHook = SetWindowsHookEx(WH_CALLWNDPROC, CallWindowProcedureHook,
                                      NULL, mUIThreadId);
        NS_ASSERTION(windowHook, "Failed to set hook!");
    }

    {
        MutexAutoUnlock unlock(mMutex);

        while (1) {
            // Wait until we have a message in the queue. MSDN docs are a bit
            // unclear, but it seems that windows from two different threads
            // (and it should be noted that a thread in another process counts
            // as a "different thread") will implicitly have their message
            // queues attached if they are parented to one another. This wait
            // call, then, will return for a message delivered to *either*
            // thread.
            DWORD result = MsgWaitForMultipleObjects(0, NULL, FALSE, INFINITE,
                                                     QS_ALLINPUT);
            if (result != WAIT_OBJECT_0) {
                NS_ERROR("Wait failed!");
                break;
            }

            // The only way to know which thread the message was
            // delivered on is to use some logic on the return values of
            // GetQueueStatus and PeekMessage. PeekMessage will return false
            // if there are no "queued" messages, but it will run all
            // "nonqueued" messages before returning. So if PeekMessage
            // returns false and there are no "nonqueued" messages that were
            // run then we know that the message we woke for was intended for
            // a window on another thread.
            bool haveSentMessagesPending = 
                HIWORD(GetQueueStatus(QS_SENDMESSAGE)) & QS_SENDMESSAGE;

            // This PeekMessage call will actually process all "nonqueued"
            // messages that are pending before returning. If we have
            // "nonqueued" messages pending then we should have switched out
            // all the window procedures above. In that case this PeekMessage
            // call won't actually cause any mozilla code (or plugin code) to
            // run.

            // We check first to see if we should break out of the loop by
            // looking for the special message from the IO thread. We pull it
            // out of the queue too.
            MSG msg = { 0 };
            if (PeekMessageW(&msg, (HWND)-1, gEventLoopMessage,
                             gEventLoopMessage, PM_REMOVE)) {
                break;
            }

            // If the following PeekMessage call fails to return a message for
            // us (and returns false) and we didn't run any "nonqueued"
            // messages then we must have woken up for a message designated for
            // a window in another thread. If we loop immediately then we could
            // enter a tight loop, so we'll give up our time slice here to let
            // the child process its message.
            if (!PeekMessageW(&msg, NULL, 0, 0, PM_NOREMOVE) &&
                !haveSentMessagesPending) {
                // Message was for child, we should wait a bit.
                SwitchToThread();
            }
        }
    }

    NS_ASSERTION(mEventLoopDepth > 0, "Event loop depth mismatch!");

    if (--mEventLoopDepth == 0) {
        if (windowHook) {
            UnhookWindowsHookEx(windowHook);
        }

        NS_ASSERTION(gNeuteredWindows == &neuteredWindows, "Bad pointer!");
        gNeuteredWindows = nsnull;

        PRUint32 count = neuteredWindows.Length();
        for (PRUint32 index = 0; index < count; index++) {
            RestoreWindowProcedure(neuteredWindows[index]);
        }

        gPumpingMessages = false;
    }
}

void
SyncChannel::WaitForNotify()
{
    RunWindowsEventLoop();
}

void
SyncChannel::NotifyWorkerThread()
{
    mMutex.AssertCurrentThreadOwns();
    NS_ASSERTION(mUIThreadId, "This should have been set already!");
    if (!PostThreadMessage(mUIThreadId, gEventLoopMessage, 0, 0)) {
        NS_WARNING("Failed to post thread message!");
    }
}

//--------------------------------------------------
// Not windows
#else

void
SyncChannel::WaitForNotify()
{
    mCvar.Wait();
}

void
SyncChannel::NotifyWorkerThread()
{
    mCvar.Notify();
}

#endif  // ifdef OS_WIN


} // namespace ipc
} // namespace mozilla
