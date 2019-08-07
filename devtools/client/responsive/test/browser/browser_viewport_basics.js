/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test viewports basics after opening, like size and location

const TEST_URL = "http://example.org/";

addRDMTask(TEST_URL, async function({ ui }) {
  const store = ui.toolWindow.store;

  // Wait until the viewport has been added
  await waitUntilState(store, state => state.viewports.length == 1);

  // A single viewport of default size appeared
  const viewport = ui.toolWindow.document.querySelector(".viewport-content");

  is(
    ui.toolWindow.getComputedStyle(viewport).getPropertyValue("width"),
    "320px",
    "Viewport has default width"
  );
  is(
    ui.toolWindow.getComputedStyle(viewport).getPropertyValue("height"),
    "480px",
    "Viewport has default height"
  );

  // Browser's location should match original tab
  await waitForFrameLoad(ui, TEST_URL);
  const location = await spawnViewportTask(ui, {}, function() {
    return content.location.href; // eslint-disable-line
  });
  is(location, TEST_URL, "Viewport location matches");
});
