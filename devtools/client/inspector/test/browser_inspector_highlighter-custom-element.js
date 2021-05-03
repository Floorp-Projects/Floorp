/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the picker works correctly with XBL anonymous nodes

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_custom_element.xhtml";

add_task(async function() {
  const { inspector, toolbox, testActor } = await openInspectorForURL(TEST_URL);

  await startPicker(toolbox);

  info("Selecting the custom element");
  await hoverElement(inspector, "#custom-element");
  await doKeyPick({ key: "VK_RETURN", options: {} });
  is(
    inspector.selection.nodeFront.className,
    "custom-element-anon",
    "The .custom-element-anon inside the div was selected"
  );

  function doKeyPick(msg) {
    info("Key pressed. Waiting for element to be picked");
    testActor.synthesizeKey(msg);
    return Promise.all([
      inspector.selection.once("new-node-front"),
      inspector.once("inspector-updated"),
    ]);
  }
});
