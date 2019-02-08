/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[NoInterfaceObject]
interface JSWindowActor {
  [Throws]
  void sendAsyncMessage(DOMString actorName,
                        DOMString messageName,
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
};
JSWindowActorChild implements JSWindowActor;