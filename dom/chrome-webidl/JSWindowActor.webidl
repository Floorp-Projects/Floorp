/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface nsISupports;

[NoInterfaceObject]
interface JSWindowActor {
  [Throws]
  void sendAsyncMessage(DOMString messageName,
                        optional any obj,
                        optional any transfers);

  [Throws]
  Promise<any> sendQuery(DOMString messageName,
                         optional any obj,
                         optional any transfers);
};

[ChromeOnly, ChromeConstructor]
interface JSWindowActorParent {
  readonly attribute WindowGlobalParent manager;
};
JSWindowActorParent implements JSWindowActor;

[ChromeOnly, ChromeConstructor]
interface JSWindowActorChild {
  readonly attribute WindowGlobalChild manager;

  [Throws]
  readonly attribute Document? document;

  [Throws]
  readonly attribute BrowsingContext? browsingContext;

  // NOTE: As this returns a window proxy, it may not be currently referencing
  // the document associated with this JSWindowActor. Generally prefer using
  // `document`.
  [Throws]
  readonly attribute WindowProxy? contentWindow;
};
JSWindowActorChild implements JSWindowActor;

// WebIDL callback interface version of the nsIObserver interface for use when
// calling the observe method on JSWindowActors.
//
// NOTE: This isn't marked as ChromeOnly, as it has no interface object, and
// thus cannot be conditionally exposed.
callback interface MozObserverCallback {
  void observe(nsISupports subject, ByteString topic, DOMString? data);
};

// WebIDL callback interface calling the `willDestroy` and `didDestroy`
// method on JSWindowActors.
[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback MozActorDestroyCallback = void();

dictionary MozActorDestroyCallbacks {
  [ChromeOnly] MozActorDestroyCallback willDestroy;
  [ChromeOnly] MozActorDestroyCallback didDestroy;
};
