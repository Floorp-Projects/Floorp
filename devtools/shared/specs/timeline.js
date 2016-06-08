/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  RetVal,
  Option,
  generateActorSpec,
  types
} = require("devtools/shared/protocol");

/**
 * Type representing an array of numbers as strings, serialized fast(er).
 * http://jsperf.com/json-stringify-parse-vs-array-join-split/3
 *
 * XXX: It would be nice if on local connections (only), we could just *give*
 * the array directly to the front, instead of going through all this
 * serialization redundancy.
 */
types.addType("array-of-numbers-as-strings", {
  write: (v) => v.join(","),
  // In Gecko <= 37, `v` is an array; do not transform in this case.
  read: (v) => typeof v === "string" ? v.split(",") : v
});

const timelineSpec = generateActorSpec({
  typeName: "timeline",

  events: {
    /**
     * Events emitted when "DOMContentLoaded" and "Load" markers are received.
     */
    "doc-loading": {
      type: "doc-loading",
      marker: Arg(0, "json"),
      endTime: Arg(0, "number")
    },

    /**
     * The "markers" events emitted every DEFAULT_TIMELINE_DATA_PULL_TIMEOUT ms
     * at most, when profile markers are found. The timestamps on each marker
     * are relative to when recording was started.
     */
    "markers": {
      type: "markers",
      markers: Arg(0, "json"),
      endTime: Arg(1, "number")
    },

    /**
     * The "memory" events emitted in tandem with "markers", if this was enabled
     * when the recording started. The `delta` timestamp on this measurement is
     * relative to when recording was started.
     */
    "memory": {
      type: "memory",
      delta: Arg(0, "number"),
      measurement: Arg(1, "json")
    },

    /**
     * The "ticks" events (from the refresh driver) emitted in tandem with
     * "markers", if this was enabled when the recording started. All ticks
     * are timestamps with a zero epoch.
     */
    "ticks": {
      type: "ticks",
      delta: Arg(0, "number"),
      timestamps: Arg(1, "array-of-numbers-as-strings")
    },

    /**
     * The "frames" events emitted in tandem with "markers", containing
     * JS stack frames. The `delta` timestamp on this frames packet is
     * relative to when recording was started.
     */
    "frames": {
      type: "frames",
      delta: Arg(0, "number"),
      frames: Arg(1, "json")
    }
  },

  methods: {
    isRecording: {
      request: {},
      response: {
        value: RetVal("boolean")
      }
    },

    start: {
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
    },

    stop: {
      response: {
        // Set as possibly nullable due to the end time possibly being
        // undefined during destruction
        value: RetVal("nullable:number")
      }
    },
  },
});

exports.timelineSpec = timelineSpec;
