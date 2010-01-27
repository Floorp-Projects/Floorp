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

using mozilla::MutexAutoLock;
using mozilla::MutexAutoUnlock;

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

RPCChannel::RPCChannel(RPCListener* aListener,
                       RacyRPCPolicy aPolicy)
  : SyncChannel(aListener),
    mPending(),
    mStack(),
    mOutOfTurnReplies(),
    mDeferred(),
    mRemoteStackDepthGuess(0),
    mRacePolicy(aPolicy),
    mBlockedOnParent(false)
{
    MOZ_COUNT_CTOR(RPCChannel);
}

RPCChannel::~RPCChannel()
{
    MOZ_COUNT_DTOR(RPCChannel);
    // FIXME/cjones: impl
}

bool
RPCChannel::Call(Message* msg, Message* reply)
{
    AssertWorkerThread();
    mMutex.AssertNotCurrentThreadOwns();
    RPC_ASSERT(!ProcessingSyncMessage(),
               "violation of sync handler invariant");
    RPC_ASSERT(msg->is_rpc(), "can only Call() RPC messages here");

    MutexAutoLock lock(mMutex);

    if (!Connected()) {
        ReportConnectionError("RPCChannel");
        return false;
    }

    msg->set_seqno(NextSeqno());
    msg->set_rpc_remote_stack_depth_guess(mRemoteStackDepthGuess);
    msg->set_rpc_local_stack_depth(1 + StackDepth());
    mStack.push(*msg);

    mIOLoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &RPCChannel::OnSend, msg));

    while (1) {
        // now might be the time to process a message deferred because
        // of race resolution
        MaybeProcessDeferredIncall();

        // here we're waiting for something to happen. see long
        // comment about the queue in RPCChannel.h
        while (Connected() && mPending.empty() &&
               (mOutOfTurnReplies.empty() ||
                mOutOfTurnReplies.top().seqno() < mStack.top().seqno())) {
            WaitForNotify();
        }

        if (!Connected()) {
            ReportConnectionError("RPCChannel");
            return false;
        }

        Message recvd;
        if (!mOutOfTurnReplies.empty() &&
            mOutOfTurnReplies.top().seqno() == mStack.top().seqno()) {
            recvd = mOutOfTurnReplies.top();
            mOutOfTurnReplies.pop();
        }
        else {
            recvd = mPending.front();
            mPending.pop();
        }

        if (!recvd.is_sync() && !recvd.is_rpc()) {
            MutexAutoUnlock unlock(mMutex);
            AsyncChannel::OnDispatchMessage(recvd);
            continue;
        }

        if (recvd.is_sync()) {
            RPC_ASSERT(mPending.empty(),
                       "other side should have been blocked");
            MutexAutoUnlock unlock(mMutex);
            SyncChannel::OnDispatchMessage(recvd);
            continue;
        }

        RPC_ASSERT(recvd.is_rpc(), "wtf???");

        if (recvd.is_reply()) {
            RPC_ASSERT(0 < mStack.size(), "invalid RPC stack");

            const Message& outcall = mStack.top();

            if (recvd.seqno() < outcall.seqno()) {
                mOutOfTurnReplies.push(recvd);
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
                // we may have received new messages while waiting for
                // our reply.  because we were awaiting a reply,
                // StackDepth > 0, and the IO thread didn't enqueue
                // OnMaybeDequeueOne() events for us.  so to avoid
                // "losing" the new messages, we do that now.
                EnqueuePendingMessages();

                
                RPC_ASSERT(
                    mOutOfTurnReplies.empty(),
                    "still have pending replies with no pending out-calls",
                    "rpc", true);
            }

            // finished with this RPC stack frame
            return !isError;
        }

        // in-call.  process in a new stack frame.

        // "snapshot" the current stack depth while we own the Mutex
        size_t stackDepth = StackDepth();
        {
            MutexAutoUnlock unlock(mMutex);
            // someone called in to us from the other side.  handle the call
            Incall(recvd, stackDepth);
            // FIXME/cjones: error handling
        }
    }

    return true;
}

void
RPCChannel::MaybeProcessDeferredIncall()
{
    AssertWorkerThread();
    mMutex.AssertCurrentThreadOwns();

    if (mDeferred.empty())
        return;

    size_t stackDepth = StackDepth();

    // the other side can only *under*-estimate our actual stack depth
    RPC_ASSERT(mDeferred.top().rpc_remote_stack_depth_guess() <= stackDepth,
               "fatal logic error");

    if (mDeferred.top().rpc_remote_stack_depth_guess() < stackDepth)
        return;

    // time to process this message
    Message call = mDeferred.top();
    mDeferred.pop();

    // fix up fudge factor we added to account for race
    RPC_ASSERT(0 < mRemoteStackDepthGuess, "fatal logic error");
    --mRemoteStackDepthGuess;

    MutexAutoUnlock unlock(mMutex);
    fprintf(stderr, "  (processing deferred in-call)\n");
    Incall(call, stackDepth);
}

void
RPCChannel::EnqueuePendingMessages()
{
    AssertWorkerThread();
    mMutex.AssertCurrentThreadOwns();
    RPC_ASSERT(mDeferred.empty() || 1 == mDeferred.size(),
               "expected mDeferred to have 0 or 1 items");

    if (!mDeferred.empty())
        mWorkerLoop->PostTask(
            FROM_HERE,
            NewRunnableMethod(this, &RPCChannel::OnMaybeDequeueOne));

    // XXX performance tuning knob: could process all or k pending
    // messages here, rather than enqueuing for later processing

    for (size_t i = 0; i < mPending.size(); ++i)
        mWorkerLoop->PostTask(
            FROM_HERE,
            NewRunnableMethod(this, &RPCChannel::OnMaybeDequeueOne));
}

