/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { actorBridgeWithSpec } = require("devtools/server/actors/common");
const { Framerate } = require("devtools/server/performance/framerate");
const { framerateSpec } = require("devtools/shared/specs/framerate");

/**
 * An actor wrapper around Framerate. Uses exposed
 * methods via bridge and provides RDP definitions.
 *
 * @see devtools/server/performance/framerate.js for documentation.
 */
exports.FramerateActor = ActorClassWithSpec(framerateSpec, {
  initialize: function(conn, targetActor) {
    Actor.prototype.initialize.call(this, conn);
    this.bridge = new Framerate(targetActor);
  },
  destroy: function(conn) {
    Actor.prototype.destroy.call(this, conn);
    this.bridge.destroy();
  },

  startRecording: actorBridgeWithSpec("startRecording"),
  stopRecording: actorBridgeWithSpec("stopRecording"),
  cancelRecording: actorBridgeWithSpec("cancelRecording"),
  isRecording: actorBridgeWithSpec("isRecording"),
  getPendingTicks: actorBridgeWithSpec("getPendingTicks"),
});
