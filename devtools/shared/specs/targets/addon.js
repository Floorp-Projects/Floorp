/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {RetVal, generateActorSpec} = require("devtools/shared/protocol");

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
});

exports.addonTargetSpec = addonTargetSpec;
