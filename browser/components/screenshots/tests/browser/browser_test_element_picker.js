/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_element_picker() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      await helper.clickTestPageElement();

      let rect = await helper.getTestPageElementRect();
      let region = await helper.getSelectionRegionDimensions();

      is(
        region.left,
        rect.left,
        "The selected region left is the same as the element left"
      );
      is(
        region.right,
        rect.right,
        "The selected region right is the same as the element right"
      );
      is(
        region.top,
        rect.top,
        "The selected region top is the same as the element top"
      );
      is(
        region.bottom,
        rect.bottom,
        "The selected region bottom is the same as the element bottom"
      );

      mouse.click(10, 10);
      await helper.waitForStateChange(["crosshairs"]);

      let hoverElementRegionValid = await helper.isHoverElementRegionValid();

      ok(!hoverElementRegionValid, "Hover element rect is null");

      mouse.click(10, 10);
      await helper.waitForStateChange(["crosshairs"]);
    }
  );
});

add_task(async function test_element_pickerRTL() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: RTL_TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      await helper.clickTestPageElement();

      let rect = await helper.getTestPageElementRect();
      let region = await helper.getSelectionRegionDimensions();

      info(`element rect: ${JSON.stringify(rect, null, 2)}`);
      info(`selected region: ${JSON.stringify(region, null, 2)}`);

      is(
        region.width,
        rect.width,
        "The selected region width is the same as the element width"
      );
      is(
        region.height,
        rect.height,
        "The selected region height is the same as the element height"
      );
    }
  );
});
