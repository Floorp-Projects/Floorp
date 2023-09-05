/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ipc_glue_MessageLink_h
#define ipc_glue_MessageLink_h

#include <cstdint>
#include "base/message_loop.h"
#include "mojo/core/ports/node.h"
#include "mojo/core/ports/port_ref.h"
#include "mozilla/Assertions.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/ipc/ScopedPort.h"

namespace IPC {
class Message;
class MessageReader;
class MessageWriter;
}  // namespace IPC

namespace mozilla {
namespace ipc {

class MessageChannel;
class NodeController;

struct HasResultCodes {
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

enum Side : uint8_t { ParentSide, ChildSide, UnknownSide };

const char* StringFromIPCSide(Side side);

class MessageLink {
 public:
  typedef IPC::Message Message;

  explicit MessageLink(MessageChannel* aChan);
  virtual ~MessageLink();

  // n.b.: These methods all require that the channel monitor is
  // held when they are invoked.
  virtual void SendMessage(mozilla::UniquePtr<Message> msg) = 0;

  // Synchronously close the connection, such that no further notifications will
  // be delivered to the MessageChannel instance. Must be called with the
  // channel monitor held.
  virtual void Close() = 0;

  virtual bool IsClosed() const = 0;

#ifdef FUZZING_SNAPSHOT
  virtual Maybe<mojo::core::ports::PortName> GetPortName() { return Nothing(); }
#endif

 protected:
  MessageChannel* mChan;
};

class PortLink final : public MessageLink {
  using PortRef = mojo::core::ports::PortRef;
  using PortStatus = mojo::core::ports::PortStatus;
  using UserMessage = mojo::core::ports::UserMessage;
  using UserMessageEvent = mojo::core::ports::UserMessageEvent;

 public:
  PortLink(MessageChannel* aChan, ScopedPort aPort);
  virtual ~PortLink();

  void SendMessage(UniquePtr<Message> aMessage) override;
  void Close() override;

  bool IsClosed() const override;

#ifdef FUZZING_SNAPSHOT
  Maybe<mojo::core::ports::PortName> GetPortName() override {
    return Some(mPort.name());
  }
#endif

 private:
  class PortObserverThunk;
  friend class PortObserverThunk;

  void OnPortStatusChanged();

  // Called either when an error is detected on the port from the port observer,
  // or when `SendClose()` is called.
  void Clear();

  const RefPtr<NodeController> mNode;
  const PortRef mPort;

  RefPtr<PortObserverThunk> mObserver;
};

}  // namespace ipc
}  // namespace mozilla

#endif  // ifndef ipc_glue_MessageLink_h
