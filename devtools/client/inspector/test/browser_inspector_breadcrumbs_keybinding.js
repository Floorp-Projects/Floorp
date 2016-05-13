/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the breadcrumbs keybindings work.

const TEST_URI = URL_ROOT + "doc_inspector_breadcrumbs.html";
const TEST_DATA = [{
  desc: "Pressing left should select the parent <body>",
  key: "VK_LEFT",
  newSelection: "body"
}, {
  desc: "Pressing left again should select the parent <html>",
  key: "VK_LEFT",
  newSelection: "html"
}, {
  desc: "Pressing left again should stay on <html>, it's the first element",
  key: "VK_LEFT",
  newSelection: "html"
}, {
  desc: "Pressing right should go to <body>",
  key: "VK_RIGHT",
  newSelection: "body"
}, {
  desc: "Pressing right again should go to #i2",
  key: "VK_RIGHT",
  newSelection: "#i2"
}, {
  desc: "Pressing right again should stay on #i2, it's the last element",
  key: "VK_RIGHT",
  newSelection: "#i2"
}];

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URI);

  info("Selecting the test node");
  yield selectNode("#i2", inspector);

  info("Clicking on the corresponding breadcrumbs node to focus it");
  let container = inspector.panelDoc.getElementById("inspector-breadcrumbs");

  let button = container.querySelector("button[checked]");
  button.click();

  let currentSelection = "#id2";
  for (let {desc, key, newSelection} of TEST_DATA) {
    info(desc);

    // If the selection will change, wait for the breadcrumb to update,
    // otherwise continue.
    let onUpdated = null;
    if (newSelection !== currentSelection) {
      info("Expecting a new node to be selected");
      onUpdated = inspector.once("breadcrumbs-updated");
    }

    EventUtils.synthesizeKey(key, {});
    yield onUpdated;

    let newNodeFront = yield getNodeFront(newSelection, inspector);
    is(newNodeFront, inspector.selection.nodeFront,
       "The current selection is correct");

    currentSelection = newSelection;
  }
});
