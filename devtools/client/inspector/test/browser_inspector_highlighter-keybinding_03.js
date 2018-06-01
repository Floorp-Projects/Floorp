/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the keybindings for Picker work alright

const IS_OSX = Services.appinfo.OS === "Darwin";
const TEST_URL = URL_ROOT + "doc_inspector_highlighter_dom.html";

add_task(async function() {
  const {inspector, toolbox, testActor} = await openInspectorForURL(TEST_URL);

  await startPicker(toolbox);
  await moveMouseOver("#another");

  info("Testing enter/return key as pick-node command");
  await doKeyPick({key: "VK_RETURN", options: {}});
  is(inspector.selection.nodeFront.id, "another",
     "The #another node was selected. Passed.");

  info("Testing escape key as cancel-picker command");
  await startPicker(toolbox);
  await moveMouseOver("#ahoy");
  await doKeyStop({key: "VK_ESCAPE", options: {}});
  is(inspector.selection.nodeFront.id, "another",
     "The #another DIV is still selected. Passed.");

  info("Testing Ctrl+Shift+C shortcut as cancel-picker command");
  await startPicker(toolbox);
  await moveMouseOver("#ahoy");
  const shortcutOpts = {key: "VK_C", options: {}};
  if (IS_OSX) {
    shortcutOpts.options.metaKey = true;
    shortcutOpts.options.altKey = true;
  } else {
    shortcutOpts.options.ctrlKey = true;
    shortcutOpts.options.shiftKey = true;
  }
  await doKeyStop(shortcutOpts);
  is(inspector.selection.nodeFront.id, "another",
     "The #another DIV is still selected. Passed.");

  function doKeyPick(args) {
    info("Key pressed. Waiting for element to be picked");
    testActor.synthesizeKey(args);
    return promise.all([
      inspector.selection.once("new-node-front"),
      inspector.once("inspector-updated"),
      inspector.toolbox.once("picker-stopped")
    ]);
  }

  function doKeyStop(args) {
    info("Key pressed. Waiting for picker to be canceled");
    testActor.synthesizeKey(args);
    return inspector.toolbox.once("picker-stopped");
  }

  function moveMouseOver(selector) {
    info("Waiting for element " + selector + " to be highlighted");
    const onHighlighterReady = toolbox.once("highlighter-ready");
    const onPickerNodeHovered = inspector.toolbox.once("picker-node-hovered");
    testActor.synthesizeMouse({
      options: {type: "mousemove"},
      center: true,
      selector: selector
    });
    return promise.all([onHighlighterReady, onPickerNodeHovered]);
  }
});
