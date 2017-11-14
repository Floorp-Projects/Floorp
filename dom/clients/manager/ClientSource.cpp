/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientSource.h"

#include "ClientManager.h"
#include "ClientManagerChild.h"
#include "ClientSourceChild.h"
#include "ClientValidation.h"
#include "mozilla/dom/ClientIPCTypes.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::PrincipalInfo;

void
ClientSource::Shutdown()
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  if (IsShutdown()) {
    return;
  }

  ShutdownThing();

  mManager = nullptr;
}

ClientSource::ClientSource(ClientManager* aManager,
                           const ClientSourceConstructorArgs& aArgs)
  : mManager(aManager)
  , mClientInfo(aArgs.id(), aArgs.type(), aArgs.principalInfo(), aArgs.creationTime())
{
  MOZ_ASSERT(mManager);
}

void
ClientSource::Activate(PClientManagerChild* aActor)
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  MOZ_ASSERT(!GetActor());

  if (IsShutdown()) {
    return;
  }

  // Fast fail if we don't understand this particular kind of PrincipalInfo.
  // This can happen since we use MozURL for validation which does not handle
  // some of the more obscure internal principal/url combinations.  Normal
  // content pages will pass this check.
  if (NS_WARN_IF(!ClientIsValidPrincipalInfo(mClientInfo.PrincipalInfo()))) {
    Shutdown();
    return;
  }

  ClientSourceConstructorArgs args(mClientInfo.Id(), mClientInfo.Type(),
                                   mClientInfo.PrincipalInfo(),
                                   mClientInfo.CreationTime());
  PClientSourceChild* actor = aActor->SendPClientSourceConstructor(args);
  if (!actor) {
    Shutdown();
    return;
  }

  ActivateThing(static_cast<ClientSourceChild*>(actor));
}

ClientSource::~ClientSource()
{
  Shutdown();
}

} // namespace dom
} // namespace mozilla
