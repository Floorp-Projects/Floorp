/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* globals registerTestActor, getTestActor, Task, openToolboxForTab, gBrowser */

// This file contains functions related to the inspector that are also of interest to
// other test directores as well.

/**
 * Open the toolbox, with the inspector tool visible.
 * @param {String} hostType Optional hostType, as defined in Toolbox.HostType
 * @return a promise that resolves when the inspector is ready
 */
var openInspector = Task.async(function* (hostType) {
  info("Opening the inspector");

  let toolbox = yield openToolboxForTab(gBrowser.selectedTab, "inspector",
                                        hostType);
  let inspector = toolbox.getPanel("inspector");

  if (inspector._updateProgress) {
    info("Need to wait for the inspector to update");
    yield inspector.once("inspector-updated");
  }

  info("Waiting for actor features to be detected");
  yield inspector._detectingActorFeatures;

  yield registerTestActor(toolbox.target.client);
  let testActor = yield getTestActor(toolbox);

  return {toolbox, inspector, testActor};
});

/**
 * Open the toolbox, with the inspector tool visible, and the one of the sidebar
 * tabs selected.
 *
 * @param {String} id
 *        The ID of the sidebar tab to be opened
 * @return a promise that resolves when the inspector is ready and the tab is
 * visible and ready
 */
var openInspectorSidebarTab = Task.async(function* (id) {
  let {toolbox, inspector, testActor} = yield openInspector();

  info("Selecting the " + id + " sidebar");
  inspector.sidebar.select(id);

  return {
    toolbox,
    inspector,
    testActor
  };
});

/**
 * Open the toolbox, with the inspector tool visible, and the rule-view
 * sidebar tab selected.
 *
 * @return a promise that resolves when the inspector is ready and the rule view
 * is visible and ready
 */
function openRuleView() {
  return openInspectorSidebarTab("ruleview").then(data => {
    // Replace the view to use a custom throttle function that can be triggered manually
    // through an additional ".flush()" property.
    data.inspector.ruleview.view.throttle = manualThrottle();

    return {
      toolbox: data.toolbox,
      inspector: data.inspector,
      testActor: data.testActor,
      view: data.inspector.ruleview.view
    };
  });
}

/**
 * Open the toolbox, with the inspector tool visible, and the computed-view
 * sidebar tab selected.
 *
 * @return a promise that resolves when the inspector is ready and the computed
 * view is visible and ready
 */
function openComputedView() {
  return openInspectorSidebarTab("computedview").then(data => {
    return {
      toolbox: data.toolbox,
      inspector: data.inspector,
      testActor: data.testActor,
      view: data.inspector.computedview.computedView
    };
  });
}

/**
 * Select the rule view sidebar tab on an already opened inspector panel.
 *
 * @param {InspectorPanel} inspector
 *        The opened inspector panel
 * @return {CssRuleView} the rule view
 */
function selectRuleView(inspector) {
  inspector.sidebar.select("ruleview");
  return inspector.ruleview.view;
}

/**
 * Select the computed view sidebar tab on an already opened inspector panel.
 *
 * @param {InspectorPanel} inspector
 *        The opened inspector panel
 * @return {CssComputedView} the computed view
 */
function selectComputedView(inspector) {
  inspector.sidebar.select("computedview");
  return inspector.computedview.computedView;
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
 * Set the inspector's current selection to the first match of the given css
 * selector
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @param {String} reason Defaults to "test" which instructs the inspector not
 * to highlight the node upon selection
 * @return {Promise} Resolves when the inspector is updated with the new node
 */
var selectNode = Task.async(function* (selector, inspector, reason = "test") {
  info("Selecting the node for '" + selector + "'");
  let nodeFront = yield getNodeFront(selector, inspector);
  let updated = inspector.once("inspector-updated");
  inspector.selection.setNodeFront(nodeFront, reason);
  yield updated;
});

/**
 * Create a throttling function that can be manually "flushed". This is to replace the
 * use of the `throttle` function from `devtools/client/inspector/shared/utils.js`, which
 * has a setTimeout that can cause intermittents.
 * @return {Function} This function has the same function signature as throttle, but
 *                    the property `.flush()` has been added for flushing out any
 *                    throttled calls.
 */
function manualThrottle() {
  let calls = [];

  function throttle(func, wait, scope) {
    return function () {
      let existingCall = calls.find(call => call.func === func);
      if (existingCall) {
        existingCall.args = arguments;
      } else {
        calls.push({ func, wait, scope, args: arguments });
      }
    };
  }

  throttle.flush = function () {
    calls.forEach(({func, scope, args}) => func.apply(scope, args));
    calls = [];
  };

  return throttle;
}
