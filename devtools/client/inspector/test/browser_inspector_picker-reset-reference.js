/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the node picker reset its reference for the last hovered node when the user
// stops picking (See Bug 1736183).

const TEST_URL =
  "data:text/html;charset=utf8,<h1 id=target>Pick target</h1><h2>ignore me</h2>";

add_task(async () => {
  const { inspector, toolbox, highlighterTestFront } =
    await openInspectorForURL(TEST_URL);

  const { waitForHighlighterTypeHidden } = getHighlighterTestHelpers(inspector);

  info(
    "Start the picker and hover an element to populate the picker hovered node reference"
  );
  await startPicker(toolbox);
  await hoverElement(inspector, "#target");
  ok(
    await highlighterTestFront.assertHighlightedNode("#target"),
    "The highlighter is shown on the expected node"
  );

  info("Hit Escape to cancel picking");
  let onHighlighterHidden = waitForHighlighterTypeHidden(
    inspector.highlighters.TYPES.BOXMODEL
  );
  await stopPickerWithEscapeKey(toolbox);
  await onHighlighterHidden;

  info("And start it again, and hover the same node again");
  await startPicker(toolbox);
  await hoverElement(inspector, "#target");
  ok(
    await highlighterTestFront.assertHighlightedNode("#target"),
    "The highlighter is shown on the expected node again"
  );

  info("Pick the element to stop the picker");
  onHighlighterHidden = waitForHighlighterTypeHidden(
    inspector.highlighters.TYPES.BOXMODEL
  );
  // nodePicker isPicking property is set to false _after_ picker-node-picked event, so
  // we need to wait for picker-stopped here.
  const onPickerStopped = toolbox.nodePicker.once("picker-stopped");
  await pickElement(inspector, "#target", 5, 5);
  await onHighlighterHidden;
  await onPickerStopped;

  info("And start it and hover the same node, again");
  await startPicker(toolbox);
  await hoverElement(inspector, "#target");
  ok(
    await highlighterTestFront.assertHighlightedNode("#target"),
    "The highlighter is shown on the expected node again"
  );

  info("Stop the picker to avoid pending Promise");
  onHighlighterHidden = waitForHighlighterTypeHidden(
    inspector.highlighters.TYPES.BOXMODEL
  );
  await stopPickerWithEscapeKey(toolbox);
  await onHighlighterHidden;
});
