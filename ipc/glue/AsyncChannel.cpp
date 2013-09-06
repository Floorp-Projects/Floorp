/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/AsyncChannel.h"
#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/ipc/ProtocolUtils.h"

#include "nsDebug.h"
#include "nsTraceRefcnt.h"
#include "nsXULAppAPI.h"

using namespace mozilla;
using namespace std;

template<>
struct RunnableMethodTraits<mozilla::ipc::AsyncChannel>
{
    static void RetainCallee(mozilla::ipc::AsyncChannel* obj) { }
    static void ReleaseCallee(mozilla::ipc::AsyncChannel* obj) { }
};

template<>
struct RunnableMethodTraits<mozilla::ipc::AsyncChannel::ProcessLink>
{
    static void RetainCallee(mozilla::ipc::AsyncChannel::ProcessLink* obj) { }
    static void ReleaseCallee(mozilla::ipc::AsyncChannel::ProcessLink* obj) { }
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

AsyncChannel::Link::Link(AsyncChannel *aChan)
    : mChan(aChan)
{
}

AsyncChannel::Link::~Link()
{
    mChan = 0;
}

AsyncChannel::ProcessLink::ProcessLink(AsyncChannel *aChan)
    : Link(aChan)
    , mExistingListener(nullptr)
{
}

AsyncChannel::ProcessLink::~ProcessLink()
{
    mIOLoop = 0;
    if (mTransport) {
        mTransport->set_listener(0);
        
        // we only hold a weak ref to the transport, which is "owned"
        // by GeckoChildProcess/GeckoThread
        mTransport = 0;
    }
}

void 
AsyncChannel::ProcessLink::Open(mozilla::ipc::Transport* aTransport,
                                MessageLoop *aIOLoop,
                                Side aSide)
{
    NS_PRECONDITION(aTransport, "need transport layer");

    // FIXME need to check for valid channel

    mTransport = aTransport;

    // FIXME figure out whether we're in parent or child, grab IO loop
    // appropriately
    bool needOpen = true;
    if(aIOLoop) {
        // We're a child or using the new arguments.  Either way, we
        // need an open.
        needOpen = true;
        mChan->mChild = (aSide == AsyncChannel::Unknown) || (aSide == AsyncChannel::Child);
    } else {
        NS_PRECONDITION(aSide == Unknown, "expected default side arg");

        // parent
        mChan->mChild = false;
        needOpen = false;
        aIOLoop = XRE_GetIOMessageLoop();
    }

    mIOLoop = aIOLoop;

    NS_ASSERTION(mIOLoop, "need an IO loop");
    NS_ASSERTION(mChan->mWorkerLoop, "need a worker loop");

    {
        MonitorAutoLock lock(*mChan->mMonitor);

        if (needOpen) {
            // Transport::Connect() has not been called.  Call it so
            // we start polling our pipe and processing outgoing
            // messages.
            mIOLoop->PostTask(
                FROM_HERE,
                NewRunnableMethod(this, &ProcessLink::OnChannelOpened));
        } else {
            // Transport::Connect() has already been called.  Take
            // over the channel from the previous listener and process
            // any queued messages.
            mIOLoop->PostTask(
                FROM_HERE,
                NewRunnableMethod(this, &ProcessLink::OnTakeConnectedChannel));
        }

        // Should not wait here if something goes wrong with the channel.
        while (!mChan->Connected() &&
               mChan->mChannelState != AsyncChannel::ChannelError) {
            mChan->mMonitor->Wait();
        }
    }
}

void
AsyncChannel::ProcessLink::EchoMessage(Message *msg)
{
    mChan->AssertWorkerThread();
    mChan->mMonitor->AssertCurrentThreadOwns();

    // NB: Go through this OnMessageReceived indirection so that
    // echoing this message does the right thing for SyncChannel
    // and RPCChannel too
    mIOLoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ProcessLink::OnEchoMessage, msg));
    // OnEchoMessage takes ownership of |msg|
}

