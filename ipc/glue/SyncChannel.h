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

#ifndef ipc_glue_SyncChannel_h
#define ipc_glue_SyncChannel_h 1

#include "mozilla/ipc/AsyncChannel.h"

namespace mozilla {
namespace ipc {
//-----------------------------------------------------------------------------

class SyncChannel : public AsyncChannel
{
protected:
    typedef uint16 MessageId;

public:
    static const int32 kNoTimeout;

    class /*NS_INTERFACE_CLASS*/ SyncListener : 
        public AsyncChannel::AsyncListener
    {
    public:
        virtual ~SyncListener() { }

        virtual void OnChannelClose() = 0;
        virtual void OnChannelError() = 0;
        virtual Result OnMessageReceived(const Message& aMessage) = 0;
        virtual void OnProcessingError(Result aError) = 0;
        virtual bool OnReplyTimeout() = 0;
        virtual Result OnMessageReceived(const Message& aMessage,
                                         Message*& aReply) = 0;
    };

    SyncChannel(SyncListener* aListener);
    virtual ~SyncChannel();

    NS_OVERRIDE
    virtual bool Send(Message* msg) {
        return AsyncChannel::Send(msg);
    }

    // Synchronously send |msg| (i.e., wait for |reply|)
    virtual bool Send(Message* msg, Message* reply);

    void SetReplyTimeoutMs(int32 aTimeoutMs) {
        AssertWorkerThread();
        mTimeoutMs = (aTimeoutMs <= 0) ? kNoTimeout : aTimeoutMs;
    }

    // Override the AsyncChannel handler so we can dispatch sync messages
    NS_OVERRIDE virtual void OnMessageReceived(const Message& msg);
    NS_OVERRIDE virtual void OnChannelError();

    static bool IsPumpingMessages() {
        return sIsPumpingMessages;
    }
    static void SetIsPumpingMessages(bool aIsPumping) {
        sIsPumpingMessages = aIsPumping;
    }

#ifdef OS_WIN
    struct NS_STACK_CLASS SyncStackFrame
    {
        SyncStackFrame(SyncChannel* channel, bool rpc);
        ~SyncStackFrame();

        bool mRPC;
        bool mSpinNestedEvents;
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
    // Executed on the worker thread
    bool ProcessingSyncMessage() const {
        return mProcessingSyncMessage;
    }

    void OnDispatchMessage(const Message& aMsg);

    NS_OVERRIDE
    bool OnSpecialMessage(uint16 id, const Message& msg)
    {
        // SyncChannel doesn't care about any special messages yet
        return AsyncChannel::OnSpecialMessage(id, msg);
    }

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
        mMutex.AssertCurrentThreadOwns();
        return mPendingReply != 0;
    }

    int32 NextSeqno() {
        AssertWorkerThread();
        return mChild ? --mNextSeqno : ++mNextSeqno;
    }

    MessageId mPendingReply;
    bool mProcessingSyncMessage;
    Message mRecvd;
    // This is only accessed from the worker thread; seqno's are
    // completely opaque to the IO thread.
    int32 mNextSeqno;

    static bool sIsPumpingMessages;

    int32 mTimeoutMs;

#ifdef OS_WIN
    HANDLE mEvent;
#endif

private:
    bool EventOccurred();
};


} // namespace ipc
} // namespace mozilla
#endif  // ifndef ipc_glue_SyncChannel_h
