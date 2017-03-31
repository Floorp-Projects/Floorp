/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This script is the entry point of devtools-launchpad. Make netmonitor possible
 * to run on standalone browser tab without chrome privilege.
 * See README.md for more information.
 */
const React = require("react");
const ReactDOM = require("react-dom");
const { bootstrap } = require("devtools-launchpad");
const { EventEmitter } = require("devtools-modules");
const { configureStore } = require("./src/utils/create-store");

require("./src/assets/styles/netmonitor.css");

EventEmitter.decorate(window);

const App = require("./src/components/app");
const store = window.gStore = configureStore();
const { NetMonitorController } = require("./src/netmonitor-controller");

bootstrap(React, ReactDOM, App, null, store).then(connection => {
  if (!connection || !connection.tab) {
    return;
  }
  NetMonitorController.startupNetMonitor(connection);
});
