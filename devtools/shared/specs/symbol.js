/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { generateActorSpec } = require("resource://devtools/shared/protocol.js");

const symbolSpec = generateActorSpec({
  typeName: "symbol",

  methods: {
    release: {
      request: {},
      response: {},
    },
  },
});

exports.symbolSpec = symbolSpec;
