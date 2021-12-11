/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/NodeChannel.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_message_utils.h"
#include "mojo/core/ports/name.h"
#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/ipc/ProtocolMessageUtils.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

template <>
struct IPC::ParamTraits<mozilla::ipc::NodeChannel::Introduction> {
  using paramType = mozilla::ipc::NodeChannel::Introduction;
  static void Write(Message* aMsg, paramType&& aParam) {
    WriteParam(aMsg, aParam.mName);
    WriteParam(aMsg, std::move(aParam.mTransport));
    WriteParam(aMsg, aParam.mMode);
    WriteParam(aMsg, aParam.mMyPid);
    WriteParam(aMsg, aParam.mOtherPid);
  }
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mName) &&
           ReadParam(aMsg, aIter, &aResult->mTransport) &&
           ReadParam(aMsg, aIter, &aResult->mMode) &&
           ReadParam(aMsg, aIter, &aResult->mMyPid) &&
           ReadParam(aMsg, aIter, &aResult->mOtherPid);
  }
};

namespace mozilla::ipc {

NodeChannel::NodeChannel(const NodeName& aName,
                         UniquePtr<IPC::Channel> aChannel, Listener* aListener,
                         int32_t aPid)
    : mListener(aListener),
      mName(aName),
      mOtherPid(aPid),
      mChannel(std::move(aChannel)) {}

NodeChannel::~NodeChannel() {
  AssertIOThread();
  if (!mClosed) {
    mChannel->Close();
  }
}

// Called when the NodeChannel's refcount drops to `0`.
void NodeChannel::Destroy() {
  // Dispatch the `delete` operation to the IO thread. We need to do this even
  // if we're already on the IO thread, as we could be in an `IPC::Channel`
  // callback which unfortunately will not hold a strong reference to keep
  // `this` alive.
  MessageLoop* ioThread = XRE_GetIOMessageLoop();
  if (ioThread->IsAcceptingTasks()) {
    ioThread->PostTask(NewNonOwningRunnableMethod("NodeChannel::Destroy", this,
                                                  &NodeChannel::FinalDestroy));
    return;
  }

  // If the IOThread has already been destroyed, we must be shutting it down and
  // need to synchronously invoke `FinalDestroy` to ensure we're cleaned up
  // before the thread dies. This is safe as we can't be in a non-owning
  // IPC::Channel callback at this point.
  if (MessageLoop::current() == ioThread) {
    FinalDestroy();
    return;
  }

  MOZ_ASSERT_UNREACHABLE("Leaking NodeChannel after IOThread destroyed!");
}

void NodeChannel::FinalDestroy() {
  AssertIOThread();
  delete this;
}

void NodeChannel::Start(bool aCallConnect) {
  AssertIOThread();

  mExistingListener = mChannel->set_listener(this);

  std::queue<IPC::Message> pending;
  if (mExistingListener) {
    mExistingListener->GetQueuedMessages(pending);
  }

  if (aCallConnect) {
    MOZ_ASSERT(pending.empty(), "unopened channel with pending messages?");
    if (!mChannel->Connect()) {
      OnChannelError();
    }
  } else {
    // Check if our channel has already been connected, and knows the other PID.
    int32_t otherPid = mChannel->OtherPid();
    if (otherPid != -1) {
      SetOtherPid(otherPid);
    }

    // Handle any events the previous listener had queued up. Make sure to stop
    // if an error causes our channel to become closed.
    while (!pending.empty() && !mClosed) {
      OnMessageReceived(std::move(pending.front()));
      pending.pop();
    }
  }
}

void NodeChannel::Close() {
  AssertIOThread();

  if (!mClosed) {
    mChannel->Close();
    mChannel->set_listener(mExistingListener);
  }
  mClosed = true;
}

void NodeChannel::SetOtherPid(int32_t aNewPid) {
  AssertIOThread();
  MOZ_ASSERT(aNewPid != -1);

  int32_t previousPid = -1;
  if (!mOtherPid.compare_exchange_strong(previousPid, aNewPid)) {
    // The PID was already set before this call, double-check that it's correct.
    MOZ_RELEASE_ASSERT(previousPid == aNewPid,
                       "Different sources disagree on the correct pid?");
  }
}

void NodeChannel::SendEventMessage(UniquePtr<IPC::Message> aMessage) {
  // Make sure we're not sending a message with one of our special internal
  // types ,as those should only be sent using the corresponding methods on
  // NodeChannel.
  MOZ_DIAGNOSTIC_ASSERT(aMessage->type() != BROADCAST_MESSAGE_TYPE &&
                        aMessage->type() != INTRODUCE_MESSAGE_TYPE &&
                        aMessage->type() != REQUEST_INTRODUCTION_MESSAGE_TYPE &&
                        aMessage->type() != ACCEPT_INVITE_MESSAGE_TYPE);
  SendMessage(std::move(aMessage));
}

void NodeChannel::RequestIntroduction(const NodeName& aPeerName) {
  MOZ_ASSERT(aPeerName != mojo::core::ports::kInvalidNodeName);
  auto message = MakeUnique<IPC::Message>(MSG_ROUTING_CONTROL,
                                          REQUEST_INTRODUCTION_MESSAGE_TYPE);
  WriteParam(message.get(), aPeerName);
  SendMessage(std::move(message));
}

void NodeChannel::Introduce(Introduction aIntroduction) {
  auto message =
      MakeUnique<IPC::Message>(MSG_ROUTING_CONTROL, INTRODUCE_MESSAGE_TYPE);
  WriteParam(message.get(), std::move(aIntroduction));
  SendMessage(std::move(message));
}

void NodeChannel::Broadcast(UniquePtr<IPC::Message> aMessage) {
  MOZ_DIAGNOSTIC_ASSERT(aMessage->type() == BROADCAST_MESSAGE_TYPE,
                        "Can only broadcast messages with the correct type");
  SendMessage(std::move(aMessage));
}

void NodeChannel::AcceptInvite(const NodeName& aRealName,
                               const PortName& aInitialPort) {
  MOZ_ASSERT(aRealName != mojo::core::ports::kInvalidNodeName);
  MOZ_ASSERT(aInitialPort != mojo::core::ports::kInvalidPortName);
  auto message =
      MakeUnique<IPC::Message>(MSG_ROUTING_CONTROL, ACCEPT_INVITE_MESSAGE_TYPE);
  WriteParam(message.get(), aRealName);
  WriteParam(message.get(), aInitialPort);
  SendMessage(std::move(message));
}

void NodeChannel::SendMessage(UniquePtr<IPC::Message> aMessage) {
  if (aMessage->size() > IPC::Channel::kMaximumMessageSize) {
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::IPCMessageName,
        nsDependentCString(aMessage->name()));
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::IPCMessageSize,
        static_cast<unsigned int>(aMessage->size()));
    MOZ_CRASH("IPC message size is too large");
  }
  aMessage->AssertAsLargeAsHeader();

  XRE_GetIOMessageLoop()->PostTask(
      NewRunnableMethod<StoreCopyPassByRRef<UniquePtr<IPC::Message>>>(
          "NodeChannel::DoSendMessage", this, &NodeChannel::DoSendMessage,
          std::move(aMessage)));
}

