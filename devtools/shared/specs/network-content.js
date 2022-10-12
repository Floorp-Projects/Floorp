/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  generateActorSpec,
  RetVal,
  Arg,
} = require("resource://devtools/shared/protocol.js");

const networkContentSpec = generateActorSpec({
  typeName: "networkContent",
  methods: {
    sendHTTPRequest: {
      request: {
        request: Arg(0, "json"),
      },
      response: RetVal("number"),
    },
    getStackTrace: {
      request: { resourceId: Arg(0) },
      response: {
        // stacktrace is an "array:string", but not always.
        stacktrace: RetVal("json"),
      },
    },
  },
});

exports.networkContentSpec = networkContentSpec;
