/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, RetVal, generateActorSpec } = require("devtools/shared/protocol");

const performanceSpec = generateActorSpec({
  typeName: "performance",

  /**
   * The set of events the PerformanceActor emits over RDP.
   */
  events: {
    "recording-started": {
      recording: Arg(0, "performance-recording"),
    },
    "recording-stopping": {
      recording: Arg(0, "performance-recording"),
    },
    "recording-stopped": {
      recording: Arg(0, "performance-recording"),
      data: Arg(1, "json"),
    },
    "profiler-status": {
      data: Arg(0, "json"),
    },
    "console-profile-start": {},
    "timeline-data": {
      name: Arg(0, "string"),
      data: Arg(1, "json"),
      recordings: Arg(2, "array:performance-recording"),
    },
  },

  methods: {
    connect: {
      request: { options: Arg(0, "nullable:json") },
      response: RetVal("json")
    },

    canCurrentlyRecord: {
      request: {},
      response: { value: RetVal("json") }
    },

    startRecording: {
      request: {
        options: Arg(0, "nullable:json"),
      },
      response: {
        recording: RetVal("nullable:performance-recording")
      }
    },

    stopRecording: {
      request: {
        options: Arg(0, "performance-recording"),
      },
      response: {
        recording: RetVal("performance-recording")
      }
    },

    isRecording: {
      request: {},
      response: { isRecording: RetVal("boolean") }
    },

    getRecordings: {
      request: {},
      response: { recordings: RetVal("array:performance-recording") }
    },

    getConfiguration: {
      request: {},
      response: { config: RetVal("json") }
    },

    setProfilerStatusInterval: {
      request: { interval: Arg(0, "number") },
      oneway: true
    },
  }
});

exports.performanceSpec = performanceSpec;
