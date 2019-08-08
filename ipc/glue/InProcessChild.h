/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_InProcessChild_h
#define mozilla_ipc_InProcessChild_h

#include "mozilla/ipc/PInProcessChild.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace dom {
class PWindowGlobalParent;
class PWindowGlobalChild;
}  // namespace dom

namespace ipc {

class InProcessParent;

/**
 * The `InProcessChild` class represents the child half of a main-thread to
 * main-thread actor.
 *
 * The `PInProcess` actor should be used as an alternate manager to `PContent`
 * for async actors which want to communicate uniformly between Content->Chrome
 * and Chrome->Chrome situations.
 */
class InProcessChild : public PInProcessChild {
 public:
  friend class InProcessParent;
  friend class PInProcessChild;

  NS_INLINE_DECL_REFCOUNTING(InProcessChild)

  // Get the singleton instance of this actor.
  static InProcessChild* Singleton();

  // Get the parent side of the in-process child actor |aActor|. If |aActor| is
  // not an in-process actor, or is not connected, this method will return
  // |nullptr|.
  static IProtocol* ParentActorFor(IProtocol* aActor);

 private:
  // NOTE: PInProcess lifecycle management is declared as staic methods and
  // state on InProcessParent, and implemented in InProcessImpl.cpp.
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual void ActorDealloc() override;
  ~InProcessChild() = default;

  static StaticRefPtr<InProcessChild> sSingleton;
};

}  // namespace ipc
}  // namespace mozilla

#endif  // defined(mozilla_ipc_InProcessChild_h)
