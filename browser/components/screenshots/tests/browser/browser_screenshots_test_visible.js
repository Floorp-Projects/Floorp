/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function assertPixel(actual, expected, message) {
  info(message);
  isfuzzy(actual[0], expected[0], 1, "R color value");
  isfuzzy(actual[1], expected[1], 1, "G color value");
  isfuzzy(actual[2], expected[2], 1, "B color value");
}

add_task(async function test_visibleScreenshot() {
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
        devicePixelRatio * contentInfo.clientWidth
      );
      let expectedHeight = Math.floor(
        devicePixelRatio * contentInfo.clientHeight
      );

      // click toolbar button so panel shows
      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();

      let screenshotReady = TestUtils.topicObserved(
        "screenshots-preview-ready"
      );

      let panel = gBrowser.selectedBrowser.ownerDocument.querySelector(
        "#screenshotsPagePanel"
      );

      // click the visible page button in panel
      let visiblePageButton = panel
        .querySelector("screenshots-buttons")
        .shadowRoot.querySelector(".visible-page");
      visiblePageButton.click();

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

      // Due to https://bugzil.la/1396587, the pasted image colors differ from
      // the original image on macOS. Once that bug is fixed, we can remove the
      // special check for macOS.
      if (AppConstants.platform === "macosx") {
        assertPixel(result.color.topLeft, [130, 130, 130], "Top left pixel");
        assertPixel(result.color.topRight, [130, 130, 130], "Top right pixel");
        assertPixel(
          result.color.bottomLeft,
          [130, 130, 130],
          "Bottom left pixel"
        );
        assertPixel(
          result.color.bottomRight,
          [130, 130, 130],
          "Bottom right pixel"
        );
      } else {
        assertPixel(result.color.topLeft, [111, 111, 111], "Top left pixel");
        assertPixel(result.color.topRight, [111, 111, 111], "Top right pixel");
        assertPixel(
          result.color.bottomLeft,
          [111, 111, 111],
          "Bottom left pixel"
        );
        assertPixel(
          result.color.bottomRight,
          [111, 111, 111],
          "Bottom right pixel"
        );
      }
    }
  );
});

add_task(async function test_visibleScreenshotScrolledY() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], () => {
        content.scrollTo(0, 2008);
      });

      let helper = new ScreenshotsHelper(browser);
      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");
      let devicePixelRatio = await getContentDevicePixelRatio(browser);

      let expectedWidth = Math.floor(
        devicePixelRatio * contentInfo.clientWidth
      );
      let expectedHeight = Math.floor(
        devicePixelRatio * contentInfo.clientHeight
      );

      // click toolbar button so panel shows
      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      let panel = gBrowser.selectedBrowser.ownerDocument.querySelector(
        "#screenshotsPagePanel"
      );

      let screenshotReady = TestUtils.topicObserved(
        "screenshots-preview-ready"
      );

      // click the visible page button in panel
      let visiblePageButton = panel
        .querySelector("screenshots-buttons")
        .shadowRoot.querySelector(".visible-page");
      visiblePageButton.click();

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

      // Due to https://bugzil.la/1396587, the pasted image colors differ from
      // the original image on macOS. Once that bug is fixed, we can remove the
      // special check for macOS.
      if (AppConstants.platform === "macosx") {
        assertPixel(result.color.topLeft, [125, 75, 125], "Top left pixel");
        assertPixel(result.color.topRight, [125, 75, 125], "Top right pixel");
        assertPixel(
          result.color.bottomLeft,
          [125, 75, 125],
          "Bottom left pixel"
        );
        assertPixel(
          result.color.bottomRight,
          [125, 75, 125],
          "Bottom right pixel"
        );
      } else {
        assertPixel(result.color.topLeft, [105, 55, 105], "Top left pixel");
        assertPixel(result.color.topRight, [105, 55, 105], "Top right pixel");
        assertPixel(
          result.color.bottomLeft,
          [105, 55, 105],
          "Bottom left pixel"
        );
        assertPixel(
          result.color.bottomRight,
          [105, 55, 105],
          "Bottom right pixel"
        );
      }
    }
  );
});

