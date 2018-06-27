/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the picker works correctly with XBL anonymous nodes

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_xbl.xul";

add_task(async function() {
  const {inspector, toolbox, testActor} = await openInspectorForURL(TEST_URL);

  await startPicker(toolbox);

  info("Selecting the tree");
  await moveMouseOver("#tree");
  await doKeyPick({key: "VK_RETURN", options: {}});
  is(inspector.selection.nodeFront.className, "tree-bodybox",
     "The .tree-bodybox inside the tree was selected");

  function doKeyPick(msg) {
    info("Key pressed. Waiting for element to be picked");
    testActor.synthesizeKey(msg);
    return promise.all([
      inspector.selection.once("new-node-front"),
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
