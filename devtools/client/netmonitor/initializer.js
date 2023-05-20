/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* exported initialize */

"use strict";

/**
 * This script is the entry point of Network monitor panel.
 * See README.md for more information.
 */
const { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/shared/loader/browser-loader.js"
);

const require = (window.windowRequire = BrowserLoader({
  baseURI: "resource://devtools/client/netmonitor/",
  window,
}).require);

const {
  NetMonitorAPI,
} = require("resource://devtools/client/netmonitor/src/api.js");
const {
  NetMonitorApp,
} = require("resource://devtools/client/netmonitor/src/app.js");
const EventEmitter = require("resource://devtools/shared/event-emitter.js");

// Inject EventEmitter into global window.
EventEmitter.decorate(window);

/**
 * This is the initialization point for the Network monitor.
 *
 * @param {Object} api Allows reusing existing API object.
 */
function initialize(api) {
  const app = new NetMonitorApp(api);

  // Inject to global window for testing
  window.Netmonitor = app;
  window.api = api;
  window.store = app.api.store;
  window.connector = app.api.connector;
  window.actions = app.api.actions;

  return app;
}

/**
 * The following code is used to open Network monitor in a tab.
 * Like the Launchpad, but without Launchpad.
 *
 * For example:
 * chrome://devtools/content/netmonitor/index.html?type=process
 * loads the netmonitor for the parent process, exactly like the
 * one in the browser toolbox
 *
 * It's also possible to connect to a tab.
 * 1) go in about:debugging
 * 2) In menu Tabs, click on a Debug button for particular tab
 *
 * This  will open an about:devtools-toolbox url, from which you can
 * take type and id query parameters and reuse them for the chrome url
 * of the netmonitor
 *
 * chrome://devtools/content/netmonitor/index.html?type=tab&id=1234 URLs
 * where 1234 is the tab id, you can retrieve from about:debugging#tabs links.
 * Simply copy the id from about:devtools-toolbox?type=tab&id=1234 URLs.
 */

// URL constructor doesn't support chrome: scheme
const href = window.location.href.replace(/chrome:/, "http://");
const url = new window.URL(href);

// If query parameters are given in a chrome tab, the inspector
// is running in standalone.
if (window.location.protocol === "chrome:" && url.search.length > 1) {
  const {
    commandsFromURL,
  } = require("resource://devtools/client/framework/commands-from-url.js");

  (async function () {
    try {
      const commands = await commandsFromURL(url);
      const target = await commands.descriptorFront.getTarget();
      // Create a fake toolbox object
      const toolbox = {
        target,
        viewSourceInDebugger() {
          throw new Error(
            "toolbox.viewSourceInDebugger is not implement from a tab"
          );
        },
        commands,
      };

      const api = new NetMonitorAPI();
      await api.connect(toolbox);
      const app = window.initialize(api);
      app.bootstrap({
        toolbox,
        document: window.document,
      });
    } catch (err) {
      window.alert("Unable to start the network monitor:" + err);
    }
  })();
}
