/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientManagerOpParent.h"

#include "ClientManagerService.h"

namespace mozilla {
namespace dom {

template <typename Method, typename... Args>
void
ClientManagerOpParent::DoServiceOp(Method aMethod, Args&&... aArgs)
{
  // Note, we need perfect forarding of the template type in order
  // to allow already_AddRefed<> to be passed as an arg.
  RefPtr<ClientOpPromise> p = (mService->*aMethod)(Forward<Args>(aArgs)...);

  // Capturing `this` is safe here because we disconnect the promise in
  // ActorDestroy() which ensures neither lambda is called if the actor
  // is destroyed before the source operation completes.
  p->Then(GetCurrentThreadSerialEventTarget(), __func__,
    [this] (const mozilla::dom::ClientOpResult& aResult) {
      mPromiseRequestHolder.Complete();
      Unused << PClientManagerOpParent::Send__delete__(this, aResult);
    }, [this] (nsresult aRv) {
      mPromiseRequestHolder.Complete();
      Unused << PClientManagerOpParent::Send__delete__(this, aRv);
    })->Track(mPromiseRequestHolder);
}

void
ClientManagerOpParent::ActorDestroy(ActorDestroyReason aReason)
{
  mPromiseRequestHolder.DisconnectIfExists();
}

ClientManagerOpParent::ClientManagerOpParent(ClientManagerService* aService)
  : mService(aService)
{
  MOZ_DIAGNOSTIC_ASSERT(mService);
}

void
ClientManagerOpParent::Init(const ClientOpConstructorArgs& aArgs)
{
  switch (aArgs.type()) {
    case ClientOpConstructorArgs::TClientNavigateArgs:
    {
      DoServiceOp(&ClientManagerService::Navigate,
                  aArgs.get_ClientNavigateArgs());
      break;
    }
    case ClientOpConstructorArgs::TClientMatchAllArgs:
    {
      DoServiceOp(&ClientManagerService::MatchAll,
                  aArgs.get_ClientMatchAllArgs());
      break;
    }
    case ClientOpConstructorArgs::TClientClaimArgs:
    {
      DoServiceOp(&ClientManagerService::Claim, aArgs.get_ClientClaimArgs());
      break;
    }
    case ClientOpConstructorArgs::TClientGetInfoAndStateArgs:
    {
      DoServiceOp(&ClientManagerService::GetInfoAndState,
                  aArgs.get_ClientGetInfoAndStateArgs());
      break;
    }
    default:
    {
      MOZ_ASSERT_UNREACHABLE("Unknown Client operation!");
      break;
    }
  }
}

} // namespace dom
} // namespace mozilla
