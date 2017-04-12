/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// After deleting a node, whitespace siblings that had an impact on the layout might no
// longer have any impact. This tests that the markup view is correctly rendered after
// deleting a node that triggers such a change.

const HTML =
  `<div>
    <p id="container">
      <span id="before-whitespace">1</span>      <span id="after-whitespace">2</span>
    </p>
  </div>`;

const TEST_URL = "data:text/html;charset=utf-8," + encodeURIComponent(HTML);

add_task(function* deleteNodeAfterWhitespace() {
  let {inspector} = yield openInspectorForURL(TEST_URL);

  info("Test deleting a node that will modify the whitespace nodes rendered in the " +
    "markup view.");

  yield selectAndFocusNode("#after-whitespace", inspector);
  yield deleteCurrentSelection(inspector);

  // TODO: There is still an issue with selection here.  When the span is deleted, the
  // selection goes to text-node. But since the text-node gets removed from the markup
  // view after losing its impact on the layout, the selection remains on a node which
  // is no longer part of the markup view (but still a valid node in the content DOM).
  let parentNodeFront = yield inspector.selection.nodeFront.parentNode();
  let nodeFront = yield getNodeFront("#container", inspector);
  is(parentNodeFront, nodeFront, "Selection is as expected after deletion");

  info("Check that the node was really removed");
  let node = yield getNodeFront("#after-whitespace", inspector);
  ok(!node, "The node can't be found in the page anymore");

  info("Undo the deletion to restore the original markup");
  yield undoChange(inspector);
  node = yield getNodeFront("#after-whitespace", inspector);
  ok(node, "The node is back");

  info("Test deleting the node before the whitespace and performing an undo preserves " +
       "the node order");

  yield selectAndFocusNode("#before-whitespace", inspector);
  yield deleteCurrentSelection(inspector);

  info("Undo the deletion to restore the original markup");
  yield undoChange(inspector);
  node = yield getNodeFront("#before-whitespace", inspector);
  ok(node, "The node is back");

  let nextSibling = yield getNodeFront("#before-whitespace + *", inspector);
  let afterWhitespace = yield getNodeFront("#after-whitespace", inspector);
  is(nextSibling, afterWhitespace, "Order has been preserved after restoring the node");
});

function* selectAndFocusNode(selector, inspector) {
  info(`Select node ${selector} and make sure it is focused`);
  yield selectNode(selector, inspector);
  yield clickContainer(selector, inspector);
}

function* deleteCurrentSelection(inspector) {
  info("Delete the node with the delete key");
  let mutated = inspector.once("markupmutation");
  EventUtils.sendKey("delete", inspector.panelWin);
  yield Promise.all([mutated, inspector.once("inspector-updated")]);
}
