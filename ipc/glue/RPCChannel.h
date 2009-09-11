/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * ***** BEGIN LICENSE BLOCK *****
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

#ifndef ipc_glue_RPCChannel_h
#define ipc_glue_RPCChannel_h 1

// FIXME/cjones probably shouldn't depend on STL
#include <queue>
#include <stack>

#include "mozilla/ipc/SyncChannel.h"

namespace mozilla {
namespace ipc {
//-----------------------------------------------------------------------------

class RPCChannel : public SyncChannel
{
public:
    class /*NS_INTERFACE_CLASS*/ RPCListener :
        public SyncChannel::SyncListener
    {
    public:
        virtual ~RPCListener() { }
        virtual Result OnMessageReceived(const Message& aMessage) = 0;
        virtual Result OnMessageReceived(const Message& aMessage,
                                         Message*& aReply) = 0;
        virtual Result OnCallReceived(const Message& aMessage,
                                      Message*& aReply) = 0;
    };

    RPCChannel(RPCListener* aListener) :
        SyncChannel(aListener),
        mPending(),
        mRemoteStackDepth(0)
    {
    }

    virtual ~RPCChannel()
    {
        // FIXME/cjones: impl
    }

    // Make an RPC to the other side of the channel
    bool Call(Message* msg, Message* reply);

    // Override the SyncChannel handler so we can dispatch RPC messages
    virtual void OnMessageReceived(const Message& msg);

protected:
    // Only exists because we can't schedule SyncChannel::OnDispatchMessage
    // or AsyncChannel::OnDispatchMessage from within Call() when we flush
    // the pending queue
    void OnDelegate(const Message& msg);

private:
    void OnIncall(const Message& msg);
    void ProcessIncall(const Message& call, size_t stackDepth);

    // Called from both threads
    size_t StackDepth() {
        mMutex.AssertCurrentThreadOwns();
        return mStack.size();
    }

    // 
    // Stack of all the RPC out-calls on which this RPCChannel is
    // awaiting a response.
    //
    std::stack<Message> mStack;

    // 
    // After the worker thread is blocked on an RPC out-call
    // (i.e. awaiting a reply), the IO thread uses this queue to
    // transfer received messages to the worker thread for processing.
    // If both this side and the other side are functioning correctly,
    // the queue is only allowed to have certain configurations.  Let
    // 
    //   |A<| be an async in-message,
    //   |S<| be a sync in-message,
    //   |C<| be an RPC in-call,
    //   |R<| be an RPC reply.
    // 
    // After the worker thread wakes us up to process the queue,
    // the queue can only match this configuration
    // 
    //  A<* (S< | C< | R< (?{mStack.size() == 1} A<* (S< | C<)))
    // 
    // After we send an RPC message, the other side can send as many
    // async messages |A<*| as it wants before sending back any other
    // message type.
    // 
    // The first "other message type" case is |S<|, a sync in-msg.
    // The other side must be blocked, and thus can't send us any more
    // messages until we process the sync in-msg.
    // 
    // The second case is |C<|, an RPC in-call; the other side
    // re-entered us while processing our out-call.  It therefore must
    // be blocked.  (There's a subtlety here: this in-call might have
    // raced with our out-call, but we detect that with the mechanism
    // below, |mRemoteStackDepth|, and races don't matter to the
    // queue.)
    // 
    // Final case, the other side replied to our most recent out-call
    // |R<|.  If that was the *only* out-call on our stack, |{
    // mStack.size() == 1}|, then other side "finished with us," and
    // went back to its own business.  That business might have
    // included sending any number of async message |A<*| until
    // sending a blocking message |(S< | C<)|.  We just flush these to
    // the event loop to process in order, it will do the Right Thing,
    // since only the last message can be a blocking message.
    // HOWEVER, if we had more than one RPC call on our stack, the
    // other side *better* not have sent us another blocking message,
    // because it's blocked on a reply from us.
    // 
    std::queue<Message> mPending;

    //
    // This is what we think the RPC stack depth is on the "other
    // side" of this RPC channel.  We maintain this variable so that
    // we can detect racy RPC calls.  With each RPC out-call sent, we
    // send along what *we* think the stack depth of the remote side
    // is *before* it will receive the RPC call.
    //
    // After sending the out-call, our stack depth is "incremented"
    // by pushing that pending message onto mPending.
    //
    // Then when processing an in-call |c|, it must be true that
    //
    //   mPending.size() == c.remoteDepth
    //
    // i.e., my depth is actually the same as what the other side
    // thought it was when it sent in-call |c|.  If this fails to
    // hold, we have detected racy RPC calls.
    //
    // We then increment mRemoteStackDepth *just before* processing
    // the in-call, since we know the other side is waiting on it, and
    // decrement it *just after* finishing processing that in-call,
    // since our response will pop the top of the other side's
    // |mPending|.
    //
    // One nice aspect of this race detection is that it is symmetric;
    // if one side detects a race, then the other side must also 
    // detect the same race.
    //
    // TODO: and when we detect a race, what should we actually *do* ... ?
    //
    size_t mRemoteStackDepth;
};


} // namespace ipc
} // namespace mozilla
#endif  // ifndef ipc_glue_RPCChannel_h
