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

addRDMTask(
  null,
  async function () {
    const tab = await addTab(TEST_URL);
    const browser = tab.linkedBrowser;

    const { ui, manager } = await openRDM(tab);
    await waitForDeviceAndViewportState(ui);
    await setViewportSize(ui, manager, WIDTH, HEIGHT);

    info("Ensure outer size values are unchanged at different zoom levels.");
    for (let i = 0; i < ZOOM_LEVELS.length; i++) {
      info(`Setting zoom level to ${ZOOM_LEVELS[i]}`);
      await promiseRDMZoom(ui, browser, ZOOM_LEVELS[i]);

      await checkWindowOuterSize(ui, ZOOM_LEVELS[i]);
      await checkWindowScreenSize(ui, ZOOM_LEVELS[i]);
    }
  },
  { onlyPrefAndTask: true }
);

async function checkWindowOuterSize(ui, zoom_level) {
  return SpecialPowers.spawn(
    ui.getViewportBrowser(),
    [{ width: WIDTH, height: HEIGHT, zoom: zoom_level }],
    async function ({ width, height, zoom }) {
      // Approximate the outer size value returned on the window content with the expected
      // value. We should expect, at the very most, a 2px difference between the two due
      // to floating point rounding errors that occur when scaling from inner size CSS
      // integer values to outer size CSS integer values. See Part 1 of Bug 1107456.
      // Some of the drift is also due to full zoom scaling effects; see Bug 1577775.
      Assert.lessOrEqual(
        Math.abs(content.outerWidth - width),
        2,
        `window.outerWidth zoom ${zoom} should be ${width} and we got ${content.outerWidth}.`
      );
      Assert.lessOrEqual(
        Math.abs(content.outerHeight - height),
        2,
        `window.outerHeight zoom ${zoom} should be ${height} and we got ${content.outerHeight}.`
      );
    }
  );
}

async function checkWindowScreenSize(ui, zoom_level) {
  return SpecialPowers.spawn(
    ui.getViewportBrowser(),
    [{ width: WIDTH, height: HEIGHT, zoom: zoom_level }],
    async function ({ width, height, zoom }) {
      const { screen } = content;

      Assert.lessOrEqual(
        Math.abs(screen.availWidth - width),
        2,
        `screen.availWidth zoom ${zoom} should be ${width} and we got ${screen.availWidth}.`
      );

      Assert.lessOrEqual(
        Math.abs(screen.availHeight - height),
        2,
        `screen.availHeight zoom ${zoom} should be ${height} and we got ${screen.availHeight}.`
      );

      Assert.lessOrEqual(
        Math.abs(screen.width - width),
        2,
        `screen.width zoom " ${zoom} should be ${width} and we got ${screen.width}.`
      );

      Assert.lessOrEqual(
        Math.abs(screen.height - height),
        2,
        `screen.height zoom " ${zoom} should be ${height} and we got ${screen.height}.`
      );
    }
  );
}
