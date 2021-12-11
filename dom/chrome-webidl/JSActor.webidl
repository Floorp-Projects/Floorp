/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


interface mixin JSActor {
    [Throws]
    void sendAsyncMessage(DOMString messageName,
                          optional any obj);

    [Throws]
    Promise<any> sendQuery(DOMString messageName,
                           optional any obj);

    readonly attribute UTF8String name;
};

/**
 * WebIDL callback interface version of the nsIObserver interface for use when
 * calling the observe method on JSActors.
 *
 * NOTE: This isn't marked as ChromeOnly, as it has no interface object, and
 * thus cannot be conditionally exposed.
 */
[Exposed=Window]
callback interface MozObserverCallback {
  void observe(nsISupports subject, ByteString topic, DOMString? data);
};

/**
 * WebIDL callback interface calling the `didDestroy`, and
 * `actorCreated` methods on JSActors.
 */
[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback MozJSActorCallback = void();

/**
 * The didDestroy method, if present, will be called after the actor is no
 * longer able to receive any more messages.
 * The actorCreated method, if present, will be called immediately after the
 * actor has been created and initialized.
 */
[GenerateInit]
dictionary MozJSActorCallbacks {
  [ChromeOnly] MozJSActorCallback didDestroy;
  [ChromeOnly] MozJSActorCallback actorCreated;
};
