/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {types, RetVal, generateActorSpec} = require("devtools/shared/protocol");

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
    workerListChanged: {
      type: "workerListChanged",
    },
  },
});

exports.contentProcessTargetSpec = contentProcessTargetSpec;
