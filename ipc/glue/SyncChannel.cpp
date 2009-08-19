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

#include "mozilla/ipc/SyncChannel.h"
#include "mozilla/ipc/GeckoThread.h"

#include "nsDebug.h"

using mozilla::MutexAutoLock;

template<>
struct RunnableMethodTraits<mozilla::ipc::SyncChannel>
{
    static void RetainCallee(mozilla::ipc::SyncChannel* obj) { }
    static void ReleaseCallee(mozilla::ipc::SyncChannel* obj) { }
};

namespace mozilla {
namespace ipc {

bool
SyncChannel::Send(Message* msg, Message* reply)
{
    NS_ASSERTION(ChannelIdle == mChannelState
                 || ChannelWaiting == mChannelState,
                 "trying to Send() to a channel not yet open");

    NS_PRECONDITION(msg->is_sync(), "can only Send() sync messages here");

    MutexAutoLock lock(mMutex);

    mChannelState = ChannelWaiting;
    mPendingReply = msg->type() + 1;
    /*assert*/AsyncChannel::Send(msg);

    while (1) {
        // here we're waiting for something to happen.  it may be either:
        //  (1) the reply we're waiting for (mPendingReply)
        // or
        //  (2) any other message
        //
        // In case (1), we return this reply back to the caller.
        // In case (2), we defer processing of the message until our reply
        // comes back.
        mCvar.Wait();

        if (mRecvd.is_reply() && mPendingReply == mRecvd.type()) {
            // case (1)
            mPendingReply = 0;
            *reply = mRecvd;

            if (!WaitingForReply()) {
                mChannelState = ChannelIdle;
            }

            return true;
        }
        else {
            // case (2)
            NS_ASSERTION(!mRecvd.is_reply(), "can't process replies here");
            // post a task to our own event loop; this delays processing
            // of mRecvd
            mWorkerLoop->PostTask(
                FROM_HERE,
                NewRunnableMethod(this, &SyncChannel::OnDispatchMessage, mRecvd));
        }
    }
}

void
SyncChannel::OnDispatchMessage(const Message& msg)
{
    NS_ASSERTION(!msg.is_reply(), "can't process replies here");
    NS_ASSERTION(!msg.is_rpc(), "sync or async only here");

    if (!msg.is_sync()) {
        return AsyncChannel::OnDispatchMessage(msg);
    }

    Message* reply;
    switch (static_cast<SyncListener*>(mListener)->OnMessageReceived(msg, reply)) {
    case MsgProcessed:
        mIOLoop->PostTask(FROM_HERE,
                          NewRunnableMethod(this,
                                            &SyncChannel::OnSendReply,
                                            reply));
        return;

    case MsgNotKnown:
    case MsgNotAllowed:
    case MsgPayloadError:
    case MsgRouteError:
    case MsgValueError:
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
SyncChannel::OnMessageReceived(const Message& msg)
{
    mMutex.Lock();

    if (ChannelIdle == mChannelState) {
        // wake up the worker, there's work to do
        if (msg.is_sync()) {
            mWorkerLoop->PostTask(
                FROM_HERE,
                NewRunnableMethod(this, &SyncChannel::OnDispatchMessage, msg));
        }
        else {
            mMutex.Unlock();
            return AsyncChannel::OnMessageReceived(msg);
        }
    }
    else if (ChannelWaiting == mChannelState) {
        // let the worker know something new has happened
        mRecvd = msg;
        mCvar.Notify();
    }
    else {
        // FIXME/cjones: could reach here in error conditions.  impl me
        NOTREACHED();
    }

    mMutex.Unlock();
}

void
SyncChannel::OnSendReply(Message* aReply)
{
    mTransport->Send(aReply);
}


} // namespace ipc
} // namespace mozilla
