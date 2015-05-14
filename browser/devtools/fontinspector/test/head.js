 /* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;
const Cc = Components.classes;

const { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});

let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let TargetFactory = devtools.TargetFactory;

const BASE_URI = "http://mochi.test:8888/browser/browser/devtools/fontinspector/test/"

// All test are asynchronous
waitForExplicitFinish();

gDevTools.testing = true;
SimpleTest.registerCleanupFunction(() => {
  gDevTools.testing = false;
});

registerCleanupFunction(function*() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  yield gDevTools.closeToolbox(target);

  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when the url is loaded
 */
function loadTab(url) {
  let deferred = promise.defer();

  let tab = gBrowser.selectedTab = gBrowser.addTab(url);
  let browser = gBrowser.getBrowserForTab(tab);

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    deferred.resolve({tab: tab, browser: browser});
  }, true);

  return deferred.promise;
}

/**
 * Open the toolbox, with the inspector tool visible.
 * @param {Function} cb Optional callback, if you don't want to use the returned
 * promise
 * @return a promise that resolves when the inspector is ready
 */
let openInspector = Task.async(function*(cb) {
  info("Opening the inspector");
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  let inspector, toolbox;

  // Checking if the toolbox and the inspector are already loaded
  // The inspector-updated event should only be waited for if the inspector
  // isn't loaded yet
  toolbox = gDevTools.getToolbox(target);
  if (toolbox) {
    inspector = toolbox.getPanel("inspector");
    if (inspector) {
      info("Toolbox and inspector already open");
      if (cb) {
        return cb(inspector, toolbox);
      } else {
        return {
          toolbox: toolbox,
          inspector: inspector
        };
      }
    }
  }

  info("Opening the toolbox");
  toolbox = yield gDevTools.showToolbox(target, "inspector");
  yield waitForToolboxFrameFocus(toolbox);
  inspector = toolbox.getPanel("inspector");

  info("Waiting for the inspector to update");
  yield inspector.once("inspector-updated");

  if (cb) {
    return cb(inspector, toolbox);
  } else {
    return {
      toolbox: toolbox,
      inspector: inspector
    };
  }
});

/**
 * Adds a new tab with the given URL, opens the inspector and selects the
 * font-inspector tab.
 *
 * @return Object
 *  {
 *    toolbox,
 *    inspector,
 *    fontInspector
 *  }
 */
let openFontInspectorForURL = Task.async(function* (url) {
  info("Opening tab " + url);
  yield loadTab(url);

  let { toolbox, inspector } = yield openInspector();

  /**
   * Call selectNode to trigger font-inspector update so that we don't timeout
   * if following conditions hold
   * a) the initial 'fontinspector-updated' was emitted while we were waiting
   *    for openInspector to resolve
   * b) the font-inspector tab was selected by default which means the call to
   *    select will not trigger another update.
   *
   * selectNode calls setNodeFront which always emits 'new-node' which calls
   * FontInspector.update that emits the 'fontinspector-updated' event.
   */
  let updated = inspector.once("fontinspector-updated");

  yield selectNode("body", inspector);
  inspector.sidebar.select("fontinspector");

  info("Waiting for font-inspector to update.");
  yield updated;

  info("Font Inspector ready.");

  let { fontInspector } = inspector.sidebar.getWindowForTab("fontinspector");
  return {
    fontInspector,
    inspector,
    toolbox
  };
});

/**
 * Select a node in the inspector given its selector.
 */
let selectNode = Task.async(function*(selector, inspector, reason="test") {
  info("Selecting the node for '" + selector + "'");
  let nodeFront = yield getNodeFront(selector, inspector);
  let updated = inspector.once("inspector-updated");
  inspector.selection.setNodeFront(nodeFront, reason);
  yield updated;
});

/**
 * Get the NodeFront for a given css selector, via the protocol
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return {Promise} Resolves to the NodeFront instance
 */
function getNodeFront(selector, {walker}) {
  if (selector._form) {
    return selector;
  }
  return walker.querySelector(walker.rootNode, selector);
}

/**
 * Wait for the toolbox frame to receive focus after it loads
 * @param {Toolbox} toolbox
 * @return a promise that resolves when focus has been received
 */
function waitForToolboxFrameFocus(toolbox) {
  info("Making sure that the toolbox's frame is focused");
  let def = promise.defer();
  let win = toolbox.frame.contentWindow;
  waitForFocus(def.resolve, win);
  return def.promise;
}
