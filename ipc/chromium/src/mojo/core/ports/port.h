// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PORTS_PORT_H_
#define MOJO_CORE_PORTS_PORT_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "mojo/core/ports/event.h"
#include "mojo/core/ports/message_queue.h"
#include "mojo/core/ports/user_data.h"
#include "mozilla/Atomics.h"
#include "mozilla/PlatformMutex.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"

#ifdef MOZ_TSAN
// In TSAN builds, a single static mutex is used for all ports, rather than
// per-port mutexes, to avoid overloading the maximum 64 concurrently-held locks
// limit of its deadlock detector when sending messages with a large number of
// attached ports.
#  define MOZ_USE_SINGLETON_PORT_MUTEX 1
#endif

#ifdef MOZ_USE_SINGLETON_PORT_MUTEX
#  include "mozilla/StaticMutex.h"
#endif

namespace mojo {
namespace core {
namespace ports {

class PortLocker;

namespace detail {

// Ports cannot use mozilla::Mutex, as the acquires-before relationships handled
// by PortLocker can overload the debug-only deadlock detector.
class MOZ_CAPABILITY("mutex") PortMutex
#ifndef MOZ_USE_SINGLETON_PORT_MUTEX
    : private ::mozilla::detail::MutexImpl
#endif
{
 public:
  void AssertCurrentThreadOwns() const MOZ_ASSERT_CAPABILITY(this) {
#ifdef DEBUG
    MOZ_ASSERT(mOwningThread == PR_GetCurrentThread());
#endif
#ifdef MOZ_USE_SINGLETON_PORT_MUTEX
    sSingleton.AssertCurrentThreadOwns();
#endif
  }

 private:
  // PortMutex should only be locked/unlocked via PortLocker
  friend class ::mojo::core::ports::PortLocker;

#ifdef MOZ_USE_SINGLETON_PORT_MUTEX
  // If the singleton mutex is in use, it must be locked before calling `Lock()`
  // on any port, and must only be unlocked after calling `Unlock()` on every
  // locked port.
  static ::mozilla::StaticMutex sSingleton;
#endif

  void Lock() MOZ_CAPABILITY_ACQUIRE() {
#ifdef MOZ_USE_SINGLETON_PORT_MUTEX
    sSingleton.AssertCurrentThreadOwns();
#else
    ::mozilla::detail::MutexImpl::lock();
#endif
#ifdef DEBUG
    mOwningThread = PR_GetCurrentThread();
#endif
  }
  void Unlock() MOZ_CAPABILITY_RELEASE() {
#ifdef DEBUG
    MOZ_ASSERT(mOwningThread == PR_GetCurrentThread());
    mOwningThread = nullptr;
#endif
#ifdef MOZ_USE_SINGLETON_PORT_MUTEX
    sSingleton.AssertCurrentThreadOwns();
#else
    ::mozilla::detail::MutexImpl::unlock();
#endif
  }

#ifdef DEBUG
  mozilla::Atomic<PRThread*, mozilla::Relaxed> mOwningThread;
#endif
};

}  // namespace detail

// A Port is essentially a node in a circular list of addresses. For the sake of
// this documentation such a list will henceforth be referred to as a "route."
// Routes are the fundamental medium upon which all Node event circulation takes
// place and are thus the backbone of all Mojo message passing.
//
// Each Port is identified by a 128-bit address within a Node (see node.h). A
// Port doesn't really *do* anything per se: it's a named collection of state,
// and its owning Node manages all event production, transmission, routing, and
// processing logic. See Node for more details on how Ports may be used to
// transmit arbitrary user messages as well as other Ports.
//
// Ports may be in any of a handful of states (see State below) which dictate
// how they react to system events targeting them. In the simplest and most
// common case, Ports are initially created as an entangled pair (i.e. a simple
// cycle consisting of two Ports) both in the |kReceiving| State. Consider Ports
// we'll label |A| and |B| here, which may be created using
// Node::CreatePortPair():
//
//     +-----+          +-----+
//     |     |--------->|     |
//     |  A  |          |  B  |
//     |     |<---------|     |
//     +-----+          +-----+
//
// |A| references |B| via |peer_node_name| and |peer_port_name|, while |B| in
// turn references |A|. Note that a Node is NEVER aware of who is sending events
// to a given Port; it is only aware of where it must route events FROM a given
// Port.
//
// For the sake of documentation, we refer to one receiving port in a route as
// the "conjugate" of the other. A receiving port's conjugate is also its peer
// upon initial creation, but because of proxying this may not be the case at a
// later time.
//
// ALL access to this data structure must be guarded by |lock_| acquisition,
// which is only possible using a PortLocker. PortLocker ensures that
// overlapping Port lock acquisitions on a single thread are always acquired in
// a globally consistent order.
class Port {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Port)

 public:
  // The state of a given Port. A Port may only exist in one of these states at
  // any given time.
  enum State {
    // The Port is not yet paired with a peer and is therefore unusable. See
    // Node::CreateUninitializedPort and Node::InitializePort for motivation.
    kUninitialized,

    // The Port is publicly visible outside of its Node and may be used to send
    // and receive user messages. There are always AT MOST two |kReceiving|
    // Ports along any given route. A user message event sent from a receiving
    // port is always circulated along the Port's route until it reaches either
    // a dead-end -- in which case the route is broken -- or it reaches the
    // other receiving Port in the route -- in which case it lands in that
    // Port's incoming message queue which can by read by user code.
    kReceiving,

    // The Port has been taken out of the |kReceiving| state in preparation for
    // proxying to a new destination. A Port enters this state immediately when
    // it's attached to a user message and may only leave this state when
    // transitioning to |kProxying|. See Node for more details.
    kBuffering,