void
RPCChannel::OnMaybeDequeueOne()
{
    // XXX performance tuning knob: could process all or k pending
    // messages here

    AssertWorkerThread();
    mMutex.AssertNotCurrentThreadOwns();
    RPC_ASSERT(mDeferred.empty() || 1 == mDeferred.size(),
               "expected mDeferred to have 0 or 1 items, but it has %lu");

    Message recvd;
    {
        MutexAutoLock lock(mMutex);

        if (!mDeferred.empty())
            return MaybeProcessDeferredIncall();

        if (mPending.empty())
            return;

        recvd = mPending.front();
        mPending.pop();
    }

    if (recvd.is_rpc())
        return Incall(recvd, 0);
    else if (recvd.is_sync())
        return SyncChannel::OnDispatchMessage(recvd);
    else
        return AsyncChannel::OnDispatchMessage(recvd);
}

void
RPCChannel::Incall(const Message& call, size_t stackDepth)
{
    AssertWorkerThread();
    mMutex.AssertNotCurrentThreadOwns();
    RPC_ASSERT(call.is_rpc() && !call.is_reply(), "wrong message type");

    // Race detection: see the long comment near
    // mRemoteStackDepthGuess in RPCChannel.h.  "Remote" stack depth
    // means our side, and "local" means other side.
    if (call.rpc_remote_stack_depth_guess() != stackDepth) {
        NS_WARNING("RPC in-calls have raced!");

        RPC_ASSERT(call.rpc_remote_stack_depth_guess() < stackDepth,
                   "fatal logic error");
        RPC_ASSERT(1 == (stackDepth - call.rpc_remote_stack_depth_guess()),
                   "got more than 1 RPC message out of sync???");
        RPC_ASSERT(1 == (call.rpc_local_stack_depth() -mRemoteStackDepthGuess),
                   "RPC unexpected not symmetric");

        // the "winner", if there is one, gets to defer processing of
        // the other side's in-call
        bool defer;
        const char* winner;
        switch (mRacePolicy) {
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

        fprintf(stderr, "  (%s won, so we're%sdeferring)\n",
                winner, defer ? " " : " not ");

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

    DispatchIncall(call);
}

void
RPCChannel::DispatchIncall(const Message& call)
{
    AssertWorkerThread();
    mMutex.AssertNotCurrentThreadOwns();
    RPC_ASSERT(call.is_rpc() && !call.is_reply(),
               "wrong message type");

    Message* reply = nsnull;

    ++mRemoteStackDepthGuess;
    Result rv =
        static_cast<RPCListener*>(mListener)->OnCallReceived(call, reply);
    --mRemoteStackDepthGuess;

    if (!MaybeHandleError(rv, "RPCChannel")) {
        delete reply;
        reply = new Message();
        reply->set_rpc();
        reply->set_reply();
        reply->set_reply_error();
    }

    reply->set_seqno(call.seqno());

    mIOLoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &RPCChannel::OnSend, reply));
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

    MutexAutoLock lock(mMutex);

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

            MutexAutoUnlock unlock(mMutex);
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
    MutexAutoLock lock(mMutex);
    mBlockedOnParent = false;
}

void
RPCChannel::DebugAbort(const char* file, int line, const char* cond,
                       const char* why,
                       const char* type, bool reply)
{
    fprintf(stderr,
            "###!!! [RPCChannel][%s][%s:%d] "
            "Assertion (%s) failed.  %s (triggered by %s%s)\n",
            mChild ? "Child" : "Parent",
            file, line, cond,
            why,
            type, reply ? "reply" : "");
    // technically we need the mutex for this, but we're dying anyway
    fprintf(stderr, "  local RPC stack size: %lu\n",
            mStack.size());
    fprintf(stderr, "  remote RPC stack guess: %lu\n",
            mRemoteStackDepthGuess);
    fprintf(stderr, "  deferred stack size: %lu\n",
            mDeferred.size());
    fprintf(stderr, "  out-of-turn RPC replies stack size: %lu\n",
            mOutOfTurnReplies.size());
    fprintf(stderr, "  Pending queue size: %lu, front to back:\n",
            mPending.size());
    while (!mPending.empty()) {
        fprintf(stderr, "    [ %s%s ]\n",
                mPending.front().is_rpc() ? "rpc" :
                (mPending.front().is_sync() ? "sync" : "async"),
                mPending.front().is_reply() ? "reply" : "");
        mPending.pop();
    }

    NS_RUNTIMEABORT(why);
}

//
// The methods below run in the context of the IO thread, and can proxy
// back to the methods above
//

void
RPCChannel::OnMessageReceived(const Message& msg)
{
    AssertIOThread();
    MutexAutoLock lock(mMutex);

    // regardless of the RPC stack, if we're awaiting a sync reply, we
    // know that it needs to be immediately handled to unblock us.
    if (AwaitingSyncReply() && msg.is_sync()) {
        // wake up worker thread waiting at SyncChannel::Send
        mRecvd = msg;
        NotifyWorkerThread();
        return;
    }

    mPending.push(msg);

    if (0 == StackDepth() && !mBlockedOnParent)
        // the worker thread might be idle, make sure it wakes up
        mWorkerLoop->PostTask(
            FROM_HERE,
            NewRunnableMethod(this, &RPCChannel::OnMaybeDequeueOne));
    else
        NotifyWorkerThread();
}


void
RPCChannel::OnChannelError()
{
    AssertIOThread();

    AsyncChannel::OnChannelError();

    // skip SyncChannel::OnError(); we subsume its duties
    MutexAutoLock lock(mMutex);
    if (AwaitingSyncReply()
        || 0 < StackDepth())
        NotifyWorkerThread();
}


} // namespace ipc
} // namespace mozilla

