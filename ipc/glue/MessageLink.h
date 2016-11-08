/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ipc_glue_MessageLink_h
#define ipc_glue_MessageLink_h 1

#include "base/basictypes.h"
#include "base/message_loop.h"

#include "mozilla/WeakPtr.h"
#include "mozilla/ipc/Transport.h"

namespace mozilla {
namespace ipc {

class MessageChannel;

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

enum Side {
    ParentSide,
    ChildSide,
    UnknownSide
};

enum ChannelState {
    ChannelClosed,
    ChannelOpening,
    ChannelConnected,
    ChannelTimeout,
    ChannelClosing,
    ChannelError
};

// What happens if Interrupt calls race?
enum RacyInterruptPolicy {
    RIPError,
    RIPChildWins,
    RIPParentWins
};

class MessageListener
  : protected HasResultCodes,
    public mozilla::SupportsWeakPtr<MessageListener>
{
  public:
    MOZ_DECLARE_WEAKREFERENCE_TYPENAME(MessageListener)
    typedef IPC::Message Message;
    typedef IPC::MessageInfo MessageInfo;

    virtual ~MessageListener() { }

    virtual void OnChannelClose() = 0;
    virtual void OnChannelError() = 0;
    virtual Result OnMessageReceived(const Message& aMessage) = 0;
    virtual Result OnMessageReceived(const Message& aMessage, Message *& aReply) = 0;
    virtual Result OnCallReceived(const Message& aMessage, Message *& aReply) = 0;
    virtual void OnProcessingError(Result aError, const char* aMsgName) = 0;
    virtual void OnChannelConnected(int32_t peer_pid) {}
    virtual bool OnReplyTimeout() {
        return false;
    }

    // WARNING: This function is called with the MessageChannel monitor held.
    virtual void IntentionalCrash() {
        MOZ_CRASH("Intentional IPDL crash");
    }

    // The code here is only useful for fuzzing. It should not be used for any
    // other purpose.
#ifdef DEBUG
    // Returns true if we should simulate a timeout.
    // WARNING: This is a testing-only function that is called with the
    // MessageChannel monitor held. Don't do anything fancy here or we could
    // deadlock.
    virtual bool ArtificialTimeout() {
        return false;
    }

    // Returns true if we want to cause the worker thread to sleep with the
    // monitor unlocked.
    virtual bool NeedArtificialSleep() {
        return false;
    }

    // This function should be implemented to sleep for some amount of time on
    // the worker thread. Will only be called if NeedArtificialSleep() returns
    // true.
    virtual void ArtificialSleep() {}
#else
    bool ArtificialTimeout() { return false; }
    bool NeedArtificialSleep() { return false; }
    void ArtificialSleep() {}
#endif

    virtual void OnEnteredCxxStack() {
        NS_RUNTIMEABORT("default impl shouldn't be invoked");
    }
    virtual void OnExitedCxxStack() {
        NS_RUNTIMEABORT("default impl shouldn't be invoked");
    }
    virtual void OnEnteredCall() {
        NS_RUNTIMEABORT("default impl shouldn't be invoked");
    }
    virtual void OnExitedCall() {
        NS_RUNTIMEABORT("default impl shouldn't be invoked");
    }
    virtual RacyInterruptPolicy MediateInterruptRace(const MessageInfo& parent,
                                                     const MessageInfo& child)
    {
        return RIPChildWins;
    }

    /**
     * Return true if windows messages can be handled while waiting for a reply
     * to a sync IPDL message.
     */
    virtual bool HandleWindowsMessages(const Message& aMsg) const { return true; }

    virtual void OnEnteredSyncSend() {
    }
    virtual void OnExitedSyncSend() {
    }

    virtual void ProcessRemoteNativeEventsInInterruptCall() {
    }

    // FIXME/bug 792652: this doesn't really belong here, but a
    // large refactoring is needed to put it where it belongs.
    virtual int32_t GetProtocolTypeId() = 0;
};

class MessageLink
{
  public:
    typedef IPC::Message Message;

    explicit MessageLink(MessageChannel *aChan);
    virtual ~MessageLink();

    // n.b.: These methods all require that the channel monitor is
    // held when they are invoked.
    virtual void EchoMessage(Message *msg) = 0;
    virtual void SendMessage(Message *msg) = 0;
    virtual void SendClose() = 0;

    virtual bool Unsound_IsClosed() const = 0;
    virtual uint32_t Unsound_NumQueuedMessages() const = 0;

  protected:
    MessageChannel *mChan;
};

class ProcessLink
  : public MessageLink,
    public Transport::Listener
{
    void OnCloseChannel();
    void OnChannelOpened();
    void OnTakeConnectedChannel();
    void OnEchoMessage(Message* msg);

    void AssertIOThread() const
    {
        MOZ_ASSERT(mIOLoop == MessageLoop::current(),
                   "not on I/O thread!");
    }

  public:
    explicit ProcessLink(MessageChannel *chan);
    virtual ~ProcessLink();

    // The ProcessLink will register itself as the IPC::Channel::Listener on the
    // transport passed here. If the transport already has a listener registered
    // then a listener chain will be established (the ProcessLink listener
    // methods will be called first and may call some methods on the original
    // listener as well). Once the channel is closed (either via normal shutdown
    // or a pipe error) the chain will be destroyed and the original listener
    // will again be registered.
    void Open(Transport* aTransport, MessageLoop *aIOLoop, Side aSide);
    
    // Run on the I/O thread, only when using inter-process link.
    // These methods acquire the monitor and forward to the
    // similarly named methods in AsyncChannel below
    // (OnMessageReceivedFromLink(), etc)
    virtual void OnMessageReceived(Message&& msg) override;
    virtual void OnChannelConnected(int32_t peer_pid) override;
    virtual void OnChannelError() override;

    virtual void EchoMessage(Message *msg) override;
    virtual void SendMessage(Message *msg) override;
    virtual void SendClose() override;

    virtual bool Unsound_IsClosed() const override;
    virtual uint32_t Unsound_NumQueuedMessages() const override;

  protected:
    Transport* mTransport;
    MessageLoop* mIOLoop;       // thread where IO happens
    Transport::Listener* mExistingListener; // channel's previous listener
};

class ThreadLink : public MessageLink
{
  public:
    ThreadLink(MessageChannel *aChan, MessageChannel *aTargetChan);
    virtual ~ThreadLink();

    virtual void EchoMessage(Message *msg) override;
    virtual void SendMessage(Message *msg) override;
    virtual void SendClose() override;

    virtual bool Unsound_IsClosed() const override;
    virtual uint32_t Unsound_NumQueuedMessages() const override;

  protected:
    MessageChannel* mTargetChan;
};

} // namespace ipc
} // namespace mozilla

#endif  // ifndef ipc_glue_MessageLink_h

