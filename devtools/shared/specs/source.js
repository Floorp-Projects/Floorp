/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Arg, RetVal, generateActorSpec} = require("devtools/shared/protocol");

const sourceSpec = generateActorSpec({
  typeName: "source",

  methods: {
    getExecutableLines: { response: { lines: RetVal("json") } },
    onSource: {
      request: { type: "source" },
      response: RetVal("json")
    },
    prettyPrint: {
      request: { indent: Arg(0, "number") },
      response: RetVal("json")
    },
    disablePrettyPrint: {
      response: RetVal("json")
    },
    blackbox: { response: { pausedInSource: RetVal("boolean") } },
    unblackbox: {},
    setBreakpoint: {
      request: {
        location: {
          line: Arg(0, "number"),
          column: Arg(1, "nullable:number")
        },
        condition: Arg(2, "nullable:string"),
        noSliding: Arg(3, "nullable:boolean")
      },
      response: RetVal("json")
    },
  },
});

exports.sourceSpec = sourceSpec;
