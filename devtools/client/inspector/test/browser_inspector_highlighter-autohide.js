/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that clicking on a node in the markup view or picking a node with the node picker
// shows a highlighter which is automatically hidden after a delay.
add_task(async function() {
  info("Loading the test document and opening the inspector");
  const { inspector, toolbox } = await openInspectorForURL(
    "data:text/html;charset=utf-8,<p id='one'>TEST 1</p><p id='two'>TEST 2</p>"
  );
  const {
    waitForHighlighterTypeShown,
    waitForHighlighterTypeHidden,
  } = getHighlighterTestHelpers(inspector);

  // While in test mode, the configuration to automatically hide Box Model Highlighters
  // after a delay is ignored to prevent intermittent test failures from race conditions.
  // Restore this behavior just for this test because it is explicitly checked.
  const HIGHLIGHTER_AUTOHIDE_TIMER = inspector.HIGHLIGHTER_AUTOHIDE_TIMER;
  inspector.HIGHLIGHTER_AUTOHIDE_TIMER = 500;

  registerCleanupFunction(() => {
    // Restore the value to avoid impacting other tests.
    inspector.HIGHLIGHTER_AUTOHIDE_TIMER = HIGHLIGHTER_AUTOHIDE_TIMER;
  });

  const ACTIONS = [
    async () => {
      info("Select the #one element by clicking in the markup-view");
      await clickContainer("#one", inspector);
    },

    async () => {
      info("Select the #two element by using the node picker");
      await startPicker(toolbox);
      await pickElement(inspector, "#two", 0, 0);
    },
  ];

  for (const action of ACTIONS) {
    const onHighlighterShown = waitForHighlighterTypeShown(
      inspector.highlighters.TYPES.BOXMODEL
    );

    const onHighlighterHidden = waitForHighlighterTypeHidden(
      inspector.highlighters.TYPES.BOXMODEL
    );

    await action();

    info("Wait for Box Model Highlighter shown");
    await onHighlighterShown;
    info("Wait for Box Model Highlighter hidden");
    await onHighlighterHidden;
  }
});
