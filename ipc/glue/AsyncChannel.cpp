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
#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/ipc/ProtocolUtils.h"

#include "nsDebug.h"
#include "nsTraceRefcnt.h"
#include "nsXULAppAPI.h"

using mozilla::MonitorAutoLock;

template<>
struct RunnableMethodTraits<mozilla::ipc::AsyncChannel>
{
    static void RetainCallee(mozilla::ipc::AsyncChannel* obj) { }
    static void ReleaseCallee(mozilla::ipc::AsyncChannel* obj) { }
};

// We rely on invariants about the lifetime of the transport:
//
//  - outlives this AsyncChannel
//  - deleted on the IO thread
//
// These invariants allow us to send messages directly through the
// transport without having to worry about orphaned Send() tasks on
// the IO thread touching AsyncChannel memory after it's been deleted
// on the worker thread.  We also don't need to refcount the
// Transport, because whatever task triggers its deletion only runs on
// the IO thread, and only runs after this AsyncChannel is done with
// the Transport.
template<>
struct RunnableMethodTraits<mozilla::ipc::AsyncChannel::Transport>
{
    static void RetainCallee(mozilla::ipc::AsyncChannel::Transport* obj) { }
    static void ReleaseCallee(mozilla::ipc::AsyncChannel::Transport* obj) { }
};

namespace {

// This is an async message
class GoodbyeMessage : public IPC::Message
{
public:
    enum { ID = GOODBYE_MESSAGE_TYPE };
    GoodbyeMessage() :
        IPC::Message(MSG_ROUTING_NONE, ID, PRIORITY_NORMAL)
    {
    }
    // XXX not much point in implementing this; maybe could help with
    // debugging?
    static bool Read(const Message* msg)
    {
        return true;
    }
    void Log(const std::string& aPrefix,
             FILE* aOutf) const
    {
        fputs("(special `Goodbye' message)", aOutf);
    }
};

} // namespace <anon>

