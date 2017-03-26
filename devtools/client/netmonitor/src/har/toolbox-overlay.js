/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

loader.lazyRequireGetter(this, "HarAutomation", "devtools/client/netmonitor/har/har-automation", true);

// Map of all created overlays. There is always one instance of
// an overlay per Toolbox instance (i.e. one per browser tab).
const overlays = new WeakMap();

/**
 * This object is responsible for initialization and cleanup for HAR
 * export feature. It represents an overlay for the Toolbox
 * following the same life time by listening to its events.
 *
 * HAR APIs are designed for integration with tools (such as Selenium)
 * that automates the browser. Primarily, it is for automating web apps
 * and getting HAR file for every loaded page.
 */
function ToolboxOverlay(toolbox) {
  this.toolbox = toolbox;

  this.onInit = this.onInit.bind(this);
  this.onDestroy = this.onDestroy.bind(this);

  this.toolbox.on("ready", this.onInit);
  this.toolbox.on("destroy", this.onDestroy);
}

ToolboxOverlay.prototype = {
  /**
   * Executed when the toolbox is ready.
   */
  onInit: function () {
    let autoExport = Services.prefs.getBoolPref(
      "devtools.netmonitor.har.enableAutoExportToFile");

    if (!autoExport) {
      return;
    }

    this.initAutomation();
  },

  /**
   * Executed when the toolbox is destroyed.
   */
  onDestroy: function (eventId, toolbox) {
    this.destroyAutomation();
  },

  // Automation

  initAutomation: function () {
    this.automation = new HarAutomation(this.toolbox);
  },

  destroyAutomation: function () {
    if (this.automation) {
      this.automation.destroy();
    }
  },
};

// Registration
function register(toolbox) {
  if (overlays.has(toolbox)) {
    throw Error("There is an existing overlay for the toolbox");
  }

  // Instantiate an overlay for the toolbox.
  let overlay = new ToolboxOverlay(toolbox);
  overlays.set(toolbox, overlay);
}

function get(toolbox) {
  return overlays.get(toolbox);
}

// Exports from this module
exports.register = register;
exports.get = get;
