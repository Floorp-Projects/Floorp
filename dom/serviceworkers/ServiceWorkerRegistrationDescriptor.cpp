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
ServiceWorkerRegistrationDescriptor::NewestInternal() const {
  Maybe<IPCServiceWorkerDescriptor> result;
  if (mData->installing().isSome()) {
    result.emplace(mData->installing().ref());
  } else if (mData->waiting().isSome()) {
    result.emplace(mData->waiting().ref());
  } else if (mData->active().isSome()) {
    result.emplace(mData->active().ref());
  }
  return result;
}

ServiceWorkerRegistrationDescriptor::ServiceWorkerRegistrationDescriptor(
    uint64_t aId, uint64_t aVersion, nsIPrincipal* aPrincipal,
    const nsACString& aScope, ServiceWorkerUpdateViaCache aUpdateViaCache)
    : mData(MakeUnique<IPCServiceWorkerRegistrationDescriptor>()) {
  MOZ_ALWAYS_SUCCEEDS(
      PrincipalToPrincipalInfo(aPrincipal, &mData->principalInfo()));

  mData->id() = aId;
  mData->version() = aVersion;
  mData->scope() = aScope;
  mData->updateViaCache() = aUpdateViaCache;
  mData->installing() = Nothing();
  mData->waiting() = Nothing();
  mData->active() = Nothing();
}

ServiceWorkerRegistrationDescriptor::ServiceWorkerRegistrationDescriptor(
    uint64_t aId, uint64_t aVersion,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo, const nsACString& aScope,
    ServiceWorkerUpdateViaCache aUpdateViaCache)
    : mData(MakeUnique<IPCServiceWorkerRegistrationDescriptor>(
          aId, aVersion, aPrincipalInfo, nsCString(aScope), aUpdateViaCache,
          Nothing(), Nothing(), Nothing())) {}

ServiceWorkerRegistrationDescriptor::ServiceWorkerRegistrationDescriptor(
    const IPCServiceWorkerRegistrationDescriptor& aDescriptor)
    : mData(MakeUnique<IPCServiceWorkerRegistrationDescriptor>(aDescriptor)) {
  MOZ_DIAGNOSTIC_ASSERT(IsValid());
}

ServiceWorkerRegistrationDescriptor::ServiceWorkerRegistrationDescriptor(
    const ServiceWorkerRegistrationDescriptor& aRight) {
  // UniquePtr doesn't have a default copy constructor, so we can't rely
  // on default copy construction.  Use the assignment operator to
  // minimize duplication.
  operator=(aRight);
}

ServiceWorkerRegistrationDescriptor&
ServiceWorkerRegistrationDescriptor::operator=(
    const ServiceWorkerRegistrationDescriptor& aRight) {
  if (this == &aRight) {
    return *this;
  }
  mData.reset();
  mData = MakeUnique<IPCServiceWorkerRegistrationDescriptor>(*aRight.mData);
  MOZ_DIAGNOSTIC_ASSERT(IsValid());
  return *this;
}

ServiceWorkerRegistrationDescriptor::ServiceWorkerRegistrationDescriptor(
    ServiceWorkerRegistrationDescriptor&& aRight)
    : mData(std::move(aRight.mData)) {
  MOZ_DIAGNOSTIC_ASSERT(IsValid());
}

ServiceWorkerRegistrationDescriptor&
ServiceWorkerRegistrationDescriptor::operator=(
    ServiceWorkerRegistrationDescriptor&& aRight) {
  mData.reset();
  mData = std::move(aRight.mData);
  MOZ_DIAGNOSTIC_ASSERT(IsValid());
  return *this;
}

ServiceWorkerRegistrationDescriptor::~ServiceWorkerRegistrationDescriptor() {
  // Non-default destructor to avoid exposing the IPC type in the header.
}

bool ServiceWorkerRegistrationDescriptor::operator==(
    const ServiceWorkerRegistrationDescriptor& aRight) const {
  return *mData == *aRight.mData;
}

uint64_t ServiceWorkerRegistrationDescriptor::Id() const { return mData->id(); }

uint64_t ServiceWorkerRegistrationDescriptor::Version() const {
  return mData->version();
}

ServiceWorkerUpdateViaCache
ServiceWorkerRegistrationDescriptor::UpdateViaCache() const {
  return mData->updateViaCache();
}

const mozilla::ipc::PrincipalInfo&
ServiceWorkerRegistrationDescriptor::PrincipalInfo() const {
  return mData->principalInfo();
}

Result<nsCOMPtr<nsIPrincipal>, nsresult>
ServiceWorkerRegistrationDescriptor::GetPrincipal() const {
  AssertIsOnMainThread();
  return PrincipalInfoToPrincipal(mData->principalInfo());
}

