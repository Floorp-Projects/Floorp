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
const { bindActionCreators } = require("redux");
const { bootstrap, renderRoot } = require("devtools-launchpad");
const EventEmitter = require("devtools-modules/src/utils/event-emitter");
const { Services: { appinfo, pref }} = require("devtools-modules");
const { configureStore } = require("./src/utils/create-store");

require("./src/assets/styles/netmonitor.css");

EventEmitter.decorate(window);

pref("devtools.netmonitor.enabled", true);
pref("devtools.netmonitor.filters", "[\"all\"]");
pref("devtools.netmonitor.visibleColumns",
     "[\"status\",\"method\",\"file\",\"domain\",\"cause\"," +
     "\"type\",\"transferred\",\"contentSize\",\"waterfall\"]"
);
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
const store = configureStore();
const actions = bindActionCreators(require("./src/actions"), store.dispatch);
const { onConnect } = require("./src/connector");

// Inject to global window for testing
window.store = store;

/**
 * Stylesheet links in devtools xhtml files are using chrome or resource URLs.
 * Rewrite the href attribute to remove the protocol. web-server.js contains redirects
 * to map CSS urls to the proper file. Supports urls using:
 *   - devtools/client/
 *   - devtools/content/
 *   - skin/
 * Will also add mandatory classnames and attributes to be compatible with devtools theme
 * stylesheet.
 */
window.addEventListener("DOMContentLoaded", () => {
  for (let link of document.head.querySelectorAll("link")) {
    link.href = link.href.replace(/(resource|chrome)\:\/\//, "/");
  }

  if (appinfo.OS === "Darwin") {
    document.documentElement.setAttribute("platform", "mac");
  } else if (appinfo.OS === "Linux") {
    document.documentElement.setAttribute("platform", "linux");
  } else {
    document.documentElement.setAttribute("platform", "win");
  }
});

bootstrap(React, ReactDOM).then((connection) => {
  if (!connection) {
    return;
  }
  renderRoot(React, ReactDOM, App, store);
  onConnect(connection, actions, store.getState);
});
