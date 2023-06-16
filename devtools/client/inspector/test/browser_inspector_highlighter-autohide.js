/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that clicking on a node in the markup view or picking a node with the node picker
// shows a highlighter which is automatically hidden after a delay.
add_task(async function () {
  info("Loading the test document and opening the inspector");
  const { inspector, toolbox, highlighterTestFront } =
    await openInspectorForURL(
      "data:text/html;charset=utf-8,<p id='one'>TEST 1</p><p id='two'>TEST 2</p>"
    );
  const { waitForHighlighterTypeShown, waitForHighlighterTypeHidden } =
    getHighlighterTestHelpers(inspector);

  // While in test mode, the configuration to automatically hide Box Model Highlighters
  // after a delay is ignored to prevent intermittent test failures from race conditions.
  // Restore this behavior just for this test because it is explicitly checked.
  const HIGHLIGHTER_AUTOHIDE_TIMER = inspector.HIGHLIGHTER_AUTOHIDE_TIMER;
  inspector.HIGHLIGHTER_AUTOHIDE_TIMER = 1000;

  registerCleanupFunction(() => {
    // Restore the value to avoid impacting other tests.
    inspector.HIGHLIGHTER_AUTOHIDE_TIMER = HIGHLIGHTER_AUTOHIDE_TIMER;
  });

  info(
    "Check that selecting an element in the markup-view shows the highlighter and auto-hides it"
  );
  let onHighlighterShown = waitForHighlighterTypeShown(
    inspector.highlighters.TYPES.BOXMODEL
  );
  const onHighlighterHidden = waitForHighlighterTypeHidden(
    inspector.highlighters.TYPES.BOXMODEL
  );

  let delay;
  const start = Date.now();
  onHighlighterHidden.then(() => {
    delay = Date.now() - start;
  });

  await clickContainer("#one", inspector);

  info("Wait for Box Model Highlighter shown");
  await onHighlighterShown;
  info("Wait for Box Model Highlighter hidden");
  await onHighlighterHidden;

  ok(true, "Highlighter was shown and hidden");
  ok(
    delay >= inspector.HIGHLIGHTER_AUTOHIDE_TIMER,
    `Highlighter was hidden after expected delay (${delay}ms)`
  );

  info("Check that picking a node hides the highlighter right away");
  onHighlighterShown = waitForHighlighterTypeShown(
    inspector.highlighters.TYPES.BOXMODEL
  );
  await startPicker(toolbox);
  await hoverElement(inspector, "#two", 0, 0);
  await onHighlighterShown;
  ok(
    await highlighterTestFront.isHighlighting(),
    "Highlighter was shown when hovering the node"
  );

  await pickElement(inspector, "#two", 0, 0);
  is(
    await highlighterTestFront.isHighlighting(),
    false,
    "Highlighter gets hidden without delay after picking a node"
  );
});
