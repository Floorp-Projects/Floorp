/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  screenshotContentSpec,
} = require("resource://devtools/shared/specs/screenshot-content.js");
const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");

class ScreenshotContentFront extends FrontClassWithSpec(screenshotContentSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "screenshotContentActor";
  }
}

exports.ScreenshotContentFront = ScreenshotContentFront;
registerFront(ScreenshotContentFront);
