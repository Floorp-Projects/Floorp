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
    typedef IPC::Message Message;

    virtual ~MessageListener() { }

    virtual void OnChannelClose() = 0;
    virtual void OnChannelError() = 0;
    virtual Result OnMessageReceived(const Message& aMessage) = 0;
    virtual Result OnMessageReceived(const Message& aMessage, Message *& aReply) = 0;
    virtual Result OnCallReceived(const Message& aMessage, Message *& aReply) = 0;
    virtual void OnProcessingError(Result aError) = 0;
    virtual void OnChannelConnected(int32_t peer_pid) {}
    virtual bool OnReplyTimeout() {
        return false;
    }

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
    virtual RacyInterruptPolicy MediateInterruptRace(const Message& parent,
                                                     const Message& child)
    {
        return RIPChildWins;
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

    MessageLink(MessageChannel *aChan);
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
        NS_ABORT_IF_FALSE(mIOLoop == MessageLoop::current(),
                          "not on I/O thread!");
    }

  public:
    ProcessLink(MessageChannel *chan);
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

    virtual void EchoMessage(Message *msg) MOZ_OVERRIDE;
    virtual void SendMessage(Message *msg) MOZ_OVERRIDE;
    virtual void SendClose() MOZ_OVERRIDE;

    virtual bool Unsound_IsClosed() const MOZ_OVERRIDE;
    virtual uint32_t Unsound_NumQueuedMessages() const MOZ_OVERRIDE;

  protected:
    MessageChannel* mTargetChan;
};

} // namespace ipc
} // namespace mozilla

#endif  // ifndef ipc_glue_MessageLink_h

