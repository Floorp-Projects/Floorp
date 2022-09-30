/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  listDevices,
} = require("resource://devtools/client/shared/remote-debugging/adb/commands/list-devices.js");
const {
  prepareTCPConnection,
} = require("resource://devtools/client/shared/remote-debugging/adb/commands/prepare-tcp-connection.js");
const {
  runCommand,
} = require("resource://devtools/client/shared/remote-debugging/adb/commands/run-command.js");
const {
  shell,
} = require("resource://devtools/client/shared/remote-debugging/adb/commands/shell.js");
const {
  TrackDevicesCommand,
} = require("resource://devtools/client/shared/remote-debugging/adb/commands/track-devices.js");

module.exports = {
  listDevices,
  prepareTCPConnection,
  runCommand,
  shell,
  TrackDevicesCommand,
};
