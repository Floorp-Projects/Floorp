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

#ifndef ipc_glue_AsyncChannel_h
#define ipc_glue_AsyncChannel_h 1

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "chrome/common/ipc_channel.h"

#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"


//-----------------------------------------------------------------------------

namespace mozilla {
namespace ipc {

struct HasResultCodes
{
    enum Result {
        MsgProcessed,
        MsgNotKnown,
        MsgNotAllowed,
        MsgPayloadError,
        MsgRouteError,
        MsgValueError,
    };
};

class AsyncChannel : public IPC::Channel::Listener, protected HasResultCodes
{
protected:
    typedef mozilla::CondVar CondVar;
    typedef mozilla::Mutex Mutex;

    enum ChannelState {
        ChannelClosed,
        ChannelOpening,
        ChannelConnected,
        ChannelTimeout,
        ChannelClosing,
        ChannelError
    };

public:
    typedef IPC::Channel Transport;
    typedef IPC::Message Message;

    class /*NS_INTERFACE_CLASS*/ AsyncListener: protected HasResultCodes
    {
    public:
        virtual ~AsyncListener() { }

        virtual void OnChannelClose() = 0;
        virtual void OnChannelError() = 0;
        virtual Result OnMessageReceived(const Message& aMessage) = 0;
    };

public:
    //
    // These methods are called on the "worker" thread
    //
    AsyncChannel(AsyncListener* aListener);
    virtual ~AsyncChannel();

    // "Open" from the perspective of the transport layer; the underlying
    // socketpair/pipe should already be created.
    //
    // Returns true iff the transport layer was successfully connected,
    // i.e., mChannelState == ChannelConnected.
    bool Open(Transport* aTransport, MessageLoop* aIOLoop=0);
    
    // Close the underlying transport channel.
    void Close();

    // Asynchronously send a message to the other side of the channel
    virtual bool Send(Message* msg);

    //
    // These methods are called on the "IO" thread
    //

    // Implement the IPC::Channel::Listener interface
    NS_OVERRIDE virtual void OnMessageReceived(const Message& msg);
    NS_OVERRIDE virtual void OnChannelConnected(int32 peer_pid);
    NS_OVERRIDE virtual void OnChannelError();

protected:
    // Can be run on either thread
    void AssertWorkerThread()
    {
        NS_ABORT_IF_FALSE(mWorkerLoop == MessageLoop::current(),
                          "not on worker thread!");
    }

    void AssertIOThread()
    {
        NS_ABORT_IF_FALSE(mIOLoop == MessageLoop::current(),
                          "not on IO thread!");
    }

    bool Connected() {
        mMutex.AssertCurrentThreadOwns();
        return ChannelConnected == mChannelState;
    }

    // Run on the worker thread
    void OnDispatchMessage(const Message& aMsg);
    virtual bool OnSpecialMessage(uint16 id, const Message& msg);
    void SendSpecialMessage(Message* msg);

    // Tell the IO thread to close the channel and wait for it to ACK.
    void SynchronouslyClose();

    bool MaybeHandleError(Result code, const char* channelName);
    void ReportConnectionError(const char* channelName);

    void PrintErrorMessage(const char* channelName, const char* msg)
    {
        fprintf(stderr, "\n###!!! [%s][%s] Error: %s\n\n",
                mChild ? "Child" : "Parent", channelName, msg);
    }

    // Run on the worker thread

    void SendThroughTransport(Message* msg);

    void OnNotifyMaybeChannelError();
    virtual bool ShouldDeferNotifyMaybeError() {
        return false;
    }
    void NotifyChannelClosed();
    void NotifyMaybeChannelError();

    virtual void Clear();

    // Run on the IO thread

    void OnChannelOpened();
    void OnCloseChannel();
    void PostErrorNotifyTask();

    // Return true if |msg| is a special message targeted at the IO
    // thread, in which case it shouldn't be delivered to the worker.
    bool MaybeInterceptSpecialIOMessage(const Message& msg);
    void ProcessGoodbyeMessage();

    Transport* mTransport;
    AsyncListener* mListener;
    ChannelState mChannelState;
    Mutex mMutex;
    CondVar mCvar;
    MessageLoop* mIOLoop;       // thread where IO happens
    MessageLoop* mWorkerLoop;   // thread where work is done
    bool mChild;                // am I the child or parent?
    CancelableTask* mChannelErrorTask; // NotifyMaybeChannelError runnable
};


} // namespace ipc
} // namespace mozilla
#endif  // ifndef ipc_glue_AsyncChannel_h
