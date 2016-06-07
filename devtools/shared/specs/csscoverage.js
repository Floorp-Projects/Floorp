/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Arg, RetVal, generateActorSpec} = require("devtools/shared/protocol");

const cssUsageSpec = generateActorSpec({
  typeName: "cssUsage",

  events: {
    "state-change": {
      type: "stateChange",
      stateChange: Arg(0, "json")
    }
  },

  methods: {
    start: {
      request: { url: Arg(0, "boolean") }
    },
    stop: {},
    toggle: {},
    oneshot: {},
    createEditorReport: {
      request: { url: Arg(0, "string") },
      response: { reports: RetVal("array:json") }
    },
    createPageReport: {
      response: RetVal("json")
    },
    _testOnlyVisitedPages: {
      response: { value: RetVal("array:string") }
    },
  },
});

exports.cssUsageSpec = cssUsageSpec;
