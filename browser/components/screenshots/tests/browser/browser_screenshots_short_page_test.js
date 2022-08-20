/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test ensures the overlay is covering the entire window event thought the body is only 100px by 100px
 */
add_task(async function() {
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

      let dimensions = await helper.getSelectionLayerDimensions();
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
    }
  );
});
