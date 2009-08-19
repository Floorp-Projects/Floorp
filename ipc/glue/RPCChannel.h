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

// FIXME/cjones probably shouldn't depend on this
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

private:
    void OnIncall(const Message& msg);
    void ProcessIncall(const Message& call, size_t stackDepth);

    size_t StackDepth() {
        mMutex.AssertCurrentThreadOwns();
        NS_ABORT_IF_FALSE(
            mPending.empty()
            || (mPending.top().is_rpc() && !mPending.top().is_reply()),
            "StackDepth() called from an inconsistent state");

        return mPending.size();
    }

    //
    // In quiescent states, |mPending| is a stack of all the RPC
    // out-calls on which this RPCChannel is awaiting a response.
    //
    // The stack is also used by the IO thread to transfer received
    // messages to the worker thread, only when the worker thread is
    // awaiting an RPC response.  Until the worker pops the top of the
    // stack, it may (legally) contain either of
    //
    // - RPC in-call (msg.is_rpc() && !msg.is_reply())
    // - RPC reply (msg.is_rpc() && msg.is_reply())
    //
    // In both cases, the worker will pop the message off the stack
    // and process it ASAP, returning |mPending| to a quiescent state.
    //
    std::stack<Message> mPending;

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
