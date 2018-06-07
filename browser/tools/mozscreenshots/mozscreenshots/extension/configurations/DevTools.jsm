/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["DevTools"];

ChromeUtils.import("resource://devtools/client/framework/gDevTools.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");

let { devtools } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
let TargetFactory = devtools.TargetFactory;

function getTargetForSelectedTab() {
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  let target = TargetFactory.forTab(browserWindow.gBrowser.selectedTab);
  return target;
}

function selectToolbox() {
  return gDevTools.getToolbox(getTargetForSelectedTab()).win.document.querySelector("#toolbox-container");
}

var DevTools = {
  init(libDir) {
    let panels = ["options", "webconsole", "jsdebugger", "styleeditor",
                  "performance", "netmonitor"];

    panels.forEach(panel => {
      this.configurations[panel] = {};
      this.configurations[panel].selectors = [selectToolbox];
      this.configurations[panel].applyConfig = async function() {
        Services.prefs.setIntPref("devtools.toolbox.footer.height", 800);
        await gDevTools.showToolbox(getTargetForSelectedTab(), panel, "bottom");
        await new Promise(resolve => setTimeout(resolve, 500));
      };
    });
  },

  configurations: {
    bottomToolbox: {
      selectors: [selectToolbox],
      async applyConfig() {
        Services.prefs.clearUserPref("devtools.toolbox.footer.height");
        await gDevTools.showToolbox(getTargetForSelectedTab(), "inspector", "bottom");
        await new Promise(resolve => setTimeout(resolve, 1000));
      },
    },
    sideToolbox: {
      selectors: [selectToolbox],
      async applyConfig() {
        await gDevTools.showToolbox(getTargetForSelectedTab(), "inspector", "right");
        await new Promise(resolve => setTimeout(resolve, 500));
      },
    },
    undockedToolbox: {
      selectors: [selectToolbox],
      windowType: "devtools:toolbox",
      async applyConfig() {
        await gDevTools.showToolbox(getTargetForSelectedTab(), "inspector", "window");
        await new Promise(resolve => setTimeout(resolve, 500));
      },
    }
  },
};