void
AsyncChannel::ProcessLink::SendMessage(Message *msg)
{
    mChan->AssertWorkerThread();
    mChan->mMonitor->AssertCurrentThreadOwns();

    mIOLoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(mTransport, &Transport::Send, msg));
}

void
AsyncChannel::ProcessLink::SendClose()
{
    mChan->AssertWorkerThread();
    mChan->mMonitor->AssertCurrentThreadOwns();

    mIOLoop->PostTask(
        FROM_HERE, NewRunnableMethod(this, &ProcessLink::OnCloseChannel));
}

AsyncChannel::ThreadLink::ThreadLink(AsyncChannel *aChan,
                                     AsyncChannel *aTargetChan)
    : Link(aChan),
      mTargetChan(aTargetChan)
{
}

AsyncChannel::ThreadLink::~ThreadLink()
{
    MonitorAutoLock lock(*mChan->mMonitor);
    // Bug 848949: We need to prevent the other side
    // from sending us any more messages to avoid Use-After-Free.
    // The setup here is as shown:
    //
    //          (Us)         (Them)
    //       AsyncChannel  AsyncChannel
    //         |  ^     \ /     ^ |
    //         |  |      X      | |
    //         v  |     / \     | v
    //        ThreadLink   ThreadLink
    //
    // We want to null out the diagonal link from their ThreadLink
    // to our AsyncChannel.  Note that we must hold the monitor so
    // that we do this atomically with respect to them trying to send
    // us a message.
    if (mTargetChan) {
        static_cast<ThreadLink*>(mTargetChan->mLink)->mTargetChan = 0;
    }
    mTargetChan = 0;
}

void
AsyncChannel::ThreadLink::EchoMessage(Message *msg)
{
    mChan->AssertWorkerThread();
    mChan->mMonitor->AssertCurrentThreadOwns();

    mChan->OnMessageReceivedFromLink(*msg);
    delete msg;
}

void
AsyncChannel::ThreadLink::SendMessage(Message *msg)
{
    mChan->AssertWorkerThread();
    mChan->mMonitor->AssertCurrentThreadOwns();

    if (mTargetChan)
        mTargetChan->OnMessageReceivedFromLink(*msg);
    delete msg;
}

void
AsyncChannel::ThreadLink::SendClose()
{
    mChan->AssertWorkerThread();
    mChan->mMonitor->AssertCurrentThreadOwns();

    mChan->mChannelState = ChannelClosed;

    // In a ProcessLink, we would close our half the channel.  This
    // would show up on the other side as an error on the I/O thread.
    // The I/O thread would then invoke OnChannelErrorFromLink().
    // As usual, we skip that process and just invoke the
    // OnChannelErrorFromLink() method directly.
    if (mTargetChan)
        mTargetChan->OnChannelErrorFromLink();
}

bool
AsyncChannel::ThreadLink::Unsound_IsClosed() const
{
    MonitorAutoLock lock(*mChan->mMonitor);
    return mChan->mChannelState == ChannelClosed;
}

uint32_t
AsyncChannel::ThreadLink::Unsound_NumQueuedMessages() const
{
    // ThreadLinks don't have a message queue.
    return 0;
}

AsyncChannel::AsyncChannel(AsyncListener* aListener)
  : mListener(aListener->asWeakPtr()),
    mChannelState(ChannelClosed),
    mWorkerLoop(),
    mChild(false),
    mChannelErrorTask(nullptr),
    mLink(nullptr),
    mWorkerLoopID(-1)
{
    MOZ_COUNT_CTOR(AsyncChannel);
}

AsyncChannel::~AsyncChannel()
{
    MOZ_COUNT_DTOR(AsyncChannel);
    Clear();
}

bool
AsyncChannel::Open(Transport* aTransport,
                   MessageLoop* aIOLoop,
                   AsyncChannel::Side aSide)
{
    ProcessLink *link;
    NS_PRECONDITION(!mLink, "Open() called > once");
    mMonitor = new RefCountedMonitor();
    mWorkerLoop = MessageLoop::current();
    mWorkerLoopID = mWorkerLoop->id();
    mLink = link = new ProcessLink(this);
    link->Open(aTransport, aIOLoop, aSide); // n.b.: sets mChild
    return true;
}

