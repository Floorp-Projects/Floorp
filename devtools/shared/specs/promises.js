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

// Teach protocol.js how to deal with legacy actor types
types.addType("ObjectActor", {
  write: actor => actor.grip(),
  read: grip => grip
});

const promisesSpec = generateActorSpec({
  typeName: "promises",

  events: {
    // Event emitted for new promises allocated in debuggee and bufferred by
    // sending the list of promise objects in a batch.
    "new-promises": {
      type: "new-promises",
      data: Arg(0, "array:ObjectActor"),
    },
    // Event emitted for promise settlements.
    "promises-settled": {
      type: "promises-settled",
      data: Arg(0, "array:ObjectActor")
    }
  },

  methods: {
    attach: {
      request: {},
      response: {},
    },

    detach: {
      request: {},
      response: {},
    },

    listPromises: {
      request: {},
      response: {
        promises: RetVal("array:ObjectActor"),
      },
    }
  }
});

exports.promisesSpec = promisesSpec;