namespace mozilla {
namespace ipc {

AsyncChannel::AsyncChannel(AsyncListener* aListener)
  : mTransport(0),
    mListener(aListener),
    mChannelState(ChannelClosed),
    mMonitor("mozilla.ipc.AsyncChannel.mMonitor"),
    mIOLoop(),
    mWorkerLoop(),
    mChild(false),
    mChannelErrorTask(NULL),
    mExistingListener(NULL)
{
    MOZ_COUNT_CTOR(AsyncChannel);
}

AsyncChannel::~AsyncChannel()
{
    MOZ_COUNT_DTOR(AsyncChannel);
    Clear();
}

bool
AsyncChannel::Open(Transport* aTransport, MessageLoop* aIOLoop, Side aSide)
{
    NS_PRECONDITION(!mTransport, "Open() called > once");
    NS_PRECONDITION(aTransport, "need transport layer");

    // FIXME need to check for valid channel

    mTransport = aTransport;
    mExistingListener = mTransport->set_listener(this);

    // FIXME figure out whether we're in parent or child, grab IO loop
    // appropriately
    bool needOpen = true;
    if(aIOLoop) {
        // We're a child or using the new arguments.  Either way, we
        // need an open.
        needOpen = true;
        mChild = (aSide == Unknown) || (aSide == Child);
    } else {
        NS_PRECONDITION(aSide == Unknown, "expected default side arg");

        // parent
        mChild = false;
        needOpen = false;
        aIOLoop = XRE_GetIOMessageLoop();
        // FIXME assuming that the parent waits for the OnConnected event.
        // FIXME see GeckoChildProcessHost.cpp.  bad assumption!
        mChannelState = ChannelConnected;
    }

    mIOLoop = aIOLoop;
    mWorkerLoop = MessageLoop::current();

    NS_ASSERTION(mIOLoop, "need an IO loop");
    NS_ASSERTION(mWorkerLoop, "need a worker loop");

    if (needOpen) {             // child process
        MonitorAutoLock lock(mMonitor);

        mIOLoop->PostTask(FROM_HERE, 
                          NewRunnableMethod(this,
                                            &AsyncChannel::OnChannelOpened));

        // FIXME/cjones: handle errors
        while (mChannelState != ChannelConnected) {
            mMonitor.Wait();
        }
    }

    return true;
}

void
AsyncChannel::Close()
{
    AssertWorkerThread();

    {
        MonitorAutoLock lock(mMonitor);

        if (ChannelError == mChannelState ||
            ChannelTimeout == mChannelState) {
            // See bug 538586: if the listener gets deleted while the
            // IO thread's NotifyChannelError event is still enqueued
            // and subsequently deletes us, then the error event will
            // also be deleted and the listener will never be notified
            // of the channel error.
            if (mListener) {
                MonitorAutoUnlock unlock(mMonitor);
                NotifyMaybeChannelError();
            }
            return;
        }

        if (ChannelConnected != mChannelState)
            // XXX be strict about this until there's a compelling reason
            // to relax
            NS_RUNTIMEABORT("Close() called on closed channel!");

        AssertWorkerThread();

        // notify the other side that we're about to close our socket
        SendSpecialMessage(new GoodbyeMessage());

        SynchronouslyClose();
    }

    NotifyChannelClosed();
}

void 
AsyncChannel::SynchronouslyClose()
{
    AssertWorkerThread();
    mMonitor.AssertCurrentThreadOwns();

    mIOLoop->PostTask(
        FROM_HERE, NewRunnableMethod(this, &AsyncChannel::OnCloseChannel));

    while (ChannelClosed != mChannelState)
        mMonitor.Wait();
}

bool
AsyncChannel::Send(Message* _msg)
{
    nsAutoPtr<Message> msg(_msg);
    AssertWorkerThread();
    mMonitor.AssertNotCurrentThreadOwns();
    NS_ABORT_IF_FALSE(MSG_ROUTING_NONE != msg->routing_id(), "need a route");

    {
        MonitorAutoLock lock(mMonitor);

        if (!Connected()) {
            ReportConnectionError("AsyncChannel");
            return false;
        }

        SendThroughTransport(msg.forget());
    }

    return true;
}

bool
AsyncChannel::Echo(Message* _msg)
{
    nsAutoPtr<Message> msg(_msg);
    AssertWorkerThread();
    mMonitor.AssertNotCurrentThreadOwns();
    NS_ABORT_IF_FALSE(MSG_ROUTING_NONE != msg->routing_id(), "need a route");

    {
        MonitorAutoLock lock(mMonitor);

        if (!Connected()) {
            ReportConnectionError("AsyncChannel");
            return false;
        }

        // NB: Go through this OnMessageReceived indirection so that
        // echoing this message does the right thing for SyncChannel
        // and RPCChannel too
        mIOLoop->PostTask(
            FROM_HERE,
            NewRunnableMethod(this, &AsyncChannel::OnEchoMessage, msg.forget()));
        // OnEchoMessage takes ownership of |msg|
    }

    return true;
}

void
AsyncChannel::OnDispatchMessage(const Message& msg)
{
    AssertWorkerThread();
    NS_ASSERTION(!msg.is_reply(), "can't process replies here");
    NS_ASSERTION(!(msg.is_sync() || msg.is_rpc()), "async dispatch only");

    if (MSG_ROUTING_NONE == msg.routing_id()) {
        if (!OnSpecialMessage(msg.type(), msg))
            // XXX real error handling
            NS_RUNTIMEABORT("unhandled special message!");
        return;
    }

    // it's OK to dispatch messages if the channel is closed/error'd,
    // since we don't have a reply to send back

    (void)MaybeHandleError(mListener->OnMessageReceived(msg), "AsyncChannel");
}

bool
AsyncChannel::OnSpecialMessage(uint16 id, const Message& msg)
{
    return false;
}

void
AsyncChannel::SendSpecialMessage(Message* msg) const
{
    AssertWorkerThread();
    SendThroughTransport(msg);
}

void
AsyncChannel::SendThroughTransport(Message* msg) const
{
    AssertWorkerThread();

    mIOLoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(mTransport, &Transport::Send, msg));
}

void
AsyncChannel::OnNotifyMaybeChannelError()
{
    AssertWorkerThread();
    mMonitor.AssertNotCurrentThreadOwns();

    // OnChannelError holds mMonitor when it posts this task and this
    // task cannot be allowed to run until OnChannelError has
    // exited. We enforce that order by grabbing the mutex here which
    // should only continue once OnChannelError has completed.
    {
        MonitorAutoLock lock(mMonitor);
        // nothing to do here
    }

    if (ShouldDeferNotifyMaybeError()) {
        mChannelErrorTask =
            NewRunnableMethod(this, &AsyncChannel::OnNotifyMaybeChannelError);
        // 10 ms delay is completely arbitrary
        mWorkerLoop->PostDelayedTask(FROM_HERE, mChannelErrorTask, 10);
        return;
    }

    NotifyMaybeChannelError();
}

void
AsyncChannel::NotifyChannelClosed()
{
    mMonitor.AssertNotCurrentThreadOwns();

    if (ChannelClosed != mChannelState)
        NS_RUNTIMEABORT("channel should have been closed!");

    // OK, the IO thread just closed the channel normally.  Let the
    // listener know about it.
    mListener->OnChannelClose();

    Clear();
}

void
AsyncChannel::NotifyMaybeChannelError()
{
    mMonitor.AssertNotCurrentThreadOwns();

    // TODO sort out Close() on this side racing with Close() on the
    // other side
    if (ChannelClosing == mChannelState) {
        // the channel closed, but we received a "Goodbye" message
        // warning us about it. no worries
        mChannelState = ChannelClosed;
        NotifyChannelClosed();
        return;
    }

    // Oops, error!  Let the listener know about it.
    mChannelState = ChannelError;
    mListener->OnChannelError();

    Clear();
}

void
AsyncChannel::Clear()
{
    mListener = 0;
    mIOLoop = 0;
    mWorkerLoop = 0;

    if (mTransport) {
        mTransport->set_listener(0);

        // we only hold a weak ref to the transport, which is "owned"
        // by GeckoChildProcess/GeckoThread
        mTransport = 0;
    }
    if (mChannelErrorTask) {
        mChannelErrorTask->Cancel();
        mChannelErrorTask = NULL;
    }
}

static void
PrintErrorMessage(bool isChild, const char* channelName, const char* msg)
{
#ifdef DEBUG
    fprintf(stderr, "\n###!!! [%s][%s] Error: %s\n\n",
            isChild ? "Child" : "Parent", channelName, msg);
#endif
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
    case MsgProcessingError:
        errorMsg = "Processing error: message was deserialized, but the handler returned false (indicating failure)";
        break;
    case MsgRouteError:
        errorMsg = "Route error: message sent to unknown actor ID";
        break;
    case MsgValueError:
        errorMsg = "Value error: message was deserialized, but contained an illegal value";
        break;

    default:
        NS_RUNTIMEABORT("unknown Result code");
        return false;
    }

