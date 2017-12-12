/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _mozilla_dom_ClientState_h
#define _mozilla_dom_ClientState_h

#include "mozilla/dom/DocumentBinding.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

class IPCClientState;
class IPCClientWindowState;
class IPCClientWorkerState;

// This class defines the mutable nsGlobalWindow state we support querying
// through the ClientManagerService.  It is a snapshot of the state and
// is not live updated.
class ClientWindowState final
{
  UniquePtr<IPCClientWindowState> mData;

public:
  ClientWindowState(mozilla::dom::VisibilityState aVisibilityState,
                    const TimeStamp& aLastFocusTime,
                    nsContentUtils::StorageAccess aStorageAccess,
                    bool aFocused);

  explicit ClientWindowState(const IPCClientWindowState& aData);

  ClientWindowState(const ClientWindowState& aRight);
  ClientWindowState(ClientWindowState&& aRight);

  ClientWindowState&
  operator=(const ClientWindowState& aRight);

  ClientWindowState&
  operator=(ClientWindowState&& aRight);

  ~ClientWindowState();

  mozilla::dom::VisibilityState
  VisibilityState() const;

  const TimeStamp&
  LastFocusTime() const;

  bool
  Focused() const;

  nsContentUtils::StorageAccess
  GetStorageAccess() const;

  const IPCClientWindowState&
  ToIPC() const;
};

// This class defines the mutable worker state we support querying
// through the ClientManagerService.  It is a snapshot of the state and
// is not live updated.  Right now, we don't actually providate any
// worker specific state values, but we may in the future.  This
// class also services as a placeholder that the state is referring
// to a worker in ClientState.
class ClientWorkerState final
{
  UniquePtr<IPCClientWorkerState> mData;

public:
  explicit ClientWorkerState(nsContentUtils::StorageAccess aStorageAccess);

  explicit ClientWorkerState(const IPCClientWorkerState& aData);

  ClientWorkerState(const ClientWorkerState& aRight);
  ClientWorkerState(ClientWorkerState&& aRight);

  ClientWorkerState&
  operator=(const ClientWorkerState& aRight);

  ClientWorkerState&
  operator=(ClientWorkerState&& aRight);

  ~ClientWorkerState();

  nsContentUtils::StorageAccess
  GetStorageAccess() const;

  const IPCClientWorkerState&
  ToIPC() const;
};

// This is a union of the various types of mutable state we support
// querying in ClientManagerService.  Right now it can contain either
// window or worker states.
class ClientState final
{
  Maybe<Variant<ClientWindowState, ClientWorkerState>> mData;

public:
  ClientState();

  explicit ClientState(const ClientWindowState& aWindowState);
  explicit ClientState(const ClientWorkerState& aWorkerState);
  explicit ClientState(const IPCClientWindowState& aData);
  explicit ClientState(const IPCClientWorkerState& aData);

  ClientState(const ClientState& aRight) = default;
  ClientState(ClientState&& aRight);

  ClientState&
  operator=(const ClientState& aRight) = default;

  ClientState&
  operator=(ClientState&& aRight);

  ~ClientState();

  static ClientState
  FromIPC(const IPCClientState& aData);

  bool
  IsWindowState() const;

  const ClientWindowState&
  AsWindowState() const;

  bool
  IsWorkerState() const;

  const ClientWorkerState&
  AsWorkerState() const;

  nsContentUtils::StorageAccess
  GetStorageAccess() const;

  const IPCClientState
  ToIPC() const;
};

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientState_h
