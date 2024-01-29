/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  objectsManagerSpec,
} = require("resource://devtools/shared/specs/objects-manager.js");
const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");

class ObjectsManagerFront extends FrontClassWithSpec(objectsManagerSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "objectsManagerActor";
  }
}

module.exports = ObjectsManagerFront;
registerFront(ObjectsManagerFront);
