/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { dumpn } = require("devtools/shared/DevToolsUtils");
/**
 * The listDevices command is currently unused in DevTools. We are keeping it while
 * working on RemoteDebugging NG, in case it becomes needed later. We will remove it from
 * the codebase if unused at the end of the project. See Bug 1511779.
 */
const listDevices = function() {
  dumpn("listDevices");

  return this.runCommand("host:devices").then(function onSuccess(data) {
    const lines = data.split("\n");
    const res = [];
    lines.forEach(function(line) {
      if (line.length == 0) {
        return;
      }
      const [device] = line.split("\t");
      res.push(device);
    });
    return res;
  });
};
exports.listDevices = listDevices;
