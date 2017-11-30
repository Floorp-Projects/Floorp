/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { RetVal, generateActorSpec } = require("devtools/shared/protocol");

const perfDescription = {
  typeName: "perf",

  events: {
    "profiler-started": {
      type: "profiler-started"
    },
    "profiler-stopped": {
      type: "profiler-stopped"
    },
    "profile-locked-by-private-browsing": {
      type: "profile-locked-by-private-browsing"
    },
    "profile-unlocked-from-private-browsing": {
      type: "profile-unlocked-from-private-browsing"
    }
  },

  methods: {
    startProfiler: {
      request: {},
      response: { value: RetVal("boolean") }
    },

    /**
     * Returns null when unable to return the profile.
     */
    getProfileAndStopProfiler: {
      request: {},
      response: RetVal("nullable:json")
    },

    stopProfilerAndDiscardProfile: {
      request: {},
      response: {}
    },

    isActive: {
      request: {},
      response: { value: RetVal("boolean") }
    },

    isSupportedPlatform: {
      request: {},
      response: { value: RetVal("boolean") }
    },

    isLockedForPrivateBrowsing: {
      request: {},
      response: { value: RetVal("boolean") }
    }
  }
};

exports.perfDescription = perfDescription;

const perfSpec = generateActorSpec(perfDescription);

exports.perfSpec = perfSpec;
