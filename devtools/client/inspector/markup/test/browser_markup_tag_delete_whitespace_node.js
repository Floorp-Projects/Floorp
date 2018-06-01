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

add_task(async function deleteNodeAfterWhitespace() {
  const {inspector} = await openInspectorForURL(TEST_URL);

  info("Test deleting a node that will modify the whitespace nodes rendered in the " +
    "markup view.");

  await selectAndFocusNode("#after-whitespace", inspector);
  await deleteCurrentSelection(inspector);

  // TODO: There is still an issue with selection here.  When the span is deleted, the
  // selection goes to text-node. But since the text-node gets removed from the markup
  // view after losing its impact on the layout, the selection remains on a node which
  // is no longer part of the markup view (but still a valid node in the content DOM).
  const parentNodeFront = await inspector.selection.nodeFront.parentNode();
  const nodeFront = await getNodeFront("#container", inspector);
  is(parentNodeFront, nodeFront, "Selection is as expected after deletion");

  info("Check that the node was really removed");
  let node = await getNodeFront("#after-whitespace", inspector);
  ok(!node, "The node can't be found in the page anymore");

  info("Undo the deletion to restore the original markup");
  await undoChange(inspector);
  node = await getNodeFront("#after-whitespace", inspector);
  ok(node, "The node is back");

  info("Test deleting the node before the whitespace and performing an undo preserves " +
       "the node order");

  await selectAndFocusNode("#before-whitespace", inspector);
  await deleteCurrentSelection(inspector);

  info("Undo the deletion to restore the original markup");
  await undoChange(inspector);
  node = await getNodeFront("#before-whitespace", inspector);
  ok(node, "The node is back");

  const nextSibling = await getNodeFront("#before-whitespace + *", inspector);
  const afterWhitespace = await getNodeFront("#after-whitespace", inspector);
  is(nextSibling, afterWhitespace, "Order has been preserved after restoring the node");
});

async function selectAndFocusNode(selector, inspector) {
  info(`Select node ${selector} and make sure it is focused`);
  await selectNode(selector, inspector);
  await clickContainer(selector, inspector);
}

async function deleteCurrentSelection(inspector) {
  info("Delete the node with the delete key");
  const mutated = inspector.once("markupmutation");
  EventUtils.sendKey("delete", inspector.panelWin);
  await Promise.all([mutated, inspector.once("inspector-updated")]);
}
