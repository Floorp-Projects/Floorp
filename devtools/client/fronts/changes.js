/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");
const { changesSpec } = require("resource://devtools/shared/specs/changes.js");

/**
 * ChangesFront, the front object for the ChangesActor
 */
class ChangesFront extends FrontClassWithSpec(changesSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "changesActor";
  }

  async initialize() {
    // Ensure the corresponding ChangesActor is immediately available and ready to track
    // changes by calling a method on it. Actors are lazy and won't be created until
    // actually used.
    await super.start();
  }
}

exports.ChangesFront = ChangesFront;
registerFront(ChangesFront);
