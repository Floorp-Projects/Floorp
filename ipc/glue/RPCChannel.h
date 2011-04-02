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

#include <stdio.h>

// FIXME/cjones probably shouldn't depend on STL
#include <queue>
#include <stack>
#include <vector>

#include "base/basictypes.h"

#include "nsAtomicRefcnt.h"

#include "mozilla/ipc/SyncChannel.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace ipc {
//-----------------------------------------------------------------------------

class RPCChannel : public SyncChannel
{
    friend class CxxStackFrame;

public:
    // What happens if RPC calls race?
    enum RacyRPCPolicy {
        RRPError,
        RRPChildWins,
        RRPParentWins
    };

    class /*NS_INTERFACE_CLASS*/ RPCListener :
        public SyncChannel::SyncListener
    {
    public:
        virtual ~RPCListener() { }

        virtual void OnChannelClose() = 0;
        virtual void OnChannelError() = 0;
        virtual Result OnMessageReceived(const Message& aMessage) = 0;
        virtual void OnProcessingError(Result aError) = 0;
        virtual bool OnReplyTimeout() = 0;
        virtual Result OnMessageReceived(const Message& aMessage,
                                         Message*& aReply) = 0;
        virtual Result OnCallReceived(const Message& aMessage,
                                      Message*& aReply) = 0;
        virtual void OnChannelConnected(int32 peer_pid) {};

        virtual void OnEnteredCxxStack()
        {
            NS_RUNTIMEABORT("default impl shouldn't be invoked");
        }

        virtual void OnExitedCxxStack()
        {
            NS_RUNTIMEABORT("default impl shouldn't be invoked");
        }

        virtual void OnEnteredCall()
        {
            NS_RUNTIMEABORT("default impl shouldn't be invoked");
        }

        virtual void OnExitedCall()
        {
            NS_RUNTIMEABORT("default impl shouldn't be invoked");
        }

        virtual RacyRPCPolicy MediateRPCRace(const Message& parent,
                                             const Message& child)
        {
            return RRPChildWins;
        }
    };

    RPCChannel(RPCListener* aListener);

    virtual ~RPCChannel();

    NS_OVERRIDE
    void Clear();

    // Make an RPC to the other side of the channel
    bool Call(Message* msg, Message* reply);

    // RPCChannel overrides these so that the async and sync messages
    // can be counted against mStackFrames
    NS_OVERRIDE
    virtual bool Send(Message* msg);
    NS_OVERRIDE
    virtual bool Send(Message* msg, Message* reply);

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

    // Return true iff this has code on the C++ stack.
    bool IsOnCxxStack() const {
        return !mCxxStackFrames.empty();
    }

    NS_OVERRIDE
    virtual bool OnSpecialMessage(uint16 id, const Message& msg);

    // Override the SyncChannel handler so we can dispatch RPC
    // messages.  Called on the IO thread only.
    NS_OVERRIDE
    virtual void OnMessageReceived(const Message& msg);
    NS_OVERRIDE
    virtual void OnChannelError();

    /**
     * If there is a pending RPC message, process all pending messages.
     *
     * @note This method is used on Windows when we detect that an outbound
     * OLE RPC call is being made to unblock the parent.
     */
    void FlushPendingRPCQueue();

#ifdef OS_WIN
    void ProcessNativeEventsInRPCCall();

protected:
    bool WaitForNotify();
    void SpinInternalEventLoop();
#endif

  private:
    // Called on worker thread only

    RPCListener* Listener() const {
        return static_cast<RPCListener*>(mListener);
    }

    NS_OVERRIDE
    virtual bool ShouldDeferNotifyMaybeError() const {
        return IsOnCxxStack();
    }

    bool EventOccurred() const;

    void MaybeUndeferIncall();
    void EnqueuePendingMessages();

    /**
     * Process one deferred or pending message.
     * @return true if a message was processed
     */
    bool OnMaybeDequeueOne();

    void Incall(const Message& call, size_t stackDepth);
    void DispatchIncall(const Message& call);

    void BlockOnParent();
    void UnblockFromParent();

    // This helper class managed RPCChannel.mCxxStackDepth on behalf
    // of RPCChannel.  When the stack depth is incremented from zero
    // to non-zero, it invokes an RPCChannel callback, and similarly
    // for when the depth goes from non-zero to zero;
    void EnteredCxxStack()
    {
        Listener()->OnEnteredCxxStack();
    }

    void ExitedCxxStack();

    void EnteredCall()
    {
        Listener()->OnEnteredCall();
    }

    void ExitedCall()
    {
        Listener()->OnExitedCall();
    }

    enum Direction { IN_MESSAGE, OUT_MESSAGE };
    struct RPCFrame {
        RPCFrame(Direction direction, const Message* msg) :
            mDirection(direction), mMsg(msg)
        { }

