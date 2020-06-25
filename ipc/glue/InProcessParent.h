/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_InProcessParent_h
#define mozilla_ipc_InProcessParent_h

#include "mozilla/ipc/PInProcessParent.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace dom {
class PWindowGlobalParent;
class PWindowGlobalChild;
}  // namespace dom

namespace ipc {

class InProcessChild;

/**
 * The `InProcessParent` class represents the parent half of a main-thread to
 * main-thread actor.
 *
 * The `PInProcess` actor should be used as an alternate manager to `PContent`
 * for async actors which want to communicate uniformly between Content->Chrome
 * and Chrome->Chrome situations.
 */
class InProcessParent final : public nsIObserver, public PInProcessParent {
 public:
  friend class InProcessChild;
  friend class PInProcessParent;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // Get the singleton instance of this actor.
  static InProcessParent* Singleton();

  // Get the child side of the in-process child actor |aActor|. If |aActor| is
  // not an in-process actor, or is not connected, this method will return
  // |nullptr|.
  static IProtocol* ChildActorFor(IProtocol* aActor);

 private:
  // Lifecycle management is implemented in InProcessImpl.cpp
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  ~InProcessParent() = default;

  static void Startup();
  static void Shutdown();

  static StaticRefPtr<InProcessParent> sSingleton;
  static bool sShutdown;
};

}  // namespace ipc
}  // namespace mozilla

#endif  // defined(mozilla_ipc_InProcessParent_h)
