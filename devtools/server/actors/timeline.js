/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Many Gecko operations (painting, reflows, restyle, ...) can be tracked
 * in real time. A marker is a representation of one operation. A marker
 * has a name, start and end timestamps. Markers are stored in docShells.
 *
 * This actor exposes this tracking mechanism to the devtools protocol.
 * Most of the logic is handled in devtools/server/performance/timeline.js
 * This just wraps that module up and exposes it via RDP.
 *
 * For more documentation:
 * @see devtools/server/performance/timeline.js
 */

const protocol = require("devtools/shared/protocol");
const { Option, RetVal } = protocol;
const { actorBridgeWithSpec } = require("devtools/server/actors/common");
const { Timeline } = require("devtools/server/performance/timeline");
const { timelineSpec } = require("devtools/shared/specs/timeline");
const events = require("sdk/event/core");

/**
 * The timeline actor pops and forwards timeline markers registered in docshells.
 */
var TimelineActor = exports.TimelineActor = protocol.ActorClassWithSpec(timelineSpec, {
  /**
   * Initializes this actor with the provided connection and tab actor.
   */
  initialize: function (conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;
    this.bridge = new Timeline(tabActor);

    this._onTimelineEvent = this._onTimelineEvent.bind(this);
    events.on(this.bridge, "*", this._onTimelineEvent);
  },

  /**
   * Destroys this actor, stopping recording first.
   */
  destroy: function () {
    events.off(this.bridge, "*", this._onTimelineEvent);
    this.bridge.destroy();
    this.bridge = null;
    this.tabActor = null;
    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Propagate events from the Timeline module over RDP if the event is defined
   * here.
   */
  _onTimelineEvent: function (eventName, ...args) {
    events.emit(this, eventName, ...args);
  },

  isRecording: actorBridgeWithSpec("isRecording", {
    request: {},
    response: {
      value: RetVal("boolean")
    }
  }),

  start: actorBridgeWithSpec("start", {
    request: {
      withMarkers: Option(0, "boolean"),
      withTicks: Option(0, "boolean"),
      withMemory: Option(0, "boolean"),
      withFrames: Option(0, "boolean"),
      withGCEvents: Option(0, "boolean"),
      withDocLoadingEvents: Option(0, "boolean")
    },
    response: {
      value: RetVal("number")
    }
  }),

  stop: actorBridgeWithSpec("stop", {
    response: {
      // Set as possibly nullable due to the end time possibly being
      // undefined during destruction
      value: RetVal("nullable:number")
    }
  }),
});
