/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a node can be deleted from the markup-view with the delete key.
// Also checks that after deletion the correct element is highlighted.
// The next sibling is preferred, but the parent is a fallback.

const TEST_URL = "data:text/html,<div id='parent'><div id='first'></div><div id='second'></div><div id='third'></div></div>";

function* checkDeleteAndSelection(inspector, key, nodeSelector, focusedNodeSelector) {
  yield selectNode(nodeSelector, inspector);
  yield clickContainer(nodeSelector, inspector);

  info(`Deleting the element "${nodeSelector}" using the ${key} key`);
  let mutated = inspector.once("markupmutation");
  EventUtils.sendKey(key, inspector.panelWin);

  yield Promise.all([mutated, inspector.once("inspector-updated")]);

  let nodeFront = yield getNodeFront(focusedNodeSelector, inspector);
  is(inspector.selection.nodeFront, nodeFront,
    focusedNodeSelector + " should be selected after " + nodeSelector + " node gets deleted.");

  info("Checking that it's gone, baby gone!");
  ok(!content.document.querySelector(nodeSelector), "The test node does not exist");

  yield undoChange(inspector);
  ok(content.document.querySelector(nodeSelector), "The test node is back!");
}

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  info("Selecting the test node by clicking on it to make sure it receives focus");

  yield checkDeleteAndSelection(inspector, "delete", "#first", "#second");
  yield checkDeleteAndSelection(inspector, "delete", "#second", "#third");
  yield checkDeleteAndSelection(inspector, "delete", "#third", "#second");

  yield checkDeleteAndSelection(inspector, "back_space", "#first", "#second");
  yield checkDeleteAndSelection(inspector, "back_space", "#second", "#first");
  yield checkDeleteAndSelection(inspector, "back_space", "#third", "#second");

  // Removing the siblings of #first.
  let mutated = inspector.once("markupmutation");
  for (let node of content.document.querySelectorAll("#second, #third")) {
    node.remove();
  }
  yield mutated;
  // Testing with an only child.
  info("testing with an only child");
  yield checkDeleteAndSelection(inspector, "delete", "#first", "#parent");
  yield checkDeleteAndSelection(inspector, "back_space", "#first", "#parent");

  yield inspector.once("inspector-updated");
});
