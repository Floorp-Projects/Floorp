/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { generateActorSpec, Arg, RetVal } = require("devtools/shared/protocol");

const networkParentSpec = generateActorSpec({
  typeName: "networkParent",

  methods: {
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
  },
});

exports.networkParentSpec = networkParentSpec;
