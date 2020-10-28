/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { networkSpec } = require("devtools/shared/specs/network");

/**
 * Client side for the network actor, used for managing network requests.
 */

class NetworkFront extends FrontClassWithSpec(networkSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "networkActor";
  }
}

registerFront(NetworkFront);
