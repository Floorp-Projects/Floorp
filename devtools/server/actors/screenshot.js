/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const {captureScreenshot} = require("devtools/shared/screenshot/capture");
const {screenshotSpec} = require("devtools/shared/specs/screenshot");

exports.ScreenshotActor = protocol.ActorClassWithSpec(screenshotSpec, {
  initialize: function(conn, targetActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.document = targetActor.window.document;
  },

  capture: function(args) {
    return captureScreenshot(args, this.document);
  }
});
