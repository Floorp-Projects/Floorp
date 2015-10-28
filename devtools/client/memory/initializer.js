/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
const BrowserLoaderModule = {};
Cu.import("resource://devtools/client/shared/browser-loader.js", BrowserLoaderModule);
const { require } = BrowserLoaderModule.BrowserLoader("resource://devtools/client/memory/", this);
const { Task } = require("resource://gre/modules/Task.jsm");
const { createFactory, createElement, render } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");
const App = createFactory(require("devtools/client/memory/app"));
const Store = require("devtools/client/memory/store");

/**
 * The current target, toolbox, MemoryFront, and HeapAnalysesClient, set by this tool's host.
 */
var gToolbox, gTarget, gFront, gHeapAnalysesClient;

/**
 * Variables set by `initialize()`
 */
var gStore, unsubscribe, isHighlighted;

function initialize () {
  return Task.spawn(function*() {
    let root = document.querySelector("#app");
    gStore = Store();
    let app = createElement(App, { toolbox: gToolbox, front: gFront, heapWorker: gHeapAnalysesClient });
    let provider = createElement(Provider, { store: gStore }, app);
    render(provider, root);
    unsubscribe = gStore.subscribe(onStateChange);
  });
}

function destroy () {
  return Task.spawn(function*(){
    unsubscribe();
  });
}

/**
 * Fired on any state change, currently only handles toggling
 * the highlighting of the tool when recording allocations.
 */
function onStateChange () {
  let isRecording = gStore.getState().allocations.recording;

  if (isRecording !== isHighlighted) {
    isRecording ?
      gToolbox.highlightTool("memory") :
      gToolbox.unhighlightTool("memory");
    isHighlighted = isRecording;
  }
}
