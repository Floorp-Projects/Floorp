/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Set of protocol messages that affect thread state, and the
 * state the actor is in after each message.
 */
const ThreadStateTypes = {
  paused: "paused",
  resumed: "attached",
  detached: "detached",
  running: "attached",
};

/**
 * Set of protocol messages that are sent by the server without a prior request
 * by the client. This only applies to Actors for which we are not using a
 * protocol.js Front and instead use DevToolsClient directly.
 */
const UnsolicitedNotifications = {
  networkEventUpdate: "networkEventUpdate",
};

module.exports = {
  ThreadStateTypes,
  UnsolicitedNotifications,
};
