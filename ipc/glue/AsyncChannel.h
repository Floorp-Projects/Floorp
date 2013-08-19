/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ipc_glue_AsyncChannel_h
#define ipc_glue_AsyncChannel_h 1

#include "base/basictypes.h"
#include "base/message_loop.h"

#include "mozilla/WeakPtr.h"
#include "mozilla/Monitor.h"
#include "mozilla/ipc/Transport.h"
#include "nsAutoPtr.h"

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

    class /*NS_INTERFACE_CLASS*/ AsyncListener
        : protected HasResultCodes
        , public mozilla::SupportsWeakPtr<AsyncListener>
    {
    public:
        virtual ~AsyncListener() { }

        virtual void OnChannelClose() = 0;
        virtual void OnChannelError() = 0;
        virtual Result OnMessageReceived(const Message& aMessage) = 0;
        virtual void OnProcessingError(Result aError) = 0;
        // FIXME/bug 792652: this doesn't really belong here, but a
        // large refactoring is needed to put it where it belongs.
        virtual int32_t GetProtocolTypeId() = 0;
        virtual void OnChannelConnected(int32_t peer_pid) {}
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

    // Force the channel to behave as if a channel error occurred. Valid
    // for process links only, not thread links.
    void CloseWithError();

    // Asynchronously send a message to the other side of the channel
    virtual bool Send(Message* msg);

    // Asynchronously deliver a message back to this side of the
    // channel
    virtual bool Echo(Message* msg);

    // Send OnChannelConnected notification to listeners.
    void DispatchOnChannelConnected(int32_t peer_pid);

    // Unsound_IsClosed and Unsound_NumQueuedMessages are safe to call from any
    // thread, but they make no guarantees about whether you'll get an
    // up-to-date value; the values are written on one thread and read without
    // locking, on potentially different threads.  Thus you should only use
    // them when you don't particularly care about getting a recent value (e.g.
    // in a memory report).
    bool Unsound_IsClosed() const;
    uint32_t Unsound_NumQueuedMessages() const;

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

        virtual bool Unsound_IsClosed() const = 0;
        virtual uint32_t Unsound_NumQueuedMessages() const = 0;
    };

    class ProcessLink : public Link, public Transport::Listener {
    protected:
        Transport* mTransport;
        MessageLoop* mIOLoop;       // thread where IO happens
        Transport::Listener* mExistingListener; // channel's previous listener
    
        void OnCloseChannel();
        void OnChannelOpened();
        void OnTakeConnectedChannel();
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
        virtual void OnMessageReceived(const Message& msg) MOZ_OVERRIDE;
        virtual void OnChannelConnected(int32_t peer_pid) MOZ_OVERRIDE;
        virtual void OnChannelError() MOZ_OVERRIDE;

        virtual void EchoMessage(Message *msg) MOZ_OVERRIDE;
        virtual void SendMessage(Message *msg) MOZ_OVERRIDE;
        virtual void SendClose() MOZ_OVERRIDE;

        virtual bool Unsound_IsClosed() const MOZ_OVERRIDE;
        virtual uint32_t Unsound_NumQueuedMessages() const MOZ_OVERRIDE;
    };
    
    class ThreadLink : public Link {
    protected:
        AsyncChannel* mTargetChan;
    
    public:
        ThreadLink(AsyncChannel *aChan, AsyncChannel *aTargetChan);
        virtual ~ThreadLink();

        virtual void EchoMessage(Message *msg) MOZ_OVERRIDE;
        virtual void SendMessage(Message *msg) MOZ_OVERRIDE;
        virtual void SendClose() MOZ_OVERRIDE;

        virtual bool Unsound_IsClosed() const MOZ_OVERRIDE;
        virtual uint32_t Unsound_NumQueuedMessages() const MOZ_OVERRIDE;
    };

protected:
    // The "link" thread is either the I/O thread (ProcessLink) or the
    // other actor's work thread (ThreadLink).  In either case, it is
    // NOT our worker thread.
    void AssertLinkThread() const
    {
        NS_ABORT_IF_FALSE(mWorkerLoopID != MessageLoop::current()->id(),
                          "on worker thread but should not be!");
    }

    // Can be run on either thread
    void AssertWorkerThread() const
    {
        NS_ABORT_IF_FALSE(mWorkerLoopID == MessageLoop::current()->id(),
                          "not on worker thread!");
    }

    bool Connected() const {
        mMonitor->AssertCurrentThreadOwns();
        // The transport layer allows us to send messages before
        // receiving the "connected" ack from the remote side.
        return (ChannelOpening == mChannelState ||
                ChannelConnected == mChannelState);
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
    virtual bool OnSpecialMessage(uint16_t id, const Message& msg);
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

    mozilla::WeakPtr<AsyncListener> mListener;
    ChannelState mChannelState;
    nsRefPtr<RefCountedMonitor> mMonitor;
    MessageLoop* mWorkerLoop;   // thread where work is done
    bool mChild;                // am I the child or parent?
    CancelableTask* mChannelErrorTask; // NotifyMaybeChannelError runnable
    Link *mLink;                // link to other thread/process

    // id() of mWorkerLoop.  This persists even after mWorkerLoop is cleared
    // during channel shutdown.
    int mWorkerLoopID;
};

} // namespace ipc
} // namespace mozilla
#endif  // ifndef ipc_glue_AsyncChannel_h
