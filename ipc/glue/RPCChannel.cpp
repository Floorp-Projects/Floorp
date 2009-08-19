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
    NS_ASSERTION(ChannelIdle == mChannelState
                 || ChannelWaiting == mChannelState,
                 "trying to Send() to a channel not yet open");

    NS_PRECONDITION(msg->is_rpc(), "can only Call() RPC messages here");

    mMutex.Lock();

    mChannelState = ChannelWaiting;

    msg->set_rpc_remote_stack_depth(mRemoteStackDepth);
    mPending.push(*msg);

    // bypass |SyncChannel::Send| b/c RPCChannel implements its own
    // waiting semantics
    AsyncChannel::Send(msg);

    while (1) {
        // here we're waiting for something to happen.  it may either:
        //  (1) a reply to an outstanding message
        //  (2) a recursive call from the other side
        // or
        //  (3) any other message
        mCvar.Wait();

        Message recvd = mPending.top();
        mPending.pop();

        NS_ABORT_IF_FALSE(recvd.is_rpc(),
                          "should have been delegated to SyncChannel");

        // RPC reply message
        if (recvd.is_reply()) {
            NS_ASSERTION(0 < mPending.size(), "invalid RPC stack");

            const Message& pending = mPending.top();

            if (recvd.type() != (pending.type()+1) && !recvd.is_reply_error()) {
                // FIXME/cjones: handle error
                NS_ASSERTION(0, "somebody's misbehavin'");
            }

            // we received a reply to our most recent outstanding
            // call.  pop this frame and return the reply
            mPending.pop();

            bool isError = recvd.is_reply_error();
            if (!isError) {
                *reply = recvd;
            }

            if (!WaitingForReply()) {
                mChannelState = ChannelIdle;
            }

            mMutex.Unlock();
            return !isError;
        }
        // RPC in-call
        else {
            // "snapshot" the current stack depth while we own the Mutex
            size_t stackDepth = StackDepth();
            mMutex.Unlock();

            // someone called in to us from the other side.  handle the call
            ProcessIncall(recvd, stackDepth);
            // FIXME/cjones: error handling

            mMutex.Lock();
        }
    }

    delete msg;

    return true;
}

void
RPCChannel::OnIncall(const Message& call)
{
    // We were called from the IO thread when StackDepth() == 0, and
    // we were "idle".  That's the "snapshot" of the state of
    // the RPCChannel we use when processing this message.
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
    if (!msg.is_rpc()) {
        return SyncChannel::OnMessageReceived(msg);
    }

    MutexAutoLock lock(mMutex);

    if (0 == StackDepth()) {
        // wake up the worker, there's work to do

        // NB: the interaction between this and SyncChannel is rather
        // subtle.  We may be awaiting a synchronous reply when this
        // code is executed.  If that's the case, posting this in-call
        // to the worker will defer processing of the in-call until
        // after the synchronous reply is received.
        mWorkerLoop->PostTask(FROM_HERE,
                              NewRunnableMethod(this,
                                                &RPCChannel::OnIncall, msg));
    }
    else {
        // let the worker know something new has happened
        mPending.push(msg);
        mCvar.Notify();
    }
}


} // namespace ipc
} // namespace mozilla
