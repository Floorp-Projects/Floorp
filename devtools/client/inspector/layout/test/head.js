/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this);

Services.prefs.setIntPref("devtools.toolbox.footer.height", 350);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.toolbox.footer.height");
});

/**
 * Simple DOM node accesor function that takes either a node or a string css
 * selector as argument and returns the corresponding node
 * FIXME: Delete this function and use inspector/test/head.js' getNode instead,
 * and fix all layout-view tests to use nodeFronts instead of CPOWs.
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
 * Open the toolbox, with the inspector tool visible, and the layout-view
 * sidebar tab selected.
 * @return a promise that resolves when the inspector is ready and the layout
 * view is visible and ready
 */
function openLayoutView() {
  return openInspectorSidebarTab("layoutview").then(objects => {
    // The actual highligher show/hide methods are mocked in layoutview tests.
    // The highlighter is tested in devtools/inspector/test.
    function mockHighlighter({highlighter}) {
      highlighter.showBoxModel = function(nodeFront, options) {
        return promise.resolve();
      };
      highlighter.hideBoxModel = function() {
        return promise.resolve();
      };
    }
    mockHighlighter(objects.toolbox);

    return objects;
  });
}

/**
 * Wait for the layoutview-updated event.
 * @return a promise
 */
function waitForUpdate(inspector) {
  return inspector.once("layoutview-updated");
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
  TESTS.push([message, Task.async(func)]);
}

var runTests = Task.async(function*(...args) {
  for (let [message, test] of TESTS) {
    info("Running new test case: " + message);
    yield test.apply(null, args);
  }
});
