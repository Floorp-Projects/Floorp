/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/NodeController.h"
#include "MainThreadUtils.h"
#include "base/process_util.h"
#include "chrome/common/ipc_message.h"
#include "mojo/core/ports/name.h"
#include "mojo/core/ports/node.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/RandomNum.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Assertions.h"
#include "mozilla/ToString.h"
#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/mozalloc.h"
#include "nsISerialEventTarget.h"
#include "nsTArray.h"
#include "nsXULAppAPI.h"
#include "nsPrintfCString.h"

#define PORTS_ALWAYS_OK(expr) MOZ_ALWAYS_TRUE(mojo::core::ports::OK == (expr))

namespace mozilla::ipc {

static StaticRefPtr<NodeController> gNodeController;

static LazyLogModule gNodeControllerLog{"NodeController"};

// Helper logger macro which includes the name of the `this` NodeController in
// the logged messages.
#define NODECONTROLLER_LOG(level_, fmt_, ...) \
  MOZ_LOG(gNodeControllerLog, level_,         \
          ("[%s]: " fmt_, ToString(mName).c_str(), ##__VA_ARGS__))

// Helper warning macro which both does logger logging and emits NS_WARNING logs
// under debug mode.
#ifdef DEBUG
#  define NODECONTROLLER_WARNING(fmt_, ...)                                \
    do {                                                                   \
      nsPrintfCString warning("[%s]: " fmt_, ToString(mName).c_str(),      \
                              ##__VA_ARGS__);                              \
      NS_WARNING(warning.get());                                           \
      MOZ_LOG(gNodeControllerLog, LogLevel::Debug, ("%s", warning.get())); \
    } while (0)
#else
#  define NODECONTROLLER_WARNING(fmt_, ...) \
    NODECONTROLLER_LOG(LogLevel::Warning, fmt_, ##__VA_ARGS__)
#endif

NodeController::NodeController(const NodeName& aName)
    : mName(aName), mNode(MakeUnique<Node>(aName, this)) {}

NodeController::~NodeController() {
  auto state = mState.Lock();
  MOZ_RELEASE_ASSERT(state->mPeers.IsEmpty(),
                     "Destroying NodeController before closing all peers");
  MOZ_RELEASE_ASSERT(state->mInvites.IsEmpty(),
                     "Destroying NodeController before closing all invites");
};

// FIXME: Actually provide some way to create the thing.
/* static */ NodeController* NodeController::GetSingleton() {
  MOZ_ASSERT(gNodeController);
  return gNodeController;
}

std::pair<ScopedPort, ScopedPort> NodeController::CreatePortPair() {
  PortRef port0, port1;
  PORTS_ALWAYS_OK(mNode->CreatePortPair(&port0, &port1));
  return {ScopedPort{std::move(port0), this},
          ScopedPort{std::move(port1), this}};
}

auto NodeController::GetPort(const PortName& aName) -> PortRef {
  PortRef port;
  int rv = mNode->GetPort(aName, &port);
  if (NS_WARN_IF(rv != mojo::core::ports::OK)) {
    NODECONTROLLER_WARNING("Call to GetPort(%s) Failed",
                           ToString(aName).c_str());
    return {};
  }
  return port;
}

void NodeController::SetPortObserver(const PortRef& aPort,
                                     PortObserver* aObserver) {
  PORTS_ALWAYS_OK(mNode->SetUserData(aPort, aObserver));
}

auto NodeController::GetStatus(const PortRef& aPort) -> Maybe<PortStatus> {
  PortStatus status{};
  int rv = mNode->GetStatus(aPort, &status);
  if (rv != mojo::core::ports::OK) {
    return Nothing();
  }
  return Some(status);
}

void NodeController::ClosePort(const PortRef& aPort) {
  PORTS_ALWAYS_OK(mNode->ClosePort(aPort));
}

bool NodeController::GetMessage(const PortRef& aPort,
                                UniquePtr<IPC::Message>* aMessage) {
  UniquePtr<UserMessageEvent> messageEvent;
  int rv = mNode->GetMessage(aPort, &messageEvent, nullptr);
  if (rv != mojo::core::ports::OK) {
    if (rv == mojo::core::ports::ERROR_PORT_PEER_CLOSED) {
      return false;
    }
    MOZ_CRASH("GetMessage on port in invalid state");
  }

  if (messageEvent) {
    UniquePtr<IPC::Message> message = messageEvent->TakeMessage<IPC::Message>();

    // If our UserMessageEvent has any ports directly attached to it, fetch them
    // from our node and attach them to the IPC::Message we extracted.
    //
    // It's important to only do this if we have nonempty set of ports on the
    // event, as we may have never serialized our IPC::Message's ports onto the
    // event if it was routed in-process.
    if (messageEvent->num_ports() > 0) {
      nsTArray<ScopedPort> attachedPorts(messageEvent->num_ports());
      for (size_t i = 0; i < messageEvent->num_ports(); ++i) {
        attachedPorts.AppendElement(
            ScopedPort{GetPort(messageEvent->ports()[i]), this});
      }
      message->SetAttachedPorts(std::move(attachedPorts));
    }

    *aMessage = std::move(message);
  } else {
    *aMessage = nullptr;
  }
  return true;
}

bool NodeController::SendUserMessage(const PortRef& aPort,
                                     UniquePtr<IPC::Message> aMessage) {
  auto messageEvent = MakeUnique<UserMessageEvent>(0);
  messageEvent->AttachMessage(std::move(aMessage));

  int rv = mNode->SendUserMessage(aPort, std::move(messageEvent));
  if (rv == mojo::core::ports::OK) {
    return true;
  }
  if (rv == mojo::core::ports::ERROR_PORT_PEER_CLOSED) {
    NODECONTROLLER_LOG(LogLevel::Debug,
                       "Ignoring message to port %s as peer was closed",
                       ToString(aPort.name()).c_str());
    return true;
  }
  NODECONTROLLER_WARNING("Failed to send message to port %s",
                         ToString(aPort.name()).c_str());
  return false;
}

auto NodeController::SerializeEventMessage(UniquePtr<Event> aEvent,
                                           const NodeName* aRelayTarget,
                                           uint32_t aType)
    -> UniquePtr<IPC::Message> {
  UniquePtr<IPC::Message> message;
  if (aEvent->type() == Event::kUserMessage) {
    MOZ_DIAGNOSTIC_ASSERT(
        aType == EVENT_MESSAGE_TYPE,
        "Can only send a UserMessage in an EVENT_MESSAGE_TYPE");
    message = static_cast<UserMessageEvent*>(aEvent.get())
                  ->TakeMessage<IPC::Message>();
  } else {
    message = MakeUnique<IPC::Message>(MSG_ROUTING_CONTROL, aType);
  }

  message->set_relay(aRelayTarget != nullptr);

  size_t length = aEvent->GetSerializedSize();
  if (aRelayTarget) {
    length += sizeof(NodeName);
  }

  // Use an intermediate buffer to serialize to avoid potential issues with the
  // segmented `IPC::Message` bufferlist. This should be fairly cheap, as the
  // majority of events are fairly small.
  Vector<char, 256, InfallibleAllocPolicy> buffer;
  (void)buffer.initLengthUninitialized(length);
  if (aRelayTarget) {
    memcpy(buffer.begin(), aRelayTarget, sizeof(NodeName));
    aEvent->Serialize(buffer.begin() + sizeof(NodeName));
  } else {
    aEvent->Serialize(buffer.begin());
  }

  message->WriteFooter(buffer.begin(), buffer.length());
  message->set_event_footer_size(buffer.length());

#ifdef DEBUG
  // Debug-assert that we can read the same data back out of the buffer.
  MOZ_ASSERT(message->event_footer_size() == length);
  Vector<char, 256, InfallibleAllocPolicy> buffer2;
  (void)buffer2.initLengthUninitialized(message->event_footer_size());
  MOZ_ASSERT(message->ReadFooter(buffer2.begin(), buffer2.length(),
                                 /* truncate */ false));
  MOZ_ASSERT(!memcmp(buffer2.begin(), buffer.begin(), buffer.length()));
#endif

  return message;
}

auto NodeController::DeserializeEventMessage(UniquePtr<IPC::Message> aMessage,
                                             NodeName* aRelayTarget)
    -> UniquePtr<Event> {
  if (aMessage->is_relay() && !aRelayTarget) {
    NODECONTROLLER_WARNING("Unexpected relay message '%s'", aMessage->name());
    return nullptr;
  }

  Vector<char, 256, InfallibleAllocPolicy> buffer;
  (void)buffer.initLengthUninitialized(aMessage->event_footer_size());
  // Truncate the message when reading the footer, so that the extra footer data
  // is no longer present in the message. This allows future code to eventually
  // send the same `IPC::Message` to another process.
  if (!aMessage->ReadFooter(buffer.begin(), buffer.length(),
                            /* truncate */ true)) {
    NODECONTROLLER_WARNING("Call to ReadFooter for message '%s' Failed",
                           aMessage->name());
    return nullptr;
  }
  aMessage->set_event_footer_size(0);

  UniquePtr<Event> event;
  if (aRelayTarget) {
    MOZ_ASSERT(aMessage->is_relay());
    if (buffer.length() < sizeof(NodeName)) {
      NODECONTROLLER_WARNING(
          "Insufficient space in message footer for message '%s'",
          aMessage->name());
      return nullptr;
    }
    memcpy(aRelayTarget, buffer.begin(), sizeof(NodeName));
    event = Event::Deserialize(buffer.begin() + sizeof(NodeName),
                               buffer.length() - sizeof(NodeName));
  } else {
    event = Event::Deserialize(buffer.begin(), buffer.length());
  }

  if (!event) {
    NODECONTROLLER_WARNING("Call to Event::Deserialize for message '%s' Failed",
                           aMessage->name());
    return nullptr;
  }

  if (event->type() == Event::kUserMessage) {
    static_cast<UserMessageEvent*>(event.get())
        ->AttachMessage(std::move(aMessage));
  }
  return event;
}

already_AddRefed<NodeChannel> NodeController::GetNodeChannel(
    const NodeName& aName) {
  auto state = mState.Lock();
  return do_AddRef(state->mPeers.Get(aName));
}

void NodeController::DropPeer(NodeName aNodeName) {
  AssertIOThread();

#ifdef FUZZING_SNAPSHOT
  MOZ_FUZZING_IPC_DROP_PEER("NodeController::DropPeer");
#endif

  Invite invite;
  RefPtr<NodeChannel> channel;
  nsTArray<PortRef> pendingMerges;
  {
    auto state = mState.Lock();
    state->mPeers.Remove(aNodeName, &channel);
    state->mPendingMessages.Remove(aNodeName);
    state->mInvites.Remove(aNodeName, &invite);
    state->mPendingMerges.Remove(aNodeName, &pendingMerges);
  }

  NODECONTROLLER_LOG(LogLevel::Info, "Dropping Peer %s (pid: %" PRIPID ")",
                     ToString(aNodeName).c_str(),
                     channel ? channel->OtherPid() : base::kInvalidProcessId);

  if (channel) {
    channel->Close();
  }
  if (invite.mChannel) {
    invite.mChannel->Close();
  }
  if (invite.mToMerge.is_valid()) {
    // Ignore any possible errors here.
    (void)mNode->ClosePort(invite.mToMerge);
  }
  for (auto& port : pendingMerges) {
    // Ignore any possible errors here.
    (void)mNode->ClosePort(port);
  }
  mNode->LostConnectionToNode(aNodeName);
}

void NodeController::ForwardEvent(const NodeName& aNode,
                                  UniquePtr<Event> aEvent) {
  if (aNode == mName) {
    (void)mNode->AcceptEvent(std::move(aEvent));
  } else {
    // On Windows and macOS, messages holding HANDLEs or mach ports must be
    // relayed via the broker process so it can transfer ownership.
    bool needsRelay = false;
#if defined(XP_WIN) || defined(XP_MACOSX)
    if (!IsBroker() && aNode != kBrokerNodeName &&
        aEvent->type() == Event::kUserMessage) {
      auto* userEvent = static_cast<UserMessageEvent*>(aEvent.get());
      needsRelay =
          userEvent->HasMessage() &&
          userEvent->GetMessage<IPC::Message>()->num_relayed_attachments() > 0;
    }
#endif

    UniquePtr<IPC::Message> message =
        SerializeEventMessage(std::move(aEvent), needsRelay ? &aNode : nullptr);
    MOZ_ASSERT(message->is_relay() == needsRelay,
               "Message relay status set incorrectly");

    RefPtr<NodeChannel> peer;
    RefPtr<NodeChannel> broker;
    bool needsIntroduction = false;
    {
      auto state = mState.Lock();

      // Check if we know this peer. If we don't, we'll need to request an
      // introduction.
      peer = state->mPeers.Get(aNode);
      if (!peer || needsRelay) {
        if (IsBroker()) {
          NODECONTROLLER_WARNING("Ignoring message '%s' to unknown peer %s",
                                 message->name(), ToString(aNode).c_str());
          return;
        }

        broker = state->mPeers.Get(kBrokerNodeName);
        if (!broker) {
          NODECONTROLLER_WARNING(
              "Ignoring message '%s' to peer %s due to a missing broker",
              message->name(), ToString(aNode).c_str());
          return;
        }

        if (!needsRelay) {
          auto& queue =
              state->mPendingMessages.LookupOrInsertWith(aNode, [&]() {
                needsIntroduction = true;
                return Queue<UniquePtr<IPC::Message>, 64>{};
              });
          queue.Push(std::move(message));
        }
      }
    }

    MOZ_ASSERT(!needsIntroduction || !needsRelay,
               "Only one of the two should ever be set");

    if (needsRelay) {
      NODECONTROLLER_LOG(LogLevel::Info,
                         "Relaying message '%s' for peer %s due to %" PRIu32
                         " attachments",
                         message->name(), ToString(aNode).c_str(),
                         message->num_relayed_attachments());
      MOZ_ASSERT(message->num_relayed_attachments() > 0 && broker);
      broker->SendEventMessage(std::move(message));
    } else if (needsIntroduction) {
      MOZ_ASSERT(broker);
      broker->RequestIntroduction(aNode);
    } else if (peer) {
      peer->SendEventMessage(std::move(message));
    }
  }
}

void NodeController::BroadcastEvent(UniquePtr<Event> aEvent) {
  UniquePtr<IPC::Message> message =
      SerializeEventMessage(std::move(aEvent), nullptr, BROADCAST_MESSAGE_TYPE);

  if (IsBroker()) {
    OnBroadcast(mName, std::move(message));
  } else if (RefPtr<NodeChannel> broker = GetNodeChannel(kBrokerNodeName)) {
    broker->Broadcast(std::move(message));
  } else {
    NODECONTROLLER_WARNING(
        "Trying to broadcast event, but no connection to broker");
  }
}

void NodeController::PortStatusChanged(const PortRef& aPortRef) {
  RefPtr<UserData> userData;
  int rv = mNode->GetUserData(aPortRef, &userData);
  if (rv != mojo::core::ports::OK) {
    NODECONTROLLER_WARNING("GetUserData call for port '%s' failed",
                           ToString(aPortRef.name()).c_str());
    return;
  }
  if (userData) {
    // All instances of `UserData` attached to ports in this node must be of
    // type `PortObserver`, so we can call `OnPortStatusChanged` directly on
    // them.
    static_cast<PortObserver*>(userData.get())->OnPortStatusChanged();
  }
}

void NodeController::OnEventMessage(const NodeName& aFromNode,
                                    UniquePtr<IPC::Message> aMessage) {
  AssertIOThread();

  bool isRelay = aMessage->is_relay();
  if (isRelay && aMessage->num_relayed_attachments() == 0) {
    NODECONTROLLER_WARNING(
        "Invalid relay message without relayed attachments from peer %s!",
        ToString(aFromNode).c_str());
    DropPeer(aFromNode);
    return;
  }

  NodeName relayTarget;
  UniquePtr<Event> event = DeserializeEventMessage(
      std::move(aMessage), isRelay ? &relayTarget : nullptr);
  if (!event) {
    NODECONTROLLER_WARNING("Invalid EventMessage from peer %s!",
                           ToString(aFromNode).c_str());
    DropPeer(aFromNode);
    return;
  }

  NodeName fromNode = aFromNode;
#if defined(XP_WIN) || defined(XP_MACOSX)
  if (isRelay) {
    if (event->type() != Event::kUserMessage) {
      NODECONTROLLER_WARNING(
          "Unexpected relay of non-UserMessage event from peer %s!",
          ToString(aFromNode).c_str());
      DropPeer(aFromNode);
      return;
    }

    // If we're the broker, then we'll need to forward this message on to the
    // true recipient. To do this, we re-serialize the message, passing along
    // the original source node, and send it to the final node.
    if (IsBroker()) {
      UniquePtr<IPC::Message> message =
          SerializeEventMessage(std::move(event), &aFromNode);
      if (!message) {
        NODECONTROLLER_WARNING(
            "Relaying EventMessage from peer %s failed to re-serialize!",
            ToString(aFromNode).c_str());
        DropPeer(aFromNode);
        return;
      }
      MOZ_ASSERT(message->is_relay(), "Message stopped being a relay message?");
      MOZ_ASSERT(message->num_relayed_attachments() > 0,
                 "Message doesn't have relayed attachments?");

      NODECONTROLLER_LOG(
          LogLevel::Info,
          "Relaying message '%s' from peer %s to peer %s (%" PRIu32
          " attachments)",
          message->name(), ToString(aFromNode).c_str(),
          ToString(relayTarget).c_str(), message->num_relayed_attachments());

      RefPtr<NodeChannel> peer;
      {
        auto state = mState.Lock();
        peer = state->mPeers.Get(relayTarget);
      }
      if (!peer) {
        NODECONTROLLER_WARNING(
            "Dropping relayed message from %s to unknown peer %s",
            ToString(aFromNode).c_str(), ToString(relayTarget).c_str());
        return;
      }

      peer->SendEventMessage(std::move(message));
      return;
    }

    // Otherwise, we're the final recipient, so we can continue & process the
    // message as usual.
    if (aFromNode != kBrokerNodeName) {
      NODECONTROLLER_WARNING(
          "Unexpected relayed EventMessage from non-broker peer %s!",
          ToString(aFromNode).c_str());
      DropPeer(aFromNode);
      return;
    }
    fromNode = relayTarget;

    NODECONTROLLER_LOG(LogLevel::Info, "Got relayed message from peer %s",
                       ToString(fromNode).c_str());
  }
#endif

  // If we're getting a requested port merge from another process, check to make
  // sure that we're expecting the request, and record that the merge has
  // arrived so we don't try to close the port on error.
  if (event->type() == Event::kMergePort) {
    // Check that the target port for the merge actually exists.
    auto targetPort = GetPort(event->port_name());
    if (!targetPort.is_valid()) {
      NODECONTROLLER_WARNING(
          "Unexpected MergePortEvent from peer %s for unknown port %s",
          ToString(fromNode).c_str(), ToString(event->port_name()).c_str());
      DropPeer(fromNode);
      return;
    }

    // Check if `targetPort` is in our pending merges entry for the given source
    // node. If this makes the `mPendingMerges` entry empty, remove it.
    bool expectingMerge = [&] {
      auto state = mState.Lock();
      auto pendingMerges = state->mPendingMerges.Lookup(aFromNode);
      if (!pendingMerges) {
        return false;
      }
      size_t removed = pendingMerges->RemoveElementsBy(
          [&](auto& port) { return port.name() == targetPort.name(); });
      if (removed != 0 && pendingMerges->IsEmpty()) {
        pendingMerges.Remove();
      }
      return removed != 0;
    }();

    if (!expectingMerge) {
      NODECONTROLLER_WARNING(
          "Unexpected MergePortEvent from peer %s for port %s",
          ToString(fromNode).c_str(), ToString(event->port_name()).c_str());
      DropPeer(fromNode);
      return;
    }
  }

  (void)mNode->AcceptEvent(std::move(event));
}

void NodeController::OnBroadcast(const NodeName& aFromNode,
                                 UniquePtr<IPC::Message> aMessage) {
  MOZ_DIAGNOSTIC_ASSERT(aMessage->type() == BROADCAST_MESSAGE_TYPE);

  // NOTE: This method may be called off of the IO thread by the
  // `BroadcastEvent` node callback.
  if (!IsBroker()) {
    NODECONTROLLER_WARNING("Broadcast request received by non-broker node");
    return;
  }

  UniquePtr<Event> event = DeserializeEventMessage(std::move(aMessage));
  if (!event) {
    NODECONTROLLER_WARNING("Invalid broadcast message from peer");
    return;
  }

  nsTArray<RefPtr<NodeChannel>> peers;
  {
    auto state = mState.Lock();
    peers.SetCapacity(state->mPeers.Count());
    for (const auto& peer : state->mPeers.Values()) {
      peers.AppendElement(peer);
    }
  }
  for (const auto& peer : peers) {
    // NOTE: This `clone` operation is only supported for a limited number of
    // message types by the ports API, which provides some extra security by
    // only allowing those specific types of messages to be broadcasted.
    // Messages which don't support `Clone` cannot be broadcast, and the ports
    // library will not attempt to broadcast them.
    auto clone = event->Clone();
    if (!clone) {
      NODECONTROLLER_WARNING("Attempt to broadcast unsupported message");
      break;
    }

    peer->SendEventMessage(SerializeEventMessage(std::move(clone)));
  }
}

void NodeController::OnIntroduce(const NodeName& aFromNode,
                                 NodeChannel::Introduction aIntroduction) {
  AssertIOThread();

  if (aFromNode != kBrokerNodeName) {
    NODECONTROLLER_WARNING("Introduction received from non-broker node");
    DropPeer(aFromNode);
    return;
  }

  MOZ_ASSERT(aIntroduction.mMyPid == base::GetCurrentProcId(),
             "We're the wrong process to receive this?");

  if (!aIntroduction.mHandle) {
    NODECONTROLLER_WARNING("Could not be introduced to peer %s",
                           ToString(aIntroduction.mName).c_str());
    mNode->LostConnectionToNode(aIntroduction.mName);

    auto state = mState.Lock();
    state->mPendingMessages.Remove(aIntroduction.mName);
    return;
  }

  auto channel =
      MakeUnique<IPC::Channel>(std::move(aIntroduction.mHandle),
                               aIntroduction.mMode, aIntroduction.mOtherPid);
  auto nodeChannel = MakeRefPtr<NodeChannel>(
      aIntroduction.mName, std::move(channel), this, aIntroduction.mOtherPid);

  {
    auto state = mState.Lock();
    bool isNew = false;
    state->mPeers.LookupOrInsertWith(aIntroduction.mName, [&]() {
      isNew = true;
      return nodeChannel;
    });
    if (!isNew) {
      // We got a duplicate introduction. This can happen during normal
      // execution if both sides request an introduction at the same time. We
      // can just ignore the second one, as they'll arrive in the same order in
      // both processes.
      nodeChannel->Close();
      return;
    }

    // Deliver any pending messages, then remove the entry from our table. We do
    // this while `mState` is still held to ensure that these messages are
    // all sent before another thread can observe the newly created channel.
    // As the channel hasn't been `Connect()`-ed yet, this will only queue the
    // messages up to be sent, so is OK to do with the mutex held.  These
    // messages will be processed to be sent during `Start()` below, which is
    // performed outside of the lock.
    if (auto pending = state->mPendingMessages.Lookup(aIntroduction.mName)) {
      while (!pending->IsEmpty()) {
        nodeChannel->SendEventMessage(pending->Pop());
      }
      pending.Remove();
    }
  }

  // NodeChannel::Start must be called with the lock not held, as it may lead to
  // callbacks being made into `OnChannelError` or `OnMessageReceived`, which
  // will attempt to re-acquire our lock.
  nodeChannel->Start();
}

void NodeController::OnRequestIntroduction(const NodeName& aFromNode,
                                           const NodeName& aName) {
  AssertIOThread();
  if (NS_WARN_IF(!IsBroker())) {
    return;
  }

  RefPtr<NodeChannel> peerA = GetNodeChannel(aFromNode);
  if (!peerA || aName == mojo::core::ports::kInvalidNodeName) {
    NODECONTROLLER_WARNING("Invalid OnRequestIntroduction message from node %s",
                           ToString(aFromNode).c_str());
    DropPeer(aFromNode);
    return;
  }

  RefPtr<NodeChannel> peerB = GetNodeChannel(aName);
  IPC::Channel::ChannelHandle handleA, handleB;
  if (!peerB || !IPC::Channel::CreateRawPipe(&handleA, &handleB)) {
    NODECONTROLLER_WARNING(
        "Rejecting introduction request from '%s' for unknown peer '%s'",
        ToString(aFromNode).c_str(), ToString(aName).c_str());

    // We don't know this peer, or ran into issues creating the descriptor! Send
    // an invalid introduction to content to clean up any pending outbound
    // messages.
    NodeChannel::Introduction intro{aName, nullptr, IPC::Channel::MODE_SERVER,
                                    peerA->OtherPid(), base::kInvalidProcessId};
    peerA->Introduce(std::move(intro));
    return;
  }

  NodeChannel::Introduction introA{aName, std::move(handleA),
                                   IPC::Channel::MODE_SERVER, peerA->OtherPid(),
                                   peerB->OtherPid()};
  NodeChannel::Introduction introB{aFromNode, std::move(handleB),
                                   IPC::Channel::MODE_CLIENT, peerB->OtherPid(),
                                   peerA->OtherPid()};
  peerA->Introduce(std::move(introA));
  peerB->Introduce(std::move(introB));
}

void NodeController::OnAcceptInvite(const NodeName& aFromNode,
                                    const NodeName& aRealName,
                                    const PortName& aInitialPort) {
  AssertIOThread();
  if (!IsBroker()) {
    NODECONTROLLER_WARNING("Ignoring AcceptInvite message as non-broker");
    return;
  }

  if (aRealName == mojo::core::ports::kInvalidNodeName ||
      aInitialPort == mojo::core::ports::kInvalidPortName) {
    NODECONTROLLER_WARNING("Invalid name in AcceptInvite message");
    DropPeer(aFromNode);
    return;
  }

  bool inserted = false;
  Invite invite;
  {
    auto state = mState.Lock();
    MOZ_ASSERT(state->mPendingMessages.IsEmpty(),
               "Shouldn't have pending messages in broker");

    // Try to remove the source node from our invites list and insert it into
    // our peers map under the new name.
    if (state->mInvites.Remove(aFromNode, &invite)) {
      MOZ_ASSERT(invite.mChannel && invite.mToMerge.is_valid());
      state->mPeers.LookupOrInsertWith(aRealName, [&]() {
        inserted = true;
        return invite.mChannel;
      });
    }
  }
  if (!inserted) {
    NODECONTROLLER_WARNING("Invalid AcceptInvite message from node %s",
                           ToString(aFromNode).c_str());
    DropPeer(aFromNode);
    return;
  }

  // Update the name of the node. This field is only accessed from the IO
  // thread, so it's safe to update it without a lock held.
  invite.mChannel->SetName(aRealName);

  // Start the port merge to allow our existing initial port to begin
  // communicating with the remote port.
  PORTS_ALWAYS_OK(mNode->MergePorts(invite.mToMerge, aRealName, aInitialPort));
}

void NodeController::OnChannelError(const NodeName& aFromNode) {
  AssertIOThread();
  DropPeer(aFromNode);
}

static mojo::core::ports::NodeName RandomNodeName() {
  return {RandomUint64OrDie(), RandomUint64OrDie()};
}

std::tuple<ScopedPort, RefPtr<NodeChannel>> NodeController::InviteChildProcess(
    UniquePtr<IPC::Channel> aChannel,
    GeckoChildProcessHost* aChildProcessHost) {
  MOZ_ASSERT(IsBroker());
  AssertIOThread();

  // Create the peer with a randomly generated name, and store it in `mInvites`.
  // This channel and name will be used for communication with the node until it
  // sends us its' real name in an `AcceptInvite` message.
  auto ports = CreatePortPair();
  auto inviteName = RandomNodeName();
  auto nodeChannel =
      MakeRefPtr<NodeChannel>(inviteName, std::move(aChannel), this,
                              base::kInvalidProcessId, aChildProcessHost);
  {
    auto state = mState.Lock();
    MOZ_DIAGNOSTIC_ASSERT(!state->mPeers.Contains(inviteName),
                          "UUID conflict?");
    MOZ_DIAGNOSTIC_ASSERT(!state->mInvites.Contains(inviteName),
                          "UUID conflict?");
    state->mInvites.InsertOrUpdate(inviteName,
                                   Invite{nodeChannel, ports.second.Release()});
  }

  nodeChannel->Start();
  return std::tuple{std::move(ports.first), std::move(nodeChannel)};
}

void NodeController::InitBrokerProcess() {
  AssertIOThread();
  MOZ_ASSERT(!gNodeController);
  gNodeController = new NodeController(kBrokerNodeName);
}

ScopedPort NodeController::InitChildProcess(UniquePtr<IPC::Channel> aChannel,
                                            base::ProcessId aParentPid) {
  AssertIOThread();
  MOZ_ASSERT(!gNodeController);

  auto nodeName = RandomNodeName();
  gNodeController = new NodeController(nodeName);

  auto ports = gNodeController->CreatePortPair();
  PortRef toMerge = ports.second.Release();

  auto nodeChannel = MakeRefPtr<NodeChannel>(
      kBrokerNodeName, std::move(aChannel), gNodeController, aParentPid);
  {
    auto state = gNodeController->mState.Lock();
    MOZ_DIAGNOSTIC_ASSERT(!state->mPeers.Contains(kBrokerNodeName));
    state->mPeers.InsertOrUpdate(kBrokerNodeName, nodeChannel);
    MOZ_DIAGNOSTIC_ASSERT(!state->mPendingMerges.Contains(kBrokerNodeName));
    state->mPendingMerges.LookupOrInsert(kBrokerNodeName)
        .AppendElement(toMerge);
  }

  nodeChannel->Start();
  nodeChannel->AcceptInvite(nodeName, toMerge.name());
  return std::move(ports.first);
}

void NodeController::CleanUp() {
  AssertIOThread();
  MOZ_ASSERT(gNodeController);

  RefPtr<NodeController> nodeController = gNodeController;
  gNodeController = nullptr;

  // Collect all objects from our state which need to be cleaned up.
  nsTArray<NodeName> lostConnections;
  nsTArray<RefPtr<NodeChannel>> channelsToClose;
  nsTArray<PortRef> portsToClose;
  {
    auto state = nodeController->mState.Lock();
    for (const auto& chan : state->mPeers) {
      lostConnections.AppendElement(chan.GetKey());
      channelsToClose.AppendElement(chan.GetData());
    }
    for (const auto& invite : state->mInvites.Values()) {
      channelsToClose.AppendElement(invite.mChannel);
      portsToClose.AppendElement(invite.mToMerge);
    }
    for (const auto& pendingPorts : state->mPendingMerges.Values()) {
      portsToClose.AppendElements(pendingPorts);
    }
    state->mPeers.Clear();
    state->mPendingMessages.Clear();
    state->mInvites.Clear();
    state->mPendingMerges.Clear();
  }
  for (auto& nodeChannel : channelsToClose) {
    nodeChannel->Close();
  }
  for (auto& port : portsToClose) {
    nodeController->mNode->ClosePort(port);
  }
  for (auto& name : lostConnections) {
    nodeController->mNode->LostConnectionToNode(name);
  }
}

#undef NODECONTROLLER_LOG
#undef NODECONTROLLER_WARNING

}  // namespace mozilla::ipc
