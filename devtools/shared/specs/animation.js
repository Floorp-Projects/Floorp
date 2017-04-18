/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  RetVal,
  generateActorSpec,
  types
} = require("devtools/shared/protocol");
require("devtools/shared/specs/inspector");

/**
 * Sent with the 'mutations' event as part of an array of changes, used to
 * inform fronts of the type of change that occured.
 */
types.addDictType("animationMutationChange", {
  // The type of change ("added" or "removed").
  type: "string",
  // The changed AnimationPlayerActor.
  player: "animationplayer"
});

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
    },
    getAnimationTypes: {
      request: {
        propertyNames: Arg(0, "array:string")
      },
      response: {
        animationTypes: RetVal("json")
      }
    }
  }
});

exports.animationPlayerSpec = animationPlayerSpec;

const animationsSpec = generateActorSpec({
  typeName: "animations",

  events: {
    "mutations": {
      type: "mutations",
      changes: Arg(0, "array:animationMutationChange")
    }
  },

  methods: {
    setWalkerActor: {
      request: {
        walker: Arg(0, "domwalker")
      },
      response: {}
    },
    getAnimationPlayersForNode: {
      request: {
        actorID: Arg(0, "domnode")
      },
      response: {
        players: RetVal("array:animationplayer")
      }
    },
    stopAnimationPlayerUpdates: {
      request: {},
      response: {}
    },
    pauseAll: {
      request: {},
      response: {}
    },
    playAll: {
      request: {},
      response: {}
    },
    toggleAll: {
      request: {},
      response: {}
    },
    toggleSeveral: {
      request: {
        players: Arg(0, "array:animationplayer"),
        shouldPause: Arg(1, "boolean")
      },
      response: {}
    },
    setCurrentTimes: {
      request: {
        players: Arg(0, "array:animationplayer"),
        time: Arg(1, "number"),
        shouldPause: Arg(2, "boolean")
      },
      response: {}
    },
    setPlaybackRates: {
      request: {
        players: Arg(0, "array:animationplayer"),
        rate: Arg(1, "number")
      },
      response: {}
    }
  }
});

exports.animationsSpec = animationsSpec;
