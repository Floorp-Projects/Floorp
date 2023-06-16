/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test viewports basics after opening, like size and location

const TEST_URL = "https://example.org/";
addRDMTask(TEST_URL, async function ({ ui }) {
  const browser = ui.getViewportBrowser();

  is(
    ui.toolWindow.getComputedStyle(browser).getPropertyValue("width"),
    "320px",
    "Viewport has default width"
  );
  is(
    ui.toolWindow.getComputedStyle(browser).getPropertyValue("height"),
    "480px",
    "Viewport has default height"
  );

  // Browser's location should match original tab
  await navigateTo(TEST_URL, { browser });

  const location = await spawnViewportTask(ui, {}, function () {
    return content.location.href; // eslint-disable-line
  });
  is(location, TEST_URL, "Viewport location matches");
});
