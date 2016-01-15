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
  desc: "Pressing left again should stay on root <html>",
  key: "VK_LEFT",
  newSelection: "html"
}, {
  desc: "Pressing right should go down to <body>",
  key: "VK_RIGHT",
  newSelection: "body"
}, {
  desc: "Pressing right again should go down to #i2",
  key: "VK_RIGHT",
  newSelection: "#i2"
}, {
  desc: "Continue down to #i21",
  key: "VK_RIGHT",
  newSelection: "#i21"
}, {
  desc: "Continue down to #i211",
  key: "VK_RIGHT",
  newSelection: "#i211"
}, {
  desc: "Continue down to #i2111",
  key: "VK_RIGHT",
  newSelection: "#i2111"
}, {
  desc: "Pressing right once more should stay at leaf node #i2111",
  key: "VK_RIGHT",
  newSelection: "#i2111"
}, {
  desc: "Go back to #i211",
  key: "VK_LEFT",
  newSelection: "#i211"
}, {
  desc: "Go back to #i21",
  key: "VK_LEFT",
  newSelection: "#i21"
}, {
  desc: "Pressing down should move to next sibling #i22",
  key: "VK_DOWN",
  newSelection: "#i22"
}, {
  desc: "Pressing up should move to previous sibling #i21",
  key: "VK_UP",
  newSelection: "#i21"
}, {
  desc: "Pressing up again should stay on #i21 as there's no previous sibling",
  key: "VK_UP",
  newSelection: "#i21"
}, {
  desc: "Going back down to #i22",
  key: "VK_DOWN",
  newSelection: "#i22"
}, {
  desc: "Pressing down again should stay on #i22 as there's no next sibling",
  key: "VK_DOWN",
  newSelection: "#i22"
}];

add_task(function*() {
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

    let onUpdated;
    if (newSelection !== currentSelection) {
      info("Expecting a new node to be selected");
      onUpdated = inspector.once("breadcrumbs-updated");
    } else {
      info("Expecting the same node to remain selected");
      onUpdated = inspector.once("breadcrumbs-navigation-cancelled");
    }

    EventUtils.synthesizeKey(key, {});
    yield onUpdated;

    let newNodeFront = yield getNodeFront(newSelection, inspector);
    is(newNodeFront, inspector.selection.nodeFront,
       "The current selection is correct");

    currentSelection = newSelection;
  }
});
