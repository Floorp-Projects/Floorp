/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals window, document, NetMonitorController, NetMonitorView */
/* exported Netmonitor, NetMonitorController, NetMonitorView, $, $all, dumpn */

"use strict";

const Cu = Components.utils;
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
const { BrowserLoader } = Cu.import("resource://devtools/client/shared/browser-loader.js", {});

function Netmonitor(toolbox) {
  const { require } = BrowserLoader({
    baseURI: "resource://devtools/client/netmonitor/",
    window,
    commonLibRequire: toolbox.browserRequire,
  });

  window.windowRequire = require;

  const { NetMonitorController } = require("./netmonitor-controller.js");
  const { NetMonitorView } = require("./netmonitor-view.js");

  window.NetMonitorController = NetMonitorController;
  window.NetMonitorView = NetMonitorView;

  NetMonitorController._toolbox = toolbox;
  NetMonitorController._target = toolbox.target;
}

Netmonitor.prototype = {
  init() {
    return window.NetMonitorController.startupNetMonitor();
  },

  destroy() {
    return window.NetMonitorController.shutdownNetMonitor();
  }
};

/**
 * DOM query helper.
 * TODO: Move it into "dom-utils.js" module and "require" it when needed.
 */
var $ = (selector, target = document) => target.querySelector(selector);
var $all = (selector, target = document) => target.querySelectorAll(selector);

/**
 * Helper method for debugging.
 * @param string
 */
function dumpn(str) {
  if (wantLogging) {
    dump("NET-FRONTEND: " + str + "\n");
  }
}

var wantLogging = Services.prefs.getBoolPref("devtools.debugger.log");
