/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../../framework/test/shared-head.js */
/* import-globals-from ../../test/head.js */
"use strict";

// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this);

Services.prefs.setBoolPref("devtools.layoutview.enabled", true);
Services.prefs.setIntPref("devtools.toolbox.footer.height", 350);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.layoutview.enabled");
  Services.prefs.clearUserPref("devtools.toolbox.footer.height");
});

/**
 * Highlight a node and set the inspector's current selection to the node or
 * the first match of the given css selector.
 *
 * @param  {String|NodeFront} selectorOrNodeFront
 *         The selector for the node to be set, or the nodeFront.
 * @param  {InspectorPanel} inspector
 *         The instance of InspectorPanel currently loaded in the toolbox.
 * @return {Promise} a promise that resolves when the inspector is updated with the new
 *         node.
 */
function* selectAndHighlightNode(selectorOrNodeFront, inspector) {
  info("Highlighting and selecting the node " + selectorOrNodeFront);

  let nodeFront = yield getNodeFront(selectorOrNodeFront, inspector);
  let updated = inspector.toolbox.once("highlighter-ready");
  inspector.selection.setNodeFront(nodeFront, "test-highlight");
  yield updated;
}

/**
 * Open the toolbox, with the inspector tool visible, and the computed view
 * sidebar tab selected to display the box model view.
 *
 * @return {Promise} a promise that resolves when the inspector is ready and the box model
 *         view is visible and ready.
 */
function openBoxModelView() {
  return openInspectorSidebarTab("computedview").then(data => {
    // The actual highligher show/hide methods are mocked in box model tests.
    // The highlighter is tested in devtools/inspector/test.
    function mockHighlighter({highlighter}) {
      highlighter.showBoxModel = function () {
        return promise.resolve();
      };
      highlighter.hideBoxModel = function () {
        return promise.resolve();
      };
    }
    mockHighlighter(data.toolbox);

    return {
      toolbox: data.toolbox,
      inspector: data.inspector,
      view: data.inspector.getPanel("computedview"),
      testActor: data.testActor
    };
  });
}

/**
 * Wait for the boxmodel-view-updated event.
 *
 * @param  {InspectorPanel} inspector
 *         The instance of InspectorPanel currently loaded in the toolbox.
 * @param  {Boolean} waitForSelectionUpdate
 *         Should the boxmodel-view-updated event come from a new selection.
 * @return {Promise} a promise
 */
function waitForUpdate(inspector, waitForSelectionUpdate) {
  return new Promise(resolve => {
    inspector.on("boxmodel-view-updated", function onUpdate(e, reasons) {
      // Wait for another update event if we are waiting for a selection related event.
      if (waitForSelectionUpdate && !reasons.includes("new-selection")) {
        return;
      }

      inspector.off("boxmodel-view-updated", onUpdate);
      resolve();
    });
  });
}

/**
 * Wait for both boxmode-view-updated and markuploaded events.
 *
 * @return {Promise} a promise that resolves when both events have been received.
 */
function waitForMarkupLoaded(inspector) {
  return Promise.all([
    waitForUpdate(inspector),
    inspector.once("markuploaded"),
  ]);
}

function getStyle(testActor, selector, propertyName) {
  return testActor.eval(`
    content.document.querySelector("${selector}")
                    .style.getPropertyValue("${propertyName}");
  `);
}

function setStyle(testActor, selector, propertyName, value) {
  return testActor.eval(`
    content.document.querySelector("${selector}")
                    .style.${propertyName} = "${value}";
  `);
}

/**
 * The box model doesn't participate in the inspector's update mechanism, so simply
 * calling the default selectNode isn't enough to guarantee that the box model view has
 * finished updating. We also need to wait for the "boxmodel-view-updated" event.
 */
var _selectNode = selectNode;
selectNode = function* (node, inspector, reason) {
  let onUpdate = waitForUpdate(inspector, true);
  yield _selectNode(node, inspector, reason);
  yield onUpdate;
};
