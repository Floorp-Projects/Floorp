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

#include "mozilla/Monitor.h"
#include "mozilla/ipc/Transport.h"

//-----------------------------------------------------------------------------

namespace mozilla {
namespace ipc {

struct HasResultCodes
{
    enum Result {
        MsgProcessed,
        MsgDropped,
        MsgNotKnown,
        MsgNotAllowed,
        MsgPayloadError,
        MsgProcessingError,
        MsgRouteError,
        MsgValueError
    };
};


class RefCountedMonitor : public Monitor
{
public:
    RefCountedMonitor() 
        : Monitor("mozilla.ipc.AsyncChannel.mMonitor")
    {}

    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefCountedMonitor)
};

class AsyncChannel : protected HasResultCodes
{
protected:
    typedef mozilla::Monitor Monitor;

    enum ChannelState {
        ChannelClosed,
        ChannelOpening,
        ChannelConnected,
        ChannelTimeout,
        ChannelClosing,
        ChannelError
    };

public:
    typedef IPC::Message Message;
    typedef mozilla::ipc::Transport Transport;

    class /*NS_INTERFACE_CLASS*/ AsyncListener: protected HasResultCodes
    {
    public:
        virtual ~AsyncListener() { }

        virtual void OnChannelClose() = 0;
        virtual void OnChannelError() = 0;
        virtual Result OnMessageReceived(const Message& aMessage) = 0;
        virtual void OnProcessingError(Result aError) = 0;
        virtual void OnChannelConnected(int32 peer_pid) {};
    };

    enum Side { Parent, Child, Unknown };

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
    bool Open(Transport* aTransport, MessageLoop* aIOLoop=0, Side aSide=Unknown);
    
    // "Open" a connection to another thread in the same process.
    //
    // Returns true iff the transport layer was successfully connected,
    // i.e., mChannelState == ChannelConnected.
    //
    // For more details on the process of opening a channel between
    // threads, see the extended comment on this function
    // in AsyncChannel.cpp.
    bool Open(AsyncChannel *aTargetChan, MessageLoop *aTargetLoop, Side aSide);

    // Close the underlying transport channel.
    void Close();

    // Asynchronously send a message to the other side of the channel
    virtual bool Send(Message* msg);

    // Asynchronously deliver a message back to this side of the
    // channel
    virtual bool Echo(Message* msg);

    // Send OnChannelConnected notification to listeners.
    void DispatchOnChannelConnected(int32 peer_pid);

    //
    // Each AsyncChannel is associated with either a ProcessLink or a
    // ThreadLink via the field mLink.  The type of link is determined
    // by whether this AsyncChannel is communicating with another
    // process or another thread.  In the former case, file
    // descriptors or a socket are used via the I/O queue.  In the
    // latter case, messages are enqueued directly onto the target
    // thread's work queue.
    //

    class Link {
    protected:
        AsyncChannel *mChan;

    public:
        Link(AsyncChannel *aChan);
        virtual ~Link();

        // n.b.: These methods all require that the channel monitor is
        // held when they are invoked.
        virtual void EchoMessage(Message *msg) = 0;
        virtual void SendMessage(Message *msg) = 0;
        virtual void SendClose() = 0;
    };

    class ProcessLink : public Link, public Transport::Listener {
    protected:
        Transport* mTransport;
        MessageLoop* mIOLoop;       // thread where IO happens
        Transport::Listener* mExistingListener; // channel's previous listener
    
        void OnCloseChannel();
        void OnChannelOpened();
        void OnEchoMessage(Message* msg);

        void AssertIOThread() const
        {
            NS_ABORT_IF_FALSE(mIOLoop == MessageLoop::current(),
                              "not on I/O thread!");
        }

    public:
        ProcessLink(AsyncChannel *chan);
        virtual ~ProcessLink();
        void Open(Transport* aTransport, MessageLoop *aIOLoop, Side aSide);
        
