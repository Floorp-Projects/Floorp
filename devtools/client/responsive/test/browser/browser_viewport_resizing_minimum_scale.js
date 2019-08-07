/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test viewport resizing, with and without meta viewport support.

const TEST_URL =
  "data:text/html;charset=utf-8," +
  '<head><meta name="viewport" content="initial-scale=1.0, ' +
  'minimum-scale=1.0, width=device-width"></head>' +
  '<div style="width:100%;background-color:green">test</div>' +
  "</body>";
addRDMTask(TEST_URL, async function({ ui, manager }) {
  // Turn on the pref that allows meta viewport support.
  await SpecialPowers.pushPrefEnv({
    set: [["devtools.responsive.metaViewport.enabled", true]],
  });

  const store = ui.toolWindow.store;

  // Wait until the viewport has been added.
  await waitUntilState(store, state => state.viewports.length == 1);

  info("--- Starting viewport test output ---");

  // We're going to take a 300,600 viewport (before) and resize it
  // to 600,300 (after) and then resize it back. At the before and
  // after points, we'll measure zoom and the layout viewport width
  // and height.
  const expected = [
    {
      before: {
        zoom: 1.0,
        width: 300,
        height: 600,
      },
      after: {
        zoom: 1.0,
        width: 600,
        height: 300,
      },
    },
  ];

  for (const e of expected) {
    const b = e.before;
    const a = e.after;

    const message = "Meta Viewport ON";

    // Ensure meta viewport is set.
    info(message + " setting meta viewport support.");
    await setTouchAndMetaViewportSupport(ui, true);

    // Get to the initial size and check values.
    await setViewportSizeAndAwaitReflow(ui, manager, 300, 600);
    await testViewportZoomWidthAndHeight(
      message + " before resize",
      ui,
      b.zoom,
      b.width,
      b.height
    );

    // Move to the smaller size.
    await setViewportSizeAndAwaitReflow(ui, manager, 600, 300);
    await testViewportZoomWidthAndHeight(
      message + " after resize",
      ui,
      a.zoom,
      a.width,
      a.height
    );

    // Go back to the initial size and check again.
    await setViewportSizeAndAwaitReflow(ui, manager, 300, 600);
    await testViewportZoomWidthAndHeight(
      message + " return to initial size",
      ui,
      b.zoom,
      b.width,
      b.height
    );
  }
});
