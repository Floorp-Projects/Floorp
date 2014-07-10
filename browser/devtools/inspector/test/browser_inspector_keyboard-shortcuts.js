/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests that the keybindings for highlighting different elements work as
// intended.

const TEST_URI = "data:text/html;charset=utf-8," +
  "<html><head><title>Test for the highlighter keybindings</title></head>" +
  "<body><h1>Hello</h1><p><strong>Greetings, earthlings!</strong>" +
  " I come in peace.</p></body></html>";

const TEST_DATA = [
  { key: "VK_RIGHT", selectedNode: "h1" },
  { key: "VK_DOWN", selectedNode: "p" },
  { key: "VK_UP", selectedNode: "h1" },
  { key: "VK_LEFT", selectedNode: "body" },
];

let test = asyncTest(function* () {
  let { inspector } = yield openInspectorForURL(TEST_URI);
  is(inspector.selection.node, getNode("body"),
    "Body should be selected initially.");

  info("Focusing the currently active breadcrumb button");
  let bc = inspector.breadcrumbs;
  bc.nodeHierarchy[bc.currentIndex].button.focus();

  for (let { key, selectedNode } of TEST_DATA) {
    info("Pressing " + key + " to select " + selectedNode);

    let updated = inspector.once("inspector-updated");
    EventUtils.synthesizeKey(key, {});
    yield updated;

    is(inspector.selection.node, getNode(selectedNode),
      selectedNode + " is selected.");
  }
});
