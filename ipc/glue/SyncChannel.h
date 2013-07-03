/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ipc_glue_SyncChannel_h
#define ipc_glue_SyncChannel_h 1

#include "mozilla/ipc/AsyncChannel.h"

namespace mozilla {
namespace ipc {
//-----------------------------------------------------------------------------

class SyncChannel : public AsyncChannel
{
protected:
    typedef IPC::Message::msgid_t msgid_t;

public:
    static const int32_t kNoTimeout;

    class /*NS_INTERFACE_CLASS*/ SyncListener : 
        public AsyncChannel::AsyncListener
    {
    public:
        virtual ~SyncListener() { }

        virtual void OnChannelClose() = 0;
        virtual void OnChannelError() = 0;
        virtual Result OnMessageReceived(const Message& aMessage) = 0;
        virtual void OnProcessingError(Result aError) = 0;
        virtual int32_t GetProtocolTypeId() = 0;
        virtual bool OnReplyTimeout() = 0;
        virtual Result OnMessageReceived(const Message& aMessage,
                                         Message*& aReply) = 0;
        virtual void OnChannelConnected(int32_t peer_pid) {}
    };

    SyncChannel(SyncListener* aListener);
    virtual ~SyncChannel();

    virtual bool Send(Message* msg) MOZ_OVERRIDE {
        return AsyncChannel::Send(msg);
    }

    // Synchronously send |msg| (i.e., wait for |reply|)
    virtual bool Send(Message* msg, Message* reply);

    // Set channel timeout value. Since this is broken up into
    // two period, the minimum timeout value is 2ms.
    void SetReplyTimeoutMs(int32_t aTimeoutMs) {
        AssertWorkerThread();
        mTimeoutMs = (aTimeoutMs <= 0) ? kNoTimeout :
          // timeouts are broken up into two periods
          (int32_t)ceil((double)aTimeoutMs/2.0);
    }

    static bool IsPumpingMessages() {
        return sIsPumpingMessages;
    }
    static void SetIsPumpingMessages(bool aIsPumping) {
        sIsPumpingMessages = aIsPumping;
    }

#ifdef OS_WIN
public:
    struct MOZ_STACK_CLASS SyncStackFrame
    {
        SyncStackFrame(SyncChannel* channel, bool rpc);
        ~SyncStackFrame();

        bool mRPC;
        bool mSpinNestedEvents;
        bool mListenerNotified;
        SyncChannel* mChannel;

        /* the previous stack frame for this channel */
        SyncStackFrame* mPrev;

        /* the previous stack frame on any channel */
        SyncStackFrame* mStaticPrev;
    };
    friend struct SyncChannel::SyncStackFrame;

    static bool IsSpinLoopActive() {
        for (SyncStackFrame* frame = sStaticTopFrame;
             frame;
             frame = frame->mPrev) {
            if (frame->mSpinNestedEvents)
                return true;
        }
        return false;
    }

protected:
    /* the deepest sync stack frame for this channel */
    SyncStackFrame* mTopFrame;

    /* the deepest sync stack frame on any channel */
    static SyncStackFrame* sStaticTopFrame;
#endif // OS_WIN

protected:
    // Executed on the link thread
    // Override the AsyncChannel handler so we can dispatch sync messages
    virtual void OnMessageReceivedFromLink(const Message& msg) MOZ_OVERRIDE;
    virtual void OnChannelErrorFromLink() MOZ_OVERRIDE;

    // Executed on the worker thread
    bool ProcessingSyncMessage() const {
        return mProcessingSyncMessage;
    }

    void OnDispatchMessage(const Message& aMsg);

    //
    // Return true if the wait ended because a notification was
    // received.  That is, true => event received.
    //
    // Return false if the time elapsed from when we started the
    // process of waiting until afterwards exceeded the currently
    // allotted timeout.  That *DOES NOT* mean false => "no event" (==
    // timeout); there are many circumstances that could cause the
    // measured elapsed time to exceed the timeout EVEN WHEN we were
    // notified.
    //
    // So in sum: true is a meaningful return value; false isn't,
    // necessarily.
    //
    bool WaitForNotify();

    bool ShouldContinueFromTimeout();

    // Executed on the IO thread.
    void NotifyWorkerThread();

    // On both
    bool AwaitingSyncReply() const {
        mMonitor->AssertCurrentThreadOwns();
        return mPendingReply != 0;
    }

    Message TakeReply() {
        Message reply = mRecvd;
        mRecvd = Message();
        return reply;
    }

    int32_t NextSeqno() {
        AssertWorkerThread();
        return mChild ? --mNextSeqno : ++mNextSeqno;
    }

    msgid_t mPendingReply;
    bool mProcessingSyncMessage;
    Message mRecvd;
    // This is only accessed from the worker thread; seqno's are
    // completely opaque to the IO thread.
    int32_t mNextSeqno;

    static bool sIsPumpingMessages;

    // Timeout periods are broken up in two to prevent system suspension from
    // triggering an abort. This method (called by WaitForNotify with a 'did
    // timeout' flag) decides if we should wait again for half of mTimeoutMs
    // or give up.
    bool WaitResponse(bool aWaitTimedOut);
    bool mInTimeoutSecondHalf;
    int32_t mTimeoutMs;

    std::deque<Message> mUrgent;

#ifdef OS_WIN
    HANDLE mEvent;
#endif

private:
    bool EventOccurred();
    bool ProcessUrgentMessages();
};


} // namespace ipc
} // namespace mozilla
#endif  // ifndef ipc_glue_SyncChannel_h
