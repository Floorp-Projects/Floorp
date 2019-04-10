/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const CONNECTION_TYPES = {
  NETWORK: "network",
  THIS_FIREFOX: "this-firefox",
  UNKNOWN: "unknown",
  USB: "usb",
};

const DEBUG_TARGET_TYPES = {
  EXTENSION: "extension",
  PROCESS: "process",
  TAB: "tab",
  WORKER: "worker",
};

module.exports = {
  CONNECTION_TYPES,
  DEBUG_TARGET_TYPES,
};
