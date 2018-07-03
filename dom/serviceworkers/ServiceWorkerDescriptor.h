/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ServiceWorkerDescriptor_h
#define _mozilla_dom_ServiceWorkerDescriptor_h

#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsIPrincipal;

namespace mozilla {

namespace ipc {
class PrincipalInfo;
} // namespace ipc

namespace dom {

class IPCServiceWorkerDescriptor;
enum class ServiceWorkerState : uint8_t;

// This class represents a snapshot of a particular ServiceWorkerInfo object.
// It is threadsafe and can be transferred across processes.  This is useful
// because most of its values are immutable and can be relied upon to be
// accurate. Currently the only variable field is the ServiceWorkerState.
class ServiceWorkerDescriptor final
{
  // This class is largely a wrapper around an IPDL generated struct.  We
  // need the wrapper class since IPDL generated code includes windows.h
  // which is in turn incompatible with bindings code.
  UniquePtr<IPCServiceWorkerDescriptor> mData;

public:
  ServiceWorkerDescriptor(uint64_t aId,
                          uint64_t aRegistrationId,
                          uint64_t aRegistrationVersion,
                          nsIPrincipal* aPrincipal,
                          const nsACString& aScope,
                          const nsACString& aScriptURL,
                          ServiceWorkerState aState);

  ServiceWorkerDescriptor(uint64_t aId,
                          uint64_t aRegistrationId,
                          uint64_t aRegistrationVersion,
                          const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                          const nsACString& aScope,
                          const nsACString& aScriptURL,
                          ServiceWorkerState aState);

  explicit ServiceWorkerDescriptor(const IPCServiceWorkerDescriptor& aDescriptor);

  ServiceWorkerDescriptor(const ServiceWorkerDescriptor& aRight);

  ServiceWorkerDescriptor&
  operator=(const ServiceWorkerDescriptor& aRight);

  ServiceWorkerDescriptor(ServiceWorkerDescriptor&& aRight);

  ServiceWorkerDescriptor&
  operator=(ServiceWorkerDescriptor&& aRight);

  ~ServiceWorkerDescriptor();

  bool
  operator==(const ServiceWorkerDescriptor& aRight) const;

  uint64_t
  Id() const;

  uint64_t
  RegistrationId() const;

  uint64_t
  RegistrationVersion() const;

  const mozilla::ipc::PrincipalInfo&
  PrincipalInfo() const;

  nsCOMPtr<nsIPrincipal>
  GetPrincipal() const;

  const nsCString&
  Scope() const;

  const nsCString&
  ScriptURL() const;

  ServiceWorkerState
  State() const;

  void
  SetState(ServiceWorkerState aState);

  void
  SetRegistrationVersion(uint64_t aVersion);

  // Try to determine if two workers match each other.  This is less strict
  // than an operator==() call since it ignores mutable values like State().
  bool
  Matches(const ServiceWorkerDescriptor& aDescriptor) const;

  // Expose the underlying IPC type so that it can be passed via IPC.
  const IPCServiceWorkerDescriptor&
  ToIPC() const;
};

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ServiceWorkerDescriptor_h
