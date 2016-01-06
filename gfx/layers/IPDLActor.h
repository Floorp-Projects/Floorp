/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_IPDLACTOR_H
#define MOZILLA_LAYERS_IPDLACTOR_H

#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/unused.h"

namespace mozilla {
namespace layers {

/// A base class to facilitate the deallocation of IPDL actors.
///
/// This class implements a simple deallocation protocol that avoids races
/// between async messages and actor destruction. The priniple is that the
/// child side is always the one that tells the parent that the actor pair is
/// going to be destroyed through the async Destroy message, and the parent
/// side always sends the __delete__ message.
/// Once ChildActor::Destroy has been called it is invalid to send any other
/// IPDL message from the child, although the child might receive messages
/// from the parent that were sent before the parent received the Destroy
/// message. Always check the result of ChildActor::Destroy before sending
/// anything.
///
/// The actual IPDL protocol must have the following messages:
///
/// child:
///   async __delete__();
/// parent:
///   async Destroy();
///   sync DestroySynchronously();
///
template<typename Protocol>
class ChildActor : public Protocol
{
public:
  ChildActor() : mDestroyed(false) {}

  ~ChildActor() { MOZ_ASSERT(mDestroyed); }

  /// Check the return of CanSend before sending any message!
  bool CanSend() const { return !mDestroyed; }

  /// The normal way to destroy the actor.
  ///
  /// This will asynchronously send a Destroy message to the parent actor, whom
  /// will send the delete message.
  void Destroy(CompositableForwarder* aFwd = nullptr)
  {
    MOZ_ASSERT(!mDestroyed);
    if (!mDestroyed) {
      mDestroyed = true;
      DestroyManagees();
      if (!aFwd || !aFwd->DestroyInTransaction(this, false)) {
        this->SendDestroy();
      }
    }
  }

  /// The ugly and slow way to destroy the actor.
  ///
  /// This will block until the Parent actor has handled the Destroy message,
  /// and then start the asynchronous handshake (and destruction will already
  /// be done on the parent side, when the async part happens).
  void DestroySynchronously(CompositableForwarder* aFwd = nullptr)
  {
    MOZ_PERFORMANCE_WARNING("gfx", "IPDL actor requires synchronous deallocation");
    MOZ_ASSERT(!mDestroyed);
    if (!mDestroyed) {
      DestroyManagees();
      mDestroyed = true;
      if (!aFwd || !aFwd->DestroyInTransaction(this, true)) {
        this->SendDestroySync();
        this->SendDestroy();
      }
    }
  }

  /// If the transaction that was supposed to destroy the texture fails for
  /// whatever reason, fallback to destroying the actor synchronously.
  static bool DestroyFallback(Protocol* aActor)
  {
    return aActor->SendDestroySync();
  }

  /// Override this if the protocol manages other protocols, and destroy the
  /// managees from there
  virtual void DestroyManagees() {}

private:
  bool mDestroyed;
};

/// A base class to facilitate the deallocation of IPDL actors.
///
/// Implements the parent side of the simple deallocation handshake.
/// Override the Destroy method rather than the ActorDestroy method.
template<typename Protocol>
class ParentActor : public Protocol
{
public:
  ParentActor() : mDestroyed(false) {}

  ~ParentActor() { MOZ_ASSERT(mDestroyed); }

  bool CanSend() const { return !mDestroyed; }

  // Override this rather than ActorDestroy
  virtual void Destroy() {}

  virtual bool RecvDestroy() override
  {
    if (!mDestroyed) {
      Destroy();
      mDestroyed = true;
    }
    Unused << Protocol::Send__delete__(this);
    return true;
  }

  virtual bool RecvDestroySync() override
  {
    if (!mDestroyed) {
      Destroy();
      mDestroyed = true;
    }
    return true;
  }

  typedef ipc::IProtocolManager<ipc::IProtocol>::ActorDestroyReason Why;

  virtual void ActorDestroy(Why) override
  {
    if (!mDestroyed) {
      Destroy();
      mDestroyed = true;
    }
  }

private:
  bool mDestroyed;
};

} // namespace
} // namespace

#endif
