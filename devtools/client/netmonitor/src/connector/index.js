/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Generic connector wrapper object that is responsible for
 * instantiating specific connector implementation according
 * to the client type.
 */
class Connector {
  constructor() {
    this.connector = null;

    // Bind public API
    this.connect = this.connect.bind(this);
    this.disconnect = this.disconnect.bind(this);
    this.connectChrome = this.connectChrome.bind(this);
    this.connectFirefox = this.connectFirefox.bind(this);
    this.getLongString = this.getLongString.bind(this);
    this.getNetworkRequest = this.getNetworkRequest.bind(this);
    this.getTabTarget = this.getTabTarget.bind(this);
    this.sendHTTPRequest = this.sendHTTPRequest.bind(this);
    this.setPreferences = this.setPreferences.bind(this);
    this.triggerActivity = this.triggerActivity.bind(this);
    this.viewSourceInDebugger = this.viewSourceInDebugger.bind(this);
  }

  // Connect/Disconnect API

  connect(connection, actions, getState) {
    if (!connection || !connection.tab) {
      return;
    }

    let { clientType } = connection.tab;
    switch (clientType) {
      case "chrome":
        this.connectChrome(connection, actions, getState);
        break;
      case "firefox":
        this.connectFirefox(connection, actions, getState);
        break;
      default:
        throw Error(`Unknown client type - ${clientType}`);
    }
  }

  disconnect() {
    this.connector && this.connector.disconnect();
  }

  connectChrome(connection, actions, getState) {
    this.connector = require("./chrome-connector");
    this.connector.connect(connection, actions, getState);
  }

  connectFirefox(connection, actions, getState) {
    this.connector = require("./firefox-connector");
    this.connector.connect(connection, actions, getState);
  }

  pause() {
    this.connector.pause();
  }

  resume() {
    this.connector.resume();
  }

  // Public API

  getLongString() {
    return this.connector.getLongString(...arguments);
  }

  getNetworkRequest() {
    return this.connector.getNetworkRequest(...arguments);
  }

  getTabTarget() {
    return this.connector.getTabTarget();
  }

  sendHTTPRequest() {
    return this.connector.sendHTTPRequest(...arguments);
  }

  setPreferences() {
    return this.connector.setPreferences(...arguments);
  }

  triggerActivity() {
    return this.connector.triggerActivity(...arguments);
  }

  viewSourceInDebugger() {
    return this.connector.viewSourceInDebugger(...arguments);
  }
}

module.exports.Connector = Connector;
