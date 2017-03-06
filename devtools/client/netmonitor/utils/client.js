/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global gStore */

"use strict";

const Services = require("Services");
const Actions = require("../actions/index");
const { EVENTS } = require("../constants");

let activeConsole;

/**
 * Called for each location change in the monitored tab.
 *
 * @param {String} type Packet type.
 * @param {Object} packet Packet received from the server.
 */
function navigated(type) {
  window.emit(EVENTS.TARGET_DID_NAVIGATE);
}

/**
 * Called for each location change in the monitored tab.
 *
 * @param {String} type Packet type.
 * @param {Object} packet Packet received from the server.
 */
function willNavigate(type) {
  // Reset UI.
  if (!Services.prefs.getBoolPref("devtools.webconsole.persistlog")) {
    gStore.dispatch(Actions.batchReset());
    gStore.dispatch(Actions.clearRequests());
  } else {
    // If the log is persistent, just clear all accumulated timing markers.
    gStore.dispatch(Actions.clearTimingMarkers());
  }

  window.emit(EVENTS.TARGET_WILL_NAVIGATE);
}

/**
 * Process connection events.
 *
 * @param {Object} tabTarget
 */
function onFirefoxConnect(tabTarget) {
  activeConsole = tabTarget.activeConsole;
  tabTarget.on("navigate", navigated);
  tabTarget.on("will-navigate", willNavigate);
}

/**
 * Process disconnect events.
 *
 * @param {Object} tabTarget
 */
function onFirefoxDisconnect(tabTarget) {
  activeConsole = null;
  tabTarget.off("navigate", navigated);
  tabTarget.off("will-navigate", willNavigate);
}

/**
 * Retrieve webconsole object
 *
 * @returns {Object} webConsole
 */
function getWebConsoleClient() {
  return activeConsole;
}

module.exports = {
  getWebConsoleClient,
  onFirefoxConnect,
  onFirefoxDisconnect,
};
