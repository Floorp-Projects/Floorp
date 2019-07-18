/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the keybindings for Picker work alright

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_dom.html";

add_task(async function() {
  const { toolbox, testActor } = await openInspectorForURL(TEST_URL);

  await startPicker(toolbox);

  // Previously chosen child memory
  info("Testing whether previously chosen child is remembered");

  info("Selecting the ahoy paragraph DIV");
  await moveMouseOver("#ahoy");

  await doKeyHover({ key: "VK_LEFT", options: {} });
  ok(
    await testActor.assertHighlightedNode("#simple-div2"),
    "The highlighter shows #simple-div2. OK."
  );

  await doKeyHover({ key: "VK_RIGHT", options: {} });
  ok(
    await testActor.assertHighlightedNode("#ahoy"),
    "The highlighter shows #ahoy. OK."
  );

  info("Going back up to the complex-div DIV");
  await doKeyHover({ key: "VK_LEFT", options: {} });
  await doKeyHover({ key: "VK_LEFT", options: {} });
  ok(
    await testActor.assertHighlightedNode("#complex-div"),
    "The highlighter shows #complex-div. OK."
  );

  await doKeyHover({ key: "VK_RIGHT", options: {} });
  ok(
    await testActor.assertHighlightedNode("#simple-div2"),
    "The highlighter shows #simple-div2. OK."
  );

  info("Previously chosen child is remembered. Passed.");

  info("Stopping the picker");
  await toolbox.inspectorFront.nodePicker.stop();

  function doKeyHover(args) {
    info("Key pressed. Waiting for element to be highlighted/hovered");
    const onHighlighterReady = toolbox.once("highlighter-ready");
    const onPickerNodeHovered = toolbox.inspectorFront.nodePicker.once(
      "picker-node-hovered"
    );
    testActor.synthesizeKey(args);
    return promise.all([onHighlighterReady, onPickerNodeHovered]);
  }

  function moveMouseOver(selector) {
    info("Waiting for element " + selector + " to be highlighted");
    const onHighlighterReady = toolbox.once("highlighter-ready");
    const onPickerNodeHovered = toolbox.inspectorFront.nodePicker.once(
      "picker-node-hovered"
    );
    testActor.synthesizeMouse({
      options: { type: "mousemove" },
      center: true,
      selector: selector,
    });
    return promise.all([onHighlighterReady, onPickerNodeHovered]);
  }
});
