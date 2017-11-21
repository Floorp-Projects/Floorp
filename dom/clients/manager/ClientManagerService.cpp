/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientManagerService.h"

#include "ClientSourceParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::AssertIsOnBackgroundThread;
using mozilla::ipc::ContentPrincipalInfo;
using mozilla::ipc::PrincipalInfo;

namespace {

ClientManagerService* sClientManagerServiceInstance = nullptr;

bool
MatchPrincipalInfo(const PrincipalInfo& aLeft, const PrincipalInfo& aRight)
{
  if (aLeft.type() != aRight.type()) {
    return false;
  }

  switch (aLeft.type()) {
    case PrincipalInfo::TContentPrincipalInfo:
    {
      const ContentPrincipalInfo& leftContent = aLeft.get_ContentPrincipalInfo();
      const ContentPrincipalInfo& rightContent = aRight.get_ContentPrincipalInfo();
      return leftContent.attrs() == rightContent.attrs() &&
             leftContent.originNoSuffix() == rightContent.originNoSuffix();
    }
    case PrincipalInfo::TSystemPrincipalInfo:
    {
      // system principal always matches
      return true;
    }
    case PrincipalInfo::TNullPrincipalInfo:
    {
      // null principal never matches
      return false;
    }
    default:
    {
      break;
    }
  }

  // Clients (windows/workers) should never have an expanded principal type.
  MOZ_CRASH("unexpected principal type!");
}

} // anonymous namespace

ClientManagerService::ClientManagerService()
{
  AssertIsOnBackgroundThread();
}

ClientManagerService::~ClientManagerService()
{
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(mSourceTable.Count() == 0);

  MOZ_DIAGNOSTIC_ASSERT(sClientManagerServiceInstance == this);
  sClientManagerServiceInstance = nullptr;
}

// static
already_AddRefed<ClientManagerService>
ClientManagerService::GetOrCreateInstance()
{
  AssertIsOnBackgroundThread();

  if (!sClientManagerServiceInstance) {
    sClientManagerServiceInstance = new ClientManagerService();
  }

  RefPtr<ClientManagerService> ref(sClientManagerServiceInstance);
  return ref.forget();
}

bool
ClientManagerService::AddSource(ClientSourceParent* aSource)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aSource);
  auto entry = mSourceTable.LookupForAdd(aSource->Info().Id());
  // Do not permit overwriting an existing ClientSource with the same
  // UUID.  This would allow a spoofed ClientParentSource actor to
  // intercept postMessage() intended for the real actor.
  if (NS_WARN_IF(!!entry)) {
    return false;
  }
  entry.OrInsert([&] { return aSource; });
  return true;
}

bool
ClientManagerService::RemoveSource(ClientSourceParent* aSource)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aSource);
  auto entry = mSourceTable.Lookup(aSource->Info().Id());
  if (NS_WARN_IF(!entry)) {
    return false;
  }
  entry.Remove();
  return true;
}

ClientSourceParent*
ClientManagerService::FindSource(const nsID& aID, const PrincipalInfo& aPrincipalInfo)
{
  AssertIsOnBackgroundThread();

  auto entry = mSourceTable.Lookup(aID);
  if (!entry) {
    return nullptr;
  }

  ClientSourceParent* source = entry.Data();
  if (source->IsFrozen() ||
      !MatchPrincipalInfo(source->Info().PrincipalInfo(), aPrincipalInfo)) {
    return nullptr;
  }

  return source;
}

} // namespace dom
} // namespace mozilla