void NodeChannel::DoSendMessage(UniquePtr<IPC::Message> aMessage) {
  AssertIOThread();
  if (mClosed) {
    NS_WARNING("Dropping message as channel has been closed");
    return;
  }

  if (!mChannel->Send(std::move(aMessage))) {
    NS_WARNING("Call to Send() failed");
    OnChannelError();
  }
}

void NodeChannel::OnMessageReceived(IPC::Message&& aMessage) {
  AssertIOThread();

  if (!aMessage.is_valid()) {
    NS_WARNING("Received an invalid message");
    OnChannelError();
    return;
  }

  PickleIterator iter(aMessage);
  switch (aMessage.type()) {
    case REQUEST_INTRODUCTION_MESSAGE_TYPE: {
      NodeName name;
      if (IPC::ReadParam(&aMessage, &iter, &name)) {
        mListener->OnRequestIntroduction(mName, name);
        return;
      }
      break;
    }
    case INTRODUCE_MESSAGE_TYPE: {
      Introduction introduction;
      if (IPC::ReadParam(&aMessage, &iter, &introduction)) {
        mListener->OnIntroduce(mName, std::move(introduction));
        return;
      }
      break;
    }
    case BROADCAST_MESSAGE_TYPE: {
      mListener->OnBroadcast(mName,
                             MakeUnique<IPC::Message>(std::move(aMessage)));
      return;
    }
    case ACCEPT_INVITE_MESSAGE_TYPE: {
      NodeName realName;
      PortName initialPort;
      if (IPC::ReadParam(&aMessage, &iter, &realName) &&
          IPC::ReadParam(&aMessage, &iter, &initialPort)) {
        mListener->OnAcceptInvite(mName, realName, initialPort);
        return;
      }
      break;
    }
    // Assume all unrecognized types are intended as user event messages, and
    // deliver them to our listener as such. This allows us to use the same type
    // field for both internal messages and protocol messages.
    //
    // FIXME: Consider doing something cleaner in the future?
    case EVENT_MESSAGE_TYPE:
    default: {
      mListener->OnEventMessage(mName,
                                MakeUnique<IPC::Message>(std::move(aMessage)));
      return;
    }
  }

  // If we got to this point without early returning the message was malformed
  // in some way. Report an error.

  NS_WARNING("NodeChannel received a malformed message");
  OnChannelError();
}

void NodeChannel::OnChannelConnected(int32_t aPeerPid) {
  AssertIOThread();

  SetOtherPid(aPeerPid);

  // We may need to tell our original listener (which will be the process launch
  // code) that the the channel has been connected to unblock completing the
  // process launch.
  // FIXME: This is super sketchy, but it's also what we were already doing. We
  // should swap this out for something less sketchy.
  if (mExistingListener) {
    mExistingListener->OnChannelConnected(aPeerPid);
  }
}

void NodeChannel::OnChannelError() {
  AssertIOThread();

  // Clean up the channel and make sure we're no longer the active listener.
  mChannel->Close();
  MOZ_ALWAYS_TRUE(this == mChannel->set_listener(mExistingListener));
  mClosed = true;

  // Tell our listener about the error.
  mListener->OnChannelError(mName);
}

}  // namespace mozilla::ipc
