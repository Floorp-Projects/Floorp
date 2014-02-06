/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;

let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let TargetFactory = devtools.TargetFactory;
let {console} = Cu.import("resource://gre/modules/devtools/Console.jsm", {});
let promise = devtools.require("sdk/core/promise");

// Clear preferences that may be set during the course of tests.
function clearUserPrefs() {
  Services.prefs.clearUserPref("devtools.inspector.htmlPanelOpen");
  Services.prefs.clearUserPref("devtools.inspector.sidebarOpen");
  Services.prefs.clearUserPref("devtools.inspector.activeSidebar");
}

registerCleanupFunction(clearUserPrefs);

Services.prefs.setBoolPref("devtools.debugger.log", true);
SimpleTest.registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.debugger.log");
});

function getContainerForRawNode(markupView, rawNode) {
  let front = markupView.walker.frontForRawNode(rawNode);
  let container = markupView.getContainer(front);
  return container;
}

/**
 * Open the toolbox, with the inspector tool visible.
 * @return a promise that resolves when the inspector is ready
 */
function openInspector() {
  let deferred = promise.defer();

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    let inspector = toolbox.getCurrentPanel();
    inspector.once("inspector-updated", () => {
      deferred.resolve({toolbox: toolbox, inspector: inspector});
    });
  }).then(null, console.error);

  return deferred.promise;
}

function getNode(nodeOrSelector) {
  let node = nodeOrSelector;

  if (typeof nodeOrSelector === "string") {
    node = content.document.querySelector(nodeOrSelector);
    ok(node, "A node was found for selector " + nodeOrSelector);
  }

  return node;
}

/**
 * Set the inspector's current selection to a node or to the first match of the
 * given css selector
 * @return a promise that resolves when the inspector is updated with the new
 * node
 */
function selectNode(nodeOrSelector, inspector) {
  let node = getNode(nodeOrSelector);
  let updated = inspector.once("inspector-updated");
  inspector.selection.setNode(node, "test");
  return updated;
}

/**
 * Simulate a mouse-over on the markup-container (a line in the markup-view)
 * that corresponds to the node or selector passed.
 * @return a promise that resolves when the container is hovered and the higlighter
 * is shown on the corresponding node
 */
function hoverContainer(nodeOrSelector, inspector) {
  let highlit = inspector.toolbox.once("node-highlight");
  let container = getContainerForRawNode(inspector.markup, getNode(nodeOrSelector));
  EventUtils.synthesizeMouse(container.tagLine, 2, 2, {type: "mousemove"},
    inspector.markup.doc.defaultView);
  return highlit;
}

/**
 * Simulate a click on the markup-container (a line in the markup-view)
 * that corresponds to the node or selector passed.
 * @return a promise that resolves when the node has been selected.
 */
function clickContainer(nodeOrSelector, inspector) {
  let updated = inspector.once("inspector-updated");
  let container = getContainerForRawNode(inspector.markup, getNode(nodeOrSelector));
  EventUtils.synthesizeMouseAtCenter(container.tagLine, {type: "mousedown"},
    inspector.markup.doc.defaultView);
  EventUtils.synthesizeMouseAtCenter(container.tagLine, {type: "mouseup"},
    inspector.markup.doc.defaultView);
  return updated;
}

/**
 * Checks if the highlighter is visible currently
 */
function isHighlighterVisible() {
  let outline = gBrowser.selectedBrowser.parentNode.querySelector(".highlighter-container .highlighter-outline");
  return outline && !outline.hasAttribute("hidden");
}

/**
 * Simulate the mouse leaving the markup-view area
 * @return a promise when done
 */
function mouseLeaveMarkupView(inspector) {
  let deferred = promise.defer();

  // Find another element to mouseover over in order to leave the markup-view
  let btn = inspector.toolbox.doc.querySelector(".toolbox-dock-button");

  EventUtils.synthesizeMouse(btn, 2, 2, {type: "mousemove"},
    inspector.toolbox.doc.defaultView);
  executeSoon(deferred.resolve);

  return deferred.promise;
}

