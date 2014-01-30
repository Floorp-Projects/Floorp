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
      deferred.resolve(inspector, toolbox);
    });
  }).then(null, console.error);

  return deferred.promise;
}

/**
 * Set the inspector's current selection to the first match of the given css
 * selector
 * @return a promise that resolves when the inspector is updated with the new
 * node
 */
function selectNode(selector, inspector) {
  let deferred = promise.defer();
  let node = content.document.querySelector(selector);
  ok(node, "A node was found for selector " + selector + ". Selecting it now");
  inspector.selection.setNode(node, "test");
  inspector.once("inspector-updated", () => {
    deferred.resolve(node);
  });
  return deferred.promise;
}
