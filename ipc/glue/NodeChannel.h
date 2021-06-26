/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_NodeChannel_h
#define mozilla_ipc_NodeChannel_h

#include "mojo/core/ports/node.h"
#include "mojo/core/ports/node_delegate.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_channel.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/AutoTransportDescriptor.h"
#include "mozilla/ipc/Transport.h"
#include "nsISupports.h"
#include "nsTHashMap.h"
#include "mozilla/Queue.h"
#include "mozilla/DataMutex.h"
#include "mozilla/UniquePtr.h"

namespace mozilla::ipc {

class NodeController;

// Represents a live connection between our Node and a remote process. This
// object acts as an IPC::Channel listener and performs basic processing on
// messages as they're passed between processes.

class NodeChannel final : public IPC::Channel::Listener {
  using NodeName = mojo::core::ports::NodeName;
  using PortName = mojo::core::ports::PortName;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DESTROY(NodeChannel, Destroy())

  struct Introduction {
    NodeName mName;
    AutoTransportDescriptor mTransport;
    Transport::Mode mMode;
    int32_t mMyPid = -1;
    int32_t mOtherPid = -1;
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
              Listener* aListener, int32_t aPid = -1);

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
  int32_t OtherPid() const { return mOtherPid; }

  // Start communicating with the remote process using this NodeChannel. MUST BE
  // CALLED FROM THE IO THREAD.
  void Start(bool aCallConnect = true);

  // Stop communicating with the remote process using this NodeChannel, MUST BE
  // CALLED FROM THE IO THREAD.
  void Close();

  // Only ever called by NodeController to update the name after an invite has
  // completed. MUST BE CALLED FROM THE IO THREAD.
  void SetName(const NodeName& aNewName) { mName = aNewName; }

 private:
  ~NodeChannel();

  void Destroy();
  void FinalDestroy();

  // Update the known PID for the remote process. IO THREAD ONLY
  void SetOtherPid(int32_t aNewPid);

  void SendMessage(UniquePtr<IPC::Message> aMessage);
  void DoSendMessage(UniquePtr<IPC::Message> aMessage);

  // IPC::Channel::Listener implementation
  void OnMessageReceived(IPC::Message&& aMessage) override;
  void OnChannelConnected(int32_t aPeerPid) override;
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
  std::atomic<int32_t> mOtherPid;

  // WARNING: This must only be accessed on the IO thread.
  mozilla::UniquePtr<IPC::Channel> mChannel;

  // WARNING: This must only be accessed on the IO thread.
  bool mClosed = false;

  // WARNING: Must only be accessed on the IO thread.
  WeakPtr<IPC::Channel::Listener> mExistingListener;
};

}  // namespace mozilla::ipc

#endif
