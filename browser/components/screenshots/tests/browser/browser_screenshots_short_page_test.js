/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test ensures the overlay is covering the entire window event thought the body is only 100px by 100px
 */
add_task(async function() {
  CustomizableUI.addWidgetToArea(
    "screenshot-button",
    CustomizableUI.AREA_NAVBAR
  );
  let screenshotBtn = document.getElementById("screenshot-button");
  Assert.ok(screenshotBtn, "The screenshots button was added to the nav bar");

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

      await ContentTask.spawn(browser, null, async () => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );
        Assert.ok(screenshotsChild._overlay._initialized, "The overlay exists");

        let dimensions = screenshotsChild._overlay.screenshotsContainer.getSelectionLayerDimensions();
        Assert.equal(
          dimensions.scrollWidth,
          content.window.innerWidth,
          "The overlay spans the width of the window"
        );

        Assert.equal(
          dimensions.scrollHeight,
          content.window.innerHeight,
          "The overlay spans the height of the window"
        );
      });
    }
  );
});
