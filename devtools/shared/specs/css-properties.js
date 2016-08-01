/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, RetVal, generateActorSpec } = require("devtools/shared/protocol");

const cssPropertiesSpec = generateActorSpec({
  typeName: "cssProperties",

  methods: {
    getCSSDatabase: {
      request: {
        clientBrowserVersion: Arg(0, "string"),
      },

      response: RetVal("json"),
    }
  }
});

exports.cssPropertiesSpec = cssPropertiesSpec;
