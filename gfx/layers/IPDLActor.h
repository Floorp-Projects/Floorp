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
/// If a sub-class overrides ActorDestroy, it should also call
/// ChildActor::ActorDestroy from there.
///
/// The typical usage of this class looks like the following:
/// some reference-counted object FooClient holds a reference to a FooChild
/// actor inheriting from ChildActor<PFooChild>.
/// Usually, the destruction of FooChild will be triggered by FooClient's
/// destructor or some other thing that really means that nothing other than
/// IPDL is holding a pointer to the actor. This is important because we track
/// this information in mReleased and use it to know when it is safe to delete
/// the actor. If FooManager::DeallocPFoChild is invoked by IPDL while mReleased
/// is still false, we don't delete the actor as we usually would. Instead we
/// set mIPCOpen to false, and ReleaseActor will delete the actor.
template<typename Protocol>
class ChildActor : public Protocol
{
public:
  ChildActor()
  : mReleased(false)
  , mSentDestroy(false)
  , mIPCOpen(true)
  {}

  ~ChildActor() { MOZ_ASSERT(mReleased); }

  /// Check the return of CanSend before sending any message!
  bool CanSend() const { return !mSentDestroy && mIPCOpen; }

  /// Return true if this actor is still connected to the IPDL system.
  bool IPCOpen() const { return mIPCOpen; }

  /// Return true if the actor was released (nothing holds on to the actor
  /// except IPDL, and the actor will most likely receive the __delete__
  /// message soon. Useful to know whether we can delete the actor when
  /// IPDL shuts down.
  bool Released() const { return mReleased; }

  /// If the transaction that was supposed to destroy the texture fails for
  /// whatever reason, fallback to destroying the actor synchronously.
  static bool DestroyFallback(Protocol* aActor)
  {
    return aActor->SendDestroySync();
  }

  typedef ipc::IProtocolManager<ipc::IProtocol>::ActorDestroyReason Why;

  virtual void ActorDestroy(Why) override
  {
    mIPCOpen = false;
  }

  /// Call this during shutdown only, if we need to destroy this
  /// actor but the object paired with the actor isn't destroyed yet.
  void ForceActorShutdown()
  {
    if (mIPCOpen && !mSentDestroy) {
      this->SendDestroy();
      mSentDestroy = true;
    }
  }

  /// The normal way to destroy the actor.
  ///
  /// This will asynchronously send a Destroy message to the parent actor, whom
  /// will send the delete message.
  /// Nothing other than IPDL should hold a pointer to the actor after this is
  /// called.
  void ReleaseActor(CompositableForwarder* aFwd = nullptr)
  {
    if (!IPCOpen()) {
      mReleased = true;
      delete this;
      return;
    }

    Destroy(aFwd, false);
  }

  /// The ugly and slow way to destroy the actor.
  ///
  /// This will block until the Parent actor has handled the Destroy message,
  /// and then start the asynchronous handshake (and destruction will already
  /// be done on the parent side, when the async part happens).
  /// Nothing other than IPDL should hold a pointer to the actor after this is
  /// called.
  void ReleaseActorSynchronously(CompositableForwarder* aFwd = nullptr)
  {
    if (!IPCOpen()) {
      mReleased = true;
      delete this;
      return;
    }

    Destroy(aFwd, true);
  }

protected:

  void Destroy(CompositableForwarder* aFwd = nullptr, bool synchronously = false)
  {
    MOZ_ASSERT(mIPCOpen);
    MOZ_ASSERT(!mReleased);
    if (mReleased) {
      return;
    }
    mReleased = true;

    if (!aFwd || !aFwd->DestroyInTransaction(this, synchronously)) {
      if (synchronously) {
        MOZ_PERFORMANCE_WARNING("gfx", "IPDL actor requires synchronous deallocation");
        this->SendDestroySync();
      } else {
        this->SendDestroy();
      }
    }
    mSentDestroy = true;
  }

private:
  bool mReleased;
  bool mSentDestroy;
  bool mIPCOpen;
};


/// A base class to facilitate the deallocation of IPDL actors.
///
/// Implements the parent side of the simple deallocation handshake.
/// Override the Destroy method rather than the ActorDestroy method.
template<typename Protocol>
class ParentActor : public Protocol
{
public:
  ParentActor() : mReleased(false) {}

  ~ParentActor() { MOZ_ASSERT(mReleased); }

  bool CanSend() const { return !mReleased; }

  // Override this rather than ActorDestroy
  virtual void Destroy() {}

  virtual bool RecvDestroy() override
  {
    if (!mReleased) {
      Destroy();
      mReleased = true;
    }
    Unused << Protocol::Send__delete__(this);
    return true;
  }

  virtual bool RecvDestroySync() override
  {
    if (!mReleased) {
      Destroy();
      mReleased = true;
    }
    return true;
  }

  typedef ipc::IProtocolManager<ipc::IProtocol>::ActorDestroyReason Why;

  virtual void ActorDestroy(Why) override
  {
    if (!mReleased) {
      Destroy();
      mReleased = true;
    }
  }

private:
  bool mReleased;
};

} // namespace
} // namespace

#endif
