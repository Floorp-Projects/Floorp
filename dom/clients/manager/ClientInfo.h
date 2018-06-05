/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _mozilla_dom_ClientInfo_h
#define _mozilla_dom_ClientInfo_h

#include "mozilla/dom/ClientBinding.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

namespace ipc {
class PrincipalInfo;
} // namespace ipc

namespace dom {

class IPCClientInfo;

// This class provides a simple structure that represents a global living
// in the system.  Its thread safe and can be transferred across process
// boundaries.  A ClientInfo object can represent either a window or a worker.
class ClientInfo final
{
  UniquePtr<IPCClientInfo> mData;

public:
  ClientInfo(const nsID& aId,
             ClientType aType,
             const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
             const TimeStamp& aCreationTime);

  ClientInfo(const ClientInfo& aRight);

  ClientInfo&
  operator=(const ClientInfo& aRight);

  ClientInfo(ClientInfo&& aRight);

  ClientInfo&
  operator=(ClientInfo&& aRight);

  explicit ClientInfo(const IPCClientInfo& aData);

  ~ClientInfo();

  bool
  operator==(const ClientInfo& aRight) const;

  // Get the unique identifier chosen at the time of the global's creation.
  const nsID&
  Id() const;

  // Determine what kind of global this is; e.g. Window, Worker, SharedWorker,
  // etc.
  ClientType
  Type() const;

  // Every global must have a principal that cannot change.
  const mozilla::ipc::PrincipalInfo&
  PrincipalInfo() const;

  // The time at which the global was created.
  const TimeStamp&
  CreationTime() const;

  // Each global has the concept of a creation URL.  For the most part this
  // does not change.  The one exception is for about:blank replacement
  // iframes.  In this case the URL starts as "about:blank", but is later
  // overriden with the final URL.
  const nsCString&
  URL() const;

  // Override the creation URL.  This should only be used for about:blank
  // replacement iframes.
  void
  SetURL(const nsACString& aURL);

  // The frame type is largely a window concept, but we track it as part
  // of the global here because of the way the Clients WebAPI was designed.
  // This is set at the time the global becomes execution ready.  Workers
  // will always return None.
  mozilla::dom::FrameType
  FrameType() const;

  // Set the frame type for the global.  This should only happen once the
  // global has become execution ready.
  void
  SetFrameType(mozilla::dom::FrameType aFrameType);

  // Convert to the ipdl generated type.
  const IPCClientInfo&
  ToIPC() const;

  // Determine if the client is in private browsing mode.
  bool
  IsPrivateBrowsing() const;

  // Get a main-thread nsIPrincipal for the client.  This may return nullptr
  // if the PrincipalInfo() fails to deserialize for some reason.
  nsCOMPtr<nsIPrincipal>
  GetPrincipal() const;
};

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientInfo_h
