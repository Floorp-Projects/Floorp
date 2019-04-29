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
 * by the client.
 */
const UnsolicitedNotifications = {
  networkEventUpdate: "networkEventUpdate",
  tabDetached: "tabDetached",
  tabListChanged: "tabListChanged",
  addonListChanged: "addonListChanged",
  workerListChanged: "workerListChanged",
  serviceWorkerRegistrationListChanged: "serviceWorkerRegistrationList",

  // newSource is still emitted on the ThreadActor, in addition to the
  // BrowsingContextActor we have to keep it here until ThreadClient is converted to
  // ThreadFront and/or we stop emitting this duplicated events.
  // See ThreadActor.onNewSourceEvent.
  newSource: "newSource",
};

/**
 * Set of pause types that are sent by the server and not as an immediate
 * response to a client request.
 */
const UnsolicitedPauses = {
  resumeLimit: "resumeLimit",
  debuggerStatement: "debuggerStatement",
  breakpoint: "breakpoint",
  DOMEvent: "DOMEvent",
  watchpoint: "watchpoint",
  exception: "exception",
  replayForcedPause: "replayForcedPause",
};

module.exports = {
  ThreadStateTypes,
  UnsolicitedNotifications,
  UnsolicitedPauses,
};
