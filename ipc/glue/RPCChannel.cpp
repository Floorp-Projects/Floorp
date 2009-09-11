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
    NS_ASSERTION(ChannelConnected == mChannelState,
                 "trying to Send() to a channel not yet open");
    NS_PRECONDITION(msg->is_rpc(),
                    "can only Call() RPC messages here");

    MutexAutoLock lock(mMutex);

    msg->set_rpc_remote_stack_depth(mRemoteStackDepth);
    mStack.push(*msg);

    // bypass |SyncChannel::Send| b/c RPCChannel implements its own
    // waiting semantics
    AsyncChannel::Send(msg);

    while (1) {
        // here we're waiting for something to happen. see long
        // comment about the queue in RPCChannel.h
        while (mPending.empty()) {
            mCvar.Wait();
        }

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
        mIOLoop->PostTask(FROM_HERE,
                          NewRunnableMethod(this,
                                            &RPCChannel::OnSendReply,
                                            reply));
        return;

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
        mIOLoop->PostTask(FROM_HERE,
                          NewRunnableMethod(this,
                                            &RPCChannel::OnSendReply,
                                            reply));
        // FIXME/cjones: error handling; OnError()?
        return;

    default:
        NOTREACHED();
        return;
    }
}

//
// The methods below run in the context of the IO thread, and can proxy
// back to the methods above
//

void
RPCChannel::OnMessageReceived(const Message& msg)
{
    MutexAutoLock lock(mMutex);

    if (0 == StackDepth()) {
        // we're idle wrt to the RPC layer, and this message could be
        // async, sync, or rpc.
        // 
        // if it's *not* an RPC message, we delegate processing to the
        // SyncChannel.  it knows how to properly dispatch sync and
        // async messages, and the sync channel also will do error
        // checking wrt to its invariants
        if (!msg.is_rpc()) {
            // unlocks mMutex
            return SyncChannel::OnMessageReceived(msg);
        }

        // wake up the worker, there's a new in-call to process

        // NB: the interaction between this and SyncChannel is rather
        // subtle.  It's possible for us to send a sync message
        // exactly when the other side sends an RPC in-call.  A sync
        // handler invariant is that the sync message must be replied
        // to before sending any other blocking message, so we know
        // that the other side must reply ASAP to the sync message we
        // just sent.  Thus by queuing this RPC in-call in that
        // situation, we specify an order on the previously unordered
        // messages and satisfy all invariants.
        //
        // It's not possible for us to otherwise receive an RPC
        // in-call while awaiting a sync response in any case where
        // both us and the other side are behaving legally.  Is it
        // worth trying to detect this case?  (It's kinda hard.)
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
        // dispatch it to the sync message handler. It will check that
        // it's a reply, and the right kind of reply, then do its
        // thing.
        // 
        // since we're waiting on an RPC answer in an older stack
        // frame, we know we'll eventually pop back to the
        // RPCChannel::Call frame where we're awaiting the RPC reply.
        // so the queue won't be forgotten!
        if (AwaitingSyncReply()
            && msg.is_sync()) {
            // wake up worker thread (at SyncChannel::Send) awaiting
            // this reply
            mRecvd = msg;
            mCvar.Notify();
            return;
        }

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


} // namespace ipc
} // namespace mozilla
