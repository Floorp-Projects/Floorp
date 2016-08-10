/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, generateActorSpec } = require("devtools/shared/protocol");

const performanceEntriesSpec = generateActorSpec({
  typeName: "performanceEntries",

  events: {
    "entry": {
      type: "entry",
      // object containing performance entry name, type, origin, and epoch.
      detail: Arg(0, "json")
    }
  },

  methods: {
    start: {},
    stop: {}
  }
});

exports.performanceEntriesSpec = performanceEntriesSpec;
