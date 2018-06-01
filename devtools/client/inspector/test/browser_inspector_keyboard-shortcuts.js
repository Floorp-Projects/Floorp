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
  { key: "KEY_ArrowLeft", selectedNode: "p" },
  { key: "KEY_ArrowLeft", selectedNode: "body" },
  { key: "KEY_ArrowLeft", selectedNode: "html" },
  { key: "KEY_ArrowRight", selectedNode: "body" },
  { key: "KEY_ArrowRight", selectedNode: "p" },
  { key: "KEY_ArrowRight", selectedNode: "strong" },
];

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URI);

  info("Selecting the deepest element to start with");
  await selectNode("strong", inspector);

  const nodeFront = await getNodeFront("strong", inspector);
  is(inspector.selection.nodeFront, nodeFront,
     "<strong> should be selected initially");

  info("Focusing the currently active breadcrumb button");
  const bc = inspector.breadcrumbs;
  bc.nodeHierarchy[bc.currentIndex].button.focus();

  for (const { key, selectedNode } of TEST_DATA) {
    info("Pressing " + key + " to select " + selectedNode);

    const updated = inspector.once("inspector-updated");
    EventUtils.synthesizeKey(key);
    await updated;

    const selectedNodeFront = await getNodeFront(selectedNode, inspector);
    is(inspector.selection.nodeFront, selectedNodeFront,
      selectedNode + " is selected.");
  }
});
