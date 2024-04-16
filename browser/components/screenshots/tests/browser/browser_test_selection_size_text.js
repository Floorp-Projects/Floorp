/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_selectionSizeTest() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      const dpr = browser.ownerGlobal.devicePixelRatio;
      let helper = new ScreenshotsHelper(browser);

      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();
      await helper.dragOverlay(100, 100, 500, 500);

      let actualText = await helper.getOverlaySelectionSizeText();

      Assert.equal(
        actualText,
        `${400 * dpr} × ${400 * dpr}`,
        "The selection size text is the same"
      );
    }
  );
});

add_task(async function test_selectionSizeTestAt1Point5Zoom() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      const zoom = 1.5;
      const dpr = browser.ownerGlobal.devicePixelRatio;
      let helper = new ScreenshotsHelper(browser);
      helper.zoomBrowser(zoom);

      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();
      await helper.dragOverlay(100, 100, 500, 500);

      let actualText = await helper.getOverlaySelectionSizeText();

      Assert.equal(
        actualText,
        `${400 * dpr * zoom} × ${400 * dpr * zoom}`,
        "The selection size text is the same"
      );
    }
  );
});

add_task(async function test_selectionSizeTestAtPoint5Zoom() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      const zoom = 0.5;
      const dpr = browser.ownerGlobal.devicePixelRatio;
      let helper = new ScreenshotsHelper(browser);
      helper.zoomBrowser(zoom);

      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();
      await helper.dragOverlay(100, 100, 500, 500);

      let actualText = await helper.getOverlaySelectionSizeText();

      Assert.equal(
        actualText,
        `${400 * dpr * zoom} × ${400 * dpr * zoom}`,
        "The selection size text is the same"
      );
    }
  );
});
