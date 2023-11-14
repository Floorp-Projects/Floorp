/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function assertRange(lhs, rhsMin, rhsMax, msg) {
  Assert.ok(lhs >= rhsMin && lhs <= rhsMax, msg);
}

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
      let devicePixelRatio = await getContentDevicePixelRatio(browser);

      let expectedWidth = Math.floor(
        devicePixelRatio * contentInfo.scrollWidth
      );
      let expectedHeight = Math.floor(
        devicePixelRatio * contentInfo.scrollHeight
      );

      // click toolbar button so panel shows
      helper.triggerUIFromToolbar();

      let panel = await helper.waitForPanel();

      let screenshotReady = TestUtils.topicObserved(
        "screenshots-preview-ready"
      );

      // click the full page button in panel
      let visiblePage = panel
        .querySelector("screenshots-buttons")
        .shadowRoot.querySelector(".full-page");
      visiblePage.click();

      let dialog = helper.getDialog();

      await screenshotReady;

      let copyButton = dialog._frame.contentDocument.getElementById("copy");
      ok(copyButton, "Got the copy button");

      let clipboardChanged = helper.waitForRawClipboardChange(
        expectedWidth,
        expectedHeight
      );

      // click copy button on dialog box
      copyButton.click();

      info("Waiting for clipboard change");
      let result = await clipboardChanged;

      info("result: " + JSON.stringify(result, null, 2));
      info("contentInfo: " + JSON.stringify(contentInfo, null, 2));

      Assert.equal(result.width, expectedWidth, "Widths should be equal");
      Assert.equal(result.height, expectedHeight, "Heights should be equal");

      // top left
      assertRange(result.color.topLeft[0], 110, 111, "R color value");
      assertRange(result.color.topLeft[1], 110, 111, "G color value");
      assertRange(result.color.topLeft[2], 110, 111, "B color value");

      // top right
      assertRange(result.color.topRight[0], 55, 56, "R color value");
      assertRange(result.color.topRight[1], 155, 156, "G color value");
      assertRange(result.color.topRight[2], 155, 156, "B color value");

      // bottom left
      assertRange(result.color.bottomLeft[0], 105, 106, "R color value");
      assertRange(result.color.bottomLeft[1], 55, 56, "G color value");
      assertRange(result.color.bottomLeft[2], 105, 106, "B color value");

      // bottom right
      assertRange(result.color.bottomRight[0], 52, 53, "R color value");
      assertRange(result.color.bottomRight[1], 127, 128, "G color value");
      assertRange(result.color.bottomRight[2], 152, 153, "B color value");
    }
  );
});

add_task(async function test_fullpageScreenshotScrolled() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      await SpecialPowers.spawn(browser, [], () => {
        content.scrollTo(0, 2008);
      });
      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");
      let devicePixelRatio = await getContentDevicePixelRatio(browser);

      let expectedWidth = Math.floor(
        devicePixelRatio * contentInfo.scrollWidth
      );
      let expectedHeight = Math.floor(
        devicePixelRatio * contentInfo.scrollHeight
      );

      // click toolbar button so panel shows
      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      let panel = await helper.waitForPanel();

      let screenshotReady = TestUtils.topicObserved(
        "screenshots-preview-ready"
      );

      // click the full page button in panel
      let visiblePage = panel
        .querySelector("screenshots-buttons")
        .shadowRoot.querySelector(".full-page");
      visiblePage.click();

      let dialog = helper.getDialog();

      await screenshotReady;

      let copyButton = dialog._frame.contentDocument.getElementById("copy");
      ok(copyButton, "Got the copy button");

      let clipboardChanged = helper.waitForRawClipboardChange(
        expectedWidth,
        expectedHeight
      );

      // click copy button on dialog box
      copyButton.click();

      info("Waiting for clipboard change");
      let result = await clipboardChanged;

      info("result: " + JSON.stringify(result, null, 2));
      info("contentInfo: " + JSON.stringify(contentInfo, null, 2));

      Assert.equal(result.width, expectedWidth, "Widths should be equal");
      Assert.equal(result.height, expectedHeight, "Heights should be equal");

      // top left
      assertRange(result.color.topLeft[0], 110, 111, "R color value");
      assertRange(result.color.topLeft[1], 110, 111, "G color value");
      assertRange(result.color.topLeft[2], 110, 111, "B color value");

      // top right
      assertRange(result.color.topRight[0], 55, 56, "R color value");
      assertRange(result.color.topRight[1], 155, 156, "G color value");
      assertRange(result.color.topRight[2], 155, 156, "B color value");

      // bottom left
      assertRange(result.color.bottomLeft[0], 105, 106, "R color value");
      assertRange(result.color.bottomLeft[1], 55, 56, "G color value");
      assertRange(result.color.bottomLeft[2], 105, 106, "B color value");

      // bottom right
      assertRange(result.color.bottomRight[0], 52, 53, "R color value");
      assertRange(result.color.bottomRight[1], 127, 128, "G color value");
      assertRange(result.color.bottomRight[2], 152, 153, "B color value");
    }
  );
});
