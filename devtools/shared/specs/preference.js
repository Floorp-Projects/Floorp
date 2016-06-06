/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Arg, RetVal, generateActorSpec} = require("devtools/shared/protocol");

const preferenceSpec = generateActorSpec({
  typeName: "preference",

  methods: {
    getBoolPref: {
      request: { value: Arg(0) },
      response: { value: RetVal("boolean") }
    },
    getCharPref: {
      request: { value: Arg(0) },
      response: { value: RetVal("string") }
    },
    getIntPref: {
      request: { value: Arg(0) },
      response: { value: RetVal("number") }
    },
    getAllPrefs: {
      request: {},
      response: { value: RetVal("json") }
    },
    setBoolPref: {
      request: { name: Arg(0), value: Arg(1) },
      response: {}
    },
    setCharPref: {
      request: { name: Arg(0), value: Arg(1) },
      response: {}
    },
    setIntPref: {
      request: { name: Arg(0), value: Arg(1) },
      response: {}
    },
    clearUserPref: {
      request: { name: Arg(0) },
      response: {}
    }
  },
});

exports.preferenceSpec = preferenceSpec;
