/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Arg, RetVal, generateActorSpec} = require("devtools/shared/protocol");

const threadSpec = generateActorSpec({
  typeName: "context",

  methods: {
    setXHRBreakpoint: {
      request: {
        path: Arg(0, "string"),
        method: Arg(1, "string")
      },
      response: {
        value: RetVal("boolean")
      }
    },
    removeXHRBreakpoint: {
      request: {
        path: Arg(0, "string"),
        method: Arg(1, "string")
      },
      response: {
        value: RetVal("boolean")
      }
    }
  },
});

exports.threadSpec = threadSpec;
