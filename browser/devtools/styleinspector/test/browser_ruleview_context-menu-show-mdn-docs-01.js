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

const {setBaseCssDocsUrl} = require("devtools/shared/widgets/MdnDocsWidget");

/**
 * The test document tries to confuse the context menu
 * code by having a tag called "padding" and a property
 * value called "margin".
 */
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
  yield addTab("data:text/html;charset=utf8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("padding", inspector);
  yield testMdnContextMenuItemVisibility(view);
});

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
    view.styleDocument.popupNode = node;

    info("Updating context menu state");
    view._contextmenu._updateMenuItems();
    let isVisible = !view._contextmenu.menuitemShowMdnDocs.hidden;
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
