/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_NodeChannel_h
#define mozilla_ipc_NodeChannel_h

#include "mojo/core/ports/node.h"
#include "mojo/core/ports/node_delegate.h"
#include "base/process.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_channel.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "nsISupports.h"
#include "nsTHashMap.h"
#include "mozilla/Queue.h"
#include "mozilla/DataMutex.h"
#include "mozilla/UniquePtr.h"

#ifdef FUZZING_SNAPSHOT
#  include "mozilla/fuzzing/IPCFuzzController.h"
#endif

namespace mozilla::ipc {

class GeckoChildProcessHost;
class NodeController;

// Represents a live connection between our Node and a remote process. This
// object acts as an IPC::Channel listener and performs basic processing on
// messages as they're passed between processes.

class NodeChannel final : public IPC::Channel::Listener {
  using NodeName = mojo::core::ports::NodeName;
  using PortName = mojo::core::ports::PortName;

#ifdef FUZZING_SNAPSHOT
  // Required because IPCFuzzController calls OnMessageReceived.
  friend class mozilla::fuzzing::IPCFuzzController;
#endif

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DESTROY(NodeChannel, Destroy())

  struct Introduction {
    NodeName mName;
    IPC::Channel::ChannelHandle mHandle;
    IPC::Channel::Mode mMode;
    base::ProcessId mMyPid = base::kInvalidProcessId;
    base::ProcessId mOtherPid = base::kInvalidProcessId;
  };

  class Listener {
   public:
    virtual ~Listener() = default;

    NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

    virtual void OnEventMessage(const NodeName& aFromNode,
                                UniquePtr<IPC::Message> aMessage) = 0;
    virtual void OnBroadcast(const NodeName& aFromNode,
                             UniquePtr<IPC::Message> aMessage) = 0;
    virtual void OnIntroduce(const NodeName& aFromNode,
                             Introduction aIntroduction) = 0;
    virtual void OnRequestIntroduction(const NodeName& aFromNode,
                                       const NodeName& aName) = 0;
    virtual void OnAcceptInvite(const NodeName& aFromNode,
                                const NodeName& aRealName,
                                const PortName& aInitialPort) = 0;
    virtual void OnChannelError(const NodeName& aFromNode) = 0;
  };

  NodeChannel(const NodeName& aName, UniquePtr<IPC::Channel> aChannel,
              Listener* aListener,
              base::ProcessId aPid = base::kInvalidProcessId,
              GeckoChildProcessHost* aChildProcessHost = nullptr);

  // Send the given message over this peer channel link. May be called from any
  // thread.
  void SendEventMessage(UniquePtr<IPC::Message> aMessage);

  // Ask the broker process to broadcast this message to every node. May be
  // called from any thread.
  void Broadcast(UniquePtr<IPC::Message> aMessage);

  // Ask the broker process to introduce this node to another node with the
  // given name. May be called from any thread.
  void RequestIntroduction(const NodeName& aPeerName);

  // Send an introduction to the target node. May be called from any thread.
  void Introduce(Introduction aIntroduction);

  void AcceptInvite(const NodeName& aRealName, const PortName& aInitialPort);

  // The PID of the remote process, once known. May be called from any thread.
  base::ProcessId OtherPid() const { return mOtherPid; }

  // Start communicating with the remote process using this NodeChannel. MUST BE
  // CALLED FROM THE IO THREAD.
  void Start();

  // Stop communicating with the remote process using this NodeChannel, MUST BE
  // CALLED FROM THE IO THREAD.
  void Close();

  // Only ever called by NodeController to update the name after an invite has
  // completed. MUST BE CALLED FROM THE IO THREAD.
  void SetName(const NodeName& aNewName) { mName = aNewName; }

#ifdef FUZZING_SNAPSHOT
  // MUST BE CALLED FROM THE IO THREAD.
  const NodeName& GetName() { return mName; }
#endif

  // Update the known PID for the remote process. MUST BE CALLED FROM THE IO
  // THREAD.
  void SetOtherPid(base::ProcessId aNewPid);

#ifdef XP_MACOSX
  // Called by the GeckoChildProcessHost to provide the task_t for the peer
  // process. MUST BE CALLED FROM THE IO THREAD.
  void SetMachTaskPort(task_t aTask);
#endif

 private:
  ~NodeChannel();

  void Destroy();
  void FinalDestroy();

  void SendMessage(UniquePtr<IPC::Message> aMessage);

  // IPC::Channel::Listener implementation
  void OnMessageReceived(UniquePtr<IPC::Message> aMessage) override;
  void OnChannelConnected(base::ProcessId aPeerPid) override;
  void OnChannelError() override;

  // NOTE: This strong reference will create a reference cycle between the
  // listener and the NodeChannel while it is in use. The Listener must clear
  // its reference to the NodeChannel to avoid leaks before shutdown.
  const RefPtr<Listener> mListener;

  // The apparent name of this Node. This may change during the invite process
  // while waiting for the remote node name to be communicated to us.

  // WARNING: This must only be accessed on the IO thread.
  NodeName mName;

  // NOTE: This won't change once the connection has been established, but may
  // be `-1` until then. This will only be written to on the IO thread, but may
  // be read from other threads.
  std::atomic<base::ProcessId> mOtherPid;

  // WARNING: Most methods on the IPC::Channel are only safe to call on the IO
  // thread, however it is safe to call `Send()` and `IsClosed()` from other
  // threads. See IPC::Channel's documentation for details.
  const mozilla::UniquePtr<IPC::Channel> mChannel;

  // The state will start out as `State::Active`, and will only transition to
  // `State::Closed` on the IO thread. If a Send fails, the state will
  // transition to `State::Closing`, and a runnable will be dispatched to the
  // I/O thread to notify callbacks.
  enum class State { Active, Closing, Closed };
  std::atomic<State> mState = State::Active;

#ifdef FUZZING_SNAPSHOT
  std::atomic<bool> mBlockSendRecv = false;
#endif

  // WARNING: Must only be accessed on the IO thread.
  WeakPtr<mozilla::ipc::GeckoChildProcessHost> mChildProcessHost;
};

}  // namespace mozilla::ipc

#endif
