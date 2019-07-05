/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, RetVal, generateActorSpec } = require("devtools/shared/protocol");

const workerTargetSpec = generateActorSpec({
  typeName: "workerTarget",

  methods: {
    attach: {
      request: {},
      response: RetVal("json"),
    },
    detach: {
      request: {},
      response: RetVal("json"),
    },
    connect: {
      request: {
        options: Arg(0, "json"),
      },
      response: RetVal("json"),
    },
    push: {
      request: {},
      response: RetVal("json"),
    },
  },

  events: {
    // WorkerTargetActor still uses old sendActorEvent function,
    // but it should use emit instead.
    // Do not emit a `close` event as Target class emit this event on destroy
    "worker-close": {
      type: "close",
    },

    // The thread actor is no longer emitting newSource event in the name of the target
    // actor (bug 1269919), but as we may still connect to older servers which still do,
    // we have to keep it being mentioned here. Otherwise the event is considered as a
    // response to a request and confuses the packet ordering.
    // We can remove that once FF66 is no longer supported.
    newSource: {
      type: "newSource",
    },
  },
});

exports.workerTargetSpec = workerTargetSpec;
