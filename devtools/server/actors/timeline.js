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

/**
 * The timeline actor pops and forwards timeline markers registered in docshells.
 */
exports.TimelineActor = protocol.ActorClassWithSpec(timelineSpec, {
  /**
   * Initializes this actor with the provided connection and target actor.
   */
  initialize: function(conn, targetActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.targetActor = targetActor;
    this.bridge = new Timeline(targetActor);

    this._onTimelineDocLoading = this._onTimelineDocLoading.bind(this);
    this._onTimelineMarkers = this._onTimelineMarkers.bind(this);
    this._onTimelineTicks = this._onTimelineTicks.bind(this);
    this._onTimelineMemory = this._onTimelineMemory.bind(this);
    this._onTimelineFrames = this._onTimelineFrames.bind(this);

    this.bridge.on("doc-loading", this._onTimelineDocLoading);
    this.bridge.on("markers", this._onTimelineMarkers);
    this.bridge.on("ticks", this._onTimelineTicks);
    this.bridge.on("memory", this._onTimelineMemory);
    this.bridge.on("frames", this._onTimelineFrames);
  },

  /**
   * Destroys this actor, stopping recording first.
   */
  destroy: function() {
    this.bridge.off("doc-loading", this._onTimelineDocLoading);
    this.bridge.off("markers", this._onTimelineMarkers);
    this.bridge.off("ticks", this._onTimelineTicks);
    this.bridge.off("memory", this._onTimelineMemory);
    this.bridge.off("frames", this._onTimelineFrames);
    this.bridge.destroy();
    this.bridge = null;
    this.targetActor = null;
    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Propagate events from the Timeline module over RDP if the event is defined here.
   */
  _onTimelineDocLoading: function(...args) {
    this.emit("doc-loading", ...args);
  },
  _onTimelineMarkers: function(...args) {
    this.emit("markers", ...args);
  },
  _onTimelineTicks: function(...args) {
    this.emit("ticks", ...args);
  },
  _onTimelineMemory: function(...args) {
    this.emit("memory", ...args);
  },
  _onTimelineFrames: function(...args) {
    this.emit("frames", ...args);
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
