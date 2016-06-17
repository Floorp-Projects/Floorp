/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the keybindings for Picker work alright

const IS_OSX = Services.appinfo.OS === "Darwin";
const TEST_URL = URL_ROOT + "doc_inspector_highlighter_dom.html";

add_task(function* () {
  let {inspector, toolbox, testActor} = yield openInspectorForURL(TEST_URL);

  yield startPicker(toolbox);
  yield moveMouseOver("#another");

  info("Testing enter/return key as pick-node command");
  yield doKeyPick({key: "VK_RETURN", options: {}});
  is(inspector.selection.nodeFront.id, "another",
     "The #another node was selected. Passed.");

  info("Testing escape key as cancel-picker command");
  yield startPicker(toolbox);
  yield moveMouseOver("#ahoy");
  yield doKeyStop({key: "VK_ESCAPE", options: {}});
  is(inspector.selection.nodeFront.id, "another",
     "The #another DIV is still selected. Passed.");

  info("Testing Ctrl+Shift+C shortcut as cancel-picker command");
  yield startPicker(toolbox);
  yield moveMouseOver("#ahoy");
  let shortcutOpts = {key: "VK_C", options: {}};
  if (IS_OSX) {
    shortcutOpts.options.metaKey = true;
    shortcutOpts.options.altKey = true;
  } else {
    shortcutOpts.options.ctrlKey = true;
    shortcutOpts.options.shiftKey = true;
  }
  yield doKeyStop(shortcutOpts);
  is(inspector.selection.nodeFront.id, "another",
     "The #another DIV is still selected. Passed.");

  function doKeyPick(args) {
    info("Key pressed. Waiting for element to be picked");
    testActor.synthesizeKey(args);
    return promise.all([
      toolbox.selection.once("new-node-front"),
      inspector.once("inspector-updated")
    ]);
  }

  function doKeyStop(args) {
    info("Key pressed. Waiting for picker to be canceled");
    testActor.synthesizeKey(args);
    return inspector.toolbox.once("picker-stopped");
  }

  function moveMouseOver(selector) {
    info("Waiting for element " + selector + " to be highlighted");
    let onHighlighterReady = toolbox.once("highlighter-ready");
    let onPickerNodeHovered = inspector.toolbox.once("picker-node-hovered");
    testActor.synthesizeMouse({
      options: {type: "mousemove"},
      center: true,
      selector: selector
    });
    return promise.all([onHighlighterReady, onPickerNodeHovered]);
  }
});
