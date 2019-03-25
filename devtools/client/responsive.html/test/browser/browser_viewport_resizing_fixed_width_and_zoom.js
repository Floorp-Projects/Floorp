/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test viewport resizing, with and without meta viewport support.

// We call this to switch between on/off support for meta viewports.
async function setTouchAndMetaViewportSupport(ui, value) {
  const reloadNeeded = await ui.updateTouchSimulation(value);
  if (reloadNeeded) {
    info("Reload is needed -- waiting for it.");
    const reload = waitForViewportLoad(ui);
    const browser = ui.getViewportBrowser();
    browser.reload();
    await reload;
  }
}

// This function check that zoom, layout viewport width and height
// are all as expected.
async function testViewportZoomWidthAndHeight(message, ui, zoom, width, height) {
  const resolution = await spawnViewportTask(ui, {}, function() {
    return content.windowUtils.getResolution();
  });
  is(resolution, zoom, message + " should have expected zoom.");

  const layoutSize = await spawnViewportTask(ui, {}, function() {
    return {
      width: content.screen.width,
      height: content.screen.height,
    };
  });
  is(layoutSize.width, width, message + " should have expected layout width.");
  is(layoutSize.height, height, message + " should have expected layout height.");
}

const TEST_URL = "data:text/html;charset=utf-8," +
  "<head><meta name=\"viewport\" content=\"width=device-width, " +
  "initial-scale=1.0, minimum-scale=1.0, maximum-scale=1.0\"></head>" +
  "<body>meta viewport scaled locked at 1.0</body>";
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
      metaSupport: false,
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
    {
      metaSupport: true,
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

    const message = "Meta Viewport " + (e.metaSupport ? "ON" : "OFF");

    // Ensure meta viewport is set.
    info(message + " setting meta viewport support.");
    await setTouchAndMetaViewportSupport(ui, e.metaSupport);

    // Get to the initial size and check values.
    await setViewportSize(ui, manager, 300, 600);
    await testViewportZoomWidthAndHeight(
      message + " before resize",
      ui, b.zoom, b.width, b.height);

    // Move to the smaller size.
    await setViewportSize(ui, manager, 600, 300);
    await testViewportZoomWidthAndHeight(
      message + " after resize",
      ui, a.zoom, a.width, a.height);

    // Go back to the initial size and check again.
    await setViewportSize(ui, manager, 300, 600);
    await testViewportZoomWidthAndHeight(
      message + " return to initial size",
      ui, b.zoom, b.width, b.height);
  }
});