        bool IsRPCIncall() const
        {
            return mMsg->is_rpc() && IN_MESSAGE == mDirection;
        }

        bool IsRPCOutcall() const
        {
            return mMsg->is_rpc() && OUT_MESSAGE == mDirection;
        }

        void Describe(int32* id, const char** dir, const char** sems,
                      const char** name) const
        {
            *id = mMsg->routing_id();
            *dir = (IN_MESSAGE == mDirection) ? "in" : "out";
            *sems = mMsg->is_rpc() ? "rpc" : mMsg->is_sync() ? "sync" : "async";
            *name = mMsg->name();
        }

        Direction mDirection;
        const Message* mMsg;
    };

    class NS_STACK_CLASS CxxStackFrame
    {
    public:

        CxxStackFrame(RPCChannel& that, Direction direction,
                      const Message* msg) : mThat(that) {
            mThat.AssertWorkerThread();

            if (mThat.mCxxStackFrames.empty())
                mThat.EnteredCxxStack();

            mThat.mCxxStackFrames.push_back(RPCFrame(direction, msg));
            const RPCFrame& frame = mThat.mCxxStackFrames.back();

            if (frame.IsRPCIncall())
                mThat.EnteredCall();

            mThat.mSawRPCOutMsg |= frame.IsRPCOutcall();
        }

        ~CxxStackFrame() {
            bool exitingCall = mThat.mCxxStackFrames.back().IsRPCIncall();
            mThat.mCxxStackFrames.pop_back();
            bool exitingStack = mThat.mCxxStackFrames.empty();

            // mListener could have gone away if Close() was called while
            // RPCChannel code was still on the stack
            if (!mThat.mListener)
                return;

            mThat.AssertWorkerThread();
            if (exitingCall)
                mThat.ExitedCall();

            if (exitingStack)
                mThat.ExitedCxxStack();
        }
    private:
        RPCChannel& mThat;

        // disable harmful methods
        CxxStackFrame();
        CxxStackFrame(const CxxStackFrame&);
        CxxStackFrame& operator=(const CxxStackFrame&);
    };

    // Called from both threads
    size_t StackDepth() const {
        mMutex.AssertCurrentThreadOwns();
        return mStack.size();
    }

    void DebugAbort(const char* file, int line, const char* cond,
                    const char* why,
                    const char* type="rpc", bool reply=false) const;

    // This method is only safe to call on the worker thread, or in a
    // debugger with all threads paused.  |outfile| defaults to stdout.
    void DumpRPCStack(FILE* outfile=NULL, const char* const pfx="") const;

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
    typedef std::queue<Message> MessageQueue;
    MessageQueue mPending;

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

    // True iff the parent has put us in a |BlockChild()| state.
    bool mBlockedOnParent;

    // Approximation of Sync/RPCChannel-code frames on the C++ stack.
    // It can only be interpreted as the implication
    //
    //  !mCxxStackFrames.empty() => RPCChannel code on C++ stack
    //
    // This member is only accessed on the worker thread, and so is
    // not protected by mMutex.  It is managed exclusively by the
    // helper |class CxxStackFrame|.
    std::vector<RPCFrame> mCxxStackFrames;

    // Did we process an RPC out-call during this stack?  Only
    // meaningful in ExitedCxxStack(), from which this variable is
    // reset.
    bool mSawRPCOutMsg;

private:

    //
    // All dequeuing tasks require a single point of cancellation,
    // which is handled via a reference-counted task.
    //
    class RefCountedTask
    {
      public:
        RefCountedTask(CancelableTask* aTask)
        : mTask(aTask)
        , mRefCnt(0) {}
        ~RefCountedTask() { delete mTask; }
        void Run() { mTask->Run(); }
        void Cancel() { mTask->Cancel(); }
        void AddRef() {
            NS_AtomicIncrementRefcnt(mRefCnt);
        }
        void Release() {
            if (NS_AtomicDecrementRefcnt(mRefCnt) == 0)
                delete this;
        }

      private:
        CancelableTask* mTask;
        nsrefcnt mRefCnt;
    };

    //
    // Wrap an existing task which can be cancelled at any time
    // without the wrapper's knowledge.
    //
    class DequeueTask : public Task
    {
      public:
        DequeueTask(RefCountedTask* aTask) : mTask(aTask) {}
        void Run() { mTask->Run(); }
        
      private:
        nsRefPtr<RefCountedTask> mTask;
    };

    // A task encapsulating dequeuing one pending task
    nsRefPtr<RefCountedTask> mDequeueOneTask;
};


} // namespace ipc
} // namespace mozilla
#endif  // ifndef ipc_glue_RPCChannel_h
