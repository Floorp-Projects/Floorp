/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { generateActorSpec, RetVal } = require("devtools/shared/protocol");

const frameSpec = generateActorSpec({
  typeName: "frame",

  methods: {
    getEnvironment: {
      response: RetVal("json"),
    },
  },
});

exports.frameSpec = frameSpec;