    PrintErrorMessage(mChild, channelName, errorMsg);

    mListener->OnProcessingError(code);

    return false;
}

void
AsyncChannel::ReportConnectionError(const char* channelName) const
{
    const char* errorMsg;
    switch (mChannelState) {
    case ChannelClosed:
        errorMsg = "Closed channel: cannot send/recv";
        break;
    case ChannelOpening:
        errorMsg = "Opening channel: not yet ready for send/recv";
        break;
    case ChannelTimeout:
        errorMsg = "Channel timeout: cannot send/recv";
        break;
    case ChannelClosing:
        errorMsg = "Channel closing: too late to send/recv, messages will be lost";
        break;
    case ChannelError:
        errorMsg = "Channel error: cannot send/recv";
        break;

    default:
        NS_RUNTIMEABORT("unreached");
    }

    PrintErrorMessage(mChild, channelName, errorMsg);

    mListener->OnProcessingError(MsgDropped);
}

//
// The methods below run in the context of the IO thread
//

void
AsyncChannel::OnMessageReceived(const Message& msg)
{
    AssertIOThread();
    NS_ASSERTION(mChannelState != ChannelError, "Shouldn't get here!");

    MonitorAutoLock lock(mMonitor);

    if (!MaybeInterceptSpecialIOMessage(msg))
        // wake up the worker, there's work to do
        mWorkerLoop->PostTask(
            FROM_HERE,
            NewRunnableMethod(this, &AsyncChannel::OnDispatchMessage, msg));
}

void
AsyncChannel::OnEchoMessage(Message* msg)
{
    AssertIOThread();
    OnMessageReceived(*msg);
    delete msg;
}

void
AsyncChannel::OnChannelOpened()
{
    AssertIOThread();
    {
        MonitorAutoLock lock(mMonitor);
        mChannelState = ChannelOpening;
    }
    /*assert*/mTransport->Connect();
}

void
AsyncChannel::DispatchOnChannelConnected(int32 peer_pid)
{
    AssertWorkerThread();
    if (mListener)
        mListener->OnChannelConnected(peer_pid);
}

void
AsyncChannel::OnChannelConnected(int32 peer_pid)
{
    AssertIOThread();

    {
        MonitorAutoLock lock(mMonitor);
        mChannelState = ChannelConnected;
        mMonitor.Notify();
    }

    if(mExistingListener)
        mExistingListener->OnChannelConnected(peer_pid);

    mWorkerLoop->PostTask(FROM_HERE, NewRunnableMethod(this, 
                                                       &AsyncChannel::DispatchOnChannelConnected, 
                                                       peer_pid));
}

void
AsyncChannel::OnChannelError()
{
    AssertIOThread();

    MonitorAutoLock lock(mMonitor);

    if (ChannelClosing != mChannelState)
        mChannelState = ChannelError;

    PostErrorNotifyTask();
}

void
AsyncChannel::PostErrorNotifyTask()
{
    AssertIOThread();
    mMonitor.AssertCurrentThreadOwns();

    NS_ASSERTION(!mChannelErrorTask, "OnChannelError called twice?");

    // This must be the last code that runs on this thread!
    mChannelErrorTask =
        NewRunnableMethod(this, &AsyncChannel::OnNotifyMaybeChannelError);
    mWorkerLoop->PostTask(FROM_HERE, mChannelErrorTask);
}

void
AsyncChannel::OnCloseChannel()
{
    AssertIOThread();

    mTransport->Close();

    MonitorAutoLock lock(mMonitor);
    mChannelState = ChannelClosed;
    mMonitor.Notify();
}

bool
AsyncChannel::MaybeInterceptSpecialIOMessage(const Message& msg)
{
    AssertIOThread();
    mMonitor.AssertCurrentThreadOwns();

    if (MSG_ROUTING_NONE == msg.routing_id()
        && GOODBYE_MESSAGE_TYPE == msg.type()) {
        ProcessGoodbyeMessage();
        return true;
    }
    return false;
}

void
AsyncChannel::ProcessGoodbyeMessage()
{
    AssertIOThread();
    mMonitor.AssertCurrentThreadOwns();

    // TODO sort out Close() on this side racing with Close() on the
    // other side
    mChannelState = ChannelClosing;

    printf("NOTE: %s process received `Goodbye', closing down\n",
           mChild ? "child" : "parent");
}


} // namespace ipc
} // namespace mozilla
