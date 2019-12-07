/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  types,
  Option,
  RetVal,
  generateActorSpec,
} = require("devtools/shared/protocol");

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

    // The thread actor is no longer emitting newSource event in the name of the target
    // actor (bug 1269919), but as we may still connect to older servers which still do,
    // we have to keep it being mentioned here. Otherwise the event is considered as a
    // response to a request and confuses the packet ordering.
    // We can remove that once FF66 is no longer supported.
    newSource: {
      type: "newSource",
    },
    tabDetached: {
      type: "tabDetached",
      from: Option(0, "string"),
    },
  },
});

exports.contentProcessTargetSpec = contentProcessTargetSpec;
