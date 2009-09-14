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
    NS_ABORT_IF_FALSE(!ProcessingSyncMessage(),
                      "violation of sync handler invariant");
    NS_ASSERTION(ChannelConnected == mChannelState,
                 "trying to Send() to a channel not yet open");
    NS_PRECONDITION(msg->is_sync(), "can only Send() sync messages here");

    MutexAutoLock lock(mMutex);

    mPendingReply = msg->type() + 1;
    /*assert*/AsyncChannel::Send(msg);

    // wait for the next sync message to arrive
    mCvar.Wait();

    // we just received a synchronous message from the other side.
    // If it's not the reply we were awaiting, there's a serious
    // error: either a mistimed/malformed message or a sync in-message
    // that raced with our sync out-message.
    // (NB: IPDL prevents the latter from occuring in actor code)

    // FIXME/cjones: real error handling
    NS_ABORT_IF_FALSE(mRecvd.is_sync() && mRecvd.is_reply() &&
                      (mPendingReply == mRecvd.type() || mRecvd.is_reply_error()),
                      "unexpected sync message");

    mPendingReply = 0;
    *reply = mRecvd;

    return true;
}

void
SyncChannel::OnDispatchMessage(const Message& msg)
{
    NS_ABORT_IF_FALSE(msg.is_sync(), "only sync messages here");
    NS_ABORT_IF_FALSE(!msg.is_reply(), "wasn't awaiting reply");

    Message* reply;

    mProcessingSyncMessage = true;
    Result rv =
        static_cast<SyncListener*>(mListener)->OnMessageReceived(msg, reply);
    mProcessingSyncMessage = false;

    switch (rv) {
    case MsgProcessed:
        break;

    case MsgNotKnown:
    case MsgNotAllowed:
    case MsgPayloadError:
    case MsgRouteError:
    case MsgValueError:
        // FIXME/cjones: error handling; OnError()?
        delete reply;
        reply = new Message();
        reply->set_sync();
        reply->set_reply();
        reply->set_reply_error();
        break;

    default:
        NOTREACHED();
        return;
    }

    mIOLoop->PostTask(FROM_HERE,
                      NewRunnableMethod(this,
                                        &SyncChannel::OnSendReply,
                                        reply));
}

//
// The methods below run in the context of the IO thread, and can proxy
// back to the methods above
//

void
SyncChannel::OnMessageReceived(const Message& msg)
{
    if (!msg.is_sync()) {
        return AsyncChannel::OnMessageReceived(msg);
    }

    MutexAutoLock lock(mMutex);

    if (!AwaitingSyncReply()) {
        // wake up the worker, there's work to do
        mWorkerLoop->PostTask(
            FROM_HERE,
            NewRunnableMethod(this, &SyncChannel::OnDispatchMessage, msg));
    }
    else {
        // let the worker know a new sync message has arrived
        mRecvd = msg;
        mCvar.Notify();
    }
}

void
SyncChannel::OnSendReply(Message* aReply)
{
    mTransport->Send(aReply);
}


} // namespace ipc
} // namespace mozilla
