/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Arg, RetVal, generateActorSpec} = require("devtools/shared/protocol");

const environmentSpec = generateActorSpec({
  typeName: "environment",

  methods: {
    assign: {
      request: {
        name: Arg(1),
        value: Arg(2)
      }
    },
    bindings: {
      request: {},
      response: {
        bindings: RetVal("json")
      }
    },
  },
});

exports.environmentSpec = environmentSpec;
