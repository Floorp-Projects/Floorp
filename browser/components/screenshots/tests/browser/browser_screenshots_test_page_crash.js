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

      // click toolbar button so UI shows
      helper.triggerUIFromToolbar();

      let panel = gBrowser.selectedBrowser.ownerDocument.querySelector(
        "#screenshotsPagePanel"
      );
      await BrowserTestUtils.waitForMutationCondition(
        panel,
        { attributes: true },
        () => {
          return BrowserTestUtils.is_visible(panel);
        }
      );
      ok(BrowserTestUtils.is_visible(panel), "Panel buttons are visible");

      await ContentTask.spawn(browser, null, async () => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );
        Assert.ok(screenshotsChild._overlay._initialized, "The overlay exists");
      });

      let waitForPanelHide = BrowserTestUtils.waitForMutationCondition(
        panel,
        { attributes: true },
        () => {
          return BrowserTestUtils.is_hidden(panel);
        }
      );

      await BrowserTestUtils.crashFrame(browser);

      await waitForPanelHide;
      ok(
        BrowserTestUtils.is_hidden(panel),
        "Panel buttons are hidden after page crash"
      );

      await ContentTask.spawn(browser, null, async () => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );
        Assert.ok(!screenshotsChild._overlay, "The overlay doesn't exist");
      });

      let tab = gBrowser.getTabForBrowser(browser);

      SessionStore.reviveCrashedTab(tab);

      ok(
        BrowserTestUtils.is_hidden(panel),
        "Panel buttons are hidden after page crash"
      );
    }
  );
});