    // The Port is forwarding all user messages (and most other events) to its
    // peer without discretion. Ports in the |kProxying| state may never leave
    // this state and only exist temporarily until their owning Node has
    // established that no more events will target them. See Node for more
    // details.
    kProxying,

    // The Port has been closed and is now permanently unusable. Only
    // |kReceiving| ports can be closed.
    kClosed
  };

  // The current State of the Port.
  State state;

  // The Node and Port address to which events should be routed FROM this Port.
  // Note that this is NOT necessarily the address of the Port currently sending
  // events TO this Port.
  NodeName peer_node_name;
  PortName peer_port_name;

  // We keep track of the port that is currently sending messages to this port.
  // This allows us to verify that the sender node is allowed to send messages
  // to this port as a mitigation against info leak vulnerabilities.
  // Tracking the previous port has the nice side effect of keeping received
  // messages in order.
  NodeName prev_node_name;
  PortName prev_port_name;

  // Mark this port as to be merged.
  bool pending_merge_peer;

  // Next sequence number to send for all event messages.
  uint64_t next_control_sequence_num_to_send;
  uint64_t next_control_sequence_num_to_receive;

  // The next available sequence number to use for outgoing user message events
  // originating from this port.
  uint64_t next_sequence_num_to_send;

  // The largest acknowledged user message event sequence number.
  uint64_t last_sequence_num_acknowledged;

  // The interval for which acknowledge requests will be sent. A value of N will
  // cause an acknowledge request for |last_sequence_num_acknowledged| + N when
  // initially set and on received acknowledge. This means that the lower bound
  // for unread or in-transit messages is |next_sequence_num_to_send| -
  // |last_sequence_num_acknowledged| + |sequence_number_acknowledge_interval|.
  // If zero, no acknowledge requests are sent.
  uint64_t sequence_num_acknowledge_interval;

  // The sequence number of the last message this Port should ever expect to
  // receive in its lifetime. May be used to determine that a proxying port is
  // ready to be destroyed or that a receiving port's conjugate has been closed
  // and we know the sequence number of the last message it sent.
  uint64_t last_sequence_num_to_receive;

  // The sequence number of the message for which this Port should send an
  // acknowledge message. In the buffering state, holds the acknowledge request
  // value that is forwarded to the peer on transition to proxying.
  // This is zero in any port that's never received an acknowledge request, and
  // in proxies that have forwarded a stored acknowledge.
  uint64_t sequence_num_to_acknowledge;

  // The queue of incoming user messages received by this Port. Only non-empty
  // for buffering or receiving Ports. When a buffering port enters the proxying
  // state, it flushes its queue and the proxy then bypasses the queue
  // indefinitely.
  //
  // A receiving port's queue only has elements removed by user code reading
  // messages from the port.
  //
  // Note that this is a priority queue which only exposes messages to consumers
  // in strict sequential order.
  MessageQueue message_queue;

  // Buffer outgoing control messages while this port is in kBuffering state.
  std::vector<std::pair<NodeName, ScopedEvent>> control_message_queue;

  // In some edge cases, a Node may need to remember to route a single special
  // event upon destruction of this (proxying) Port. That event is stashed here
  // in the interim.
  mozilla::UniquePtr<std::pair<NodeName, ScopedEvent>> send_on_proxy_removal;

  // Arbitrary user data attached to the Port. In practice, Mojo uses this to
  // stash an observer interface which can be notified about various Port state
  // changes.
  RefPtr<UserData> user_data;

  // Indicates that this (proxying) Port has received acknowledgement that no
  // new user messages will be routed to it. If |true|, the proxy will be
  // removed once it has received and forwarded all sequenced messages up to and
  // including the one numbered |last_sequence_num_to_receive|.
  bool remove_proxy_on_last_message;

  // Indicates that this Port is aware that its nearest (in terms of forward,
  // non-zero cyclic routing distance) receiving Port has been closed.
  bool peer_closed;

  // Indicates that this Port lost its peer unexpectedly (e.g. via process death
  // rather than receiving an ObserveClosure event). In this case
  // |peer_closed| will be true but |last_sequence_num_to_receive| cannot be
  // known. Such ports will continue to make message available until their
  // message queue is empty.
  bool peer_lost_unexpectedly;

  Port(uint64_t next_sequence_num_to_send,
       uint64_t next_sequence_num_to_receive);

  Port(const Port&) = delete;
  void operator=(const Port&) = delete;

  void AssertLockAcquired() { lock_.AssertCurrentThreadOwns(); }

  // Check if the given event should be handled next based on the sequence
  // number and sender peer.
  bool IsNextEvent(const NodeName& from_node, const Event& event);

  // Get the next buffered event to be processed. If none is available, |event|
  // will not be modified.
  void NextEvent(NodeName* from_node, ScopedEvent* event);

  // Buffer the event for later processing.
  void BufferEvent(const NodeName& from_node, ScopedEvent event);

  // Flushes the queue of events pending peer verification and returns all user
  // events
  void TakePendingMessages(
      std::vector<mozilla::UniquePtr<UserMessageEvent>>& messages);

 private:
  using NodePortPair = std::pair<NodeName, PortName>;
  using EventQueue = std::vector<mozilla::UniquePtr<Event>>;
  std::map<NodePortPair, EventQueue> control_event_queues_;

  friend class PortLocker;

  ~Port();

  // This lock guards all fields in Port, but is locked in a unique way which is
  // unfortunately somewhat difficult to get to work with the thread-safety
  // analysis.
  detail::PortMutex lock_ MOZ_ANNOTATED;
};

}  // namespace ports
}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_PORTS_PORT_H_
