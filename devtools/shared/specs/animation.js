/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, RetVal, generateActorSpec } = require("devtools/shared/protocol");

const animationPlayerSpec = generateActorSpec({
  typeName: "animationplayer",

  events: {
    "changed": {
      type: "changed",
      state: Arg(0, "json")
    }
  },

  methods: {
    release: { release: true },
    getCurrentState: {
      request: {},
      response: {
        data: RetVal("json")
      }
    },
    pause: {
      request: {},
      response: {}
    },
    play: {
      request: {},
      response: {}
    },
    ready: {
      request: {},
      response: {}
    },
    setCurrentTime: {
      request: {
        currentTime: Arg(0, "number")
      },
      response: {}
    },
    setPlaybackRate: {
      request: {
        currentTime: Arg(0, "number")
      },
      response: {}
    },
    getFrames: {
      request: {},
      response: {
        frames: RetVal("json")
      }
    },
    getProperties: {
      request: {},
      response: {
        properties: RetVal("array:json")
      }
    }
  }
});

exports.animationPlayerSpec = animationPlayerSpec;
