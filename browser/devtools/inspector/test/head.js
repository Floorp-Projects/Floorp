/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;
const Cc = Components.classes;
const CC = Components.Constructor;

// Services.prefs.setBoolPref("devtools.debugger.log", true);
// SimpleTest.registerCleanupFunction(() => {
//   Services.prefs.clearUserPref("devtools.debugger.log");
// });

// Uncomment this pref to dump all devtools emitted events to the console.
// Services.prefs.setBoolPref("devtools.dump.emit", true);

let TEST_URL_ROOT = "http://example.com/browser/browser/devtools/inspector/test/";
let ROOT_TEST_DIR = getRootDirectory(gTestPath);
let { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});

// All test are asynchronous
waitForExplicitFinish();

let {TargetFactory, require} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools;
let {console} = Cu.import("resource://gre/modules/devtools/Console.jsm", {});

// Import the GCLI test helper
let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "../../../commandline/test/helpers.js", this);

// Import helpers registering the test-actor in remote targets
Services.scriptloader.loadSubScript(testDir + "../../../shared/test/test-actor-registry.js", this);

gDevTools.testing = true;
registerCleanupFunction(() => {
  gDevTools.testing = false;
});

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.dump.emit");
  Services.prefs.clearUserPref("devtools.inspector.activeSidebar");
});

registerCleanupFunction(function*() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  yield gDevTools.closeToolbox(target);

  // Move the mouse outside inspector. If the test happened fake a mouse event
  // somewhere over inspector the pointer is considered to be there when the
  // next test begins. This might cause unexpected events to be emitted when
  // another test moves the mouse.
  EventUtils.synthesizeMouseAtPoint(1, 1, {type: "mousemove"}, window);

  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when the url is loaded
 */
let addTab = Task.async(function* (url) {
  info("Adding a new tab with URL: '" + url + "'");

  window.focus();

  let tab = gBrowser.selectedTab = gBrowser.addTab(url);
  let browser = tab.linkedBrowser;

  yield once(browser, "load", true);
  info("URL '" + url + "' loading complete");

  return tab;
});

let navigateTo = function (toolbox, url) {
  let activeTab = toolbox.target.activeTab;
  return activeTab.navigateTo(url);
};

/**
 * Simple DOM node accesor function that takes either a node or a string css
 * selector as argument and returns the corresponding node
 * @param {String|DOMNode} nodeOrSelector
 * @param {Object} options
 *        An object containing any of the following options:
 *        - document: HTMLDocument that should be queried for the selector.
 *                    Default: content.document.
 *        - expectNoMatch: If true and a node matches the given selector, a
 *                         failure is logged for an unexpected match.
 *                         If false and nothing matches the given selector, a
 *                         failure is logged for a missing match.
 *                         Default: false.
 * @return {DOMNode}
 */
function getNode(nodeOrSelector, options = {}) {
  let document = options.document || content.document;
  let noMatches = !!options.expectNoMatch;

  if (typeof nodeOrSelector === "string") {
    info("Looking for a node that matches selector " + nodeOrSelector);
    let node = document.querySelector(nodeOrSelector);
    if (noMatches) {
      ok(!node, "Selector " + nodeOrSelector + " didn't match any nodes.");
    }
    else {
      ok(node, "Selector " + nodeOrSelector + " matched a node.");
    }

    return node;
  }

  info("Looking for a node but selector was not a string.");
  return nodeOrSelector;
}

/**
 * Highlight a node and set the inspector's current selection to the node or
 * the first match of the given css selector.
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox
 * @return a promise that resolves when the inspector is updated with the new
 * node
 */
function selectAndHighlightNode(selector, inspector) {
  info("Highlighting and selecting the node " + selector);
  return selectNode(selector, inspector, "test-highlight");
}

/**
 * Set the inspector's current selection to the first match of the given css
 * selector
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @param {String} reason Defaults to "test" which instructs the inspector not
 * to highlight the node upon selection
 * @return {Promise} Resolves when the inspector is updated with the new node
 */
let selectNode = Task.async(function*(selector, inspector, reason="test") {
  info("Selecting the node for '" + selector + "'");
  let nodeFront = yield getNodeFront(selector, inspector);
  let updated = inspector.once("inspector-updated");
  inspector.selection.setNodeFront(nodeFront, reason);
  yield updated;
});

/**
 * Open the inspector in a tab with given URL.
 * @param {string} url  The URL to open.
 * @param {String} hostType Optional hostType, as defined in Toolbox.HostType
 * @return A promise that is resolved once the tab and inspector have loaded
 *         with an object: { tab, toolbox, inspector }.
 */
let openInspectorForURL = Task.async(function*(url, hostType) {
  let tab = yield addTab(url);
  let { inspector, toolbox, testActor } = yield openInspector(null, hostType);
  return { tab, inspector, toolbox, testActor };
});


/**
 * Open the toolbox, with the inspector tool visible.
 * @param {Function} cb Optional callback, if you don't want to use the returned
 * promise
 * @param {String} hostType Optional hostType, as defined in Toolbox.HostType
 * @return a promise that resolves when the inspector is ready
 */
let openInspector = Task.async(function*(cb, hostType) {
  info("Opening the inspector");
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  let toolbox = gDevTools.getToolbox(target);
  if (toolbox) {
    if (toolbox.getPanel("inspector")) {
      info("Toolbox and inspector already open");
      throw new Error("Inspector is already opened, please use getActiveInspector");
    }
  }

  info("Opening the toolbox");
  toolbox = yield gDevTools.showToolbox(target, "inspector", hostType);
  yield waitForToolboxFrameFocus(toolbox);
  let inspector = toolbox.getPanel("inspector");

  info("Waiting for the inspector to update");
  yield inspector.once("inspector-updated");

  yield registerTestActor(toolbox.target.client);
  let testActor = yield getTestActor(toolbox);

  return {
    toolbox: toolbox,
    inspector: inspector,
    testActor: testActor
  };
});

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

function getActiveInspector() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  return gDevTools.getToolbox(target).getPanel("inspector");
}

