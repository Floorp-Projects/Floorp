/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InProcessChild_h
#define mozilla_dom_InProcessChild_h

#include "mozilla/dom/PInProcessChild.h"
#include "mozilla/dom/JSProcessActorChild.h"
#include "mozilla/dom/ProcessActor.h"
#include "mozilla/dom/RemoteType.h"
#include "mozilla/StaticPtr.h"
#include "nsIDOMProcessChild.h"

namespace mozilla::dom {
class PWindowGlobalParent;
class PWindowGlobalChild;
class InProcessParent;

/**
 * The `InProcessChild` class represents the child half of a main-thread to
 * main-thread actor.
 *
 * The `PInProcess` actor should be used as an alternate manager to `PContent`
 * for async actors which want to communicate uniformly between Content->Chrome
 * and Chrome->Chrome situations.
 */
class InProcessChild final : public nsIDOMProcessChild,
                             public PInProcessChild,
                             public ProcessActor {
 public:
  friend class InProcessParent;
  friend class PInProcessChild;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMPROCESSCHILD

  // Get the singleton instance of this actor.
  static InProcessChild* Singleton();

  // Get the parent side of the in-process child actor |aActor|. If |aActor| is
  // not an in-process actor, or is not connected, this method will return
  // |nullptr|.
  static IProtocol* ParentActorFor(IProtocol* aActor);

  const nsACString& GetRemoteType() const override { return NOT_REMOTE_TYPE; }

 protected:
  already_AddRefed<JSActor> InitJSActor(JS::Handle<JSObject*> aMaybeActor,
                                        const nsACString& aName,
                                        ErrorResult& aRv) override;
  mozilla::ipc::IProtocol* AsNativeActor() override { return this; }

 private:
  // NOTE: PInProcess lifecycle management is declared as staic methods and
  // state on InProcessParent, and implemented in InProcessImpl.cpp.
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  ~InProcessChild() = default;

  static StaticRefPtr<InProcessChild> sSingleton;

  nsRefPtrHashtable<nsCStringHashKey, JSProcessActorChild> mProcessActors;
};

}  // namespace mozilla::dom

#endif  // defined(mozilla_dom_InProcessChild_h)
