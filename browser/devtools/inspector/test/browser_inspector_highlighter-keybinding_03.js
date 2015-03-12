/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the keybindings for Picker work alright

const TEST_URL = TEST_URL_ROOT + "doc_inspector_highlighter_dom.html";

add_task(function*() {
  let {inspector, toolbox} = yield openInspectorForURL(TEST_URL);

  info("Starting element picker");
  yield toolbox.highlighterUtils.startPicker();

  info("Selecting the simple-div1 DIV");
  yield moveMouseOver("#simple-div2");

  // Testing pick-node shortcut
  info("Testing enter/return key as pick-node command");
  yield doKeyPick({key: "VK_RETURN", options: {}});
  is(inspector.selection.nodeFront.id, "simple-div2", "The #simple-div2 node was selected. Passed.");

  // Testing cancel-picker command
  info("Starting element picker again");
  yield toolbox.highlighterUtils.startPicker();

  info("Selecting the simple-div1 DIV");
  yield moveMouseOver("#simple-div1");

  info("Testing escape key as cancel-picker command");
  yield doKeyStop({key: "VK_ESCAPE", options: {}});
  is(inspector.selection.nodeFront.id, "simple-div2", "The simple-div2 DIV is still selected. Passed.");

  function doKeyPick(msg) {
    info("Key pressed. Waiting for element to be picked");
    executeInContent("Test:SynthesizeKey", msg);
    return promise.all([
      toolbox.selection.once("new-node-front"),
      inspector.once("inspector-updated")
    ]);
  }

  function doKeyStop(msg) {
    info("Key pressed. Waiting for picker to be canceled");
    executeInContent("Test:SynthesizeKey", msg);
    return inspector.toolbox.once("picker-stopped");
  }

  function moveMouseOver(selector) {
    info("Waiting for element " + selector + " to be highlighted");
    executeInContent("Test:SynthesizeMouse", {
      options: {type: "mousemove"},
      center: true,
      selector: selector
    }, null, false);
    return inspector.toolbox.once("picker-node-hovered");
  }

});
