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
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const React = require("devtools/client/shared/vendor/react");
const createStore = require("devtools/client/shared/redux/create-store")();
const reducers = require("devtools/client/performance-new/store/reducers");
const actions = require("devtools/client/performance-new/store/actions");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

/**
 * Initialize the panel by creating a redux store, and render the root component.
 *
 * @param toolbox - The toolbox
 * @param perfFront - The Perf actor's front. Used to start and stop recordings.
 */
function gInit(toolbox, perfFront) {
  const store = createStore(reducers);
  store.dispatch(actions.initializeStore({
    toolbox,
    perfFront,
    /**
     * This function uses privileged APIs in order to take the profile, open up a new
     * tab, and then inject it into perf.html. In order to provide a clear separation
     * in the codebase between privileged and non-privileged code, this function is
     * defined in initializer.js, and injected into the the normal component. All of
     * the React components and Redux store behave as normal unprivileged web components.
     */
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
  }));

  ReactDOM.render(
    React.createElement(
      Provider,
      { store },
      React.createElement(Perf)
    ),
    document.querySelector("#root")
  );
}

function gDestroy() {
  ReactDOM.unmountComponentAtNode(document.querySelector("#root"));
}
