/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  generateActorSpec,
  Arg,
  types,
} = require("resource://devtools/shared/protocol.js");

types.addDictType("blackboxing.position", {
  line: "number",
  column: "number",
});

types.addDictType("blackboxing.range", {
  start: "blackboxing.position",
  end: "blackboxing.position",
});

const blackboxingSpec = generateActorSpec({
  typeName: "blackboxing",

  methods: {
    blackbox: {
      request: {
        url: Arg(0, "string"),
        range: Arg(1, "array:blackboxing.range"),
      },
    },
    unblackbox: {
      request: {
        url: Arg(0, "string"),
        range: Arg(1, "array:blackboxing.range"),
      },
    },
  },
});

exports.blackboxingSpec = blackboxingSpec;
