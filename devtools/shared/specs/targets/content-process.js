/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {types, Option, RetVal, generateActorSpec} = require("devtools/shared/protocol");

types.addDictType("contentProcessTarget.workers", {
  error: "nullable:string",
  workers: "nullable:array:workerTarget",
});

const contentProcessTargetSpec = generateActorSpec({
  typeName: "contentProcessTarget",

  methods: {
    listWorkers: {
      request: {},
      response: RetVal("contentProcessTarget.workers"),
    },
  },

  events: {
    // newSource is being sent by ThreadActor in the name of its parent,
    // i.e. ContentProcessTargetActor
    newSource: {
      type: "newSource",
      source: Option(0, "json"),
    },
    workerListChanged: {
      type: "workerListChanged",
    },
  },
});

exports.contentProcessTargetSpec = contentProcessTargetSpec;
