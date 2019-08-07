/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test viewport resizing, with and without meta viewport support.

const TEST_URL =
  "data:text/html;charset=utf-8," +
  '<head><meta name="viewport" content="width=300"/></head>' +
  "<body>meta viewport width 300</body>";
addRDMTask(TEST_URL, async function({ ui, manager }) {
  // Turn on the pref that allows meta viewport support.
  await SpecialPowers.pushPrefEnv({
    set: [["devtools.responsive.metaViewport.enabled", true]],
  });

  const store = ui.toolWindow.store;

  // Wait until the viewport has been added.
  await waitUntilState(store, state => state.viewports.length == 1);

  info("--- Starting viewport test output ---");

  // We're going to take a 600,300 viewport (before) and resize it
  // to 50,50 (after) and then resize it back. At the before and
  // after points, we'll measure zoom and the layout viewport width
  // and height.
  const expected = [
    {
      metaSupport: false,
      before: [1.0, 600, 300],
      after: [1.0, 50, 50], // Zoom is unaffected.
    },
    {
      metaSupport: true,
      before: [2.0, 300, 150],
      after: [0.25, 200, 200], // This checks that min-zoom is active.
    },
  ];

  for (const e of expected) {
    const b = e.before;
    const a = e.after;

    const message = "Meta Viewport " + (e.metaSupport ? "ON" : "OFF");

    // Ensure meta viewport is set.
    info(message + " setting meta viewport support.");
    await setTouchAndMetaViewportSupport(ui, e.metaSupport);

    // Get to the initial size and check values.
    await setViewportSize(ui, manager, 600, 300);
    await testViewportZoomWidthAndHeight(
      message + " before resize",
      ui,
      b[0],
      b[1],
      b[2]
    );

    // Move to the smaller size.
    await setViewportSize(ui, manager, 50, 50);
    await testViewportZoomWidthAndHeight(
      message + " after resize",
      ui,
      a[0],
      a[1],
      a[2]
    );

    // Go back to the initial size and check again.
    await setViewportSize(ui, manager, 600, 300);
    await testViewportZoomWidthAndHeight(
      message + " return to initial size",
      ui,
      b[0],
      b[1],
      b[2]
    );
  }
});
