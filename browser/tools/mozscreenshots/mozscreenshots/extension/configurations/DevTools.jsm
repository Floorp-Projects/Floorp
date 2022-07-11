/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["DevTools"];

const { require } = ChromeUtils.import(
  "resource://devtools/shared/loader/Loader.jsm"
);
const { gDevTools } = require("devtools/client/framework/devtools");
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

async function showToolboxForSelectedTab(toolId, hostType) {
  const browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  const tab = browserWindow.gBrowser.selectedTab;
  return gDevTools.showToolboxForTab(tab, { toolId, hostType });
}

function selectToolbox(toolbox) {
  return toolbox.win.document.querySelector("#toolbox-container");
}

var DevTools = {
  init(libDir) {
    let panels = [
      "options",
      "webconsole",
      "jsdebugger",
      "styleeditor",
      "performance",
      "netmonitor",
    ];

    panels.forEach(panel => {
      this.configurations[panel] = {};
      this.configurations[panel].applyConfig = async function() {
        Services.prefs.setIntPref("devtools.toolbox.footer.height", 800);
        let toolbox = await showToolboxForSelectedTab(panel, "bottom");
        this.selectors = [selectToolbox.bind(null, toolbox)];
        await new Promise(resolve => setTimeout(resolve, 500));
      };
    });
  },

  configurations: {
    bottomToolbox: {
      async applyConfig() {
        Services.prefs.clearUserPref("devtools.toolbox.footer.height");
        let toolbox = await showToolboxForSelectedTab("inspector", "bottom");
        this.selectors = [selectToolbox.bind(null, toolbox)];
        await new Promise(resolve => setTimeout(resolve, 1000));
      },
    },
    sideToolbox: {
      async applyConfig() {
        let toolbox = await showToolboxForSelectedTab("inspector", "right");
        this.selectors = [selectToolbox.bind(null, toolbox)];
        await new Promise(resolve => setTimeout(resolve, 1000));
      },
      async verifyConfig() {
        return "Panel sizes are regularly inconsistent";
      },
    },
    undockedToolbox: {
      windowType: "devtools:toolbox",
      async applyConfig() {
        let toolbox = await showToolboxForSelectedTab("inspector", "window");
        this.selectors = [selectToolbox.bind(null, toolbox)];
        await new Promise(resolve => setTimeout(resolve, 1000));
      },
      async verifyConfig() {
        return "Panel sizes are regularly inconsistent";
      },
    },
  },
};
