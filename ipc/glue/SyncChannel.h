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

#include <queue>

#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"
#include "mozilla/ipc/AsyncChannel.h"

namespace mozilla {
namespace ipc {
//-----------------------------------------------------------------------------

class SyncChannel : public AsyncChannel
{
protected:
    typedef mozilla::CondVar CondVar;
    typedef mozilla::Mutex Mutex;
    typedef uint16 MessageId;

public:
    class /*NS_INTERFACE_CLASS*/ SyncListener : 
        public AsyncChannel::AsyncListener
    {
    public:
        virtual ~SyncListener() { }
        virtual Result OnMessageReceived(const Message& aMessage) = 0;
        virtual Result OnMessageReceived(const Message& aMessage,
                                         Message*& aReply) = 0;
    };

    SyncChannel(SyncListener* aListener) :
        AsyncChannel(aListener),
        mMutex("mozilla.ipc.SyncChannel.mMutex"),
        mCvar(mMutex, "mozilla.ipc.SyncChannel.mCvar"),
        mPendingReply(0),
        mProcessingSyncMessage(false)
    {
    }

    virtual ~SyncChannel()
    {
        // FIXME/cjones: impl
    }

    bool Send(Message* msg) {
        return AsyncChannel::Send(msg);
    }

    // Synchronously send |msg| (i.e., wait for |reply|)
    bool Send(Message* msg, Message* reply);

    // Override the AsyncChannel handler so we can dispatch sync messages
    virtual void OnMessageReceived(const Message& msg);

protected:
    // Executed on the worker thread
    bool ProcessingSyncMessage() {
        return mProcessingSyncMessage;
    }

    void OnDispatchMessage(const Message& aMsg);

    // Executed on the IO thread.
    void OnSendReply(Message* msg);

    // On both
    bool AwaitingSyncReply() {
        mMutex.AssertCurrentThreadOwns();
        return mPendingReply != 0;
    }

    Mutex mMutex;
    CondVar mCvar;
    MessageId mPendingReply;
    bool mProcessingSyncMessage;
    Message mRecvd;
};


} // namespace ipc
} // namespace mozilla
#endif  // ifndef ipc_glue_SyncChannel_h