/* Opens a connection to another thread in the same process.

   This handshake proceeds as follows:
   - Let A be the thread initiating the process (either child or parent)
     and B be the other thread.
   - A spawns thread for B, obtaining B's message loop
   - A creates ProtocolChild and ProtocolParent instances.
     Let PA be the one appropriate to A and PB the side for B.
   - A invokes PA->Open(PB, ...):
     - set state to mChannelOpening
     - this will place a work item in B's worker loop (see next bullet)
       and then spins until PB->mChannelState becomes mChannelConnected
     - meanwhile, on PB's worker loop, the work item is removed and:
       - invokes PB->SlaveOpen(PA, ...):
         - sets its state and that of PA to Connected
 */
bool
AsyncChannel::Open(AsyncChannel *aTargetChan, 
                   MessageLoop *aTargetLoop,
                   AsyncChannel::Side aSide)
{
    NS_PRECONDITION(aTargetChan, "Need a target channel");
    NS_PRECONDITION(ChannelClosed == mChannelState, "Not currently closed");

    CommonThreadOpenInit(aTargetChan, aSide);

    Side oppSide = Unknown;
    switch(aSide) {
      case Child: oppSide = Parent; break;
      case Parent: oppSide = Child; break;
      case Unknown: break;
    }

    mMonitor = new RefCountedMonitor();

    MonitorAutoLock lock(*mMonitor);
    mChannelState = ChannelOpening;
    aTargetLoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(aTargetChan, &AsyncChannel::OnOpenAsSlave,
                          this, oppSide));

    while (ChannelOpening == mChannelState)
        mMonitor->Wait();
    NS_ASSERTION(ChannelConnected == mChannelState, "not connected when awoken");
    return (ChannelConnected == mChannelState);
}

void 
AsyncChannel::CommonThreadOpenInit(AsyncChannel *aTargetChan, Side aSide)
{
    mWorkerLoop = MessageLoop::current();
    mWorkerLoopID = mWorkerLoop->id();
    mLink = new ThreadLink(this, aTargetChan);
    mChild = (aSide == Child); 
}

// Invoked when the other side has begun the open.
void
AsyncChannel::OnOpenAsSlave(AsyncChannel *aTargetChan, Side aSide)
{
    NS_PRECONDITION(ChannelClosed == mChannelState, 
                    "Not currently closed");
    NS_PRECONDITION(ChannelOpening == aTargetChan->mChannelState,
                    "Target channel not in the process of opening");
    
    CommonThreadOpenInit(aTargetChan, aSide);
    mMonitor = aTargetChan->mMonitor;

    MonitorAutoLock lock(*mMonitor);
    NS_ASSERTION(ChannelOpening == aTargetChan->mChannelState,
                 "Target channel not in the process of opening");
    mChannelState = ChannelConnected;
    aTargetChan->mChannelState = ChannelConnected;
    aTargetChan->mMonitor->Notify();
}

