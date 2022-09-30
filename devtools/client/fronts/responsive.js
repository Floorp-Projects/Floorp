/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");
const {
  responsiveSpec,
} = require("resource://devtools/shared/specs/responsive.js");

/**
 * The corresponding Front object for the Responsive actor.
 */
class ResponsiveFront extends FrontClassWithSpec(responsiveSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "responsiveActor";
  }
}

exports.ResponsiveFront = ResponsiveFront;
registerFront(ResponsiveFront);