add_task(async function test_visibleScreenshotScrolledX() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], () => {
        content.scrollTo(2004, 0);
      });

      let helper = new ScreenshotsHelper(browser);
      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");
      let devicePixelRatio = await getContentDevicePixelRatio(browser);

      let expectedWidth = Math.floor(
        devicePixelRatio * contentInfo.clientWidth
      );
      let expectedHeight = Math.floor(
        devicePixelRatio * contentInfo.clientHeight
      );

      // click toolbar button so panel shows
      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      let panel = gBrowser.selectedBrowser.ownerDocument.querySelector(
        "#screenshotsPagePanel"
      );

      let screenshotReady = TestUtils.topicObserved(
        "screenshots-preview-ready"
      );

      // click the visible page button in panel
      let visiblePageButton = panel
        .querySelector("screenshots-buttons")
        .shadowRoot.querySelector(".visible-page");
      visiblePageButton.click();

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

      // Due to https://bugzil.la/1396587, the pasted image colors differ from
      // the original image on macOS. Once that bug is fixed, we can remove the
      // special check for macOS.
      if (AppConstants.platform === "macosx") {
        assertPixel(result.color.topLeft, [66, 170, 171], "Top left pixel");
        assertPixel(result.color.topRight, [66, 170, 171], "Top right pixel");
        assertPixel(
          result.color.bottomLeft,
          [66, 170, 171],
          "Bottom left pixel"
        );
        assertPixel(
          result.color.bottomRight,
          [66, 170, 171],
          "Bottom right pixel"
        );
      } else {
        assertPixel(result.color.topLeft, [55, 155, 155], "Top left pixel");
        assertPixel(result.color.topRight, [55, 155, 155], "Top right pixel");
        assertPixel(
          result.color.bottomLeft,
          [55, 155, 155],
          "Bottom left pixel"
        );
        assertPixel(
          result.color.bottomRight,
          [55, 155, 155],
          "Bottom right pixel"
        );
      }
    }
  );
});

add_task(async function test_visibleScreenshotScrolledXAndY() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], () => {
        content.scrollTo(2004, 2008);
      });

      let helper = new ScreenshotsHelper(browser);
      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");
      let devicePixelRatio = await getContentDevicePixelRatio(browser);

      let expectedWidth = Math.floor(
        devicePixelRatio * contentInfo.clientWidth
      );
      let expectedHeight = Math.floor(
        devicePixelRatio * contentInfo.clientHeight
      );

      // click toolbar button so panel shows
      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      let panel = gBrowser.selectedBrowser.ownerDocument.querySelector(
        "#screenshotsPagePanel"
      );

      let screenshotReady = TestUtils.topicObserved(
        "screenshots-preview-ready"
      );

      // click the visible page button in panel
      let visiblePageButton = panel
        .querySelector("screenshots-buttons")
        .shadowRoot.querySelector(".visible-page");
      visiblePageButton.click();

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

      // Due to https://bugzil.la/1396587, the pasted image colors differ from
      // the original image on macOS. Once that bug is fixed, we can remove the
      // special check for macOS.
      if (AppConstants.platform === "macosx") {
        assertPixel(result.color.topLeft, [64, 145, 169], "Top left pixel");
        assertPixel(result.color.topRight, [64, 145, 169], "Top right pixel");
        assertPixel(
          result.color.bottomLeft,
          [64, 145, 169],
          "Bottom left pixel"
        );
        assertPixel(
          result.color.bottomRight,
          [64, 145, 169],
          "Bottom right pixel"
        );
      } else {
        assertPixel(result.color.topLeft, [52, 127, 152], "Top left pixel");
        assertPixel(result.color.topRight, [52, 127, 152], "Top right pixel");
        assertPixel(
          result.color.bottomLeft,
          [52, 127, 152],
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
