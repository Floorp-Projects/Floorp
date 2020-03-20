/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { framerateSpec } = require("devtools/shared/specs/framerate");

/**
 * The corresponding Front object for the FramerateActor.
 */
class FramerateFront extends FrontClassWithSpec(framerateSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "framerateActor";
  }
}

exports.FramerateFront = FramerateFront;
registerFront(FramerateFront);