        // Run on the I/O thread, only when using inter-process link.
        // These methods acquire the monitor and forward to the
        // similarly named methods in AsyncChannel below
        // (OnMessageReceivedFromLink(), etc)
        NS_OVERRIDE virtual void OnMessageReceived(const Message& msg);
        NS_OVERRIDE virtual void OnChannelConnected(int32 peer_pid);
        NS_OVERRIDE virtual void OnChannelError();

        NS_OVERRIDE virtual void EchoMessage(Message *msg);
        NS_OVERRIDE virtual void SendMessage(Message *msg);
        NS_OVERRIDE virtual void SendClose();
    };
    
    class ThreadLink : public Link {
    protected:
        AsyncChannel* mTargetChan;
    
    public:
        ThreadLink(AsyncChannel *aChan, AsyncChannel *aTargetChan);
        virtual ~ThreadLink();

        NS_OVERRIDE virtual void EchoMessage(Message *msg);
        NS_OVERRIDE virtual void SendMessage(Message *msg);
        NS_OVERRIDE virtual void SendClose();
    };

protected:
    // The "link" thread is either the I/O thread (ProcessLink) or the
    // other actor's work thread (ThreadLink).  In either case, it is
    // NOT our worker thread.
    void AssertLinkThread() const
    {
        NS_ABORT_IF_FALSE(mWorkerLoop != MessageLoop::current(),
                          "on worker thread but should not be!");
    }

    // Can be run on either thread
    void AssertWorkerThread() const
    {
        NS_ABORT_IF_FALSE(mWorkerLoop == MessageLoop::current(),
                          "not on worker thread!");
    }

    bool Connected() const {
        mMonitor->AssertCurrentThreadOwns();
        return ChannelConnected == mChannelState;
    }

    // Return true if |msg| is a special message targeted at the IO
    // thread, in which case it shouldn't be delivered to the worker.
    virtual bool MaybeInterceptSpecialIOMessage(const Message& msg);
    void ProcessGoodbyeMessage();

    // Runs on the link thread. Invoked either from the I/O thread methods above
    // or directly from the other actor if using a thread-based link.
    // 
    // n.b.: mMonitor is always held when these methods are invoked.
    // In the case of a ProcessLink, it is acquired by the ProcessLink.
    // In the case of a ThreadLink, it is acquired by the other actor, 
    // which then invokes these methods directly.
    virtual void OnMessageReceivedFromLink(const Message& msg);
    virtual void OnChannelErrorFromLink();
    void PostErrorNotifyTask();

    // Run on the worker thread
    void OnDispatchMessage(const Message& aMsg);
    virtual bool OnSpecialMessage(uint16 id, const Message& msg);
    void SendSpecialMessage(Message* msg) const;

    // Tell the IO thread to close the channel and wait for it to ACK.
    void SynchronouslyClose();

    bool MaybeHandleError(Result code, const char* channelName);
    void ReportConnectionError(const char* channelName) const;

    // Run on the worker thread

    void OnNotifyMaybeChannelError();
    virtual bool ShouldDeferNotifyMaybeError() const {
        return false;
    }
    void NotifyChannelClosed();
    void NotifyMaybeChannelError();
    void OnOpenAsSlave(AsyncChannel *aTargetChan, Side aSide);
    void CommonThreadOpenInit(AsyncChannel *aTargetChan, Side aSide);

    virtual void Clear();

    AsyncListener* mListener;
    ChannelState mChannelState;
    nsRefPtr<RefCountedMonitor> mMonitor;
    MessageLoop* mWorkerLoop;   // thread where work is done
    bool mChild;                // am I the child or parent?
    CancelableTask* mChannelErrorTask; // NotifyMaybeChannelError runnable
    Link *mLink;                // link to other thread/process
};

} // namespace ipc
} // namespace mozilla
#endif  // ifndef ipc_glue_AsyncChannel_h
