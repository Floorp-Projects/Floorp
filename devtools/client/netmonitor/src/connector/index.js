/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
let connector = {};

function onConnect(connection, actions, getState) {
  if (!connection || !connection.tab) {
    return;
  }

  let { clientType } = connection.tab;
  switch (clientType) {
    case "chrome":
      onChromeConnect(connection, actions, getState);
      break;
    case "firefox":
      onFirefoxConnect(connection, actions, getState);
      break;
    default:
      throw Error(`Unknown client type - ${clientType}`);
  }
}

function onDisconnect() {
  connector && connector.disconnect();
}

function onChromeConnect(connection, actions, getState) {
  connector = require("./chrome-connector");
  connector.connect(connection, actions, getState);
}

function onFirefoxConnect(connection, actions, getState) {
  connector = require("./firefox-connector");
  connector.connect(connection, actions, getState);
}

function inspectRequest() {
  return connector.inspectRequest(...arguments);
}

function getLongString() {
  return connector.getLongString(...arguments);
}

function getNetworkRequest() {
  return connector.getNetworkRequest(...arguments);
}

function getTabTarget() {
  return connector.getTabTarget();
}

function sendHTTPRequest() {
  return connector.sendHTTPRequest(...arguments);
}

function setPreferences() {
  return connector.setPreferences(...arguments);
}

function triggerActivity() {
  return connector.triggerActivity(...arguments);
}

function viewSourceInDebugger() {
  return connector.viewSourceInDebugger(...arguments);
}

module.exports = {
  onConnect,
  onChromeConnect,
  onFirefoxConnect,
  onDisconnect,
  getLongString,
  getNetworkRequest,
  getTabTarget,
  inspectRequest,
  sendHTTPRequest,
  setPreferences,
  triggerActivity,
  viewSourceInDebugger,
};
