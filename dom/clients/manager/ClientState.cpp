/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientState.h"

#include "mozilla/dom/ClientIPCTypes.h"

namespace mozilla {
namespace dom {

ClientWindowState::ClientWindowState(mozilla::dom::VisibilityState aVisibilityState,
                                     const TimeStamp& aLastFocusTime,
                                     nsContentUtils::StorageAccess aStorageAccess,
                                     bool aFocused)
  : mData(MakeUnique<IPCClientWindowState>(aVisibilityState, aLastFocusTime,
                                           aStorageAccess, aFocused))
{
}

ClientWindowState::ClientWindowState(const IPCClientWindowState& aData)
  : mData(MakeUnique<IPCClientWindowState>(aData))
{
}

ClientWindowState::ClientWindowState(const ClientWindowState& aRight)
{
  operator=(aRight);
}

ClientWindowState::ClientWindowState(ClientWindowState&& aRight)
  : mData(Move(aRight.mData))
{
}

ClientWindowState&
ClientWindowState::operator=(const ClientWindowState& aRight)
{
  mData.reset();
  mData = MakeUnique<IPCClientWindowState>(*aRight.mData);
  return *this;
}

ClientWindowState&
ClientWindowState::operator=(ClientWindowState&& aRight)
{
  mData.reset();
  mData = Move(aRight.mData);
  return *this;
}

ClientWindowState::~ClientWindowState()
{
}

mozilla::dom::VisibilityState
ClientWindowState::VisibilityState() const
{
  return mData->visibilityState();
}

const TimeStamp&
ClientWindowState::LastFocusTime() const
{
  return mData->lastFocusTime();
}

bool
ClientWindowState::Focused() const
{
  return mData->focused();
}

nsContentUtils::StorageAccess
ClientWindowState::GetStorageAccess() const
{
  return mData->storageAccess();
}

const IPCClientWindowState&
ClientWindowState::ToIPC() const
{
  return *mData;
}

ClientWorkerState::ClientWorkerState(nsContentUtils::StorageAccess aStorageAccess)
  : mData(MakeUnique<IPCClientWorkerState>(aStorageAccess))
{
}

ClientWorkerState::ClientWorkerState(const IPCClientWorkerState& aData)
  : mData(MakeUnique<IPCClientWorkerState>(aData))
{
}

ClientWorkerState::ClientWorkerState(ClientWorkerState&& aRight)
  : mData(Move(aRight.mData))
{
}

ClientWorkerState::ClientWorkerState(const ClientWorkerState& aRight)
{
  operator=(aRight);
}

ClientWorkerState&
ClientWorkerState::operator=(const ClientWorkerState& aRight)
{
  mData.reset();
  mData = MakeUnique<IPCClientWorkerState>(*aRight.mData);
  return *this;
}

ClientWorkerState&
ClientWorkerState::operator=(ClientWorkerState&& aRight)
{
  mData.reset();
  mData = Move(aRight.mData);
  return *this;
}

ClientWorkerState::~ClientWorkerState()
{
}

nsContentUtils::StorageAccess
ClientWorkerState::GetStorageAccess() const
{
  return mData->storageAccess();
}

const IPCClientWorkerState&
ClientWorkerState::ToIPC() const
{
  return *mData;
}

ClientState::ClientState()
{
}

ClientState::ClientState(const ClientWindowState& aWindowState)
{
  mData.emplace(AsVariant(aWindowState));
}

ClientState::ClientState(const ClientWorkerState& aWorkerState)
{
  mData.emplace(AsVariant(aWorkerState));
}

ClientState::ClientState(const IPCClientWindowState& aData)
{
  mData.emplace(AsVariant(ClientWindowState(aData)));
}

ClientState::ClientState(const IPCClientWorkerState& aData)
{
  mData.emplace(AsVariant(ClientWorkerState(aData)));
}

ClientState::ClientState(ClientState&& aRight)
  : mData(Move(aRight.mData))
{
}

ClientState&
ClientState::operator=(ClientState&& aRight)
{
  mData = Move(aRight.mData);
  return *this;
}

ClientState::~ClientState()
{
}

// static
ClientState
ClientState::FromIPC(const IPCClientState& aData)
{
  switch(aData.type()) {
    case IPCClientState::TIPCClientWindowState:
      return ClientState(aData.get_IPCClientWindowState());
    case IPCClientState::TIPCClientWorkerState:
      return ClientState(aData.get_IPCClientWorkerState());
    default:
      MOZ_CRASH("unexpected IPCClientState type");
  }
}

bool
ClientState::IsWindowState() const
{
  return mData.isSome() && mData.ref().is<ClientWindowState>();
}

const ClientWindowState&
ClientState::AsWindowState() const
{
  return mData.ref().as<ClientWindowState>();
}

bool
ClientState::IsWorkerState() const
{
  return mData.isSome() && mData.ref().is<ClientWorkerState>();
}

const ClientWorkerState&
ClientState::AsWorkerState() const
{
  return mData.ref().as<ClientWorkerState>();
}

nsContentUtils::StorageAccess
ClientState::GetStorageAccess() const
{
  if (IsWindowState()) {
    return AsWindowState().GetStorageAccess();
  }

  return AsWorkerState().GetStorageAccess();
}

const IPCClientState
ClientState::ToIPC() const
{
  if (IsWindowState()) {
    return IPCClientState(AsWindowState().ToIPC());
  }

  return IPCClientState(AsWorkerState().ToIPC());
}

} // namespace dom
} // namespace mozilla
