/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, RetVal, generateActorSpec } = require("devtools/shared/protocol");

const responsiveSpec = generateActorSpec({
  typeName: "responsive",

  methods: {
    setNetworkThrottling: {
      request: {
        options: Arg(0, "json"),
      },
      response: {
        valueChanged: RetVal("boolean"),
      },
    },

    getNetworkThrottling: {
      request: {},
      response: {
        state: RetVal("json"),
      },
    },

    clearNetworkThrottling: {
      request: {},
      response: {
        valueChanged: RetVal("boolean"),
      },
    },

    toggleTouchSimulator: {
      request: {
        options: Arg(0, "json"),
      },
      response: {
        valueChanged: RetVal("boolean"),
      },
    },

    setElementPickerState: {
      request: {
        state: Arg(0, "boolean"),
        pickerType: Arg(1, "string"),
      },
      response: {},
    },

    dispatchOrientationChangeEvent: {
      request: {},
      response: {},
    },
  },
});

exports.responsiveSpec = responsiveSpec;
