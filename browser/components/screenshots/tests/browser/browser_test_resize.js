/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const windowWidth = 768;

add_task(async function test_window_resize() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: RESIZE_TEST_PAGE,
    },
    async browser => {
      await new Promise(r => window.requestAnimationFrame(r));

      let helper = new ScreenshotsHelper(browser);
      await helper.resizeContentWindow(windowWidth, window.outerHeight);
      const originalContentDimensions = await helper.getContentDimensions();
      info(JSON.stringify(originalContentDimensions, null, 2));

      await helper.zoomBrowser(1.5);

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      await helper.scrollContentWindow(windowWidth, window.outerHeight);

      await helper.clickTestPageElement("hello");

      await helper.zoomBrowser(1);

      await helper.waitForOverlaySizeChangeTo(
        originalContentDimensions.scrollWidth,
        originalContentDimensions.scrollHeight
      );

      let contentDims = await helper.getContentDimensions();
      info(JSON.stringify(contentDims, null, 2));

      is(
        contentDims.scrollWidth,
        originalContentDimensions.scrollWidth,
        "Width of page is back to original"
      );
      is(
        contentDims.scrollHeight,
        originalContentDimensions.scrollHeight,
        "Height of page is back to original"
      );
    }
  );
});

add_task(async function test_window_resize_vertical_writing_mode() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: RESIZE_TEST_PAGE,
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], () => {
        content.document.documentElement.style = "writing-mode: vertical-lr;";
      });

      await new Promise(r => window.requestAnimationFrame(r));

      let helper = new ScreenshotsHelper(browser);
      await helper.resizeContentWindow(windowWidth, window.outerHeight);
      const originalContentDimensions = await helper.getContentDimensions();
      info(JSON.stringify(originalContentDimensions, null, 2));

      await helper.zoomBrowser(1.5);

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      await helper.scrollContentWindow(windowWidth, window.outerHeight);

      await helper.clickTestPageElement("hello");

      await helper.zoomBrowser(1);

      await helper.waitForOverlaySizeChangeTo(
        originalContentDimensions.scrollWidth,
        originalContentDimensions.scrollHeight
      );

      let contentDims = await helper.getContentDimensions();
      info(JSON.stringify(contentDims, null, 2));

      is(
        contentDims.scrollWidth,
        originalContentDimensions.scrollWidth,
        "Width of page is back to original"
      );
      is(
        contentDims.scrollHeight,
        originalContentDimensions.scrollHeight,
        "Height of page is back to original"
      );
    }
  );
});