/**
 * Get the NodeFront for a node that matches a given css selector, via the
 * protocol.
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
 * Get the NodeFront for a node that matches a given css selector inside a
 * given iframe.
 * @param {String|NodeFront} selector
 * @param {String|NodeFront} frameSelector A selector that matches the iframe
 * the node is in
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @param {String} reason Defaults to "test" which instructs the inspector not
 * to highlight the node upon selection
 * @return {Promise} Resolves when the inspector is updated with the new node
 */
let getNodeFrontInFrame = Task.async(function*(selector, frameSelector,
                                               inspector, reason="test") {
  let iframe = yield getNodeFront(frameSelector, inspector);
  let {nodes} = yield inspector.walker.children(iframe);
  return inspector.walker.querySelector(nodes[0], selector);
});

function synthesizeKeyFromKeyTag(aKeyId, aDocument = null) {
  let document = aDocument || document;
  let key = document.getElementById(aKeyId);
  isnot(key, null, "Successfully retrieved the <key> node");

  let modifiersAttr = key.getAttribute("modifiers");

  let name = null;

  if (key.getAttribute("keycode"))
    name = key.getAttribute("keycode");
  else if (key.getAttribute("key"))
    name = key.getAttribute("key");

  isnot(name, null, "Successfully retrieved keycode/key");

  let modifiers = {
    shiftKey: modifiersAttr.match("shift"),
    ctrlKey: modifiersAttr.match("ctrl"),
    altKey: modifiersAttr.match("alt"),
    metaKey: modifiersAttr.match("meta"),
    accelKey: modifiersAttr.match("accel")
  }

  EventUtils.synthesizeKey(name, modifiers);
}

let focusSearchBoxUsingShortcut = Task.async(function* (panelWin, callback) {
  info("Focusing search box");
  let searchBox = panelWin.document.getElementById("inspector-searchbox");
  let focused = once(searchBox, "focus");

  panelWin.focus();
  synthesizeKeyFromKeyTag("nodeSearchKey", panelWin.document);

  yield focused;

  if (callback) {
    callback();
  }
});

/**
 * Get the MarkupContainer object instance that corresponds to the given
 * NodeFront
 * @param {NodeFront} nodeFront
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return {MarkupContainer}
 */
function getContainerForNodeFront(nodeFront, {markup}) {
  return markup.getContainer(nodeFront);
}

/**
 * Get the MarkupContainer object instance that corresponds to the given
 * selector
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return {MarkupContainer}
 */
