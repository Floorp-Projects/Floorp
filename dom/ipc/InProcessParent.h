/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InProcessParent_h
#define mozilla_dom_InProcessParent_h

#include "mozilla/dom/PInProcessParent.h"
#include "mozilla/dom/JSProcessActorParent.h"
#include "mozilla/dom/ProcessActor.h"
#include "mozilla/dom/RemoteType.h"
#include "mozilla/StaticPtr.h"
#include "nsIDOMProcessParent.h"

namespace mozilla::dom {
class PWindowGlobalParent;
class PWindowGlobalChild;
class InProcessChild;

/**
 * The `InProcessParent` class represents the parent half of a main-thread to
 * main-thread actor.
 *
 * The `PInProcess` actor should be used as an alternate manager to `PContent`
 * for async actors which want to communicate uniformly between Content->Chrome
 * and Chrome->Chrome situations.
 */
class InProcessParent final : public nsIDOMProcessParent,
                              public nsIObserver,
                              public PInProcessParent,
                              public ProcessActor {
 public:
  friend class InProcessChild;
  friend class PInProcessParent;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMPROCESSPARENT
  NS_DECL_NSIOBSERVER

  // Get the singleton instance of this actor.
  static InProcessParent* Singleton();

  // Get the child side of the in-process child actor |aActor|. If |aActor| is
  // not an in-process actor, or is not connected, this method will return
  // |nullptr|.
  static IProtocol* ChildActorFor(IProtocol* aActor);

  const nsACString& GetRemoteType() const override { return NOT_REMOTE_TYPE; };

 protected:
  already_AddRefed<JSActor> InitJSActor(JS::Handle<JSObject*> aMaybeActor,
                                        const nsACString& aName,
                                        ErrorResult& aRv) override;
  mozilla::ipc::IProtocol* AsNativeActor() override { return this; }

 private:
  // Lifecycle management is implemented in InProcessImpl.cpp
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  ~InProcessParent() = default;

  static void Startup();
  static void Shutdown();

  static StaticRefPtr<InProcessParent> sSingleton;
  static bool sShutdown;

  nsRefPtrHashtable<nsCStringHashKey, JSProcessActorParent> mProcessActors;
};

}  // namespace mozilla::dom

#endif  // defined(mozilla_dom_InProcessParent_h)
