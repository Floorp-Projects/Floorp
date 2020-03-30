/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Wrapper around the ADB utility.

"use strict";

const { dumpn } = require("devtools/shared/DevToolsUtils");
const { setTimeout } = require("resource://gre/modules/Timer.jsm");
const {
  adbProcess,
} = require("devtools/client/shared/remote-debugging/adb/adb-process");
const client = require("devtools/client/shared/remote-debugging/adb/adb-client");

const OKAY = 0x59414b4f;

// Asynchronously runs an adb command.
// @param command The command as documented in
// http://androidxref.com/4.0.4/xref/system/core/adb/SERVICES.TXT
const runCommand = function(command) {
  dumpn("runCommand " + command);
  return new Promise((resolve, reject) => {
    if (!adbProcess.ready) {
      setTimeout(function() {
        reject("ADB_NOT_READY");
      });
      return;
    }

    const socket = client.connect();

    socket.s.onopen = function() {
      dumpn("runCommand onopen");
      const req = client.createRequest(command);
      socket.send(req);
    };

    socket.s.onerror = function() {
      dumpn("runCommand onerror");
      reject("NETWORK_ERROR");
    };

    socket.s.onclose = function() {
      dumpn("runCommand onclose");
    };

    socket.s.ondata = function(event) {
      dumpn("runCommand ondata");
      const data = event.data;

      const packet = client.unpackPacket(data, false);
      if (!client.checkResponse(data, OKAY)) {
        socket.close();
        dumpn("Error: " + packet.data);
        reject("PROTOCOL_ERROR");
        return;
      }

      resolve(packet.data);
    };
  });
};
exports.runCommand = runCommand;
