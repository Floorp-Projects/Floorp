/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/MessageLink.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/ipc/ProtocolUtils.h"

#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#include "mozilla/Preferences.h"
#endif

#include "nsDebug.h"
#include "nsISupportsImpl.h"
#include "nsXULAppAPI.h"

using namespace mozilla;
using namespace std;

template<>
struct RunnableMethodTraits<mozilla::ipc::ProcessLink>
{
    static void RetainCallee(mozilla::ipc::ProcessLink* obj) { }
    static void ReleaseCallee(mozilla::ipc::ProcessLink* obj) { }
};

// We rely on invariants about the lifetime of the transport:
//
//  - outlives this MessageChannel
//  - deleted on the IO thread
//
// These invariants allow us to send messages directly through the
// transport without having to worry about orphaned Send() tasks on
// the IO thread touching MessageChannel memory after it's been deleted
// on the worker thread.  We also don't need to refcount the
// Transport, because whatever task triggers its deletion only runs on
// the IO thread, and only runs after this MessageChannel is done with
// the Transport.
template<>
struct RunnableMethodTraits<mozilla::ipc::MessageChannel::Transport>
{
    static void RetainCallee(mozilla::ipc::MessageChannel::Transport* obj) { }
    static void ReleaseCallee(mozilla::ipc::MessageChannel::Transport* obj) { }
};

namespace mozilla {
namespace ipc {

MessageLink::MessageLink(MessageChannel *aChan)
  : mChan(aChan)
{
}

MessageLink::~MessageLink()
{
    mChan = nullptr;
}

ProcessLink::ProcessLink(MessageChannel *aChan)
  : MessageLink(aChan),
    mExistingListener(nullptr)
{
}

ProcessLink::~ProcessLink()
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
ProcessLink::Open(mozilla::ipc::Transport* aTransport, MessageLoop *aIOLoop, Side aSide)
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
        mChan->mSide = (aSide == UnknownSide) ? ChildSide : aSide;
    } else {
        NS_PRECONDITION(aSide == UnknownSide, "expected default side arg");

        // parent
        mChan->mSide = ParentSide;
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

#ifdef MOZ_NUWA_PROCESS
        if (IsNuwaProcess() &&
            Preferences::GetBool("dom.ipc.processPrelaunch.testMode")) {
            // The pref value is turned on in a deadlock test against the Nuwa
            // process. The sleep here makes it easy to trigger the deadlock
            // that an IPC channel is still opening but the worker loop is
            // already frozen.
            sleep(5);
        }
#endif

        // Should not wait here if something goes wrong with the channel.
        while (!mChan->Connected() && mChan->mChannelState != ChannelError) {
            mChan->mMonitor->Wait();
        }
    }
}

void
ProcessLink::EchoMessage(Message *msg)
{
    mChan->AssertWorkerThread();
    mChan->mMonitor->AssertCurrentThreadOwns();

    mIOLoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ProcessLink::OnEchoMessage, msg));
    // OnEchoMessage takes ownership of |msg|
}

void
ProcessLink::SendMessage(Message *msg)
{
    mChan->AssertWorkerThread();
    mChan->mMonitor->AssertCurrentThreadOwns();

    mIOLoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(mTransport, &Transport::Send, msg));
}

void
ProcessLink::SendClose()
{
    mChan->AssertWorkerThread();
    mChan->mMonitor->AssertCurrentThreadOwns();

    mIOLoop->PostTask(
        FROM_HERE, NewRunnableMethod(this, &ProcessLink::OnCloseChannel));
}

ThreadLink::ThreadLink(MessageChannel *aChan, MessageChannel *aTargetChan)
  : MessageLink(aChan),
    mTargetChan(aTargetChan)
{
}

ThreadLink::~ThreadLink()
{
    MOZ_ASSERT(mChan);
    MOZ_ASSERT(mChan->mMonitor);
    MonitorAutoLock lock(*mChan->mMonitor);

    // Bug 848949: We need to prevent the other side
    // from sending us any more messages to avoid Use-After-Free.
    // The setup here is as shown:
    //
    //          (Us)         (Them)
    //       MessageChannel  MessageChannel
    //         |  ^     \ /     ^ |
    //         |  |      X      | |
    //         v  |     / \     | v
    //        ThreadLink   ThreadLink
    //
    // We want to null out the diagonal link from their ThreadLink
    // to our MessageChannel.  Note that we must hold the monitor so
    // that we do this atomically with respect to them trying to send
    // us a message.  Since the channels share the same monitor this
    // also protects against the two ~ThreadLink() calls racing.
    if (mTargetChan) {
        MOZ_ASSERT(mTargetChan->mLink);
        static_cast<ThreadLink*>(mTargetChan->mLink)->mTargetChan = nullptr;
    }
    mTargetChan = nullptr;
}

void
ThreadLink::EchoMessage(Message *msg)
{
    mChan->AssertWorkerThread();
    mChan->mMonitor->AssertCurrentThreadOwns();

    mChan->OnMessageReceivedFromLink(*msg);
    delete msg;
}

void
ThreadLink::SendMessage(Message *msg)
{
    mChan->AssertWorkerThread();
    mChan->mMonitor->AssertCurrentThreadOwns();

    if (mTargetChan)
        mTargetChan->OnMessageReceivedFromLink(*msg);
    delete msg;
}

void
ThreadLink::SendClose()
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
ThreadLink::Unsound_IsClosed() const
{
    MonitorAutoLock lock(*mChan->mMonitor);
    return mChan->mChannelState == ChannelClosed;
}

uint32_t
ThreadLink::Unsound_NumQueuedMessages() const
{
    // ThreadLinks don't have a message queue.
    return 0;
}

//
// The methods below run in the context of the IO thread
//

void
ProcessLink::OnMessageReceived(const Message& msg)
{
    AssertIOThread();
    NS_ASSERTION(mChan->mChannelState != ChannelError, "Shouldn't get here!");
    MonitorAutoLock lock(*mChan->mMonitor);
    mChan->OnMessageReceivedFromLink(msg);
}

void
ProcessLink::OnEchoMessage(Message* msg)
{
    AssertIOThread();
    OnMessageReceived(*msg);
    delete msg;
}

void
ProcessLink::OnChannelOpened()
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
ProcessLink::OnTakeConnectedChannel()
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
ProcessLink::OnChannelConnected(int32_t peer_pid)
{
    AssertIOThread();

    {
        MonitorAutoLock lock(*mChan->mMonitor);
        mChan->mChannelState = ChannelConnected;
        mChan->mMonitor->Notify();
    }

    if (mExistingListener)
        mExistingListener->OnChannelConnected(peer_pid);

    mChan->OnChannelConnected(peer_pid);
}

void
ProcessLink::OnChannelError()
{
    AssertIOThread();
    MonitorAutoLock lock(*mChan->mMonitor);
    mChan->OnChannelErrorFromLink();
}

void
ProcessLink::OnCloseChannel()
{
    AssertIOThread();

    mTransport->Close();

    MonitorAutoLock lock(*mChan->mMonitor);
    mChan->mChannelState = ChannelClosed;
    mChan->mMonitor->Notify();
}

bool
ProcessLink::Unsound_IsClosed() const
{
    return mTransport->Unsound_IsClosed();
}

uint32_t
ProcessLink::Unsound_NumQueuedMessages() const
{
    return mTransport->Unsound_NumQueuedMessages();
}

} // namespace ipc
} // namespace mozilla
