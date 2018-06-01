/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the markup-view nodes can be navigated to with the keyboard

const TEST_URL = URL_ROOT + "doc_markup_navigation.html";
const TEST_DATA = [
  ["KEY_PageUp", "*doctype*"],
  ["KEY_ArrowDown", "html"],
  ["KEY_ArrowDown", "head"],
  ["KEY_ArrowDown", "body"],
  ["KEY_ArrowDown", "node0"],
  ["KEY_ArrowRight", "node0"],
  ["KEY_ArrowDown", "node1"],
  ["KEY_ArrowDown", "node2"],
  ["KEY_ArrowDown", "node3"],
  ["KEY_ArrowDown", "*comment*"],
  ["KEY_ArrowDown", "node4"],
  ["KEY_ArrowRight", "node4"],
  ["KEY_ArrowDown", "*text*"],
  ["KEY_ArrowDown", "node5"],
  ["KEY_ArrowDown", "*text*"],
  ["KEY_ArrowDown", "node6"],
  ["KEY_ArrowDown", "*text*"],
  ["KEY_ArrowDown", "*comment*"],
  ["KEY_ArrowDown", "node7"],
  ["KEY_ArrowRight", "node7"],
  ["KEY_ArrowDown", "*text*"],
  ["KEY_ArrowDown", "node8"],
  ["KEY_ArrowLeft", "node7"],
  ["KEY_ArrowLeft", "node7"],
  ["KEY_ArrowRight", "node7"],
  ["KEY_ArrowRight", "*text*"],
  ["KEY_ArrowDown", "node8"],
  ["KEY_ArrowDown", "*text*"],
  ["KEY_ArrowDown", "node9"],
  ["KEY_ArrowDown", "*text*"],
  ["KEY_ArrowDown", "node10"],
  ["KEY_ArrowDown", "*text*"],
  ["KEY_ArrowDown", "node11"],
  ["KEY_ArrowDown", "*text*"],
  ["KEY_ArrowDown", "node12"],
  ["KEY_ArrowRight", "node12"],
  ["KEY_ArrowDown", "*text*"],
  ["KEY_ArrowDown", "node13"],
  ["KEY_ArrowDown", "node14"],
  ["KEY_ArrowDown", "node15"],
  ["KEY_ArrowDown", "node15"],
  ["KEY_ArrowDown", "node15"],
  ["KEY_ArrowUp", "node14"],
  ["KEY_ArrowUp", "node13"],
  ["KEY_ArrowUp", "*text*"],
  ["KEY_ArrowUp", "node12"],
  ["KEY_ArrowLeft", "node12"],
  ["KEY_ArrowDown", "node14"],
  ["KEY_Home", "*doctype*"],
  ["KEY_PageDown", "*text*"],
  ["KEY_ArrowDown", "node5"],
  ["KEY_ArrowDown", "*text*"],
  ["KEY_ArrowDown", "node6"],
  ["KEY_ArrowDown", "*text*"],
  ["KEY_ArrowDown", "*comment*"],
  ["KEY_ArrowDown", "node7"],
  ["KEY_ArrowLeft", "node7"],
  ["KEY_ArrowDown", "*text*"],
  ["KEY_ArrowDown", "node9"],
  ["KEY_ArrowDown", "*text*"],
  ["KEY_ArrowDown", "node10"],
  ["KEY_PageUp", "*text*"],
  ["KEY_PageUp", "*doctype*"],
  ["KEY_ArrowDown", "html"],
  ["KEY_ArrowLeft", "html"],
  ["KEY_ArrowDown", "head"]
];

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);

  info("Making sure the markup-view frame is focused");
  inspector.markup._frame.focus();

  info("Starting to iterate through the test data");
  for (const [key, className] of TEST_DATA) {
    info("Testing step: " + key + " to navigate to " + className);
    EventUtils.synthesizeKey(key);

    info("Making sure markup-view children get updated");
    await waitForChildrenUpdated(inspector);

    info("Checking the right node is selected");
    checkSelectedNode(key, className, inspector);
  }

  // In theory, we should wait for the inspector-updated event at each iteration
  // of the previous loop where we expect the current node to change (because
  // changing the current node ends up refreshing the rule-view, breadcrumbs,
  // ...), but this would make this test a *lot* slower. Instead, having a final
  // catch-all event works too.
  await inspector.once("inspector-updated");
});

function checkSelectedNode(key, className, inspector) {
  const node = inspector.selection.nodeFront;

  if (className == "*comment*") {
    is(node.nodeType, Node.COMMENT_NODE,
       "Found a comment after pressing " + key);
  } else if (className == "*text*") {
    is(node.nodeType, Node.TEXT_NODE,
       "Found text after pressing " + key);
  } else if (className == "*doctype*") {
    is(node.nodeType, Node.DOCUMENT_TYPE_NODE,
       "Found the doctype after pressing " + key);
  } else {
    is(node.className, className,
       "Found node: " + className + " after pressing " + key);
  }
}
