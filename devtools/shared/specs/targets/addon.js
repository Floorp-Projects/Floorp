/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Option, RetVal, generateActorSpec} = require("devtools/shared/protocol");

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
    // newSource is being sent by ThreadActor in the name of its parent,
    // i.e. AddonTargetActor
    newSource: {
      type: "newSource",
      source: Option(0, "json"),
    },
  },
});

exports.addonTargetSpec = addonTargetSpec;
