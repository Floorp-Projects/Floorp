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
#include "mozilla/ipc/GeckoThread.h"

#include "nsDebug.h"

using mozilla::MutexAutoLock;
using mozilla::MutexAutoUnlock;

template<>
struct RunnableMethodTraits<mozilla::ipc::RPCChannel>
{
    static void RetainCallee(mozilla::ipc::RPCChannel* obj) { }
    static void ReleaseCallee(mozilla::ipc::RPCChannel* obj) { }
};

namespace mozilla {
namespace ipc {

bool
RPCChannel::Call(Message* msg, Message* reply)
{
    NS_ABORT_IF_FALSE(!ProcessingSyncMessage(),
                      "violation of sync handler invariant");
    NS_ABORT_IF_FALSE(msg->is_rpc(),
                      "can only Call() RPC messages here");

    if (!Connected())
        // trying to Send() to a closed or error'd channel
        return false;

    MutexAutoLock lock(mMutex);

    msg->set_rpc_remote_stack_depth(mRemoteStackDepth);
    mStack.push(*msg);

    // bypass |SyncChannel::Send| b/c RPCChannel implements its own
    // waiting semantics
    AsyncChannel::Send(msg);

    while (1) {
        // here we're waiting for something to happen. see long
        // comment about the queue in RPCChannel.h
        while (Connected() && mPending.empty()) {
            mCvar.Wait();
        }

        if (!Connected())
            // FIXME more sophisticated error handling
            return false;

        Message recvd = mPending.front();
        mPending.pop();

        // async message.  process it, go back to waiting
        if (!recvd.is_sync() && !recvd.is_rpc()) {
            MutexAutoUnlock unlock(mMutex);

            AsyncChannel::OnDispatchMessage(recvd);
            continue;
        }

        // something sync.  Let the sync dispatcher take care of it
        // (it may be an invalid message, but the sync handler will
        // check that).
        if (recvd.is_sync()) {
            NS_ABORT_IF_FALSE(mPending.empty(),
                              "other side is malfunctioning");
            MutexAutoUnlock unlock(mMutex);

            SyncChannel::OnDispatchMessage(recvd);
            continue;
        }

        // from here on, we know that recvd.is_rpc()
        NS_ABORT_IF_FALSE(recvd.is_rpc(), "wtf???");

        // reply message
        if (recvd.is_reply()) {
            NS_ABORT_IF_FALSE(0 < mStack.size(), "invalid RPC stack");

            const Message& outcall = mStack.top();

            if (recvd.type() != (outcall.type()+1) && !recvd.is_reply_error()) {
                // FIXME/cjones: handle error
                NS_ABORT_IF_FALSE(0, "somebody's misbehavin'");
            }

            // we received a reply to our most recent outstanding
            // call.  pop this frame and return the reply
            mStack.pop();

            bool isError = recvd.is_reply_error();
            if (!isError) {
                *reply = recvd;
            }

            if (0 == StackDepth()) {
                // this was the last outcall we were waiting on.
                // flush the pending queue into the "regular" event
                // queue, checking invariants along the way.  see long
                // comment in RPCChannel.h
                bool seenBlocker = false;

                // A<* (S< | C<)
                while (!mPending.empty()) {
                    Message m = mPending.front();
                    mPending.pop();

                    if (m.is_sync()) {
                        NS_ABORT_IF_FALSE(!seenBlocker,
                                          "other side is malfunctioning");
                        seenBlocker = true;

                        MessageLoop::current()->PostTask(
                            FROM_HERE,
                            NewRunnableMethod(this,
                                              &RPCChannel::OnDelegate, m));
                    }
                    else if (m.is_rpc()) {
                        NS_ABORT_IF_FALSE(!seenBlocker,
                                          "other side is malfunctioning");
                        seenBlocker = true;

                        MessageLoop::current()->PostTask(
                            FROM_HERE,
                            NewRunnableMethod(this,
                                              &RPCChannel::OnIncall,
                                              m));
                    }
                    else {
                        MessageLoop::current()->PostTask(
                            FROM_HERE,
                            NewRunnableMethod(this,
                                              &RPCChannel::OnDelegate, m));
                    }
                }
            }
            else {
                // shouldn't have queued any more messages, since
                // the other side is now supposed to be blocked on a
                // reply from us!
                if (mPending.size() > 0) {
                    NS_RUNTIMEABORT("other side should have been blocked");
                }
            }

            // unlocks mMutex
            return !isError;
        }

        // in-call.  process in a new stack frame
        NS_ABORT_IF_FALSE(mPending.empty(),
                          "other side is malfunctioning");

        // "snapshot" the current stack depth while we own the Mutex
        size_t stackDepth = StackDepth();
        {
            MutexAutoUnlock unlock(mMutex);
            // someone called in to us from the other side.  handle the call
            ProcessIncall(recvd, stackDepth);
            // FIXME/cjones: error handling
        }
    }

    return true;
}

void
RPCChannel::OnDelegate(const Message& msg)
{
    if (msg.is_sync())
        return SyncChannel::OnDispatchMessage(msg);
    else if (!msg.is_rpc())
        return AsyncChannel::OnDispatchMessage(msg);
    NS_RUNTIMEABORT("fatal logic error");
}

void
RPCChannel::OnMaybeDequeueOne()
{
    Message recvd;  
    {
        MutexAutoLock lock(mMutex);

        if (mPending.empty())
            return;

        NS_ABORT_IF_FALSE(mPending.size() == 1, "should only have one msg");
        NS_ABORT_IF_FALSE(mPending.front().is_sync(), "msg should be sync");

        recvd = mPending.front();
        mPending.pop();
    }
    return SyncChannel::OnDispatchMessage(recvd);
}

void
RPCChannel::OnIncall(const Message& call)
{
    // We only reach here from the "regular" event loop, when
    // StackDepth() == 0.  That's the "snapshot" of the state of the
    // RPCChannel we use when processing this message.
    ProcessIncall(call, 0);
}

void
RPCChannel::ProcessIncall(const Message& call, size_t stackDepth)
{
    mMutex.AssertNotCurrentThreadOwns();
    NS_ABORT_IF_FALSE(call.is_rpc(),
                      "should have been handled by SyncChannel");

    // Race detection: see the long comment near mRemoteStackDepth 
    // in RPCChannel.h
    NS_ASSERTION(stackDepth == call.rpc_remote_stack_depth(),
                 "RPC in-calls have raced!");

    Message* reply = nsnull;

    ++mRemoteStackDepth;
    Result rv =
        static_cast<RPCListener*>(mListener)->OnCallReceived(call, reply);
    --mRemoteStackDepth;

    switch (rv) {
    case MsgProcessed:
        break;

    case MsgNotKnown:
    case MsgNotAllowed:
    case MsgPayloadError:
    case MsgRouteError:
    case MsgValueError:
        delete reply;
        reply = new Message();
        reply->set_rpc();
        reply->set_reply();
        reply->set_reply_error();
        // FIXME/cjones: error handling; OnError()?
        break;

    default:
        NOTREACHED();
        return;
    }

    mIOLoop->PostTask(FROM_HERE,
                      NewRunnableMethod(this,
                                        &RPCChannel::OnSendReply,
                                        reply));

}

//
// The methods below run in the context of the IO thread, and can proxy
// back to the methods above
//

void
RPCChannel::OnMessageReceived(const Message& msg)
{
    MutexAutoLock lock(mMutex);

    // regardless of the RPC stack, if we're awaiting a sync reply, we
    // know that it needs to be immediately handled to unblock us.
    // The SyncChannel will check that msg is a reply, and the right
    // kind of reply, then do its thing.
    if (AwaitingSyncReply()
        && msg.is_sync()) {
        // wake up worker thread (at SyncChannel::Send) awaiting
        // this reply
        mRecvd = msg;
        mCvar.Notify();
        return;
    }

    // otherwise, we handle sync/async/rpc messages differently depending
    // on whether the RPC channel is idle

    if (0 == StackDepth()) {
        // we're idle wrt to the RPC layer, and this message could be
        // async, sync, or rpc.

        // async message: delegate, doesn't affect anything
        if (!msg.is_sync() && !msg.is_rpc()) {
            MutexAutoUnlock unlock(mMutex);
            return AsyncChannel::OnMessageReceived(msg);
        }

        // NB: the interaction between this and SyncChannel is rather
        // subtle; we have to handle a fairly nasty race condition.
        // The other side may send us a sync message at any time.  If
        // we receive it here while the worker thread is processing an
        // event that will eventually send an RPC message (or will
        // send an RPC message while processing any event enqueued
        // before the sync message was received), then if we tried to
        // enqueue the sync message in the worker's event queue, it
        // would get "lost": the worker would block on the RPC reply
        // without seeing the sync request, and we'd deadlock.
        // 
        // So to avoid this case, when the RPC channel is idle and
        // receives a sync request, it puts the request in the special
        // RPC message queue, and asks the worker thread to process a
        // task that might end up dequeuing that RPC message.  The
        // task *might not* dequeue a sync request --- this might
        // occur if the event the worker is currently processing sends
        // an RPC message.  If that happens, the worker will go into
        // its "wait loop" for the RPC response, and immediately
        // dequeue and process the sync request.

        if (msg.is_sync()) {
            mPending.push(msg);

            mWorkerLoop->PostTask(
                FROM_HERE,
                NewRunnableMethod(this,
                                  &RPCChannel::OnMaybeDequeueOne));
            return;
        }

        // OK: the RPC channel is idle, and we received an in-call.
        // wake up the worker thread

        NS_ABORT_IF_FALSE(msg.is_rpc(), "should be RPC");

        mWorkerLoop->PostTask(FROM_HERE,
                              NewRunnableMethod(this,
                                                &RPCChannel::OnIncall, msg));
    }
    else {
        // we're waiting on an RPC reply

        // NB some logic here is duplicated with SyncChannel.  this is
        // to allow more local reasoning

        // NBB see the second-to-last long comment in RPCChannel.h
        // describing legal queue states

        // if we're waiting on a sync reply, and this message is sync,
        // dispatch it to the sync message handler.
        // 
        // since we're waiting on an RPC answer in an older stack
        // frame, we know we'll eventually pop back to the
        // RPCChannel::Call frame where we're awaiting the RPC reply.
        // so the queue won't be forgotten!

        // waiting on a sync reply, but got an async message.  that's OK,
        // but we defer processing of it until the sync reply comes in.
        if (AwaitingSyncReply()
            && !msg.is_sync() && !msg.is_rpc()) {
            mPending.push(msg);
            return;
        }

        // if this side and the other were functioning correctly, we'd
        // never reach this case.  RPCChannel::Call explicitly checks
        // for and disallows this case.  so if we reach here, the other
        // side is malfunctioning (compromised?).
        if (AwaitingSyncReply() /* msg.is_rpc() */) {
            // FIXME other error handling?
            NS_RUNTIMEABORT("the other side is malfunctioning");
            return;             // not reached
        }

        // otherwise, we (legally) either got (i) async msg; (ii) sync
        // in-msg; (iii) re-entrant rpc in-call; (iv) rpc reply we
        // were awaiting.  Dispatch to the worker, where invariants
        // are checked and the message processed.
        mPending.push(msg);
        mCvar.Notify();
    }
}


void
RPCChannel::OnChannelError()
{
    {
        MutexAutoLock lock(mMutex);

        mChannelState = ChannelError;

        if (AwaitingSyncReply()
            || 0 < StackDepth()) {
            mCvar.Notify();
        }
    }

    // skip SyncChannel::OnError(); we subsume its duties

    return AsyncChannel::OnChannelError();
}


} // namespace ipc
} // namespace mozilla
