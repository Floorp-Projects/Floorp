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
RPCChannel::Open(Transport* aTransport, MessageLoop* aIOLoop)
{
    NS_PRECONDITION(!mTransport, "Open() called > once");
    NS_PRECONDITION(aTransport, "need transport layer");

    // FIXME need to check for valid channel

    mTransport = aTransport;
    mTransport->set_listener(this);

    // FIXME do away with this
    bool needOpen = true;
    if(!aIOLoop) {
        needOpen = false;
        aIOLoop = BrowserProcessSubThread
                  ::GetMessageLoop(BrowserProcessSubThread::IO);
    }

    mIOLoop = aIOLoop;
    mWorkerLoop = MessageLoop::current();

    NS_ASSERTION(mIOLoop, "need an IO loop");
    NS_ASSERTION(mWorkerLoop, "need a worker loop");

    if (needOpen) {
        mIOLoop->PostTask(FROM_HERE, 
                          NewRunnableMethod(this,
                                            &RPCChannel::OnChannelOpened));
    }

    return true;
}

void
RPCChannel::Close()
{
    // FIXME impl

    mChannelState = ChannelClosed;
}

bool
RPCChannel::Call(Message* msg, Message* reply)
{
    NS_PRECONDITION(MSG_ROUTING_NONE != msg->routing_id(), "need a route");

    mMutex.Lock();

    mPending.push(*msg);
    mIOLoop->PostTask(FROM_HERE, NewRunnableMethod(this,
                                                   &RPCChannel::SendCall,
                                                   msg));
    while (1) {
        // here we're waiting for something to happen.  it may either
        // be a reply to an outstanding message, or a recursive call
        // from the other side
        mCvar.Wait();

        Message recvd = mPending.top();
        mPending.pop();

        if (recvd.is_reply()) {
            // we received a reply to our most recent message.  pop this
            // frame and return the reply
            NS_ASSERTION(0 < mPending.size(), "invalid RPC stack");
            mPending.pop();
            *reply = recvd;

            mMutex.Unlock();
            return true;
        }
        else {
            mMutex.Unlock();

            // someone called in to us from the other side.  handle the call
            if (!ProcessIncomingCall(recvd))
                return false;

            mMutex.Lock();
        }
    }

    delete msg;

    return true;
}

bool
RPCChannel::ProcessIncomingCall(Message call)
{
   Message* reply;

    switch (mListener->OnCallReceived(call, reply)) {
    case Listener::MsgProcessed:
        mIOLoop->PostTask(FROM_HERE,
                          NewRunnableMethod(this,
                                            &RPCChannel::SendReply,
                                            reply));
        return true;

    case Listener::MsgNotKnown:
    case Listener::MsgNotAllowed:
    case Listener::MsgPayloadError:
    case Listener::MsgRouteError:
    case Listener::MsgValueError:
        //OnError()?
        return false;

    default:
        NOTREACHED();
        return false;
    }
}

void
RPCChannel::OnIncomingCall(Message msg)
{
    NS_ASSERTION(0 == mPending.size(),
                 "woke up the worker thread when it had outstanding work!");
    ProcessIncomingCall(msg);
}

//
// The methods below run in the context of the IO thread, and can proxy
// back to the methods above
//

void
RPCChannel::OnMessageReceived(const Message& msg)
{MutexAutoLock lock(mMutex);
    if (0 == mPending.size()) {
        // wake up the worker, there's work to do
        mWorkerLoop->PostTask(FROM_HERE,
                              NewRunnableMethod(this,
                                                &RPCChannel::OnIncomingCall,
                                                msg));
    }
    else {
        // let the worker know something new has happened
        mPending.push(msg);
        mCvar.Notify();
    }
}

void
RPCChannel::OnChannelConnected(int32 peer_pid)
{
    mChannelState = ChannelConnected;
}

void
RPCChannel::OnChannelError()
{
    // FIXME/cjones impl
    mChannelState = ChannelError;
}

void
RPCChannel::OnChannelOpened()
{
    mChannelState = ChannelOpening;
    /*assert*/mTransport->Connect();
}

void
RPCChannel::SendCall(Message* aCall)
{
    mTransport->Send(aCall);
}

void
RPCChannel::SendReply(Message* aReply)
{
    mTransport->Send(aReply);
}


} // namespace ipc
} // namespace mozilla
