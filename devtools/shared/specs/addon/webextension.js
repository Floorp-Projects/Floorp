/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {RetVal, generateActorSpec} = require("devtools/shared/protocol");

const webExtensionSpec = generateActorSpec({
  typeName: "webExtension",

  methods: {
    reload: {
      request: { },
      response: { addon: RetVal("json") },
    },

    connect: {
      request: { },
      response: { form: RetVal("json") },
    },
  },
});

exports.webExtensionSpec = webExtensionSpec;
