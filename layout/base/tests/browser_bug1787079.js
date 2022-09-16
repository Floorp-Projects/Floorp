/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGECONTENT =
  "<!DOCTYPE html>" +
  "<html>" +
  "<style>" +
  "html { height: 100vh; }" +
  "</style>" +
  "</html>";

const pageUrl = "data:text/html," + encodeURIComponent(PAGECONTENT);

add_task(async function test() {
  SpecialPowers.DOMWindowUtils.setHiDPIMode(true);
  registerCleanupFunction(() => {
    SpecialPowers.DOMWindowUtils.restoreHiDPIMode();
  });

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  // Enter fullscreen.
  let fullscreenChangePromise = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "fullscreenchange"
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.document.documentElement.requestFullscreen();
  });
  await fullscreenChangePromise;

  let [originalInnerWidth, originalInnerHeight] = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    () => {
      return [content.window.innerWidth, content.window.innerHeight];
    }
  );

  // Then change the DPI.
  let originalPixelRatio = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    () => {
      return content.window.devicePixelRatio;
    }
  );
  let dpiChangedPromise = TestUtils.waitForCondition(async () => {
    let pixelRatio = await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
      return content.window.devicePixelRatio;
    });
    return pixelRatio != originalPixelRatio;
  }, "Make sure the DPI changed");
  SpecialPowers.DOMWindowUtils.setHiDPIMode(false);
  await dpiChangedPromise;

  let [innerWidth, innerHeight] = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    () => {
      return [content.window.innerWidth, content.window.innerHeight];
    }
  );

  ok(
    originalInnerWidth < innerWidth,
    "window.innerWidth on a lower DPI should be greater than the original"
  );
  ok(
    originalInnerHeight < innerHeight,
    "window.innerHeight on a lower DPI should be greater than the original"
  );

  fullscreenChangePromise = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "fullscreenchange"
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.document.exitFullscreen();
  });
  await fullscreenChangePromise;

  BrowserTestUtils.removeTab(tab);
});
