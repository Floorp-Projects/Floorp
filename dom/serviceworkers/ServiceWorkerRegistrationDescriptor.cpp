/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ServiceWorkerRegistrationDescriptor.h"

#include "mozilla/dom/IPCServiceWorkerRegistrationDescriptor.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "ServiceWorkerInfo.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::PrincipalInfo;
using mozilla::ipc::PrincipalInfoToPrincipal;

Maybe<IPCServiceWorkerDescriptor>
ServiceWorkerRegistrationDescriptor::NewestInternal() const
{
  Maybe<IPCServiceWorkerDescriptor> result;
  if (mData->installing().type() != OptionalIPCServiceWorkerDescriptor::Tvoid_t) {
    result.emplace(mData->installing().get_IPCServiceWorkerDescriptor());
  } else if (mData->waiting().type() != OptionalIPCServiceWorkerDescriptor::Tvoid_t) {
    result.emplace(mData->waiting().get_IPCServiceWorkerDescriptor());
  } else if (mData->active().type() != OptionalIPCServiceWorkerDescriptor::Tvoid_t) {
    result.emplace(mData->active().get_IPCServiceWorkerDescriptor());
  }
  return std::move(result);
}

ServiceWorkerRegistrationDescriptor::ServiceWorkerRegistrationDescriptor(
                                    uint64_t aId,
                                    nsIPrincipal* aPrincipal,
                                    const nsACString& aScope,
                                    ServiceWorkerUpdateViaCache aUpdateViaCache)
  : mData(MakeUnique<IPCServiceWorkerRegistrationDescriptor>())
{
  MOZ_ALWAYS_SUCCEEDS(
    PrincipalToPrincipalInfo(aPrincipal, &mData->principalInfo()));

  mData->id() = aId;
  mData->scope() = aScope;
  mData->updateViaCache() = aUpdateViaCache;
  mData->installing() = void_t();
  mData->waiting() = void_t();
  mData->active() = void_t();
}

ServiceWorkerRegistrationDescriptor::ServiceWorkerRegistrationDescriptor(
                                    uint64_t aId,
                                    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                                    const nsACString& aScope,
                                    ServiceWorkerUpdateViaCache aUpdateViaCache)
  : mData(MakeUnique<IPCServiceWorkerRegistrationDescriptor>(aId,
                                                             aPrincipalInfo,
                                                             nsCString(aScope),
                                                             aUpdateViaCache,
                                                             void_t(),
                                                             void_t(),
                                                             void_t()))
{
}

ServiceWorkerRegistrationDescriptor::ServiceWorkerRegistrationDescriptor(const IPCServiceWorkerRegistrationDescriptor& aDescriptor)
  : mData(MakeUnique<IPCServiceWorkerRegistrationDescriptor>(aDescriptor))
{
  MOZ_DIAGNOSTIC_ASSERT(IsValid());
}

ServiceWorkerRegistrationDescriptor::ServiceWorkerRegistrationDescriptor(const ServiceWorkerRegistrationDescriptor& aRight)
{
  // UniquePtr doesn't have a default copy constructor, so we can't rely
  // on default copy construction.  Use the assignment operator to
  // minimize duplication.
  operator=(aRight);
}

ServiceWorkerRegistrationDescriptor&
ServiceWorkerRegistrationDescriptor::operator=(const ServiceWorkerRegistrationDescriptor& aRight)
{
  if (this == &aRight) {
    return *this;
  }
  mData.reset();
  mData = MakeUnique<IPCServiceWorkerRegistrationDescriptor>(*aRight.mData);
  MOZ_DIAGNOSTIC_ASSERT(IsValid());
  return *this;
}

ServiceWorkerRegistrationDescriptor::ServiceWorkerRegistrationDescriptor(ServiceWorkerRegistrationDescriptor&& aRight)
  : mData(std::move(aRight.mData))
{
  MOZ_DIAGNOSTIC_ASSERT(IsValid());
}

ServiceWorkerRegistrationDescriptor&
ServiceWorkerRegistrationDescriptor::operator=(ServiceWorkerRegistrationDescriptor&& aRight)
{
  mData.reset();
  mData = std::move(aRight.mData);
  MOZ_DIAGNOSTIC_ASSERT(IsValid());
  return *this;
}

ServiceWorkerRegistrationDescriptor::~ServiceWorkerRegistrationDescriptor()
{
  // Non-default destructor to avoid exposing the IPC type in the header.
}

bool
ServiceWorkerRegistrationDescriptor::operator==(const ServiceWorkerRegistrationDescriptor& aRight) const
{
  return *mData == *aRight.mData;
}

uint64_t
ServiceWorkerRegistrationDescriptor::Id() const
{
  return mData->id();
}

ServiceWorkerUpdateViaCache
ServiceWorkerRegistrationDescriptor::UpdateViaCache() const
{
  return mData->updateViaCache();
}

const mozilla::ipc::PrincipalInfo&
ServiceWorkerRegistrationDescriptor::PrincipalInfo() const
{
  return mData->principalInfo();
}

