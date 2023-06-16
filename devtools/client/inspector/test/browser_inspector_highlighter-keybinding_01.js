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

  info("Selecting the simple-div1 DIV");
  await hoverElement(inspector, "#simple-div1");

  ok(
    await highlighterTestFront.assertHighlightedNode("#simple-div1"),
    "The highlighter shows #simple-div1. OK."
  );

  // First Child selection
  info("Testing first-child selection.");

  await doKeyHover("VK_RIGHT");
  ok(
    await highlighterTestFront.assertHighlightedNode("#useless-para"),
    "The highlighter shows #useless-para. OK."
  );

  info("Selecting the useful-para paragraph DIV");
  await hoverElement(inspector, "#useful-para");
  ok(
    await highlighterTestFront.assertHighlightedNode("#useful-para"),
    "The highlighter shows #useful-para. OK."
  );

  await doKeyHover("VK_RIGHT");
  ok(
    await highlighterTestFront.assertHighlightedNode("#bold"),
    "The highlighter shows #bold. OK."
  );

  info("Going back up to the simple-div1 DIV");
  await doKeyHover("VK_LEFT");
  await doKeyHover("VK_LEFT");
  ok(
    await highlighterTestFront.assertHighlightedNode("#simple-div1"),
    "The highlighter shows #simple-div1. OK."
  );

  info("First child selection test Passed.");

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
