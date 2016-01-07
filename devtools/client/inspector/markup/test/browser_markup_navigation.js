/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the markup-view nodes can be navigated to with the keyboard

const TEST_URL = TEST_URL_ROOT + "doc_markup_navigation.html";
const TEST_DATA = [
  ["pageup", "*doctype*"],
  ["down", "html"],
  ["down", "head"],
  ["down", "body"],
  ["down", "node0"],
  ["right", "node0"],
  ["down", "node1"],
  ["down", "node2"],
  ["down", "node3"],
  ["down", "*comment*"],
  ["down", "node4"],
  ["right", "node4"],
  ["down", "*text*"],
  ["down", "node5"],
  ["down", "node6"],
  ["down", "*comment*"],
  ["down" , "node7"],
  ["right", "node7"],
  ["down", "*text*"],
  ["down", "node8"],
  ["left", "node7"],
  ["left", "node7"],
  ["right", "node7"],
  ["right", "*text*"],
  ["down", "node8"],
  ["down", "node9"],
  ["down", "node10"],
  ["down", "node11"],
  ["down", "node12"],
  ["right", "node12"],
  ["down", "*text*"],
  ["down", "node13"],
  ["down", "node14"],
  ["down", "node15"],
  ["down", "node15"],
  ["down", "node15"],
  ["up", "node14"],
  ["up", "node13"],
  ["up", "*text*"],
  ["up", "node12"],
  ["left", "node12"],
  ["down", "node14"],
  ["home", "*doctype*"],
  ["pagedown", "*text*"],
  ["down", "node5"],
  ["down", "node6"],
  ["down", "*comment*"],
  ["down", "node7"],
  ["left", "node7"],
  ["down", "node9"],
  ["down", "node10"],
  ["pageup", "node2"],
  ["pageup", "*doctype*"],
  ["down", "html"],
  ["left", "html"],
  ["down", "head"]
];

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  info("Making sure the markup-view frame is focused");
  inspector.markup._frame.focus();

  info("Starting to iterate through the test data");
  for (let [key, className] of TEST_DATA) {
    info("Testing step: " + key + " to navigate to " + className);
    pressKey(key);

    info("Making sure markup-view children get updated");
    yield waitForChildrenUpdated(inspector);

    info("Checking the right node is selected");
    checkSelectedNode(key, className, inspector);
  }
});

function pressKey(key) {
  switch(key) {
    case "right":
      EventUtils.synthesizeKey("VK_RIGHT", {});
      break;
    case "down":
      EventUtils.synthesizeKey("VK_DOWN", {});
      break;
    case "left":
      EventUtils.synthesizeKey("VK_LEFT", {});
      break;
    case "up":
      EventUtils.synthesizeKey("VK_UP", {});
      break;
    case "pageup":
      EventUtils.synthesizeKey("VK_PAGE_UP", {});
      break;
    case "pagedown":
      EventUtils.synthesizeKey("VK_PAGE_DOWN", {});
      break;
    case "home":
      EventUtils.synthesizeKey("VK_HOME", {});
      break;
  }
}

function checkSelectedNode(key, className, inspector) {
  let node = inspector.selection.nodeFront;

  if (className == "*comment*") {
    is(node.nodeType, Node.COMMENT_NODE, "Found a comment after pressing " + key);
  } else if (className == "*text*") {
    is(node.nodeType, Node.TEXT_NODE, "Found text after pressing " + key);
  } else if (className == "*doctype*") {
    is(node.nodeType, Node.DOCUMENT_TYPE_NODE, "Found the doctype after pressing " + key);
  } else {
    is(node.className, className, "Found node: " + className + " after pressing " + key);
  }
}
