/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const { captureScreenshot } = require("devtools/shared/screenshot/capture");
const { screenshotSpec } = require("devtools/shared/specs/screenshot");

exports.ScreenshotActor = protocol.ActorClassWithSpec(screenshotSpec, {
  initialize: function(conn, targetActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.targetActor = targetActor;
    this.document = targetActor.window.document;
    this._onNavigate = this._onNavigate.bind(this);

    this.targetActor.on("navigate", this._onNavigate);
  },

  destroy() {
    this.targetActor.off("navigate", this._onNavigate);
    protocol.Actor.prototype.destroy.call(this);
  },

  _onNavigate() {
    this.document = this.targetActor.window.document;
  },

  capture: function(args) {
    if (args.nodeActorID) {
      const nodeActor = this.conn.getActor(args.nodeActorID);
      if (!nodeActor) {
        throw new Error(
          `Screenshot actor failed to find Node actor for '${args.nodeActorID}'`
        );
      }
      args.rawNode = nodeActor.rawNode;
    }
    return captureScreenshot(args, this.document);
  },
});
