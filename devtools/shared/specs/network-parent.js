/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  generateActorSpec,
  Arg,
  RetVal,
} = require("resource://devtools/shared/protocol.js");

const networkParentSpec = generateActorSpec({
  typeName: "networkParent",

  methods: {
    setPersist: {
      request: {
        options: Arg(0, "boolean"),
      },
      response: {},
    },
    setNetworkThrottling: {
      request: {
        options: Arg(0, "json"),
      },
      response: {},
    },
    getNetworkThrottling: {
      request: {},
      response: {
        state: RetVal("json"),
      },
    },
    clearNetworkThrottling: {
      request: {},
      response: {},
    },
    setSaveRequestAndResponseBodies: {
      request: {
        save: Arg(0, "boolean"),
      },
      response: {},
    },
    setBlockedUrls: {
      request: {
        urls: Arg(0, "array:string"),
      },
    },
    getBlockedUrls: {
      request: {},
      response: {
        urls: RetVal("array:string"),
      },
    },
    blockRequest: {
      request: {
        filters: Arg(0, "json"),
      },
      response: {},
    },
    unblockRequest: {
      request: {
        filters: Arg(0, "json"),
      },
      response: {},
    },
    override: {
      request: {
        url: Arg(0, "string"),
        path: Arg(1, "string"),
      },
    },
    removeOverride: {
      request: {
        url: Arg(0, "string"),
      },
    },
  },
});

exports.networkParentSpec = networkParentSpec;
