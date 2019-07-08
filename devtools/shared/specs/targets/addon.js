/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { RetVal, generateActorSpec } = require("devtools/shared/protocol");

const addonTargetSpec = generateActorSpec({
  typeName: "addonTarget",

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
      request: {},
      response: RetVal("json"),
    },
    reload: {
      request: {},
      response: {},
    },
    push: {
      request: {},
      response: RetVal("json"),
    },
  },

  events: {
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

exports.addonTargetSpec = addonTargetSpec;
