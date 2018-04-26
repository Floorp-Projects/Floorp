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
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const React = require("devtools/client/shared/vendor/react");
const createStore = require("devtools/client/shared/redux/create-store")();
const selectors = require("devtools/client/performance-new/store/selectors");
const reducers = require("devtools/client/performance-new/store/reducers");
const actions = require("devtools/client/performance-new/store/actions");
const { Provider } = require("devtools/client/shared/vendor/react-redux");
const {
  receiveProfile,
  getRecordingPreferences,
  setRecordingPreferences
} = require("devtools/client/performance-new/browser");

/**
 * Initialize the panel by creating a redux store, and render the root component.
 *
 * @param toolbox - The toolbox
 * @param perfFront - The Perf actor's front. Used to start and stop recordings.
 */
async function gInit(toolbox, perfFront, preferenceFront) {
  const store = createStore(reducers);

  // Do some initialization, especially with privileged things that are part of the
  // the browser.
  store.dispatch(actions.initializeStore({
    toolbox,
    perfFront,
    receiveProfile,
    // Pull the default recording settings from the reducer, and update them according
    // to what's in the target's preferences. This way the preferences are stored
    // on the target. This could be useful for something like Android where you might
    // want to tweak the settings.
    recordingSettingsFromPreferences: await getRecordingPreferences(
      preferenceFront,
      selectors.getRecordingSettings(store.getState())
    ),
    // Go ahead and hide the implementation details for the component on how the
    // preference information is stored
    setRecordingPreferences: () => setRecordingPreferences(
      preferenceFront,
      selectors.getRecordingSettings(store.getState())
    )
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