nsCOMPtr<nsIPrincipal>
ServiceWorkerRegistrationDescriptor::GetPrincipal() const
{
  AssertIsOnMainThread();
  nsCOMPtr<nsIPrincipal> ref =  PrincipalInfoToPrincipal(mData->principalInfo());
  return std::move(ref);
}

const nsCString&
ServiceWorkerRegistrationDescriptor::Scope() const
{
  return mData->scope();
}

Maybe<ServiceWorkerDescriptor>
ServiceWorkerRegistrationDescriptor::GetInstalling() const
{
  Maybe<ServiceWorkerDescriptor> result;

  if (mData->installing().type() != OptionalIPCServiceWorkerDescriptor::Tvoid_t) {
    result.emplace(ServiceWorkerDescriptor(
      mData->installing().get_IPCServiceWorkerDescriptor()));
  }

  return std::move(result);
}

Maybe<ServiceWorkerDescriptor>
ServiceWorkerRegistrationDescriptor::GetWaiting() const
{
  Maybe<ServiceWorkerDescriptor> result;

  if (mData->waiting().type() != OptionalIPCServiceWorkerDescriptor::Tvoid_t) {
    result.emplace(ServiceWorkerDescriptor(
      mData->waiting().get_IPCServiceWorkerDescriptor()));
  }

  return std::move(result);
}

Maybe<ServiceWorkerDescriptor>
ServiceWorkerRegistrationDescriptor::GetActive() const
{
  Maybe<ServiceWorkerDescriptor> result;

  if (mData->active().type() != OptionalIPCServiceWorkerDescriptor::Tvoid_t) {
    result.emplace(ServiceWorkerDescriptor(
      mData->active().get_IPCServiceWorkerDescriptor()));
  }

  return std::move(result);
}

Maybe<ServiceWorkerDescriptor>
ServiceWorkerRegistrationDescriptor::Newest() const
{
  Maybe<ServiceWorkerDescriptor> result;
  Maybe<IPCServiceWorkerDescriptor> newest(NewestInternal());
  if (newest.isSome()) {
    result.emplace(ServiceWorkerDescriptor(newest.ref()));
  }
  return std::move(result);
}

namespace {

bool
IsValidWorker(const OptionalIPCServiceWorkerDescriptor& aWorker,
              const nsACString& aScope,
              const mozilla::ipc::ContentPrincipalInfo& aContentPrincipal)
{
  if (aWorker.type() == OptionalIPCServiceWorkerDescriptor::Tvoid_t) {
    return true;
  }

  auto& worker = aWorker.get_IPCServiceWorkerDescriptor();
  if (worker.scope() != aScope) {
    return false;
  }

  auto& principalInfo = worker.principalInfo();
  if (principalInfo.type() != mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) {
    return false;
  }

  auto& contentPrincipal = principalInfo.get_ContentPrincipalInfo();
  if (contentPrincipal.originNoSuffix() != aContentPrincipal.originNoSuffix() ||
      contentPrincipal.attrs() != aContentPrincipal.attrs()) {
    return false;
  }

  return true;
}

} // anonymous namespace

bool
ServiceWorkerRegistrationDescriptor::IsValid() const
{
  auto& principalInfo = PrincipalInfo();
  if (principalInfo.type() != mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) {
    return false;
  }

  auto& contentPrincipal = principalInfo.get_ContentPrincipalInfo();
  if (!IsValidWorker(mData->installing(), Scope(), contentPrincipal) ||
      !IsValidWorker(mData->waiting(), Scope(), contentPrincipal) ||
      !IsValidWorker(mData->active(), Scope(), contentPrincipal)) {
    return false;
  }

  return true;
}

void
ServiceWorkerRegistrationDescriptor::SetUpdateViaCache(ServiceWorkerUpdateViaCache aUpdateViaCache)
{
  mData->updateViaCache() = aUpdateViaCache;
}

void
ServiceWorkerRegistrationDescriptor::SetWorkers(ServiceWorkerInfo* aInstalling,
                                                ServiceWorkerInfo* aWaiting,
                                                ServiceWorkerInfo* aActive)
{
  if (aInstalling) {
    mData->installing() = aInstalling->Descriptor().ToIPC();
  } else {
    mData->installing() = void_t();
  }

  if (aWaiting) {
    mData->waiting() = aWaiting->Descriptor().ToIPC();
  } else {
    mData->waiting() = void_t();
  }

  if (aActive) {
    mData->active() = aActive->Descriptor().ToIPC();
  } else {
    mData->active() = void_t();
  }

  MOZ_DIAGNOSTIC_ASSERT(IsValid());
}

void
ServiceWorkerRegistrationDescriptor::SetWorkers(const OptionalIPCServiceWorkerDescriptor& aInstalling,
                                                const OptionalIPCServiceWorkerDescriptor& aWaiting,
                                                const OptionalIPCServiceWorkerDescriptor& aActive)
{
  mData->installing() = aInstalling;
  mData->waiting() = aWaiting;
  mData->active() = aActive;
  MOZ_DIAGNOSTIC_ASSERT(IsValid());
}

const IPCServiceWorkerRegistrationDescriptor&
ServiceWorkerRegistrationDescriptor::ToIPC() const
{
  return *mData;
}

} // namespace dom
} // namespace mozilla
