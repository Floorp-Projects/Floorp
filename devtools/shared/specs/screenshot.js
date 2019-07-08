/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  RetVal,
  Arg,
  generateActorSpec,
  types,
} = require("devtools/shared/protocol");

types.addDictType("screenshot.args", {
  fullpage: "nullable:boolean",
  file: "nullable:boolean",
  clipboard: "nullable:boolean",
  selector: "nullable:string",
  dpr: "nullable:string",
  delay: "nullable:string",
});

const screenshotSpec = generateActorSpec({
  typeName: "screenshot",

  methods: {
    capture: {
      request: {
        args: Arg(0, "screenshot.args"),
      },
      response: {
        value: RetVal("json"),
      },
    },
  },
});

exports.screenshotSpec = screenshotSpec;
