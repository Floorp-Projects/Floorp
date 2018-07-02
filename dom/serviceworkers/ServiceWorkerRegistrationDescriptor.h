/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ServiceWorkerRegistrationDescriptor_h
#define _mozilla_dom_ServiceWorkerRegistrationDescriptor_h

#include "mozilla/Maybe.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

namespace ipc {
class PrincipalInfo;
} // namespace ipc

namespace dom {

class IPCServiceWorkerRegistrationDescriptor;
class OptionalIPCServiceWorkerDescriptor;
class ServiceWorkerInfo;
enum class ServiceWorkerUpdateViaCache : uint8_t;

// This class represents a snapshot of a particular
// ServiceWorkerRegistrationInfo object. It is threadsafe and can be
// transferred across processes.
class ServiceWorkerRegistrationDescriptor final
{
  // This class is largely a wrapper wround an IPDL generated struct.  We
  // need the wrapper class since IPDL generated code includes windows.h
  // which is in turn incompatible with bindings code.
  UniquePtr<IPCServiceWorkerRegistrationDescriptor> mData;

  Maybe<IPCServiceWorkerDescriptor>
  NewestInternal() const;

public:
  ServiceWorkerRegistrationDescriptor(uint64_t aId,
                                      nsIPrincipal* aPrincipal,
                                      const nsACString& aScope,
                                      ServiceWorkerUpdateViaCache aUpdateViaCache);

  ServiceWorkerRegistrationDescriptor(uint64_t aId,
                                      const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                                      const nsACString& aScope,
                                      ServiceWorkerUpdateViaCache aUpdateViaCache);

  explicit ServiceWorkerRegistrationDescriptor(const IPCServiceWorkerRegistrationDescriptor& aDescriptor);

  ServiceWorkerRegistrationDescriptor(const ServiceWorkerRegistrationDescriptor& aRight);

  ServiceWorkerRegistrationDescriptor&
  operator=(const ServiceWorkerRegistrationDescriptor& aRight);

  ServiceWorkerRegistrationDescriptor(ServiceWorkerRegistrationDescriptor&& aRight);

  ServiceWorkerRegistrationDescriptor&
  operator=(ServiceWorkerRegistrationDescriptor&& aRight);

  ~ServiceWorkerRegistrationDescriptor();

  bool
  operator==(const ServiceWorkerRegistrationDescriptor& aRight) const;

  uint64_t
  Id() const;

  ServiceWorkerUpdateViaCache
  UpdateViaCache() const;

  const mozilla::ipc::PrincipalInfo&
  PrincipalInfo() const;

  nsCOMPtr<nsIPrincipal>
  GetPrincipal() const;

  const nsCString&
  Scope() const;

  Maybe<ServiceWorkerDescriptor>
  GetInstalling() const;

  Maybe<ServiceWorkerDescriptor>
  GetWaiting() const;

  Maybe<ServiceWorkerDescriptor>
  GetActive() const;

  Maybe<ServiceWorkerDescriptor>
  Newest() const;

  bool
  HasWorker(const ServiceWorkerDescriptor& aDescriptor) const;

  bool
  IsValid() const;

  void
  SetUpdateViaCache(ServiceWorkerUpdateViaCache aUpdateViaCache);

  void
  SetWorkers(ServiceWorkerInfo* aInstalling,
             ServiceWorkerInfo* aWaiting,
             ServiceWorkerInfo* aActive);

  void
  SetWorkers(const OptionalIPCServiceWorkerDescriptor& aInstalling,
             const OptionalIPCServiceWorkerDescriptor& aWaiting,
             const OptionalIPCServiceWorkerDescriptor& aActive);

  // Expose the underlying IPC type so that it can be passed via IPC.
  const IPCServiceWorkerRegistrationDescriptor&
  ToIPC() const;
};

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ServiceWorkerRegistrationDescriptor_h
