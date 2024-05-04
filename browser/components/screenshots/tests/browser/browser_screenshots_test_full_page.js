/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function assertPixel(actual, expected, message) {
  info(message);
  isfuzzy(actual[0], expected[0], 1, "R color value");
  isfuzzy(actual[1], expected[1], 1, "G color value");
  isfuzzy(actual[2], expected[2], 1, "B color value");
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
      let fullpageButton = panel
        .querySelector("screenshots-buttons")
        .shadowRoot.querySelector("#full-page");
      fullpageButton.click();

      await screenshotReady;

      let copyButton = helper.getDialogButton("copy");
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

      // Due to https://bugzil.la/1396587, the pasted image colors differ from
      // the original image on macOS. Once that bug is fixed, we can remove the
      // special check for macOS.
      if (AppConstants.platform === "macosx") {
        assertPixel(result.color.topLeft, [130, 130, 130], "Top left pixel");
        assertPixel(result.color.topRight, [66, 170, 171], "Top right pixel");
        assertPixel(
          result.color.bottomLeft,
          [125, 75, 125],
          "Bottom left pixel"
        );
        assertPixel(
          result.color.bottomRight,
          [64, 145, 169],
          "Bottom right pixel"
        );
      } else {
        assertPixel(result.color.topLeft, [111, 111, 111], "Top left pixel");
        assertPixel(result.color.topRight, [55, 155, 155], "Top right pixel");
        assertPixel(
          result.color.bottomLeft,
          [105, 55, 105],
          "Bottom left pixel"
        );
        assertPixel(
          result.color.bottomRight,
          [52, 127, 152],
          "Bottom right pixel"
        );
      }
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
      let fullpageButton = panel
        .querySelector("screenshots-buttons")
        .shadowRoot.querySelector("#full-page");
      fullpageButton.click();

      await screenshotReady;

      let copyButton = helper.getDialogButton("copy");
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

      // Due to https://bugzil.la/1396587, the pasted image colors differ from
      // the original image on macOS. Once that bug is fixed, we can remove the
      // special check for macOS.
      if (AppConstants.platform === "macosx") {
        assertPixel(result.color.topLeft, [130, 130, 130], "Top left pixel");
        assertPixel(result.color.topRight, [66, 170, 171], "Top right pixel");
        assertPixel(
          result.color.bottomLeft,
          [125, 75, 125],
          "Bottom left pixel"
        );
        assertPixel(
          result.color.bottomRight,
          [64, 145, 169],
          "Bottom right pixel"
        );
      } else {
        assertPixel(result.color.topLeft, [111, 111, 111], "Top left pixel");
        assertPixel(result.color.topRight, [55, 155, 155], "Top right pixel");
        assertPixel(
          result.color.bottomLeft,
          [105, 55, 105],
          "Bottom left pixel"
        );
        assertPixel(
          result.color.bottomRight,
          [52, 127, 152],
          "Bottom right pixel"
        );
      }
    }
  );
});

add_task(async function test_fullpageScreenshotRTL() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: RTL_TEST_PAGE,
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
      let fullpageButton = panel
        .querySelector("screenshots-buttons")
        .shadowRoot.querySelector("#full-page");
      fullpageButton.click();

      await screenshotReady;

      let copyButton = helper.getDialogButton("copy");
      ok(copyButton, "Got the copy button");

      info("contentInfo: " + JSON.stringify(contentInfo, null, 2));
      info(
        "expecting: " +
          JSON.stringify({ expectedWidth, expectedHeight }, null, 2)
      );
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

      assertPixel(result.color.topLeft, [255, 255, 255], "Top left pixel");
      assertPixel(result.color.topRight, [255, 255, 255], "Top right pixel");
      assertPixel(
        result.color.bottomLeft,
        [255, 255, 255],
        "Bottom left pixel"
      );
      assertPixel(
        result.color.bottomRight,
        [255, 255, 255],
        "Bottom right pixel"
      );
    }
  );
});
