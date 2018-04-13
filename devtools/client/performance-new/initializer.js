/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* exported gInit, gDestroy */

const BrowserLoaderModule = {};
ChromeUtils.import("resource://devtools/client/shared/browser-loader.js", BrowserLoaderModule);
const { require } = BrowserLoaderModule.BrowserLoader({
  baseURI: "resource://devtools/client/memory/",
  window
});
const Perf = require("devtools/client/performance-new/components/Perf");
const Services = require("Services");
const { render, unmountComponentAtNode } = require("devtools/client/shared/vendor/react-dom");
const { createElement } = require("devtools/client/shared/vendor/react");

/**
 * Perform a simple initialization on the panel. Hook up event listeners.
 *
 * @param toolbox - The toolbox
 * @param perfFront - The Perf actor's front. Used to start and stop recordings.
 */
function gInit(toolbox, perfFront) {
  const props = {
    toolbox,
    perfFront,
    receiveProfile: profile => {
      // Open up a new tab and send a message with the profile.
      let browser = top.gBrowser;
      if (!browser) {
        // Current isn't browser window. Looking for the recent browser.
        const win = Services.wm.getMostRecentWindow("navigator:browser");
        if (!win) {
          throw new Error("No browser window");
        }
        browser = win.gBrowser;
        Services.focus.activeWindow = win;
      }
      const tab = browser.addTab("https://perf-html.io/from-addon");
      browser.selectedTab = tab;
      const mm = tab.linkedBrowser.messageManager;
      mm.loadFrameScript(
        "chrome://devtools/content/performance-new/frame-script.js",
        false
      );
      mm.sendAsyncMessage("devtools:perf-html-transfer-profile", profile);
    }
  };
  render(createElement(Perf, props), document.querySelector("#root"));
}

function gDestroy() {
  unmountComponentAtNode(document.querySelector("#root"));
}
