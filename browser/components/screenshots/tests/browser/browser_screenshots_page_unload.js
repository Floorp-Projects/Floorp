/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await SpecialPowers.spawn(browser, [TEST_PAGE], url => {
        let a = content.document.createElement("a");
        a.id = "clickMe";
        a.href = url;
        a.textContent = "Click me to unload page";
        content.document.querySelector("body").appendChild(a);
      });

      let helper = new ScreenshotsHelper(browser);

      // click toolbar button so panel shows
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

      await SpecialPowers.spawn(browser, [], () => {
        content.document.querySelector("#clickMe").click();
      });

      await BrowserTestUtils.waitForMutationCondition(
        panel,
        { attributes: true },
        () => {
          return BrowserTestUtils.is_hidden(panel);
        }
      );
      ok(
        BrowserTestUtils.is_hidden(panel),
        "Panel buttons are hidden after page unload"
      );

      await ContentTask.spawn(browser, null, async () => {
        let screenshotsChild = content.windowGlobalChild.getActor(
          "ScreenshotsComponent"
        );
        Assert.ok(
          !screenshotsChild._overlay._initialized,
          "The overlay doesn't exist"
        );
      });
    }
  );
});
