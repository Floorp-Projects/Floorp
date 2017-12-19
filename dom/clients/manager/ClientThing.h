/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientThing_h
#define _mozilla_dom_ClientThing_h

#include "nsTArray.h"

namespace mozilla {
namespace dom {

// Base class representing various Client "things" such as ClientHandle,
// ClientSource, and ClientManager.  Currently it provides a common set
// of code for handling activation and shutdown of IPC actors.
template <typename ActorType>
class ClientThing
{
  ActorType* mActor;
  bool mShutdown;

protected:
  ClientThing()
    : mActor(nullptr)
    , mShutdown(false)
  {
  }

  ~ClientThing()
  {
    ShutdownThing();
  }

  // Return the current actor.
  ActorType*
  GetActor() const
  {
    return mActor;
  }

  // Returns true if ShutdownThing() has been called.
  bool
  IsShutdown() const
  {
    return mShutdown;
  }

  // Conditionally execute the given callable based on the current state.
  template<typename Callable>
  void
  MaybeExecute(const Callable& aSuccess,
               const std::function<void()>& aFailure = []{})
  {
    if (mShutdown) {
      aFailure();
      return;
    }
    MOZ_DIAGNOSTIC_ASSERT(mActor);
    aSuccess(mActor);
  }

  // Attach activate the thing by attaching its underlying IPC actor.  This
  // will make the thing register as the actor's owner as well.  The actor
  // must call RevokeActor() to clear this weak back reference before its
  // destroyed.
  void
  ActivateThing(ActorType* aActor)
  {
    MOZ_DIAGNOSTIC_ASSERT(aActor);
    MOZ_DIAGNOSTIC_ASSERT(!mActor);
    MOZ_DIAGNOSTIC_ASSERT(!mShutdown);
    mActor = aActor;
    mActor->SetOwner(this);
  }

  // Start destroying the underlying actor and disconnect the thing.
  void
  ShutdownThing()
  {
    if (mShutdown) {
      return;
    }
    mShutdown = true;

    // If we are shutdown before the actor, then clear the weak references
    // between the actor and the thing.
    if (mActor) {
      mActor->RevokeOwner(this);
      mActor->MaybeStartTeardown();
      mActor = nullptr;
    }
  }

public:
  // Clear the weak references between the thing and its IPC actor.
  void
  RevokeActor(ActorType* aActor)
  {
    MOZ_DIAGNOSTIC_ASSERT(mActor);
    MOZ_DIAGNOSTIC_ASSERT(mActor == aActor);
    mActor->RevokeOwner(this);
    mActor = nullptr;

    // Also consider the ClientThing shutdown.  We simply set the flag
    // instead of calling ShutdownThing() to avoid calling MaybeStartTeardown()
    // on the destroyed actor.
    mShutdown = true;
  }
};

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientThing_h
