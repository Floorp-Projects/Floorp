/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientSourceOpChild.h"

#include "ClientSource.h"
#include "ClientSourceChild.h"
#include "mozilla/Assertions.h"
#include "mozilla/Unused.h"

namespace mozilla::dom {

ClientSource* ClientSourceOpChild::GetSource() const {
  auto actor = static_cast<ClientSourceChild*>(Manager());
  return actor->GetSource();
}

template <typename Method, typename... Args>
void ClientSourceOpChild::DoSourceOp(Method aMethod, Args&&... aArgs) {
  RefPtr<ClientOpPromise> promise;
  nsCOMPtr<nsISerialEventTarget> target;

  // Some ClientSource operations can cause the ClientSource to be destroyed.
  // This means we should reference the ClientSource pointer for the minimum
  // possible to start the operation.  Use an extra block scope here to help
  // enforce this and prevent accidental usage later in the method.
  {
    ClientSource* source = GetSource();
    if (!source) {
      CopyableErrorResult rv;
      rv.ThrowAbortError("Unknown Client");
      Unused << PClientSourceOpChild::Send__delete__(this, rv);
      return;
    }

    target = source->EventTarget();

    // This may cause the ClientSource object to be destroyed.  Do not
    // use the source variable after this call.
    promise = (source->*aMethod)(std::forward<Args>(aArgs)...);
  }

  // The ClientSource methods are required to always return a promise.  If
  // they encounter an error they should just immediately resolve or reject
  // the promise as appropriate.
  MOZ_DIAGNOSTIC_ASSERT(promise);

  // Capture 'this' is safe here because we disconnect the promise
  // ActorDestroy() which ensures neither lambda is called if the
  // actor is destroyed before the source operation completes.
  //
  // Also capture the promise to ensure it lives until we get a reaction
  // or the actor starts shutting down and we disconnect our Thenable.
  // If the ClientSource is doing something async it may throw away the
  // promise on its side if the global is closed.
  promise
      ->Then(
          target, __func__,
          [this, promise](const mozilla::dom::ClientOpResult& aResult) {
            mPromiseRequestHolder.Complete();
            Unused << PClientSourceOpChild::Send__delete__(this, aResult);
          },
          [this, promise](const CopyableErrorResult& aRv) {
            mPromiseRequestHolder.Complete();
            Unused << PClientSourceOpChild::Send__delete__(this, aRv);
          })
      ->Track(mPromiseRequestHolder);
}

void ClientSourceOpChild::ActorDestroy(ActorDestroyReason aReason) {
  Cleanup();
}

void ClientSourceOpChild::Init(const ClientOpConstructorArgs& aArgs) {
  switch (aArgs.type()) {
    case ClientOpConstructorArgs::TClientControlledArgs: {
      DoSourceOp(&ClientSource::Control, aArgs.get_ClientControlledArgs());
      break;
    }
    case ClientOpConstructorArgs::TClientFocusArgs: {
      DoSourceOp(&ClientSource::Focus, aArgs.get_ClientFocusArgs());
      break;
    }
    case ClientOpConstructorArgs::TClientPostMessageArgs: {
      DoSourceOp(&ClientSource::PostMessage, aArgs.get_ClientPostMessageArgs());
      break;
    }
    case ClientOpConstructorArgs::TClientClaimArgs: {
      MOZ_ASSERT_UNREACHABLE("shouldn't happen with parent intercept");
      break;
    }
    case ClientOpConstructorArgs::TClientGetInfoAndStateArgs: {
      DoSourceOp(&ClientSource::GetInfoAndState,
                 aArgs.get_ClientGetInfoAndStateArgs());
      break;
    }
    case ClientOpConstructorArgs::TClientEvictBFCacheArgs: {
      DoSourceOp(&ClientSource::EvictFromBFCacheOp);
      break;
    }
    default: {
      MOZ_ASSERT_UNREACHABLE("unknown client operation!");
      break;
    }
  }

  mInitialized.Flip();

  if (mDeletionRequested) {
    Cleanup();
    delete this;
  }
}

void ClientSourceOpChild::ScheduleDeletion() {
  if (mInitialized) {
    Cleanup();
    delete this;
    return;
  }

  mDeletionRequested.Flip();
}

ClientSourceOpChild::~ClientSourceOpChild() {
  MOZ_DIAGNOSTIC_ASSERT(mInitialized);
}

void ClientSourceOpChild::Cleanup() {
  mPromiseRequestHolder.DisconnectIfExists();
}

}  // namespace mozilla::dom
