/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, RetVal, generateActorSpec } = require("devtools/shared/protocol");

const emulationSpec = generateActorSpec({
  typeName: "emulation",

  methods: {
    setTouchEventsOverride: {
      request: {
        flag: Arg(0, "number")
      },
      response: {
        valueChanged: RetVal("boolean")
      }
    },

    getTouchEventsOverride: {
      request: {},
      response: {
        flag: RetVal("number")
      }
    },

    clearTouchEventsOverride: {
      request: {},
      response: {
        valueChanged: RetVal("boolean")
      }
    },

    setUserAgentOverride: {
      request: {
        flag: Arg(0, "string")
      },
      response: {
        valueChanged: RetVal("boolean")
      }
    },

    getUserAgentOverride: {
      request: {},
      response: {
        userAgent: RetVal("string")
      }
    },

    clearUserAgentOverride: {
      request: {},
      response: {
        valueChanged: RetVal("boolean")
      }
    },
  }
});

exports.emulationSpec = emulationSpec;
