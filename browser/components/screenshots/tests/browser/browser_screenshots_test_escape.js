/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_fullpageScreenshot() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);
      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");

      // click toolbar button so panel shows
      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();

      BrowserTestUtils.synthesizeKey("KEY_Escape", {}, browser);

      await helper.waitForOverlayClosed();
    }
  );
});
