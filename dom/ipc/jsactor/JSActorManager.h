/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSActorManager_h
#define mozilla_dom_JSActorManager_h

#include "js/TypeDecls.h"
#include "mozilla/dom/JSActor.h"
#include "nsRefPtrHashtable.h"
#include "nsString.h"

namespace mozilla {
class ErrorResult;

namespace ipc {
class IProtocol;
}

namespace dom {

class JSActorProtocol;
class JSActorService;

class JSActorManager : public nsISupports {
 public:
  /**
   * Get or create an actor by its name.
   *
   * Will set an error on |aRv| if the actor fails to be constructed.
   */
  already_AddRefed<JSActor> GetActor(JSContext* aCx, const nsACString& aName,
                                     ErrorResult& aRv);

  /**
   * Look up an existing actor by its name, returning nullptr if it doesn't
   * already exist. Will not attempt to create the actor.
   */
  already_AddRefed<JSActor> GetExistingActor(const nsACString& aName);

  /**
   * Handle receiving a raw message from the other side.
   */
  void ReceiveRawMessage(const JSActorMessageMeta& aMetadata,
                         Maybe<ipc::StructuredCloneData>&& aData,
                         Maybe<ipc::StructuredCloneData>&& aStack);

 protected:
  /**
   * The actor is about to be destroyed so prevent it from sending any
   * more messages.
   */
  void JSActorWillDestroy();

  /**
   * Lifecycle method which will fire the `didDestroy` methods on relevant
   * actors.
   */
  void JSActorDidDestroy();

  /**
   * Return the protocol with the given name, if it is supported by the current
   * actor.
   */
  virtual already_AddRefed<JSActorProtocol> MatchingJSActorProtocol(
      JSActorService* aActorSvc, const nsACString& aName, ErrorResult& aRv) = 0;

  /**
   * Initialize a JSActor instance given the constructed JS object.
   * `aMaybeActor` may be `nullptr`, which should construct the default empty
   * actor.
   */
  virtual already_AddRefed<JSActor> InitJSActor(JS::HandleObject aMaybeActor,
                                                const nsACString& aName,
                                                ErrorResult& aRv) = 0;

  /**
   * Return this native actor. This should be the same object which is
   * implementing `JSActorManager`.
   */
  virtual mozilla::ipc::IProtocol* AsNativeActor() = 0;

 private:
  friend class JSActorService;

  /**
   * Note that a particular actor name has been unregistered, and fire the
   * `didDestroy` method on the actor, if it's been initialized.
   */
  void JSActorUnregister(const nsACString& aName);

  nsRefPtrHashtable<nsCStringHashKey, JSActor> mJSActors;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_JSActorManager_h
