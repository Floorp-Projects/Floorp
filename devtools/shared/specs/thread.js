/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  Option,
  RetVal,
  generateActorSpec,
  types,
} = require("devtools/shared/protocol");

types.addDictType("available-breakpoint-group", {
  name: "string",
  events: "array:available-breakpoint-event",
});

types.addDictType("available-breakpoint-event", {
  id: "string",
  name: "string",
});

types.addDictType("thread.frames", {
  frames: "array:frame",
});

types.addDictType("paused-reason", {
  type: "string",

  // Used for any pause type that wants to describe why the pause happened.
  message: "nullable:string",

  // Used for the stepping pause types.
  frameFinished: "nullable:json",

  // Used for the "exception" pause type.
  exception: "nullable:json",

  // Used for the "interrupted" pause type.
  onNext: "nullable:boolean",

  // Used for the "eventBreakpoint" pause type.
  breakpoint: "nullable:json",

  // Used for the "mutationBreakpoint" pause type.
  mutationType: "nullable:string",
});

const threadSpec = generateActorSpec({
  typeName: "thread",

  events: {
    paused: {
      actor: Option(0, "nullable:string"),
      frame: Option(0, "frame"),
      why: Option(0, "paused-reason"),
      poppedFrames: Option(0, "nullable:json"),
      error: Option(0, "nullable:json"),
    },
    resumed: {},
    detached: {},
    willInterrupt: {},
    newSource: {
      source: Option(0, "json"),
    },
  },

  methods: {
    attach: {
      request: {
        options: Arg(0, "json"),
      },
      response: {},
    },
    detach: {
      request: {},
      response: {},
    },
    reconfigure: {
      request: {
        options: Arg(0, "json"),
      },
      response: {},
    },
    resume: {
      request: {
        resumeLimit: Arg(0, "nullable:json"),
      },
      response: RetVal("nullable:json"),
    },
    frames: {
      request: {
        start: Arg(0, "number"),
        count: Arg(1, "number"),
      },
      response: RetVal("thread.frames"),
    },
    interrupt: {
      request: {
        when: Arg(0, "json"),
      },
    },
    sources: {
      response: RetVal("array:json"),
    },
    skipBreakpoints: {
      request: {
        skip: Arg(0, "json"),
      },
      response: {
        skip: Arg(0, "json"),
      },
    },
    dumpThread: {
      request: {},
      response: RetVal("json"),
    },
    setBreakpoint: {
      request: {
        location: Arg(0, "json"),
        options: Arg(1, "json"),
      },
    },
    removeBreakpoint: {
      request: {
        location: Arg(0, "json"),
      },
    },
    setXHRBreakpoint: {
      request: {
        path: Arg(0, "string"),
        method: Arg(1, "string"),
      },
      response: {
        value: RetVal("boolean"),
      },
    },
    removeXHRBreakpoint: {
      request: {
        path: Arg(0, "string"),
        method: Arg(1, "string"),
      },
      response: {
        value: RetVal("boolean"),
      },
    },
    getAvailableEventBreakpoints: {
      response: {
        value: RetVal("array:available-breakpoint-group"),
      },
    },
    getActiveEventBreakpoints: {
      response: {
        ids: RetVal("array:string"),
      },
    },
    setActiveEventBreakpoints: {
      request: {
        ids: Arg(0, "array:string"),
      },
    },
    pauseOnExceptions: {
      request: {
        pauseOnExceptions: Arg(0, "string"),
        ignoreCaughtExceptions: Arg(1, "string"),
      },
    },

    toggleEventLogging: {
      request: {
        logEventBreakpoints: Arg(0, "string"),
      },
    },
  },
});

exports.threadSpec = threadSpec;
