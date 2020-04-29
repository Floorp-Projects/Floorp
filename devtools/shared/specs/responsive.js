/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, RetVal, generateActorSpec } = require("devtools/shared/protocol");

const responsiveSpec = generateActorSpec({
  typeName: "responsive",

  methods: {
    setDPPXOverride: {
      request: {
        dppx: Arg(0, "number"),
      },
      response: {
        valueChanged: RetVal("boolean"),
      },
    },

    getDPPXOverride: {
      request: {},
      response: {
        dppx: RetVal("number"),
      },
    },

    clearDPPXOverride: {
      request: {},
      response: {
        valueChanged: RetVal("boolean"),
      },
    },

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

    setTouchEventsOverride: {
      request: {
        flag: Arg(0, "number"),
      },
      response: {
        valueChanged: RetVal("boolean"),
      },
    },

    getTouchEventsOverride: {
      request: {},
      response: {
        flag: RetVal("number"),
      },
    },

    clearTouchEventsOverride: {
      request: {},
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

    simulateScreenOrientationChange: {
      request: {
        orientation: Arg(0, "string"),
        angle: Arg(1, "number"),
        deviceChange: Arg(2, "boolean"),
      },
      response: {},
    },

    captureScreenshot: {
      request: {},
      response: {
        value: RetVal("json"),
      },
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
