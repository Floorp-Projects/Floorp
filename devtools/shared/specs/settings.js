/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Arg, RetVal, generateActorSpec} = require("devtools/shared/protocol");

const settingsSpec = generateActorSpec({
  typeName: "settings",

  methods: {
    getSetting: {
      request: { value: Arg(0) },
      response: { value: RetVal("json") }
    },
    setSetting: {
      request: { name: Arg(0), value: Arg(1) },
      response: {}
    },
    getAllSettings: {
      request: {},
      response: { value: RetVal("json") }
    },
    clearUserSetting: {
      request: { name: Arg(0) },
      response: {}
    }
  },
});

exports.settingsSpec = settingsSpec;
