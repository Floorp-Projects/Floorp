/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { screenshotSpec } = require("devtools/shared/specs/screenshot");
const saveScreenshot = require("devtools/shared/screenshot/save");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");

class ScreenshotFront extends FrontClassWithSpec(screenshotSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "screenshotActor";
  }

  async captureAndSave(window, args) {
    const screenshot = await this.capture(args);
    return saveScreenshot(window, args, screenshot);
  }
}

exports.ScreenshotFront = ScreenshotFront;
registerFront(ScreenshotFront);
