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

ServiceWorkerDescriptor::ServiceWorkerDescriptor()
  : mData(MakeUnique<IPCServiceWorkerDescriptor>())
{
}

ServiceWorkerDescriptor::ServiceWorkerDescriptor(uint64_t aId,
                                                 const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                                                 const nsACString& aScope,
                                                 ServiceWorkerState aState)
  : mData(MakeUnique<IPCServiceWorkerDescriptor>(aId, aPrincipalInfo,
                                                 nsCString(aScope), aState))
{
}

ServiceWorkerDescriptor::ServiceWorkerDescriptor(const IPCServiceWorkerDescriptor& aDescriptor)
  : mData(MakeUnique<IPCServiceWorkerDescriptor>(aDescriptor))
{
}

ServiceWorkerDescriptor::ServiceWorkerDescriptor(const ServiceWorkerDescriptor& aRight)
{
  operator=(aRight);
}

ServiceWorkerDescriptor&
ServiceWorkerDescriptor::operator=(const ServiceWorkerDescriptor& aRight)
{
  mData.reset();
  mData = MakeUnique<IPCServiceWorkerDescriptor>(*aRight.mData);
  return *this;
}

ServiceWorkerDescriptor::ServiceWorkerDescriptor(ServiceWorkerDescriptor&& aRight)
  : mData(Move(aRight.mData))
{
}

ServiceWorkerDescriptor&
ServiceWorkerDescriptor::operator=(ServiceWorkerDescriptor&& aRight)
{
  mData.reset();
  mData = Move(aRight.mData);
  return *this;
}

ServiceWorkerDescriptor::~ServiceWorkerDescriptor()
{
}

bool
ServiceWorkerDescriptor::operator==(const ServiceWorkerDescriptor& aRight) const
{
  return *mData == *aRight.mData;
}

uint64_t
ServiceWorkerDescriptor::Id() const
{
  return mData->id();
}

const mozilla::ipc::PrincipalInfo&
ServiceWorkerDescriptor::PrincipalInfo() const
{
  return mData->principalInfo();
}

const nsCString&
ServiceWorkerDescriptor::Scope() const
{
  return mData->scope();
}

ServiceWorkerState
ServiceWorkerDescriptor::State() const
{
  return mData->state();
}

void
ServiceWorkerDescriptor::SetState(ServiceWorkerState aState)
{
  mData->state() = aState;
}

const IPCServiceWorkerDescriptor&
ServiceWorkerDescriptor::ToIPC() const
{
  return *mData;
}

} // namespace dom
} // namespace mozilla
