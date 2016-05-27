/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["DevTools"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://devtools/client/framework/gDevTools.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

let { devtools } = Cu.import("resource://devtools/shared/Loader.jsm", {});
let TargetFactory = devtools.TargetFactory;

function getTargetForSelectedTab() {
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  let target = TargetFactory.forTab(browserWindow.gBrowser.selectedTab);
  return target;
}

this.DevTools = {
  init(libDir) {
    let panels = ["options", "webconsole", "jsdebugger", "styleeditor",
                  "performance", "netmonitor"];

    panels.forEach(panel => {
      this.configurations[panel] = {};
      this.configurations[panel].applyConfig = Task.async(function* () {
        yield gDevTools.showToolbox(getTargetForSelectedTab(), panel, "bottom");
        yield new Promise(resolve => setTimeout(resolve, 500));
      });
    });
  },

  configurations: {
    bottomToolbox: {
      applyConfig: Task.async(function* () {
        yield gDevTools.showToolbox(getTargetForSelectedTab(), "inspector", "bottom");
        yield new Promise(resolve => setTimeout(resolve, 1000));
      }),
    },
    sideToolbox: {
      applyConfig: Task.async(function* () {
        yield gDevTools.showToolbox(getTargetForSelectedTab(), "inspector", "side");
        yield new Promise(resolve => setTimeout(resolve, 500));
      }),
    },
    undockedToolbox: {
      applyConfig: Task.async(function* () {
        yield gDevTools.showToolbox(getTargetForSelectedTab(), "inspector", "window");
        yield new Promise(resolve => setTimeout(resolve, 500));
      }),
    }
  },
};
