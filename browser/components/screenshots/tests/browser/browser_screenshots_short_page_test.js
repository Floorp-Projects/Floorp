/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test ensures the overlay is covering the entire window event thought
 * the body is smaller than the viewport
 */
add_task(async function test_overlay() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: SHORT_TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);
      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");

      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();

      await helper.dragOverlay(10, 10, 500, 500);

      let { scrollWidth, scrollHeight } =
        await helper.getScreenshotsOverlayDimensions();
      Assert.equal(
        scrollWidth,
        contentInfo.clientWidth,
        "The overlay spans the width of the window"
      );

      Assert.equal(
        scrollHeight,
        contentInfo.clientHeight,
        "The overlay spans the height of the window"
      );
    }
  );
});

add_task(async function test_window_resize() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: SHORT_TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);
      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");

      const originalWindowWidth = window.outerWidth;
      const originalWindowHeight = window.outerHeight;

      const BIG_WINDOW_SIZE = 920;
      const SMALL_WINDOW_SIZE = 620;

      window.resizeTo(SMALL_WINDOW_SIZE, SMALL_WINDOW_SIZE);
      await TestUtils.waitForCondition(() => {
        info(
          `Got ${window.outerWidth}x${
            window.outerHeight
          }. Expecting ${SMALL_WINDOW_SIZE}. ${
            window.outerHeight === SMALL_WINDOW_SIZE
          } ${window.outerWidth === SMALL_WINDOW_SIZE}`
        );
        return (
          window.outerHeight === SMALL_WINDOW_SIZE &&
          window.outerWidth === SMALL_WINDOW_SIZE
        );
      }, "Waiting for window to resize");

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();
      await helper.dragOverlay(10, 10, 100, 100);

      let dimensions = await helper.getScreenshotsOverlayDimensions();
      let oldWidth = dimensions.scrollWidth;
      let oldHeight = dimensions.scrollHeight;

      window.resizeTo(BIG_WINDOW_SIZE, BIG_WINDOW_SIZE);
      await TestUtils.waitForCondition(
        () =>
          window.outerHeight === BIG_WINDOW_SIZE &&
          window.outerWidth === BIG_WINDOW_SIZE,
        "Waiting for window to resize"
      );
      await helper.waitForSelectionLayerDimensionChange(oldWidth, oldHeight);

      contentInfo = await helper.getContentDimensions();
      dimensions = await helper.getScreenshotsOverlayDimensions();
      Assert.equal(
        dimensions.scrollWidth,
        contentInfo.clientWidth,
        "The overlay spans the width of the window"
      );
      Assert.equal(
        dimensions.scrollHeight,
        contentInfo.clientHeight,
        "The overlay spans the height of the window"
      );

      oldWidth = dimensions.scrollWidth;
      oldHeight = dimensions.scrollHeight;

      window.resizeTo(SMALL_WINDOW_SIZE, SMALL_WINDOW_SIZE);
      await TestUtils.waitForCondition(
        () =>
          window.outerHeight === SMALL_WINDOW_SIZE &&
          window.outerWidth === SMALL_WINDOW_SIZE,
        "Waiting for window to resize"
      );
      await helper.waitForSelectionLayerDimensionChange(oldWidth, oldHeight);

      contentInfo = await helper.getContentDimensions();
      dimensions = await helper.getScreenshotsOverlayDimensions();
      Assert.equal(
        dimensions.scrollWidth,
        contentInfo.clientWidth,
        "The overlay spans the width of the window"
      );
      Assert.equal(
        dimensions.scrollHeight,
        contentInfo.clientHeight,
        "The overlay spans the height of the window"
      );

      Assert.less(
        dimensions.scrollWidth,
        BIG_WINDOW_SIZE,
        "Screenshots overlay is smaller than the big window width"
      );
      Assert.less(
        dimensions.scrollHeight,
        BIG_WINDOW_SIZE,
        "Screenshots overlay is smaller than the big window height"
      );

      window.resizeTo(originalWindowWidth, originalWindowHeight);
      await TestUtils.waitForCondition(
        () =>
          window.outerHeight === originalWindowHeight &&
          window.outerWidth === originalWindowWidth,
        "Waiting for window to resize"
      );
    }
  );
});
