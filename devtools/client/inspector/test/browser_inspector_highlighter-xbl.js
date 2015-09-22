/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the picker works correctly with XBL anonymous nodes

const TEST_URL = TEST_URL_ROOT + "doc_inspector_highlighter_xbl.xul";

add_task(function*() {
  let {inspector, toolbox, testActor} = yield openInspectorForURL(TEST_URL);

  info("Starting element picker");
  yield toolbox.highlighterUtils.startPicker();

  info("Selecting the scale");
  yield moveMouseOver("#scale");
  yield doKeyPick({key: "VK_RETURN", options: {}});
  is(inspector.selection.nodeFront.className, "scale-slider",
     "The .scale-slider inside the scale was selected");

  function doKeyPick(msg) {
    info("Key pressed. Waiting for element to be picked");
    testActor.synthesizeKey(msg);
    return promise.all([
      toolbox.selection.once("new-node-front"),
      inspector.once("inspector-updated")
    ]);
  }

  function moveMouseOver(selector) {
    info("Waiting for element " + selector + " to be highlighted");
    testActor.synthesizeMouse({
      options: {type: "mousemove"},
      center: true,
      selector: selector
    });
    return inspector.toolbox.once("picker-node-hovered");
  }

});
