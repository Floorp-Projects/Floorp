/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const protocol = require("devtools/shared/protocol");
const { actorBridge } = require("devtools/server/actors/common");
const { method, custom, Arg, Option, RetVal } = protocol;
const { on, once, off, emit } = require("sdk/event/core");
const { Framerate } = require("devtools/server/performance/framerate");

/**
 * An actor wrapper around Framerate. Uses exposed
 * methods via bridge and provides RDP definitions.
 *
 * @see devtools/server/performance/framerate.js for documentation.
 */
var FramerateActor = exports.FramerateActor = protocol.ActorClass({
  typeName: "framerate",
  initialize: function (conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.bridge = new Framerate(tabActor);
  },
  destroy: function (conn) {
    protocol.Actor.prototype.destroy.call(this, conn);
    this.bridge.destroy();
  },

  startRecording: actorBridge("startRecording", {}),

  stopRecording: actorBridge("stopRecording", {
    request: {
      beginAt: Arg(0, "nullable:number"),
      endAt: Arg(1, "nullable:number")
    },
    response: { ticks: RetVal("array:number") }
  }),

  cancelRecording: actorBridge("cancelRecording"),

  isRecording: actorBridge("isRecording", {
    response: { recording: RetVal("boolean") }
  }),

  getPendingTicks: actorBridge("getPendingTicks", {
    request: {
      beginAt: Arg(0, "nullable:number"),
      endAt: Arg(1, "nullable:number")
    },
    response: { ticks: RetVal("array:number") }
  }),
});

/**
 * The corresponding Front object for the FramerateActor.
 */
var FramerateFront = exports.FramerateFront = protocol.FrontClass(FramerateActor, {
  initialize: function (client, { framerateActor }) {
    protocol.Front.prototype.initialize.call(this, client, { actor: framerateActor });
    this.manage(this);
  }
});
