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

function promiseContentReflow(ui) {
  return ContentTask.spawn(ui.getViewportBrowser(), {}, async function() {
    return new Promise(resolve => {
      content.window.requestAnimationFrame(resolve);
    });
  });
}

add_task(async function() {
  const tab = await addTab(TEST_URL);
  const browser = tab.linkedBrowser;

  const { ui, manager } = await openRDM(tab);
  await setViewportSize(ui, manager, WIDTH, HEIGHT);

  info("Ensure outer size values are unchanged at different zoom levels.");
  for (let i = 0; i < ZOOM_LEVELS.length; i++) {
    info(`Setting zoom level to ${ZOOM_LEVELS[i]}`);
    ZoomManager.setZoomForBrowser(browser, ZOOM_LEVELS[i]);

    // We need to ensure that the RDM pane has had time to both change size and
    // change the zoom level. This is currently not an atomic operation. The event
    // timing is this:
    // 1) Pane changes size, content reflows.
    // 2) Pane changes zoom, content reflows.
    // So to wait for the post-zoom reflow to be complete, we have two wait on TWO
    // reflows.
    await promiseContentReflow(ui);
    await promiseContentReflow(ui);

    await checkWindowOuterSize(ui, ZOOM_LEVELS[i]);
    await checkWindowScreenSize(ui, ZOOM_LEVELS[i]);
  }
});

async function checkWindowOuterSize(ui, zoom_level) {
  return ContentTask.spawn(
    ui.getViewportBrowser(),
    { width: WIDTH, height: HEIGHT, zoom: zoom_level },
    async function({ width, height, zoom }) {
      // Approximate the outer size value returned on the window content with the expected
      // value. We should expect, at the very most, a 2px difference between the two due
      // to floating point rounding errors that occur when scaling from inner size CSS
      // integer values to outer size CSS integer values. See Part 1 of Bug 1107456.
      // Some of the drift is also due to full zoom scaling effects; see Bug 1577775.
      ok(
        Math.abs(content.outerWidth - width) <= 2,
        `window.outerWidth zoom ${zoom} should be ${width} and we got ${
          content.outerWidth
        }.`
      );
      ok(
        Math.abs(content.outerHeight - height) <= 2,
        `window.outerHeight zoom ${zoom} should be ${height} and we got ${
          content.outerHeight
        }.`
      );
    }
  );
}

async function checkWindowScreenSize(ui, zoom_level) {
  return ContentTask.spawn(
    ui.getViewportBrowser(),
    { width: WIDTH, height: HEIGHT, zoom: zoom_level },
    async function({ width, height, zoom }) {
      const { screen } = content;

      ok(
        Math.abs(screen.availWidth - width) <= 2,
        `screen.availWidth zoom ${zoom} should be ${width} and we got ${
          screen.availWidth
        }.`
      );

      ok(
        Math.abs(screen.availHeight - height) <= 2,
        `screen.availHeight zoom ${zoom} should be ${height} and we got ${
          screen.availHeight
        }.`
      );

      ok(
        Math.abs(screen.width - width) <= 2,
        `screen.width zoom " ${zoom} should be ${width} and we got ${
          screen.width
        }.`
      );

      ok(
        Math.abs(screen.height - height) <= 2,
        `screen.height zoom " ${zoom} should be ${height} and we got ${
          screen.height
        }.`
      );
    }
  );
}
