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
      await helper.waitForOverlay();

      let panel = await helper.waitForPanel(gBrowser.selectedBrowser);

      let waitForPanelHide = BrowserTestUtils.waitForMutationCondition(
        panel,
        { attributes: true },
        () => {
          return BrowserTestUtils.isHidden(panel);
        }
      );

      await BrowserTestUtils.crashFrame(browser);

      await waitForPanelHide;
      ok(
        BrowserTestUtils.isHidden(panel),
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
        BrowserTestUtils.isHidden(panel),
        "Panel buttons are hidden after page crash"
      );
    }
  );
});
