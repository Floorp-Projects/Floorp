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

#include "mozilla/ipc/AsyncChannel.h"
#include "mozilla/ipc/GeckoThread.h"

#include "nsDebug.h"
#include "nsTraceRefcnt.h"
#include "nsXULAppAPI.h"

using mozilla::MutexAutoLock;

template<>
struct RunnableMethodTraits<mozilla::ipc::AsyncChannel>
{
    static void RetainCallee(mozilla::ipc::AsyncChannel* obj) { }
    static void ReleaseCallee(mozilla::ipc::AsyncChannel* obj) { }
};

namespace mozilla {
namespace ipc {

AsyncChannel::AsyncChannel(AsyncListener* aListener)
  : mTransport(0),
    mListener(aListener),
    mChannelState(ChannelClosed),
    mMutex("mozilla.ipc.AsyncChannel.mMutex"),
    mCvar(mMutex, "mozilla.ipc.AsyncChannel.mCvar"),
    mIOLoop(),
    mWorkerLoop()
{
    MOZ_COUNT_CTOR(AsyncChannel);
}

AsyncChannel::~AsyncChannel()
{
    MOZ_COUNT_DTOR(AsyncChannel);
    if (!mChild && mTransport)
        Close();
    // we only hold a weak ref to the transport, which is "owned"
    // by GeckoChildProcess/GeckoThread
    mTransport = 0;
}

bool
AsyncChannel::Open(Transport* aTransport, MessageLoop* aIOLoop)
{
    NS_PRECONDITION(!mTransport, "Open() called > once");
    NS_PRECONDITION(aTransport, "need transport layer");

    // FIXME need to check for valid channel

    mTransport = aTransport;
    mTransport->set_listener(this);

    // FIXME figure out whether we're in parent or child, grab IO loop
    // appropriately
    bool needOpen = true;
    if(!aIOLoop) {
        // parent
        needOpen = false;
        aIOLoop = BrowserProcessSubThread
                  ::GetMessageLoop(BrowserProcessSubThread::IO);
        // FIXME assuming that the parent waits for the OnConnected event.
        // FIXME see GeckoChildProcessHost.cpp.  bad assumption!
        mChannelState = ChannelConnected;
    }

    mChild = needOpen;

    mIOLoop = aIOLoop;
    mWorkerLoop = MessageLoop::current();

    NS_ASSERTION(mIOLoop, "need an IO loop");
    NS_ASSERTION(mWorkerLoop, "need a worker loop");

    if (needOpen) {             // child process
        MutexAutoLock lock(mMutex);

        mIOLoop->PostTask(FROM_HERE, 
                          NewRunnableMethod(this,
                                            &AsyncChannel::OnChannelOpened));

        // FIXME/cjones: handle errors
        while (mChannelState != ChannelConnected) {
            mCvar.Wait();
        }
    }

    return true;
}

void
AsyncChannel::Close()
{
    MutexAutoLock lock(mMutex);

    if (!mChild && ChannelConnected == mChannelState) {
        AssertWorkerThread();

        mIOLoop->PostTask(
            FROM_HERE, NewRunnableMethod(this, &AsyncChannel::OnClose));

        while (ChannelConnected == mChannelState)
            mCvar.Wait();
    }

    mTransport = NULL;
}

bool
AsyncChannel::Send(Message* msg)
{
    AssertWorkerThread();
    mMutex.AssertNotCurrentThreadOwns();
    NS_ABORT_IF_FALSE(MSG_ROUTING_NONE != msg->routing_id(), "need a route");

    {
        MutexAutoLock lock(mMutex);

        if (!Connected()) {
            ReportConnectionError("AsyncChannel");
            return false;
        }

        mIOLoop->PostTask(FROM_HERE,
                          NewRunnableMethod(this, &AsyncChannel::OnSend, msg));
    }

    return true;
}

void
AsyncChannel::OnDispatchMessage(const Message& msg)
{
    AssertWorkerThread();
    NS_ASSERTION(!msg.is_reply(), "can't process replies here");
    NS_ASSERTION(!(msg.is_sync() || msg.is_rpc()), "async dispatch only");

    (void)MaybeHandleError(mListener->OnMessageReceived(msg), "AsyncChannel");
}

bool
AsyncChannel::MaybeHandleError(Result code, const char* channelName)
{
    if (MsgProcessed == code)
        return true;

    const char* errorMsg;
    switch (code) {
    case MsgNotKnown:
        errorMsg = "Unknown message: not processed";
        break;
    case MsgNotAllowed:
        errorMsg = "Message not allowed: cannot be sent/recvd in this state";
        break;
    case MsgPayloadError:
        errorMsg = "Payload error: message could not be deserialized";
        break;
    case MsgRouteError:
        errorMsg = "Route error: message sent to unknown actor ID";
        break;
    case MsgValueError:
        errorMsg = "Value error: message was deserialized, but contained an illegal value";
        break;

    default:
        NOTREACHED();
        return false;
    }

    PrintErrorMessage(channelName, errorMsg);
    return false;
}

void
AsyncChannel::ReportConnectionError(const char* channelName)
{
    const char* errorMsg;
    switch (mChannelState) {
    case ChannelClosed:
        errorMsg = "Closed channel: cannot send/recv";
        break;
    case ChannelOpening:
        errorMsg = "Opening channel: not yet ready for send/recv";
        break;
    case ChannelError:
        errorMsg = "Channel error: cannot send/recv";
        break;

    default:
        NOTREACHED();
    }

    PrintErrorMessage(channelName, errorMsg);
}

//
// The methods below run in the context of the IO thread, and can proxy
// back to the methods above
//

void
AsyncChannel::OnMessageReceived(const Message& msg)
{
    AssertIOThread();
    NS_ASSERTION(mChannelState != ChannelError, "Shouldn't get here!");

    // wake up the worker, there's work to do
    mWorkerLoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &AsyncChannel::OnDispatchMessage, msg));
}

void
AsyncChannel::OnChannelConnected(int32 peer_pid)
{
    AssertIOThread();

    MutexAutoLock lock(mMutex);
    mChannelState = ChannelConnected;
    mCvar.Notify();
}

void
AsyncChannel::OnChannelError()
{
    AssertIOThread();

    {
        MutexAutoLock lock(mMutex);
        mChannelState = ChannelError;
    }

    if (XRE_GetProcessType() == GeckoProcessType_Default) {
        // Parent process, one of our children died. Notify?
    }
    else {
        // Child process, initiate quit sequence.
#ifdef DEBUG
        // XXXbent this is totally out of place, but works for now.
        XRE_ShutdownChildProcess(mWorkerLoop);

        // Must exit the IO loop, which will then join with the UI loop.
        MessageLoop::current()->Quit();
#else
        // FIXME need to devote some thought to the most
        // effective/least easily overrideable, yet quiet, way to
        // exit.  abort() is a little loud
        _exit(0);
#endif
    }
}

void
AsyncChannel::OnChannelOpened()
{
    AssertIOThread();
    mChannelState = ChannelOpening;
    /*assert*/mTransport->Connect();
}

void
AsyncChannel::OnSend(Message* aMsg)
{
    AssertIOThread();
    mTransport->Send(aMsg);
    // mTransport deletes aMsg
}

void
AsyncChannel::OnClose()
{
    AssertIOThread();

    mTransport->Close();

    // don't lose error-state information
    if (ChannelError != mChannelState)
        mChannelState = ChannelClosed;

    if (!mChild) {
        MutexAutoLock lock(mMutex);
        mCvar.Notify();
    }
}


} // namespace ipc
} // namespace mozilla
