/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_RawEndpoint_h
#define mozilla_ipc_RawEndpoint_h

#include "mojo/core/ports/port_ref.h"

namespace mozilla::ipc {

class NodeController;

/// A uniquely owned raw IPC endpoint, connected to our peers over the node
/// controller. This type can be sent over IPC channels to establish
/// connections.
///
/// In general, prefer using Endpoint over ScopedPort for type safety reasons.
class ScopedPort {
  using PortName = mojo::core::ports::PortName;
  using PortRef = mojo::core::ports::PortRef;

 public:
  ScopedPort();
  ~ScopedPort();

  ScopedPort(PortRef aPort, NodeController* aController);

  ScopedPort(ScopedPort&& aOther);
  ScopedPort(const ScopedPort&) = delete;

  ScopedPort& operator=(ScopedPort&& aOther);
  ScopedPort& operator=(const ScopedPort&) = delete;

  // Allow checking if this `ScopedPort` is valid or not.
  bool IsValid() const { return mValid; }
  explicit operator bool() const { return IsValid(); }

  // Underlying port and controller which are used by this ScopedPort.
  const PortName& Name() const { return mPort.name(); }
  const PortRef& Port() const { return mPort; }
  NodeController* Controller() const { return mController; }

  // Release ownership over the contained `ScopedPort`, meaning that it will
  // not be closed when this ScopedPort is destroyed. This will make the
  // endpoint invalid.
  PortRef Release();

 private:
  void Reset();

  bool mValid = false;
  PortRef mPort;
  RefPtr<NodeController> mController;

  // NOTE: This type does not contain PID information about the other process,
  // which will need to be sent separately if necessary.
};

}  // namespace mozilla::ipc

namespace IPC {

template <typename T>
struct ParamTraits;

template <>
struct ParamTraits<mozilla::ipc::ScopedPort> {
  using paramType = mozilla::ipc::ScopedPort;

  static void Write(MessageWriter* aWriter, paramType&& aParam);
  static bool Read(MessageReader* aReader, paramType* aResult);
};

}  // namespace IPC

#endif
