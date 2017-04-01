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
const { Services: { pref }} = require("devtools-modules");
const { configureStore } = require("./src/utils/create-store");

require("./src/assets/styles/netmonitor.css");

EventEmitter.decorate(window);

pref("devtools.netmonitor.enabled", true);
pref("devtools.netmonitor.filters", "[\"all\"]");
pref("devtools.netmonitor.hiddenColumns", "[]");
pref("devtools.netmonitor.panes-network-details-width", 550);
pref("devtools.netmonitor.panes-network-details-height", 450);
pref("devtools.netmonitor.har.defaultLogDir", "");
pref("devtools.netmonitor.har.defaultFileName", "Archive %date");
pref("devtools.netmonitor.har.jsonp", false);
pref("devtools.netmonitor.har.jsonpCallback", "");
pref("devtools.netmonitor.har.includeResponseBodies", true);
pref("devtools.netmonitor.har.compress", false);
pref("devtools.netmonitor.har.forceExport", false);
pref("devtools.netmonitor.har.pageLoadedTimeout", 1500);
pref("devtools.netmonitor.har.enableAutoExportToFile", false);
pref("devtools.webconsole.persistlog", false);

const App = require("./src/components/app");
const store = window.gStore = configureStore();
const { NetMonitorController } = require("./src/netmonitor-controller");

// FIXME: Inject NetMonitorController to global window
window.NetMonitorController = NetMonitorController;

bootstrap(React, ReactDOM, App, null, store).then(connection => {
  if (!connection || !connection.tab) {
    return;
  }
  NetMonitorController.startupNetMonitor(connection);
});
