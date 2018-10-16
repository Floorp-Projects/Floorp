/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Arg, RetVal, generateActorSpec} = require("devtools/shared/protocol");

const workerTargetSpec = generateActorSpec({
  typeName: "workerTarget",

  methods: {
    attach: {
      request: {},
      response: RetVal("json")
    },
    detach: {
      request: {},
      response: RetVal("json")
    },
    connect: {
      request: {
        options: Arg(0, "json"),
      },
      response: RetVal("json")
    },
    push: {
      request: {},
      response: RetVal("json")
    },
  },

  events: {
    // WorkerTargetActor still uses old sendActorEvent function,
    // but it should use emit instead.
    close: {
      type: "close",
    },
    // newSource is being sent by ThreadActor in the name of its parent,
    // i.e. WorkerTargetActor
    newSource: {
      type: "newSource",
    }
  }
});

exports.workerTargetSpec = workerTargetSpec;
