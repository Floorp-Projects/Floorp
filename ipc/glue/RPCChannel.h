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

        virtual void OnChannelClose() = 0;
        virtual void OnChannelError() = 0;
        virtual Result OnMessageReceived(const Message& aMessage) = 0;
        virtual bool OnReplyTimeout() = 0;
        virtual Result OnMessageReceived(const Message& aMessage,
                                         Message*& aReply) = 0;
        virtual Result OnCallReceived(const Message& aMessage,
                                      Message*& aReply) = 0;
    };

    // What happens if RPC calls race?
    enum RacyRPCPolicy {
        RRPError,
        RRPChildWins,
        RRPParentWins
    };

    RPCChannel(RPCListener* aListener, RacyRPCPolicy aPolicy=RRPChildWins);

    virtual ~RPCChannel();

    // Make an RPC to the other side of the channel
    bool Call(Message* msg, Message* reply);

    // Asynchronously, send the child a message that puts it in such a
    // state that it can't send messages to the parent unless the
    // parent sends a message to it first.  The child stays in this
    // state until the parent calls |UnblockChild()|.
    //
    // It is an error to
    //  - call this on the child side of the channel.
    //  - nest |BlockChild()| calls
    //  - call this when the child is already blocked on a sync or RPC
    //    in-/out- message/call
    //
    // Return true iff successful.
    bool BlockChild();

    // Asynchronously undo |BlockChild()|.
    //
    // It is an error to
    //  - call this on the child side of the channel
    //  - call this without a matching |BlockChild()|
    //
    // Return true iff successful.
    bool UnblockChild();

    NS_OVERRIDE
    virtual bool OnSpecialMessage(uint16 id, const Message& msg);

    // Override the SyncChannel handler so we can dispatch RPC
    // messages.  Called on the IO thread only.
    NS_OVERRIDE
    virtual void OnMessageReceived(const Message& msg);
    NS_OVERRIDE
    virtual void OnChannelError();

#ifdef OS_WIN
    static bool IsSpinLoopActive() {
        return (sInnerEventLoopDepth > 0);
    }

protected:
    void WaitForNotify();
    bool IsMessagePending();
    bool SpinInternalEventLoop();
    static void EnterModalLoop() {
        sInnerEventLoopDepth++;
    }
    static void ExitModalLoop() {
        sInnerEventLoopDepth--;
        NS_ASSERTION(sInnerEventLoopDepth >= 0,
            "sInnerEventLoopDepth dropped below zero!");
    }

    static int sInnerEventLoopDepth;
#endif

  private:
    // Called on worker thread only

    bool EventOccurred();

    void MaybeProcessDeferredIncall();
    void EnqueuePendingMessages();

    void OnMaybeDequeueOne();
    void Incall(const Message& call, size_t stackDepth);
    void DispatchIncall(const Message& call);

    void BlockOnParent();
    void UnblockFromParent();

    // Called from both threads
    size_t StackDepth() {
        mMutex.AssertCurrentThreadOwns();
        return mStack.size();
    }

    void DebugAbort(const char* file, int line, const char* cond,
                    const char* why,
                    const char* type="rpc", bool reply=false);

    // 
    // Queue of all incoming messages, except for replies to sync
    // messages, which are delivered directly to the SyncChannel
    // through its mRecvd member.
    //
    // If both this side and the other side are functioning correctly,
    // the queue can only be in certain configurations.  Let
    // 
    //   |A<| be an async in-message,
    //   |S<| be a sync in-message,
    //   |C<| be an RPC in-call,
    //   |R<| be an RPC reply.
    // 
    // The queue can only match this configuration
    // 
    //  A<* (S< | C< | R< (?{mStack.size() == 1} A<* (S< | C<)))
    //
    // The other side can send as many async messages |A<*| as it
    // wants before sending us a blocking message.
    //
    // The first case is |S<|, a sync in-msg.  The other side must be
    // blocked, and thus can't send us any more messages until we
    // process the sync in-msg.
    //
    // The second case is |C<|, an RPC in-call; the other side must be
    // blocked.  (There's a subtlety here: this in-call might have
    // raced with an out-call, but we detect that with the mechanism
    // below, |mRemoteStackDepth|, and races don't matter to the
    // queue.)
    //
    // Final case, the other side replied to our most recent out-call
    // |R<|.  If that was the *only* out-call on our stack,
    // |?{mStack.size() == 1}|, then other side "finished with us,"
    // and went back to its own business.  That business might have
    // included sending any number of async message |A<*| until
    // sending a blocking message |(S< | C<)|.  If we had more than
    // one RPC call on our stack, the other side *better* not have
    // sent us another blocking message, because it's blocked on a
    // reply from us.
    // 
    std::queue<Message> mPending;

    // 
    // Stack of all the RPC out-calls on which this RPCChannel is
    // awaiting a response.
    //
    std::stack<Message> mStack;

    //
    // Map of replies received "out of turn", because of RPC
    // in-calls racing with replies to outstanding in-calls.  See
    // https://bugzilla.mozilla.org/show_bug.cgi?id=521929.
    //
    typedef std::map<size_t, Message> MessageMap;
    MessageMap mOutOfTurnReplies;

    //
    // Stack of RPC in-calls that were deferred because of race
    // conditions.
    //
    std::stack<Message> mDeferred;

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
    //   mStack.size() == c.remoteDepth
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
    size_t mRemoteStackDepthGuess;
    RacyRPCPolicy mRacePolicy;

    // True iff the parent has put us in a |BlockChild()| state.
    bool mBlockedOnParent;
};


} // namespace ipc
} // namespace mozilla
#endif  // ifndef ipc_glue_RPCChannel_h
