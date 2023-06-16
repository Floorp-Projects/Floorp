/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the keybindings for Picker work alright

const IS_OSX = Services.appinfo.OS === "Darwin";
const TEST_URL = URL_ROOT + "doc_inspector_highlighter_dom.html";

add_task(async function () {
  const { inspector, toolbox } = await openInspectorForURL(TEST_URL);

  await startPicker(toolbox);
  await hoverElement(inspector, "#another");

  info("Testing enter/return key as pick-node command");
  await doKeyPick("VK_RETURN");
  is(
    inspector.selection.nodeFront.id,
    "another",
    "The #another node was selected. Passed."
  );

  info("Testing escape key as cancel-picker command");
  await startPicker(toolbox);
  await hoverElement(inspector, "#ahoy");
  await doKeyStop("VK_ESCAPE");
  is(
    inspector.selection.nodeFront.id,
    "another",
    "The #another DIV is still selected. Passed."
  );

  info("Testing Ctrl+Shift+C shortcut as cancel-picker command");
  await startPicker(toolbox);
  await hoverElement(inspector, "#ahoy");
  const eventOptions = { key: "VK_C" };
  if (IS_OSX) {
    eventOptions.metaKey = true;
    eventOptions.altKey = true;
  } else {
    eventOptions.ctrlKey = true;
    eventOptions.shiftKey = true;
  }
  await doKeyStop("VK_C", eventOptions);
  is(
    inspector.selection.nodeFront.id,
    "another",
    "The #another DIV is still selected. Passed."
  );

  function doKeyPick(key, options = {}) {
    info("Key pressed. Waiting for element to be picked");
    BrowserTestUtils.synthesizeKey(key, options, gBrowser.selectedBrowser);
    return Promise.all([
      inspector.selection.once("new-node-front"),
      inspector.once("inspector-updated"),
      toolbox.nodePicker.once("picker-stopped"),
    ]);
  }

  function doKeyStop(key, options = {}) {
    info("Key pressed. Waiting for picker to be canceled");
    BrowserTestUtils.synthesizeKey(key, options, gBrowser.selectedBrowser);
    return toolbox.nodePicker.once("picker-stopped");
  }
});