void
AsyncChannel::Close()
{
    AssertWorkerThread();

    {
        MonitorAutoLock lock(*mMonitor);

        if (ChannelError == mChannelState ||
            ChannelTimeout == mChannelState) {
            // See bug 538586: if the listener gets deleted while the
            // IO thread's NotifyChannelError event is still enqueued
            // and subsequently deletes us, then the error event will
            // also be deleted and the listener will never be notified
            // of the channel error.
            if (mListener) {
                MonitorAutoUnlock unlock(*mMonitor);
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
    mMonitor->AssertCurrentThreadOwns();
    mLink->SendClose();
    while (ChannelClosed != mChannelState)
        mMonitor->Wait();
}

bool
AsyncChannel::Send(Message* _msg)
{
    nsAutoPtr<Message> msg(_msg);
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();
    NS_ABORT_IF_FALSE(MSG_ROUTING_NONE != msg->routing_id(), "need a route");

    {
        MonitorAutoLock lock(*mMonitor);

        if (!Connected()) {
            ReportConnectionError("AsyncChannel");
            return false;
        }

        mLink->SendMessage(msg.forget());
    }

    return true;
}

bool
AsyncChannel::Echo(Message* _msg)
{
    nsAutoPtr<Message> msg(_msg);
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();
    NS_ABORT_IF_FALSE(MSG_ROUTING_NONE != msg->routing_id(), "need a route");

    {
        MonitorAutoLock lock(*mMonitor);

        if (!Connected()) {
            ReportConnectionError("AsyncChannel");
            return false;
        }

        mLink->EchoMessage(msg.forget());
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
AsyncChannel::OnSpecialMessage(uint16_t id, const Message& msg)
{
    return false;
}

void
AsyncChannel::SendSpecialMessage(Message* msg) const
{
    AssertWorkerThread();
    mLink->SendMessage(msg);
}

void
AsyncChannel::OnNotifyMaybeChannelError()
{
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();

    mChannelErrorTask = nullptr;

    // OnChannelError holds mMonitor when it posts this task and this
    // task cannot be allowed to run until OnChannelError has
    // exited. We enforce that order by grabbing the mutex here which
    // should only continue once OnChannelError has completed.
    {
        MonitorAutoLock lock(*mMonitor);
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
    mMonitor->AssertNotCurrentThreadOwns();

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
    mMonitor->AssertNotCurrentThreadOwns();

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
    // Don't clear mWorkerLoopID; we use it in AssertLinkThread() and
    // AssertWorkerThread().
    //
    // Also don't clear mListener.  If we clear it, then sending a message
    // through this channel after it's Clear()'ed can cause this process to
    // crash.
    //
    // In practice, mListener owns the channel, so the channel gets deleted
    // before mListener.  But just to be safe, mListener is a weak pointer.

    mWorkerLoop = 0;

    delete mLink;
    mLink = 0;

    if (mChannelErrorTask) {
        mChannelErrorTask->Cancel();
        mChannelErrorTask = nullptr;
    }
}

static void
PrintErrorMessage(bool isChild, const char* channelName, const char* msg)
{
    printf_stderr("\n###!!! [%s][%s] Error: %s\n\n",
                  isChild ? "Child" : "Parent", channelName, msg);
}

bool
AsyncChannel::MaybeHandleError(Result code, const char* channelName)
{
    if (MsgProcessed == code)
        return true;

    const char* errorMsg = nullptr;
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
    const char* errorMsg = nullptr;
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

void
AsyncChannel::DispatchOnChannelConnected(int32_t peer_pid)
{
    AssertWorkerThread();
    if (mListener)
        mListener->OnChannelConnected(peer_pid);
}

bool
AsyncChannel::Unsound_IsClosed() const
{
    if (!mLink) {
        return true;
    }

    return mLink->Unsound_IsClosed();
}

uint32_t
AsyncChannel::Unsound_NumQueuedMessages() const
{
    if (!mLink) {
        return 0;
    }

    return mLink->Unsound_NumQueuedMessages();
}

//
// The methods below run in the context of the IO thread
//

void
AsyncChannel::ProcessLink::OnMessageReceived(const Message& msg)
{
    AssertIOThread();
    NS_ASSERTION(mChan->mChannelState != ChannelError, "Shouldn't get here!");
    MonitorAutoLock lock(*mChan->mMonitor);
    mChan->OnMessageReceivedFromLink(msg);
}

void
AsyncChannel::ProcessLink::OnEchoMessage(Message* msg)
{
    AssertIOThread();
    OnMessageReceived(*msg);
    delete msg;
}

void
AsyncChannel::ProcessLink::OnChannelOpened()
{
    mChan->AssertLinkThread();
    {
        MonitorAutoLock lock(*mChan->mMonitor);

        mExistingListener = mTransport->set_listener(this);
#ifdef DEBUG
        if (mExistingListener) {
            queue<Message> pending;
            mExistingListener->GetQueuedMessages(pending);
            MOZ_ASSERT(pending.empty());
        }
#endif  // DEBUG

        mChan->mChannelState = ChannelOpening;
        lock.Notify();
    }
    /*assert*/mTransport->Connect();
}

void
AsyncChannel::ProcessLink::OnTakeConnectedChannel()
{
    AssertIOThread();

    queue<Message> pending;
    {
        MonitorAutoLock lock(*mChan->mMonitor);

        mChan->mChannelState = ChannelConnected;

        mExistingListener = mTransport->set_listener(this);
        if (mExistingListener) {
            mExistingListener->GetQueuedMessages(pending);
        }
        lock.Notify();
    }

    // Dispatch whatever messages the previous listener had queued up.
    while (!pending.empty()) {
        OnMessageReceived(pending.front());
        pending.pop();
    }
}

void
AsyncChannel::ProcessLink::OnChannelConnected(int32_t peer_pid)
{
    AssertIOThread();

    {
        MonitorAutoLock lock(*mChan->mMonitor);
        mChan->mChannelState = ChannelConnected;
        mChan->mMonitor->Notify();
    }

    if(mExistingListener)
        mExistingListener->OnChannelConnected(peer_pid);

    mChan->mWorkerLoop->PostTask(
        FROM_HERE, 
        NewRunnableMethod(mChan, 
                          &AsyncChannel::DispatchOnChannelConnected, 
                          peer_pid));
}

void
AsyncChannel::ProcessLink::OnChannelError()
{
    AssertIOThread();
    MonitorAutoLock lock(*mChan->mMonitor);
    mChan->OnChannelErrorFromLink();
}

void
AsyncChannel::ProcessLink::OnCloseChannel()
{
    AssertIOThread();

    mTransport->Close();

    MonitorAutoLock lock(*mChan->mMonitor);
    mChan->mChannelState = ChannelClosed;
    mChan->mMonitor->Notify();
}

bool
AsyncChannel::ProcessLink::Unsound_IsClosed() const
{
    return mTransport->Unsound_IsClosed();
}

uint32_t
AsyncChannel::ProcessLink::Unsound_NumQueuedMessages() const
{
    return mTransport->Unsound_NumQueuedMessages();
}

//
// The methods below run in the context of the link thread
//

void
AsyncChannel::OnMessageReceivedFromLink(const Message& msg)
{
    AssertLinkThread();
    mMonitor->AssertCurrentThreadOwns();

    if (!MaybeInterceptSpecialIOMessage(msg))
        // wake up the worker, there's work to do
        mWorkerLoop->PostTask(
            FROM_HERE,
            NewRunnableMethod(this, &AsyncChannel::OnDispatchMessage, msg));
}

void
AsyncChannel::OnChannelErrorFromLink()
{
    AssertLinkThread();
    mMonitor->AssertCurrentThreadOwns();

    if (ChannelClosing != mChannelState) {
        mChannelState = ChannelError;
        mMonitor->Notify();
    }

    PostErrorNotifyTask();
}

void
AsyncChannel::CloseWithError()
{
    AssertWorkerThread();

    MonitorAutoLock lock(*mMonitor);
    if (ChannelConnected != mChannelState) {
        return;
    }
    SynchronouslyClose();
    mChannelState = ChannelError;
    PostErrorNotifyTask();
}

void
AsyncChannel::PostErrorNotifyTask()
{
    mMonitor->AssertCurrentThreadOwns();

    if (mChannelErrorTask)
        return;

    // This must be the last code that runs on this thread!
    mChannelErrorTask =
        NewRunnableMethod(this, &AsyncChannel::OnNotifyMaybeChannelError);
    mWorkerLoop->PostTask(FROM_HERE, mChannelErrorTask);
}

bool
AsyncChannel::MaybeInterceptSpecialIOMessage(const Message& msg)
{
    AssertLinkThread();
    mMonitor->AssertCurrentThreadOwns();

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
    AssertLinkThread();
    mMonitor->AssertCurrentThreadOwns();

    // TODO sort out Close() on this side racing with Close() on the
    // other side
    mChannelState = ChannelClosing;
}


} // namespace ipc
} // namespace mozilla
