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

#include "mozilla/ipc/RPCChannel.h"
#include "mozilla/ipc/ProtocolUtils.h"

#include "nsDebug.h"
#include "nsTraceRefcnt.h"

#define RPC_ASSERT(_cond, ...)                                      \
    do {                                                            \
        if (!(_cond))                                               \
            DebugAbort(__FILE__, __LINE__, #_cond,## __VA_ARGS__);  \
    } while (0)

using mozilla::MonitorAutoLock;
using mozilla::MonitorAutoUnlock;

template<>
struct RunnableMethodTraits<mozilla::ipc::RPCChannel>
{
    static void RetainCallee(mozilla::ipc::RPCChannel* obj) { }
    static void ReleaseCallee(mozilla::ipc::RPCChannel* obj) { }
};


namespace
{

// Async (from the sending side's perspective)
class BlockChildMessage : public IPC::Message
{
public:
    enum { ID = BLOCK_CHILD_MESSAGE_TYPE };
    BlockChildMessage() :
        Message(MSG_ROUTING_NONE, ID, IPC::Message::PRIORITY_NORMAL)
    { }
};

// Async
class UnblockChildMessage : public IPC::Message
{
public:
    enum { ID = UNBLOCK_CHILD_MESSAGE_TYPE };
    UnblockChildMessage() :
        Message(MSG_ROUTING_NONE, ID, IPC::Message::PRIORITY_NORMAL)
    { }
};

} // namespace <anon>


namespace mozilla {
namespace ipc {

RPCChannel::RPCChannel(RPCListener* aListener)
  : SyncChannel(aListener),
    mPending(),
    mStack(),
    mOutOfTurnReplies(),
    mDeferred(),
    mRemoteStackDepthGuess(0),
    mBlockedOnParent(false),
    mSawRPCOutMsg(false)
{
    MOZ_COUNT_CTOR(RPCChannel);

    mDequeueOneTask = new RefCountedTask(NewRunnableMethod(
                                                 this,
                                                 &RPCChannel::OnMaybeDequeueOne));
}

RPCChannel::~RPCChannel()
{
    MOZ_COUNT_DTOR(RPCChannel);
    RPC_ASSERT(mCxxStackFrames.empty(), "mismatched CxxStackFrame ctor/dtors");
}

void
RPCChannel::Clear()
{
    mDequeueOneTask->Cancel();

    AsyncChannel::Clear();
}

bool
RPCChannel::EventOccurred() const
{
    AssertWorkerThread();
    mMonitor.AssertCurrentThreadOwns();
    RPC_ASSERT(StackDepth() > 0, "not in wait loop");

    return (!Connected() ||
            !mPending.empty() ||
            (!mOutOfTurnReplies.empty() &&
             mOutOfTurnReplies.find(mStack.top().seqno())
             != mOutOfTurnReplies.end()));
}

bool
RPCChannel::Send(Message* msg)
{
    Message copy = *msg;
    CxxStackFrame f(*this, OUT_MESSAGE, &copy);
    return AsyncChannel::Send(msg);
}

bool
RPCChannel::Send(Message* msg, Message* reply)
{
    Message copy = *msg;
    CxxStackFrame f(*this, OUT_MESSAGE, &copy);
    return SyncChannel::Send(msg, reply);
}

bool
RPCChannel::Call(Message* _msg, Message* reply)
{
    nsAutoPtr<Message> msg(_msg);
    AssertWorkerThread();
    mMonitor.AssertNotCurrentThreadOwns();
    RPC_ASSERT(!ProcessingSyncMessage(),
               "violation of sync handler invariant");
    RPC_ASSERT(msg->is_rpc(), "can only Call() RPC messages here");

#ifdef OS_WIN
    SyncStackFrame frame(this, true);
#endif

    Message copy = *msg;
    CxxStackFrame f(*this, OUT_MESSAGE, &copy);

    MonitorAutoLock lock(mMonitor);

    if (!Connected()) {
        ReportConnectionError("RPCChannel");
        return false;
    }

    msg->set_seqno(NextSeqno());
    msg->set_rpc_remote_stack_depth_guess(mRemoteStackDepthGuess);
    msg->set_rpc_local_stack_depth(1 + StackDepth());
    mStack.push(*msg);

    SendThroughTransport(msg.forget());

    while (1) {
        // if a handler invoked by *Dispatch*() spun a nested event
        // loop, and the connection was broken during that loop, we
        // might have already processed the OnError event. if so,
        // trying another loop iteration will be futile because
        // channel state will have been cleared
        if (!Connected()) {
            ReportConnectionError("RPCChannel");
            return false;
        }

        // now might be the time to process a message deferred because
        // of race resolution
        MaybeUndeferIncall();

        // here we're waiting for something to happen. see long
        // comment about the queue in RPCChannel.h
        while (!EventOccurred()) {
            bool maybeTimedOut = !RPCChannel::WaitForNotify();

            if (EventOccurred() ||
                // we might have received a "subtly deferred" message
                // in a nested loop that it's now time to process
                (!maybeTimedOut &&
                 (!mDeferred.empty() || !mOutOfTurnReplies.empty())))
                break;

            if (maybeTimedOut && !ShouldContinueFromTimeout())
                return false;
        }

        if (!Connected()) {
            ReportConnectionError("RPCChannel");
            return false;
        }

        Message recvd;
        MessageMap::iterator it;
        if (!mOutOfTurnReplies.empty() &&
            ((it = mOutOfTurnReplies.find(mStack.top().seqno())) !=
            mOutOfTurnReplies.end())) {
            recvd = it->second;
            mOutOfTurnReplies.erase(it);
        }
        else if (!mPending.empty()) {
            recvd = mPending.front();
            mPending.pop();
        }
        else {
            // because of subtleties with nested event loops, it's
            // possible that we got here and nothing happened.  or, we
            // might have a deferred in-call that needs to be
            // processed.  either way, we won't break the inner while
            // loop again until something new happens.
            continue;
        }

        if (!recvd.is_sync() && !recvd.is_rpc()) {
            MonitorAutoUnlock unlock(mMonitor);

            CxxStackFrame f(*this, IN_MESSAGE, &recvd);
            AsyncChannel::OnDispatchMessage(recvd);

            continue;
        }

        if (recvd.is_sync()) {
            RPC_ASSERT(mPending.empty(),
                       "other side should have been blocked");
            MonitorAutoUnlock unlock(mMonitor);

            CxxStackFrame f(*this, IN_MESSAGE, &recvd);
            SyncChannel::OnDispatchMessage(recvd);

            continue;
        }

        RPC_ASSERT(recvd.is_rpc(), "wtf???");

        if (recvd.is_reply()) {
            RPC_ASSERT(0 < mStack.size(), "invalid RPC stack");

            const Message& outcall = mStack.top();

            // in the parent, seqno's increase from 0, and in the
            // child, they decrease from 0
            if ((!mChild && recvd.seqno() < outcall.seqno()) ||
                (mChild && recvd.seqno() > outcall.seqno())) {
                mOutOfTurnReplies[recvd.seqno()] = recvd;
                continue;
            }

            // FIXME/cjones: handle error
            RPC_ASSERT(
                recvd.is_reply_error() ||
                (recvd.type() == (outcall.type()+1) &&
                 recvd.seqno() == outcall.seqno()),
                "somebody's misbehavin'", "rpc", true);

            // we received a reply to our most recent outstanding
            // call.  pop this frame and return the reply
            mStack.pop();

            bool isError = recvd.is_reply_error();
            if (!isError) {
                *reply = recvd;
            }

            if (0 == StackDepth()) {
                RPC_ASSERT(
                    mOutOfTurnReplies.empty(),
                    "still have pending replies with no pending out-calls",
                    "rpc", true);
            }

            // finished with this RPC stack frame
            return !isError;
        }

        // in-call.  process in a new stack frame.

        // "snapshot" the current stack depth while we own the Monitor
        size_t stackDepth = StackDepth();
        {
            MonitorAutoUnlock unlock(mMonitor);
            // someone called in to us from the other side.  handle the call
            CxxStackFrame f(*this, IN_MESSAGE, &recvd);
            Incall(recvd, stackDepth);
            // FIXME/cjones: error handling
        }
    }

    return true;
}

void
RPCChannel::MaybeUndeferIncall()
{
    AssertWorkerThread();
    mMonitor.AssertCurrentThreadOwns();

    if (mDeferred.empty())
        return;

    size_t stackDepth = StackDepth();

    // the other side can only *under*-estimate our actual stack depth
    RPC_ASSERT(mDeferred.top().rpc_remote_stack_depth_guess() <= stackDepth,
               "fatal logic error");

    if (mDeferred.top().rpc_remote_stack_depth_guess() < RemoteViewOfStackDepth(stackDepth))
        return;

    // maybe time to process this message
    Message call = mDeferred.top();
    mDeferred.pop();

    // fix up fudge factor we added to account for race
    RPC_ASSERT(0 < mRemoteStackDepthGuess, "fatal logic error");
    --mRemoteStackDepthGuess;

    mPending.push(call);
}

void
RPCChannel::EnqueuePendingMessages()
{
    AssertWorkerThread();
    mMonitor.AssertCurrentThreadOwns();

    MaybeUndeferIncall();

    for (size_t i = 0; i < mDeferred.size(); ++i)
        mWorkerLoop->PostTask(
            FROM_HERE,
            new DequeueTask(mDequeueOneTask));

    // XXX performance tuning knob: could process all or k pending
    // messages here, rather than enqueuing for later processing

    for (size_t i = 0; i < mPending.size(); ++i)
        mWorkerLoop->PostTask(
            FROM_HERE,
            new DequeueTask(mDequeueOneTask));
}

void
RPCChannel::FlushPendingRPCQueue()
{
    AssertWorkerThread();
    mMonitor.AssertNotCurrentThreadOwns();

    {
        MonitorAutoLock lock(mMonitor);

        if (mDeferred.empty()) {
            if (mPending.empty())
                return;

            const Message& last = mPending.back();
            if (!last.is_rpc() || last.is_reply())
                return;
        }
    }

    while (OnMaybeDequeueOne());
}

bool
RPCChannel::OnMaybeDequeueOne()
{
    // XXX performance tuning knob: could process all or k pending
    // messages here

    AssertWorkerThread();
    mMonitor.AssertNotCurrentThreadOwns();

    Message recvd;
    {
        MonitorAutoLock lock(mMonitor);

        if (!Connected()) {
            ReportConnectionError("RPCChannel");
            return false;
        }

        if (!mDeferred.empty())
            MaybeUndeferIncall();

        if (mPending.empty())
            return false;

        recvd = mPending.front();
        mPending.pop();
    }

    if (IsOnCxxStack() && recvd.is_rpc() && recvd.is_reply()) {
        // We probably just received a reply in a nested loop for an
        // RPC call sent before entering that loop.
        mOutOfTurnReplies[recvd.seqno()] = recvd;
        return false;
    }

    CxxStackFrame f(*this, IN_MESSAGE, &recvd);

    if (recvd.is_rpc())
        Incall(recvd, 0);
    else if (recvd.is_sync())
        SyncChannel::OnDispatchMessage(recvd);
    else
        AsyncChannel::OnDispatchMessage(recvd);

    return true;
}

size_t
RPCChannel::RemoteViewOfStackDepth(size_t stackDepth) const
{
    AssertWorkerThread();
    return stackDepth - mOutOfTurnReplies.size();
}

void
RPCChannel::Incall(const Message& call, size_t stackDepth)
{
    AssertWorkerThread();
    mMonitor.AssertNotCurrentThreadOwns();
    RPC_ASSERT(call.is_rpc() && !call.is_reply(), "wrong message type");

    // Race detection: see the long comment near
    // mRemoteStackDepthGuess in RPCChannel.h.  "Remote" stack depth
    // means our side, and "local" means other side.
    if (call.rpc_remote_stack_depth_guess() != RemoteViewOfStackDepth(stackDepth)) {
        // RPC in-calls have raced.
        // the "winner", if there is one, gets to defer processing of
        // the other side's in-call
        bool defer;
        const char* winner;
        switch (Listener()->MediateRPCRace(mChild ? call : mStack.top(),
                                           mChild ? mStack.top() : call)) {
        case RRPChildWins:
            winner = "child";
            defer = mChild;
            break;
        case RRPParentWins:
            winner = "parent";
            defer = !mChild;
            break;
        case RRPError:
            NS_RUNTIMEABORT("NYI: 'Error' RPC race policy");
            return;
        default:
            NS_RUNTIMEABORT("not reached");
            return;
        }

        if (LoggingEnabled()) {
            fprintf(stderr, "  (%s: %s won, so we're%sdeferring)\n",
                    mChild ? "child" : "parent", winner, defer ? " " : " not ");
        }

        if (defer) {
            // we now know the other side's stack has one more frame
            // than we thought
            ++mRemoteStackDepthGuess; // decremented in MaybeProcessDeferred()
            mDeferred.push(call);
            return;
        }

        // we "lost" and need to process the other side's in-call.
        // don't need to fix up the mRemoteStackDepthGuess here,
        // because we're just about to increment it in DispatchCall(),
        // which will make it correct again
    }

#ifdef OS_WIN
    SyncStackFrame frame(this, true);
#endif

    DispatchIncall(call);
}

void
RPCChannel::DispatchIncall(const Message& call)
{
    AssertWorkerThread();
    mMonitor.AssertNotCurrentThreadOwns();
    RPC_ASSERT(call.is_rpc() && !call.is_reply(),
               "wrong message type");

    Message* reply = nsnull;

    ++mRemoteStackDepthGuess;
    Result rv = Listener()->OnCallReceived(call, reply);
    --mRemoteStackDepthGuess;

    if (!MaybeHandleError(rv, "RPCChannel")) {
        delete reply;
        reply = new Message();
        reply->set_rpc();
        reply->set_reply();
        reply->set_reply_error();
    }

    reply->set_seqno(call.seqno());

    {
        MonitorAutoLock lock(mMonitor);
        if (ChannelConnected == mChannelState)
            SendThroughTransport(reply);
    }
}

bool
RPCChannel::BlockChild()
{
    AssertWorkerThread();

    if (mChild)
        NS_RUNTIMEABORT("child tried to block parent");
    SendSpecialMessage(new BlockChildMessage());
    return true;
}

bool
RPCChannel::UnblockChild()
{
    AssertWorkerThread();

    if (mChild)
        NS_RUNTIMEABORT("child tried to unblock parent");
    SendSpecialMessage(new UnblockChildMessage());
    return true;
}

bool
RPCChannel::OnSpecialMessage(uint16 id, const Message& msg)
{
    AssertWorkerThread();

    switch (id) {
    case BLOCK_CHILD_MESSAGE_TYPE:
        BlockOnParent();
        return true;

    case UNBLOCK_CHILD_MESSAGE_TYPE:
        UnblockFromParent();
        return true;

    default:
        return SyncChannel::OnSpecialMessage(id, msg);
    }
}

void
RPCChannel::BlockOnParent()
{
    AssertWorkerThread();

    if (!mChild)
        NS_RUNTIMEABORT("child tried to block parent");

    MonitorAutoLock lock(mMonitor);

    if (mBlockedOnParent || AwaitingSyncReply() || 0 < StackDepth())
        NS_RUNTIMEABORT("attempt to block child when it's already blocked");

    mBlockedOnParent = true;
    do {
        // XXX this dispatch loop shares some similarities with the
        // one in Call(), but the logic is simpler and different
        // enough IMHO to warrant its own dispatch loop
        while (Connected() && mPending.empty() && mBlockedOnParent) {
            WaitForNotify();
        }

        if (!Connected()) {
            mBlockedOnParent = false;
            ReportConnectionError("RPCChannel");
            break;
        }

        if (!mPending.empty()) {
            Message recvd = mPending.front();
            mPending.pop();

            MonitorAutoUnlock unlock(mMonitor);

            CxxStackFrame f(*this, IN_MESSAGE, &recvd);
            if (recvd.is_rpc()) {
                // stack depth must be 0 here
                Incall(recvd, 0);
            }
            else if (recvd.is_sync()) {
                SyncChannel::OnDispatchMessage(recvd);
            }
            else {
                AsyncChannel::OnDispatchMessage(recvd);
            }
        }
    } while (mBlockedOnParent);

    EnqueuePendingMessages();
}

void
RPCChannel::UnblockFromParent()
{
    AssertWorkerThread();

    if (!mChild)
        NS_RUNTIMEABORT("child tried to block parent");
    MonitorAutoLock lock(mMonitor);
    mBlockedOnParent = false;
}

void
RPCChannel::ExitedCxxStack()
{
    Listener()->OnExitedCxxStack();
    if (mSawRPCOutMsg) {
        MonitorAutoLock lock(mMonitor);
        // see long comment in OnMaybeDequeueOne()
        EnqueuePendingMessages();
        mSawRPCOutMsg = false;
    }
}

void
RPCChannel::DebugAbort(const char* file, int line, const char* cond,
                       const char* why,
                       const char* type, bool reply) const
{
    fprintf(stderr,
            "###!!! [RPCChannel][%s][%s:%d] "
            "Assertion (%s) failed.  %s (triggered by %s%s)\n",
            mChild ? "Child" : "Parent",
            file, line, cond,
            why,
            type, reply ? "reply" : "");
    // technically we need the mutex for this, but we're dying anyway
    DumpRPCStack(stderr, "  ");
    fprintf(stderr, "  remote RPC stack guess: %lu\n",
            mRemoteStackDepthGuess);
    fprintf(stderr, "  deferred stack size: %lu\n",
            mDeferred.size());
    fprintf(stderr, "  out-of-turn RPC replies stack size: %lu\n",
            mOutOfTurnReplies.size());
    fprintf(stderr, "  Pending queue size: %lu, front to back:\n",
            mPending.size());

    MessageQueue pending = mPending;
    while (!pending.empty()) {
        fprintf(stderr, "    [ %s%s ]\n",
                pending.front().is_rpc() ? "rpc" :
                (pending.front().is_sync() ? "sync" : "async"),
                pending.front().is_reply() ? "reply" : "");
        pending.pop();
    }

    NS_RUNTIMEABORT(why);
}

void
RPCChannel::DumpRPCStack(FILE* outfile, const char* const pfx) const
{
    NS_WARN_IF_FALSE(MessageLoop::current() != mWorkerLoop,
                     "The worker thread had better be paused in a debugger!");

    if (!outfile)
        outfile = stdout;

    fprintf(outfile, "%sRPCChannel 'backtrace':\n", pfx);

    // print a python-style backtrace, first frame to last
    for (PRUint32 i = 0; i < mCxxStackFrames.size(); ++i) {
        int32 id;
        const char* dir, *sems, *name;
        mCxxStackFrames[i].Describe(&id, &dir, &sems, &name);

        fprintf(outfile, "%s[(%u) %s %s %s(actor=%d) ]\n", pfx,
                i, dir, sems, name, id);
    }
}

//
// The methods below run in the context of the IO thread, and can proxy
// back to the methods above
//

void
RPCChannel::OnMessageReceived(const Message& msg)
{
    AssertIOThread();
    MonitorAutoLock lock(mMonitor);

    if (MaybeInterceptSpecialIOMessage(msg))
        return;

    // regardless of the RPC stack, if we're awaiting a sync reply, we
    // know that it needs to be immediately handled to unblock us.
    if (AwaitingSyncReply() && msg.is_sync()) {
        // wake up worker thread waiting at SyncChannel::Send
        mRecvd = msg;
        NotifyWorkerThread();
        return;
    }

    mPending.push(msg);

    if (0 == StackDepth() && !mBlockedOnParent) {
        // the worker thread might be idle, make sure it wakes up
        mWorkerLoop->PostTask(FROM_HERE, new DequeueTask(mDequeueOneTask));
    }
    else if (!AwaitingSyncReply())
        NotifyWorkerThread();
}


void
RPCChannel::OnChannelError()
{
    AssertIOThread();

    MonitorAutoLock lock(mMonitor);

    if (ChannelClosing != mChannelState)
        mChannelState = ChannelError;

    // skip SyncChannel::OnError(); we subsume its duties
    if (AwaitingSyncReply() || 0 < StackDepth())
        NotifyWorkerThread();

    PostErrorNotifyTask();
}

} // namespace ipc
} // namespace mozilla

