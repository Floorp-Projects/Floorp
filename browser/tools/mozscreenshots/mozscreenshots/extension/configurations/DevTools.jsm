/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["DevTools"];

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const { TargetFactory } = require("devtools/client/framework/target");
const { gDevTools } = require("devtools/client/framework/devtools");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

async function getTargetForSelectedTab() {
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  let target = await TargetFactory.forTab(browserWindow.gBrowser.selectedTab);
  return target;
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
        let target = await getTargetForSelectedTab();
        let toolbox = await gDevTools.showToolbox(target, panel, "bottom");
        this.selectors = [selectToolbox.bind(null, toolbox)];
        await new Promise(resolve => setTimeout(resolve, 500));
      };
    });
  },

  configurations: {
    bottomToolbox: {
      async applyConfig() {
        Services.prefs.clearUserPref("devtools.toolbox.footer.height");
        let target = await getTargetForSelectedTab();
        let toolbox = await gDevTools.showToolbox(
          target,
          "inspector",
          "bottom"
        );
        this.selectors = [selectToolbox.bind(null, toolbox)];
        await new Promise(resolve => setTimeout(resolve, 1000));
      },
    },
    sideToolbox: {
      async applyConfig() {
        let target = await getTargetForSelectedTab();
        let toolbox = await gDevTools.showToolbox(target, "inspector", "right");
        this.selectors = [selectToolbox.bind(null, toolbox)];
        await new Promise(resolve => setTimeout(resolve, 1000));
      },
    },
    undockedToolbox: {
      windowType: "devtools:toolbox",
      async applyConfig() {
        let target = await getTargetForSelectedTab();
        let toolbox = await gDevTools.showToolbox(
          target,
          "inspector",
          "window"
        );
        this.selectors = [selectToolbox.bind(null, toolbox)];
        await new Promise(resolve => setTimeout(resolve, 1000));
      },
    },
  },
};
