/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the keybindings for Picker work alright

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_dom.html";

add_task(async function () {
  const { inspector, toolbox, highlighterTestFront } =
    await openInspectorForURL(TEST_URL);
  const { waitForHighlighterTypeShown } = getHighlighterTestHelpers(inspector);

  await startPicker(toolbox);

  // Previously chosen child memory
  info("Testing whether previously chosen child is remembered");

  info("Selecting the ahoy paragraph DIV");
  await hoverElement(inspector, "#ahoy");

  await doKeyHover("VK_LEFT");
  ok(
    await highlighterTestFront.assertHighlightedNode("#simple-div2"),
    "The highlighter shows #simple-div2. OK."
  );

  await doKeyHover("VK_RIGHT");
  ok(
    await highlighterTestFront.assertHighlightedNode("#ahoy"),
    "The highlighter shows #ahoy. OK."
  );

  info("Going back up to the complex-div DIV");
  await doKeyHover("VK_LEFT");
  await doKeyHover("VK_LEFT");
  ok(
    await highlighterTestFront.assertHighlightedNode("#complex-div"),
    "The highlighter shows #complex-div. OK."
  );

  await doKeyHover("VK_RIGHT");
  ok(
    await highlighterTestFront.assertHighlightedNode("#simple-div2"),
    "The highlighter shows #simple-div2. OK."
  );

  info("Previously chosen child is remembered. Passed.");

  info("Stopping the picker");
  await toolbox.nodePicker.stop({ canceled: true });

  function doKeyHover(key) {
    info("Key pressed. Waiting for element to be highlighted/hovered");
    const onPickerHovered = toolbox.nodePicker.once("picker-node-hovered");
    const onHighlighterShown = waitForHighlighterTypeShown(
      inspector.highlighters.TYPES.BOXMODEL
    );
    BrowserTestUtils.synthesizeKey(key, {}, gBrowser.selectedBrowser);
    return Promise.all([onPickerHovered, onHighlighterShown]);
  }
});