let getContainerForSelector = Task.async(function*(selector, inspector) {
  info("Getting the markup-container for node " + selector);
  let nodeFront = yield getNodeFront(selector, inspector);
  let container = getContainerForNodeFront(nodeFront, inspector);
  info("Found markup-container " + container);
  return container;
});

/**
 * Simulate a mouse-over on the markup-container (a line in the markup-view)
 * that corresponds to the selector passed.
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return {Promise} Resolves when the container is hovered and the higlighter
 * is shown on the corresponding node
 */
let hoverContainer = Task.async(function*(selector, inspector) {
  info("Hovering over the markup-container for node " + selector);

  let nodeFront = yield getNodeFront(selector, inspector);
  let container = getContainerForNodeFront(nodeFront, inspector);

  let highlit = inspector.toolbox.once("node-highlight");
  EventUtils.synthesizeMouseAtCenter(container.tagLine, {type: "mousemove"},
    inspector.markup.doc.defaultView);
  return highlit;
});

/**
 * Simulate a click on the markup-container (a line in the markup-view)
 * that corresponds to the selector passed.
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return {Promise} Resolves when the node has been selected.
 */
let clickContainer = Task.async(function*(selector, inspector) {
  info("Clicking on the markup-container for node " + selector);

  let nodeFront = yield getNodeFront(selector, inspector);
  let container = getContainerForNodeFront(nodeFront, inspector);

  let updated = inspector.once("inspector-updated");
  EventUtils.synthesizeMouseAtCenter(container.tagLine, {type: "mousedown"},
    inspector.markup.doc.defaultView);
  EventUtils.synthesizeMouseAtCenter(container.tagLine, {type: "mouseup"},
    inspector.markup.doc.defaultView);
  return updated;
});

/**
 * Simulate the mouse leaving the markup-view area
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently loaded in the toolbox
 * @return a promise when done
 */
function mouseLeaveMarkupView(inspector) {
  info("Leaving the markup-view area");
  let def = promise.defer();

  // Find another element to mouseover over in order to leave the markup-view
  let btn = inspector.toolbox.doc.querySelector("#toolbox-controls");

  EventUtils.synthesizeMouseAtCenter(btn, {type: "mousemove"},
    inspector.toolbox.doc.defaultView);
  executeSoon(def.resolve);

  return def.promise;
}

/**
 * Wait for eventName on target.
 * @param {Object} target An observable object that either supports on/off or
 * addEventListener/removeEventListener
 * @param {String} eventName
 * @param {Boolean} useCapture Optional, for addEventListener/removeEventListener
 * @return A promise that resolves when the event has been handled
 */
function once(target, eventName, useCapture=false) {
  info("Waiting for event: '" + eventName + "' on " + target + ".");

  let deferred = promise.defer();

  for (let [add, remove] of [
    ["addEventListener", "removeEventListener"],
    ["addListener", "removeListener"],
    ["on", "off"]
  ]) {
    if ((add in target) && (remove in target)) {
      target[add](eventName, function onEvent(...aArgs) {
        info("Got event: '" + eventName + "' on " + target + ".");
        target[remove](eventName, onEvent, useCapture);
        deferred.resolve.apply(deferred, aArgs);
      }, useCapture);
      break;
    }
  }

  return deferred.promise;
}

/**
 * Undo the last markup-view action and wait for the corresponding mutation to
 * occur
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return a promise that resolves when the markup-mutation has been treated or
 * rejects if no undo action is possible
 */
function undoChange(inspector) {
  let canUndo = inspector.markup.undo.canUndo();
  ok(canUndo, "The last change in the markup-view can be undone");
  if (!canUndo) {
    return promise.reject();
  }

  let mutated = inspector.once("markupmutation");
  inspector.markup.undo.undo();
  return mutated;
}

/**
 * Redo the last markup-view action and wait for the corresponding mutation to
 * occur
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return a promise that resolves when the markup-mutation has been treated or
 * rejects if no redo action is possible
 */
function redoChange(inspector) {
  let canRedo = inspector.markup.undo.canRedo();
  ok(canRedo, "The last change in the markup-view can be redone");
  if (!canRedo) {
    return promise.reject();
  }

  let mutated = inspector.once("markupmutation");
  inspector.markup.undo.redo();
  return mutated;
}
