/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const protocol = require("devtools/shared/protocol");
const {Arg, RetVal, generateActorSpec} = protocol;

const arrayBufferSpec = generateActorSpec({
  typeName: "arraybuffer",

  methods: {
    slice: {
      request: {
        start: Arg(0),
        count: Arg(1),
      },
      response: RetVal("json"),
    },
    release: { release: true },
  },
});

exports.arrayBufferSpec = arrayBufferSpec;
