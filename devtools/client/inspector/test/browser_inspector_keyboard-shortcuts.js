/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests that the keybindings for highlighting different elements work as
// intended.

const TEST_URI = "data:text/html;charset=utf-8," +
  "<html><head><title>Test for the highlighter keybindings</title></head>" +
  "<body><p><strong>Greetings, earthlings!</strong>" +
  " I come in peace.</p></body></html>";

const TEST_DATA = [
  { key: "VK_LEFT", selectedNode: "p" },
  { key: "VK_LEFT", selectedNode: "body" },
  { key: "VK_LEFT", selectedNode: "html" },
  { key: "VK_RIGHT", selectedNode: "body" },
  { key: "VK_RIGHT", selectedNode: "p" },
  { key: "VK_RIGHT", selectedNode: "strong" },
];

add_task(function* () {
  let { inspector } = yield openInspectorForURL(TEST_URI);

  info("Selecting the deepest element to start with");
  yield selectNode("strong", inspector);

  let nodeFront = yield getNodeFront("strong", inspector);
  is(inspector.selection.nodeFront, nodeFront,
     "<strong> should be selected initially");

  info("Focusing the currently active breadcrumb button");
  let bc = inspector.breadcrumbs;
  bc.nodeHierarchy[bc.currentIndex].button.focus();

  for (let { key, selectedNode } of TEST_DATA) {
    info("Pressing " + key + " to select " + selectedNode);

    let updated = inspector.once("inspector-updated");
    EventUtils.synthesizeKey(key, {});
    yield updated;

    let selectedNodeFront = yield getNodeFront(selectedNode, inspector);
    is(inspector.selection.nodeFront, selectedNodeFront,
      selectedNode + " is selected.");
  }
});
