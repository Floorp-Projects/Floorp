/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { generateActorSpec, RetVal } = require("devtools/shared/protocol");

const processDescriptorSpec = generateActorSpec({
  typeName: "processDescriptor",

  methods: {
    getTarget: {
      request: {},
      response: {
        process: RetVal("json"),
      },
    },
    getWatcher: {
      request: {},
      response: RetVal("watcher"),
    },
  },
});

exports.processDescriptorSpec = processDescriptorSpec;
