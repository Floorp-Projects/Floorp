/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { manifestSpec } = require("devtools/shared/specs/manifest");

class ManifestFront extends FrontClassWithSpec(manifestSpec) {
  constructor(client) {
    super(client);

    // attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "manifestActor";
  }
}

exports.ManifestFront = ManifestFront;
registerFront(ManifestFront);
