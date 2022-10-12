/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Arg,
  generateActorSpec,
  RetVal,
} = require("resource://devtools/shared/protocol.js");

const changesSpec = generateActorSpec({
  typeName: "changes",

  events: {
    "add-change": {
      type: "addChange",
      change: Arg(0, "json"),
    },
    "remove-change": {
      type: "removeChange",
      change: Arg(0, "json"),
    },
    "clear-changes": {
      type: "clearChanges",
    },
  },

  methods: {
    allChanges: {
      response: {
        changes: RetVal("array:json"),
      },
    },
    start: {}, // no arguments, no response
  },
});

exports.changesSpec = changesSpec;
