/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported initialize, destroy, Promise */

"use strict";

const {
  createFactory,
  createElement,
} = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { Provider } = require("devtools/client/shared/vendor/react-redux");
const App = createFactory(require("devtools/client/memory/app"));
const Store = require("devtools/client/memory/store");
const { assert } = require("devtools/shared/DevToolsUtils");

const { updateMemoryFront } = require("devtools/client/memory/actions/front");

// Shared variables used by several methods of this module.
let root, store, unsubscribe;

const initialize = async function(commands) {
  // Exposed by panel.js
  const { gToolbox, gHeapAnalysesClient } = window;

  root = document.querySelector("#app");
  store = Store();
  const app = createElement(App, {
    toolbox: gToolbox,
    commands,
    heapWorker: gHeapAnalysesClient,
  });
  const provider = createElement(Provider, { store }, app);
  ReactDOM.render(provider, root);
  unsubscribe = store.subscribe(onStateChange);

  // Exposed for tests.
  window.gStore = store;
};

const updateFront = front => {
  store.dispatch(updateMemoryFront(front));
};

const destroy = function() {
  const ok = ReactDOM.unmountComponentAtNode(root);
  assert(
    ok,
    "Should successfully unmount the memory tool's top level React component"
  );

  unsubscribe();
};

// Current state
let isHighlighted;

/**
 * Fired on any state change, currently only handles toggling
 * the highlighting of the tool when recording allocations.
 */
function onStateChange() {
  const { gToolbox } = window;

  const isRecording = store.getState().allocations.recording;
  if (isRecording === isHighlighted) {
    return;
  }

  if (isRecording) {
    gToolbox.highlightTool("memory");
  } else {
    gToolbox.unhighlightTool("memory");
  }

  isHighlighted = isRecording;
}

module.exports = { initialize, updateFront, destroy };
