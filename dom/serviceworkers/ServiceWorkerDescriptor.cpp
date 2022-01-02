/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerDescriptor.h"
#include "mozilla/dom/IPCServiceWorkerDescriptor.h"
#include "mozilla/dom/ServiceWorkerBinding.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::PrincipalInfo;
using mozilla::ipc::PrincipalInfoToPrincipal;

ServiceWorkerDescriptor::ServiceWorkerDescriptor(
    uint64_t aId, uint64_t aRegistrationId, uint64_t aRegistrationVersion,
    nsIPrincipal* aPrincipal, const nsACString& aScope,
    const nsACString& aScriptURL, ServiceWorkerState aState)
    : mData(MakeUnique<IPCServiceWorkerDescriptor>()) {
  MOZ_ALWAYS_SUCCEEDS(
      PrincipalToPrincipalInfo(aPrincipal, &mData->principalInfo()));

  mData->id() = aId;
  mData->registrationId() = aRegistrationId;
  mData->registrationVersion() = aRegistrationVersion;
  mData->scope() = aScope;
  mData->scriptURL() = aScriptURL;
  mData->state() = aState;
  // Set HandlesFetch as true in default
  mData->handlesFetch() = true;
}

ServiceWorkerDescriptor::ServiceWorkerDescriptor(
    uint64_t aId, uint64_t aRegistrationId, uint64_t aRegistrationVersion,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo, const nsACString& aScope,
    const nsACString& aScriptURL, ServiceWorkerState aState)
    : mData(MakeUnique<IPCServiceWorkerDescriptor>(
          aId, aRegistrationId, aRegistrationVersion, aPrincipalInfo,
          nsCString(aScriptURL), nsCString(aScope), aState, true)) {}

ServiceWorkerDescriptor::ServiceWorkerDescriptor(
    const IPCServiceWorkerDescriptor& aDescriptor)
    : mData(MakeUnique<IPCServiceWorkerDescriptor>(aDescriptor)) {}

ServiceWorkerDescriptor::ServiceWorkerDescriptor(
    const ServiceWorkerDescriptor& aRight) {
  operator=(aRight);
}

ServiceWorkerDescriptor& ServiceWorkerDescriptor::operator=(
    const ServiceWorkerDescriptor& aRight) {
  if (this == &aRight) {
    return *this;
  }
  mData.reset();
  mData = MakeUnique<IPCServiceWorkerDescriptor>(*aRight.mData);
  return *this;
}

ServiceWorkerDescriptor::ServiceWorkerDescriptor(
    ServiceWorkerDescriptor&& aRight)
    : mData(std::move(aRight.mData)) {}

ServiceWorkerDescriptor& ServiceWorkerDescriptor::operator=(
    ServiceWorkerDescriptor&& aRight) {
  mData.reset();
  mData = std::move(aRight.mData);
  return *this;
}

ServiceWorkerDescriptor::~ServiceWorkerDescriptor() = default;

bool ServiceWorkerDescriptor::operator==(
    const ServiceWorkerDescriptor& aRight) const {
  return *mData == *aRight.mData;
}

uint64_t ServiceWorkerDescriptor::Id() const { return mData->id(); }

uint64_t ServiceWorkerDescriptor::RegistrationId() const {
  return mData->registrationId();
}

uint64_t ServiceWorkerDescriptor::RegistrationVersion() const {
  return mData->registrationVersion();
}

const mozilla::ipc::PrincipalInfo& ServiceWorkerDescriptor::PrincipalInfo()
    const {
  return mData->principalInfo();
}

Result<nsCOMPtr<nsIPrincipal>, nsresult> ServiceWorkerDescriptor::GetPrincipal()
    const {
  AssertIsOnMainThread();
  return PrincipalInfoToPrincipal(mData->principalInfo());
}

const nsCString& ServiceWorkerDescriptor::Scope() const {
  return mData->scope();
}

const nsCString& ServiceWorkerDescriptor::ScriptURL() const {
  return mData->scriptURL();
}

ServiceWorkerState ServiceWorkerDescriptor::State() const {
  return mData->state();
}

void ServiceWorkerDescriptor::SetState(ServiceWorkerState aState) {
  mData->state() = aState;
}

void ServiceWorkerDescriptor::SetRegistrationVersion(uint64_t aVersion) {
  MOZ_DIAGNOSTIC_ASSERT(aVersion > mData->registrationVersion());
  mData->registrationVersion() = aVersion;
}

bool ServiceWorkerDescriptor::HandlesFetch() const {
  return mData->handlesFetch();
}

void ServiceWorkerDescriptor::SetHandlesFetch(bool aHandlesFetch) {
  mData->handlesFetch() = aHandlesFetch;
}

bool ServiceWorkerDescriptor::Matches(
    const ServiceWorkerDescriptor& aDescriptor) const {
  return Id() == aDescriptor.Id() && Scope() == aDescriptor.Scope() &&
         ScriptURL() == aDescriptor.ScriptURL() &&
         PrincipalInfo() == aDescriptor.PrincipalInfo();
}

const IPCServiceWorkerDescriptor& ServiceWorkerDescriptor::ToIPC() const {
  return *mData;
}

}  // namespace dom
}  // namespace mozilla
