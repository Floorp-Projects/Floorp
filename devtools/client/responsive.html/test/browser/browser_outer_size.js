/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the window's outerWidth and outerHeight values are not affected by page zoom.

const TEST_URL = "data:text/html;charset=utf-8,";
const OUTER_WIDTH = 375;
const OUTER_HEIGHT = 450;
const ZOOM_LEVELS = [0.3, 0.5, 0.9, 1, 1.5, 2, 2.4];

add_task(async function() {
  const tab = await addTab(TEST_URL);
  const browser = tab.linkedBrowser;

  const { ui, manager } = await openRDM(tab);
  await setViewportSize(ui, manager, OUTER_WIDTH, OUTER_HEIGHT);

  info("Ensure outer size values are unchanged at different zoom levels.");
  for (let i = 0; i < ZOOM_LEVELS.length; i++) {
    await checkWindowOuterSize(ui, browser, ZOOM_LEVELS[i]);
  }
});

async function checkWindowOuterSize(ui, browser, zoom) {
  info(`Setting zoom level to ${zoom}`);
  ZoomManager.setZoomForBrowser(browser, zoom);

  return ContentTask.spawn(ui.getViewportBrowser(),
    { width: OUTER_WIDTH, height: OUTER_HEIGHT },
    async function({ width, height }) {
      // Approximate the outer size value returned on the window content with the expected
      // value. We should expect, at the very most, a 1px difference between the two due
      // to floating point rounding errors that occur when scaling from inner size CSS
      // integer values to outer size CSS integer values. See Part 1 of Bug 1107456.
      ok(Math.abs(content.outerWidth - width) <= 1,
        `window.outerWidth should be ${width} and we got ${content.outerWidth}.`);
      ok(Math.abs(content.outerHeight - height) <= 1,
        `window.outerHeight should be ${height} and we got ${content.outerHeight}.`);
    });
}
