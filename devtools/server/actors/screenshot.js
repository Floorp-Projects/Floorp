/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  screenshotSpec,
} = require("resource://devtools/shared/specs/screenshot.js");

const {
  captureScreenshot,
} = require("resource://devtools/server/actors/utils/capture-screenshot.js");

exports.ScreenshotActor = class ScreenshotActor extends Actor {
  constructor(conn) {
    super(conn, screenshotSpec);
  }

  async capture(args) {
    const browsingContext = BrowsingContext.get(args.browsingContextID);
    return captureScreenshot(args, browsingContext);
  }
};
