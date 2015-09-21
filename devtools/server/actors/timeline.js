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
 * Most of the logic is handled in toolkit/devtools/shared/timeline.js
 * This just wraps that module up and exposes it via RDP.
 *
 * For more documentation:
 * @see toolkit/devtools/shared/timeline.js
 */

const protocol = require("devtools/server/protocol");
const { method, Arg, RetVal, Option } = protocol;
const events = require("sdk/event/core");
const { Timeline } = require("devtools/toolkit/shared/timeline");
const { actorBridge } = require("devtools/server/actors/common");

/**
 * Type representing an array of numbers as strings, serialized fast(er).
 * http://jsperf.com/json-stringify-parse-vs-array-join-split/3
 *
 * XXX: It would be nice if on local connections (only), we could just *give*
 * the array directly to the front, instead of going through all this
 * serialization redundancy.
 */
protocol.types.addType("array-of-numbers-as-strings", {
  write: (v) => v.join(","),
  // In Gecko <= 37, `v` is an array; do not transform in this case.
  read: (v) => typeof v === "string" ? v.split(",") : v
});

/**
 * The timeline actor pops and forwards timeline markers registered in docshells.
 */
var TimelineActor = exports.TimelineActor = protocol.ActorClass({
  typeName: "timeline",

  events: {
    /**
     * The "markers" events emitted every DEFAULT_TIMELINE_DATA_PULL_TIMEOUT ms
     * at most, when profile markers are found. The timestamps on each marker
     * are relative to when recording was started.
     */
    "markers" : {
      type: "markers",
      markers: Arg(0, "json"),
      endTime: Arg(1, "number")
    },

    /**
     * The "memory" events emitted in tandem with "markers", if this was enabled
     * when the recording started. The `delta` timestamp on this measurement is
     * relative to when recording was started.
     */
    "memory" : {
      type: "memory",
      delta: Arg(0, "number"),
      measurement: Arg(1, "json")
    },

    /**
     * The "ticks" events (from the refresh driver) emitted in tandem with
     * "markers", if this was enabled when the recording started. All ticks
     * are timestamps with a zero epoch.
     */
    "ticks" : {
      type: "ticks",
      delta: Arg(0, "number"),
      timestamps: Arg(1, "array-of-numbers-as-strings")
    },

    /**
     * The "frames" events emitted in tandem with "markers", containing
     * JS stack frames. The `delta` timestamp on this frames packet is
     * relative to when recording was started.
     */
    "frames" : {
      type: "frames",
      delta: Arg(0, "number"),
      frames: Arg(1, "json")
    }
  },

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
   * The timeline actor is the first (and last) in its hierarchy to use protocol.js
   * so it doesn't have a parent protocol actor that takes care of its lifetime.
   * So it needs a disconnect method to cleanup.
   */
  disconnect: function() {
    this.destroy();
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
   * Propagate events from the Timeline module over
   * RDP if the event is defined here.
   */
  _onTimelineEvent: function (eventName, ...args) {
    if (this.events[eventName]) {
      events.emit(this, eventName, ...args);
    }
  },

  isRecording: actorBridge("isRecording", {
    request: {},
    response: {
      value: RetVal("boolean")
    }
  }),

  start: actorBridge("start", {
    request: {
      withMemory: Option(0, "boolean"),
      withTicks: Option(0, "boolean")
    },
    response: {
      value: RetVal("number")
    }
  }),

  stop: actorBridge("stop", {
    response: {
      // Set as possibly nullable due to the end time possibly being
      // undefined during destruction
      value: RetVal("nullable:number")
    }
  }),
});

exports.TimelineFront = protocol.FrontClass(TimelineActor, {
  initialize: function(client, {timelineActor}) {
    protocol.Front.prototype.initialize.call(this, client, {actor: timelineActor});
    this.manage(this);
  },
  destroy: function() {
    protocol.Front.prototype.destroy.call(this);
  },
});
