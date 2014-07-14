/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const Cu = Components.utils;
let {gDevTools} = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let TargetFactory = devtools.TargetFactory;
let {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});
let {console} = Components.utils.import("resource://gre/modules/devtools/Console.jsm", {});

// All test are asynchronous
waitForExplicitFinish();

const TEST_URL_ROOT = "http://example.com/browser/browser/devtools/layoutview/test/";

// Uncomment to log events
// Services.prefs.setBoolPref("devtools.dump.emit", true);
// Services.prefs.setBoolPref("devtools.debugger.log", true);

// Set the testing flag on gDevTools and reset it when the test ends
gDevTools.testing = true;
registerCleanupFunction(() => gDevTools.testing = false);

// Clean-up all prefs that might have been changed during a test run
// (safer here because if the test fails, then the pref is never reverted)
Services.prefs.setIntPref("devtools.toolbox.footer.height", 350);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.dump.emit");
  Services.prefs.clearUserPref("devtools.debugger.log");
  Services.prefs.clearUserPref("devtools.toolbox.footer.height");
  Services.prefs.setCharPref("devtools.inspector.activeSidebar", "ruleview");
});

// Auto close the toolbox and close the test tabs when the test ends
registerCleanupFunction(() => {
  // For now, tests must call `yield destroyToolbox(inspector);` at the end.
  // This should normally be handled automatically here but some tests
  // are leaking when we do so (browser_editablemodel_border.js)
  // try {
  //   let target = TargetFactory.forTab(gBrowser.selectedTab);
  //   gDevTools.closeToolbox(target);
  // } catch (ex) {
  //   dump(ex);
  // }
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

/**
 * Define an async test based on a generator function
 */
function asyncTest(generator) {
  return () => Task.spawn(generator).then(null, ok.bind(null, false)).then(finish);
}

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when the url is loaded
 */
function addTab(url) {
  let def = promise.defer();

  let tab = gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    info("URL " + url + " loading complete into new test tab");
    waitForFocus(() => {
      def.resolve(tab);
    }, content);
  }, true);
  content.location = url;

  return def.promise;
}

/**
 * Destroy the toolbox
 * @param {InspectorPanel}
 * @return a promise that resolves when destroyed
 */
let destroyToolbox = Task.async(function*(inspector) {
  let onDestroyed = gDevTools.once("toolbox-destroyed");
  inspector._toolbox.destroy();
  yield onDestroyed;
});

/**
 * Simple DOM node accesor function that takes either a node or a string css
 * selector as argument and returns the corresponding node
 * @param {String|DOMNode} nodeOrSelector
 * @return {DOMNode}
 */
function getNode(nodeOrSelector) {
  return typeof nodeOrSelector === "string" ?
    content.document.querySelector(nodeOrSelector) :
    nodeOrSelector;
}

/**
 * Highlight a node and set the inspector's current selection to the node or
 * the first match of the given css selector.
 * @param {String|DOMNode} nodeOrSelector
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox
 * @return a promise that resolves when the inspector is updated with the new
 * node
 */
function selectAndHighlightNode(nodeOrSelector, inspector) {
  info("Highlighting and selecting the node " + nodeOrSelector);

  let node = getNode(nodeOrSelector);
  let updated = inspector.toolbox.once("highlighter-ready");
  inspector.selection.setNode(node, "test-highlight");
  return updated;

}

/**
 * Set the inspector's current selection to a node or to the first match of the
 * given css selector.
 * @param {String|DOMNode} nodeOrSelector
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox
 * @param {String} reason
 *        Defaults to "test" which instructs the inspector not to highlight the
 *        node upon selection
 * @return a promise that resolves when the inspector is updated with the new
 * node
 */
function selectNode(nodeOrSelector, inspector, reason="test") {
  info("Selecting the node " + nodeOrSelector);

  let node = getNode(nodeOrSelector);
  let updated = inspector.once("inspector-updated");
  inspector.selection.setNode(node, reason);
  return updated;
}

/**
 * Open the toolbox, with the inspector tool visible.
 * @return a promise that resolves when the inspector is ready
 */
let openInspector = Task.async(function*() {
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
      return {
        toolbox: toolbox,
        inspector: inspector
      };
    }
  }

  info("Opening the toolbox");
  toolbox = yield gDevTools.showToolbox(target, "inspector");
  yield waitForToolboxFrameFocus(toolbox);
  inspector = toolbox.getPanel("inspector");

  info("Waiting for the inspector to update");
  yield inspector.once("inspector-updated");

  return {
    toolbox: toolbox,
    inspector: inspector
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

/**
 * Checks whether the inspector's sidebar corresponding to the given id already
 * exists
 * @param {InspectorPanel}
 * @param {String}
 * @return {Boolean}
 */
function hasSideBarTab(inspector, id) {
  return !!inspector.sidebar.getWindowForTab(id);
}

/**
 * Open the toolbox, with the inspector tool visible, and the layout-view
 * sidebar tab selected.
 * @return a promise that resolves when the inspector is ready and the layout
 * view is visible and ready
 */
let openLayoutView = Task.async(function*() {
  let {toolbox, inspector} = yield openInspector();

  if (!hasSideBarTab(inspector, "layoutview")) {
    info("Waiting for the layoutview sidebar to be ready");
    yield inspector.sidebar.once("layoutview-ready");
  }

  info("Selecting the layoutview sidebar");
  inspector.sidebar.select("layoutview");

  return {
    toolbox: toolbox,
    inspector: inspector,
    view: inspector.sidebar.getWindowForTab("layoutview")["layoutview"]
  };
});

/**
 * Wait for the layoutview-updated event
 * @return a promise
 */
function waitForUpdate(inspector) {
  return inspector.once("layoutview-updated");
}

function getHighlighter() {
  return gBrowser.selectedBrowser.parentNode.querySelector(".highlighter-container");
}

function getBoxModelRoot() {
  let highlighter = getHighlighter();
  return highlighter.querySelector(".box-model-root");
}

function getGuideStatus(location) {
  let root = getBoxModelRoot();
  let guide = root.querySelector(".box-model-guide-" + location);

  return {
    visible: !guide.hasAttribute("hidden"),
    x1: guide.getAttribute("x1"),
    y1: guide.getAttribute("y1"),
    x2: guide.getAttribute("x2"),
    y2: guide.getAttribute("y2")
  };
}

/**
 * The addTest/runTests function couple provides a simple way to define
 * subsequent test cases in a test file. Example:
 *
 * addTest("what this test does", function*() {
 *   ... actual code for the test ...
 * });
 * addTest("what this second test does", function*() {
 *   ... actual code for the second test ...
 * });
 * runTests().then(...);
 */
var TESTS = [];

function addTest(message, func) {
  TESTS.push([message, Task.async(func)])
}

let runTests = Task.async(function*(...args) {
  for (let [message, test] of TESTS) {
    info("Running new test case: " + message);
    yield test.apply(null, args);
  }
});
