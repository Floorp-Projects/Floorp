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

    setMetaViewportOverride: {
      request: {
        flag: Arg(0, "number"),
      },
      response: {
        valueChanged: RetVal("boolean"),
      },
    },

    getMetaViewportOverride: {
      request: {},
      response: {
        flag: RetVal("number"),
      },
    },

    clearMetaViewportOverride: {
      request: {},
      response: {
        valueChanged: RetVal("boolean"),
      },
    },

    setUserAgentOverride: {
      request: {
        flag: Arg(0, "string"),
      },
      response: {
        valueChanged: RetVal("boolean"),
      },
    },

    getUserAgentOverride: {
      request: {},
      response: {
        userAgent: RetVal("string"),
      },
    },

    clearUserAgentOverride: {
      request: {},
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

    setFloatingScrollbars: {
      request: {
        state: Arg(0, "boolean"),
      },
      response: {},
    },

    setMaxTouchPoints: {
      request: {
        flag: Arg(0, "boolean"),
      },
      response: {},
    },
  },
});

exports.responsiveSpec = responsiveSpec;
