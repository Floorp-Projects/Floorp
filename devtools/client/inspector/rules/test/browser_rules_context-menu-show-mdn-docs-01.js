/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the code that integrates the Style Inspector's rule view
 * with the MDN docs tooltip.
 *
 * If you display the context click on a property name in the rule view, you
 * should see a menu item "Show MDN Docs". If you click that item, the MDN
 * docs tooltip should be shown, containing docs from MDN for that property.
 *
 * This file tests that the context menu item is shown when it should be
 * shown and hidden when it should be hidden.
 */

"use strict";

/**
 * The test document tries to confuse the context menu
 * code by having a tag called "padding" and a property
 * value called "margin".
 */

const { PrefObserver } = require("devtools/client/shared/prefs");
const PREF_ENABLE_MDN_DOCS_TOOLTIP =
  "devtools.inspector.mdnDocsTooltip.enabled";

const TEST_URI = `
  <html>
    <head>
      <style>
        padding {font-family: margin;}
      </style>
    </head>

    <body>
      <padding>MDN tooltip testing</padding>
    </body>
  </html>
`;

add_task(function* () {
  info("Ensure the pref is true to begin with");
  let initial = Services.prefs.getBoolPref(PREF_ENABLE_MDN_DOCS_TOOLTIP);
  if (initial != true) {
    yield setBooleanPref(PREF_ENABLE_MDN_DOCS_TOOLTIP, true);
  }

  yield addTab("data:text/html;charset=utf8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("padding", inspector);
  yield testMdnContextMenuItemVisibility(view);

  info("Ensure the pref is reset to its initial value");
  let eventual = Services.prefs.getBoolPref(PREF_ENABLE_MDN_DOCS_TOOLTIP);
  if (eventual != initial) {
    yield setBooleanPref(PREF_ENABLE_MDN_DOCS_TOOLTIP, initial);
  }
});

/**
 * Set a boolean pref, and wait for the pref observer to
 * trigger, so that code listening for the pref change
 * has had a chance to update itself.
 *
 * @param pref {string} Name of the pref to change
 * @param state {boolean} Desired value of the pref.
 *
 * Note that if the pref already has the value in `state`,
 * then the prefObserver will not trigger. So you should only
 * call this function if you know the pref's current value is
 * not `state`.
 */
function* setBooleanPref(pref, state) {
  let prefObserver = new PrefObserver("devtools.");
  let oncePrefChanged = new Promise(resolve => {
    prefObserver.on(pref, onPrefChanged);

    function onPrefChanged() {
      prefObserver.off(pref, onPrefChanged);
      resolve();
    }
  });

  info("Set the pref " + pref + " to: " + state);
  Services.prefs.setBoolPref(pref, state);

  info("Wait for prefObserver to call back so the UI can update");
  yield oncePrefChanged;
}

/**
 * Tests that the MDN context menu item is shown when it should be,
 * and hidden when it should be.
 *   - iterate through every node in the rule view
 *   - set that node as popupNode (the node that the context menu
 *   is shown for)
 *   - update the context menu's state
 *   - test that the MDN context menu item is hidden, or not,
 *   depending on popupNode
 */
function* testMdnContextMenuItemVisibility(view) {
  info("Test that MDN context menu item is shown only when it should be.");

  let root = rootElement(view);
  for (let node of iterateNodes(root)) {
    info("Setting " + node + " as popupNode");
    info("Creating context menu with " + node + " as popupNode");
    let allMenuItems = openStyleContextMenuAndGetAllItems(view, node);
    let menuitemShowMdnDocs = allMenuItems.find(item => item.label ===
      STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.showMdnDocs"));

    let isVisible = menuitemShowMdnDocs.visible;
    let shouldBeVisible = isPropertyNameNode(node);
    let message = shouldBeVisible ? "shown" : "hidden";
    is(isVisible, shouldBeVisible,
       "The MDN context menu item is " + message + " ; content : " +
       node.textContent + " ; type : " + node.nodeType);
  }
}

/**
 * Check if a node is a property name.
 */
function isPropertyNameNode(node) {
  return node.textContent === "font-family";
}

/**
 * A generator that iterates recursively through all child nodes of baseNode.
 */
function* iterateNodes(baseNode) {
  yield baseNode;

  for (let child of baseNode.childNodes) {
    yield* iterateNodes(child);
  }
}

/**
 * Returns the root element for the rule view.
 */
var rootElement = view => (view.element) ? view.element : view.styleDocument;
