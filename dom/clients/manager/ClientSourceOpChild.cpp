/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientSourceOpChild.h"

#include "ClientSource.h"
#include "ClientSourceChild.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace dom {

ClientSource*
ClientSourceOpChild::GetSource() const
{
  auto actor = static_cast<ClientSourceChild*>(Manager());
  return actor->GetSource();
}

template<typename Method, typename Args>
void
ClientSourceOpChild::DoSourceOp(Method aMethod, const Args& aArgs)
{
  RefPtr<ClientOpPromise> promise;
  nsCOMPtr<nsISerialEventTarget> target;

  // Some ClientSource operations can cause the ClientSource to be destroyed.
  // This means we should reference the ClientSource pointer for the minimum
  // possible to start the operation.  Use an extra block scope here to help
  // enforce this and prevent accidental usage later in the method.
  {
    ClientSource* source = GetSource();
    if (!source) {
      Unused << PClientSourceOpChild::Send__delete__(this, NS_ERROR_DOM_ABORT_ERR);
      return;
    }

    target = source->EventTarget();

    // This may cause the ClientSource object to be destroyed.  Do not
    // use the source variable after this call.
    promise = (source->*aMethod)(aArgs);
  }

  // The ClientSource methods are required to always return a promise.  If
  // they encounter an error they should just immediately resolve or reject
  // the promise as appropriate.
  MOZ_DIAGNOSTIC_ASSERT(promise);

  // Capture 'this' is safe here because we disconnect the promise
  // ActorDestroy() which ensures nethier lambda is called if the
  // actor is destroyed before the source operation completes.
  promise->Then(target, __func__,
    [this, aArgs] (const mozilla::dom::ClientOpResult& aResult) {
      mPromiseRequestHolder.Complete();
      Unused << PClientSourceOpChild::Send__delete__(this, aResult);
    },
    [this] (nsresult aRv) {
      mPromiseRequestHolder.Complete();
      Unused << PClientSourceOpChild::Send__delete__(this, aRv);
    })->Track(mPromiseRequestHolder);
}

void
ClientSourceOpChild::ActorDestroy(ActorDestroyReason aReason)
{
  mPromiseRequestHolder.DisconnectIfExists();
}

void
ClientSourceOpChild::Init(const ClientOpConstructorArgs& aArgs)
{
  switch (aArgs.type()) {
    case ClientOpConstructorArgs::TClientControlledArgs:
    {
      DoSourceOp(&ClientSource::Control, aArgs.get_ClientControlledArgs());
      break;
    }
    case ClientOpConstructorArgs::TClientClaimArgs:
    {
      DoSourceOp(&ClientSource::Claim, aArgs.get_ClientClaimArgs());
      break;
    }
    case ClientOpConstructorArgs::TClientGetInfoAndStateArgs:
    {
      DoSourceOp(&ClientSource::GetInfoAndState,
                 aArgs.get_ClientGetInfoAndStateArgs());
      break;
    }
    default:
    {
      MOZ_ASSERT_UNREACHABLE("unknown client operation!");
      break;
    }
  }
}

} // namespace dom
} // namespace mozilla
