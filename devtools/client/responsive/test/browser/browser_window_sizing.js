/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that correct window sizing values are reported and unaffected by zoom. In
// particular, we want to ensure that the values for the window's outer and screen
// sizing values reflect the size of the viewport.

const TEST_URL = "data:text/html;charset=utf-8,";
const WIDTH = 375;
const HEIGHT = 450;
const ZOOM_LEVELS = [0.3, 0.5, 0.9, 1, 1.5, 2, 2.4];

add_task(async function() {
  const tab = await addTab(TEST_URL);
  const browser = tab.linkedBrowser;

  const { ui, manager } = await openRDM(tab);
  await setViewportSize(ui, manager, WIDTH, HEIGHT);

  info("Ensure outer size values are unchanged at different zoom levels.");
  for (let i = 0; i < ZOOM_LEVELS.length; i++) {
    info(`Setting zoom level to ${ZOOM_LEVELS[i]}`);
    ZoomManager.setZoomForBrowser(browser, ZOOM_LEVELS[i]);

    await checkWindowOuterSize(ui);
    await checkWindowScreenSize(ui);
  }
});

async function checkWindowOuterSize(ui) {
  return ContentTask.spawn(
    ui.getViewportBrowser(),
    { width: WIDTH, height: HEIGHT },
    async function({ width, height }) {
      // Approximate the outer size value returned on the window content with the expected
      // value. We should expect, at the very most, a 1px difference between the two due
      // to floating point rounding errors that occur when scaling from inner size CSS
      // integer values to outer size CSS integer values. See Part 1 of Bug 1107456.
      ok(
        Math.abs(content.outerWidth - width) <= 1,
        `window.outerWidth should be ${width} and we got ${content.outerWidth}.`
      );
      ok(
        Math.abs(content.outerHeight - height) <= 1,
        `window.outerHeight should be ${height} and we got ${
          content.outerHeight
        }.`
      );
    }
  );
}

async function checkWindowScreenSize(ui) {
  return ContentTask.spawn(
    ui.getViewportBrowser(),
    { width: WIDTH, height: HEIGHT },
    async function({ width, height }) {
      const { screen } = content;

      ok(
        Math.abs(screen.availWidth - width) <= 1,
        `screen.availWidth should be ${width} and we got ${screen.availWidth}.`
      );

      ok(
        Math.abs(screen.availHeight - height) <= 1,
        `screen.availHeight should be ${height} and we got ${
          screen.availHeight
        }.`
      );

      ok(
        Math.abs(screen.width - width) <= 1,
        `screen.width should be ${width} and we got ${screen.width}.`
      );

      ok(
        Math.abs(screen.height - height) <= 1,
        `screen.height should be ${height} and we got ${screen.height}.`
      );
    }
  );
}
