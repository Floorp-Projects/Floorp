/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

ChromeUtils.import("resource:///modules/CustomizableUI.jsm", {});

add_task(async function testScreenshotButtonDisabled() {
  info("Test the Screenshots button in the panel");

  CustomizableUI.addWidgetToArea(
    "screenshot-button",
    CustomizableUI.AREA_NAVBAR
  );

  let screenshotBtn = document.getElementById("screenshot-button");
  Assert.ok(screenshotBtn, "The screenshots button was added to the nav bar");

  await BrowserTestUtils.withNewTab("https://example.com/", () => {
    Assert.equal(
      screenshotBtn.disabled,
      false,
      "Screenshots button is enabled"
    );
  });
  await BrowserTestUtils.withNewTab("about:home", () => {
    Assert.equal(
      screenshotBtn.disabled,
      true,
      "Screenshots button is now disabled"
    );
  });
});

add_task(async function test_disabledMultiWindow() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_GREEN_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);
      await helper.triggerUIFromToolbar();

      let screenshotBtn = document.getElementById("screenshot-button");
      Assert.ok(
        screenshotBtn,
        "The screenshots button was added to the nav bar"
      );

      Assert.equal(
        screenshotBtn.disabled,
        true,
        "Screenshots button is now disabled"
      );

      let newWin = await BrowserTestUtils.openNewBrowserWindow();
      await BrowserTestUtils.closeWindow(newWin);

      let deactivatedPromise = helper.waitForToolbarButtonDeactivation();
      await deactivatedPromise;
      info("Screenshots is deactivated");

      await EventUtils.synthesizeAndWaitKey("VK_ESCAPE", {});
      await BrowserTestUtils.waitForCondition(() => {
        return !screenshotBtn.disabled;
      });

      Assert.equal(
        screenshotBtn.disabled,
        false,
        "Screenshots button is enabled"
      );
    }
  );
});