const nsCString& ServiceWorkerRegistrationDescriptor::Scope() const {
  return mData->scope();
}

Maybe<ServiceWorkerDescriptor>
ServiceWorkerRegistrationDescriptor::GetInstalling() const {
  Maybe<ServiceWorkerDescriptor> result;

  if (mData->installing().isSome()) {
    result.emplace(ServiceWorkerDescriptor(mData->installing().ref()));
  }

  return result;
}

Maybe<ServiceWorkerDescriptor> ServiceWorkerRegistrationDescriptor::GetWaiting()
    const {
  Maybe<ServiceWorkerDescriptor> result;

  if (mData->waiting().isSome()) {
    result.emplace(ServiceWorkerDescriptor(mData->waiting().ref()));
  }

  return result;
}

Maybe<ServiceWorkerDescriptor> ServiceWorkerRegistrationDescriptor::GetActive()
    const {
  Maybe<ServiceWorkerDescriptor> result;

  if (mData->active().isSome()) {
    result.emplace(ServiceWorkerDescriptor(mData->active().ref()));
  }

  return result;
}

Maybe<ServiceWorkerDescriptor> ServiceWorkerRegistrationDescriptor::Newest()
    const {
  Maybe<ServiceWorkerDescriptor> result;
  Maybe<IPCServiceWorkerDescriptor> newest(NewestInternal());
  if (newest.isSome()) {
    result.emplace(ServiceWorkerDescriptor(newest.ref()));
  }
  return result;
}

bool ServiceWorkerRegistrationDescriptor::HasWorker(
    const ServiceWorkerDescriptor& aDescriptor) const {
  Maybe<ServiceWorkerDescriptor> installing = GetInstalling();
  Maybe<ServiceWorkerDescriptor> waiting = GetWaiting();
  Maybe<ServiceWorkerDescriptor> active = GetActive();
  return (installing.isSome() && installing.ref().Matches(aDescriptor)) ||
         (waiting.isSome() && waiting.ref().Matches(aDescriptor)) ||
         (active.isSome() && active.ref().Matches(aDescriptor));
}

namespace {

bool IsValidWorker(
    const Maybe<IPCServiceWorkerDescriptor>& aWorker, const nsACString& aScope,
    const mozilla::ipc::ContentPrincipalInfo& aContentPrincipal) {
  if (aWorker.isNothing()) {
    return true;
  }

  auto& worker = aWorker.ref();
  if (worker.scope() != aScope) {
    return false;
  }

  auto& principalInfo = worker.principalInfo();
  if (principalInfo.type() !=
      mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) {
    return false;
  }

  auto& contentPrincipal = principalInfo.get_ContentPrincipalInfo();
  if (contentPrincipal.originNoSuffix() != aContentPrincipal.originNoSuffix() ||
      contentPrincipal.attrs() != aContentPrincipal.attrs()) {
    return false;
  }

  return true;
}

}  // anonymous namespace

bool ServiceWorkerRegistrationDescriptor::IsValid() const {
  auto& principalInfo = PrincipalInfo();
  if (principalInfo.type() !=
      mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) {
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

void ServiceWorkerRegistrationDescriptor::SetUpdateViaCache(
    ServiceWorkerUpdateViaCache aUpdateViaCache) {
  mData->updateViaCache() = aUpdateViaCache;
}

void ServiceWorkerRegistrationDescriptor::SetWorkers(
    ServiceWorkerInfo* aInstalling, ServiceWorkerInfo* aWaiting,
    ServiceWorkerInfo* aActive) {
  if (aInstalling) {
    aInstalling->SetRegistrationVersion(Version());
    mData->installing() = Some(aInstalling->Descriptor().ToIPC());
  } else {
    mData->installing() = Nothing();
  }

  if (aWaiting) {
    aWaiting->SetRegistrationVersion(Version());
    mData->waiting() = Some(aWaiting->Descriptor().ToIPC());
  } else {
    mData->waiting() = Nothing();
  }

  if (aActive) {
    aActive->SetRegistrationVersion(Version());
    mData->active() = Some(aActive->Descriptor().ToIPC());
  } else {
    mData->active() = Nothing();
  }

  MOZ_DIAGNOSTIC_ASSERT(IsValid());
}

void ServiceWorkerRegistrationDescriptor::SetVersion(uint64_t aVersion) {
  MOZ_DIAGNOSTIC_ASSERT(aVersion > mData->version());
  mData->version() = aVersion;
}

const IPCServiceWorkerRegistrationDescriptor&
ServiceWorkerRegistrationDescriptor::ToIPC() const {
  return *mData;
}

}  // namespace dom
}  // namespace mozilla
