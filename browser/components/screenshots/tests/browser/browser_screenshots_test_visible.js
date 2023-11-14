/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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

      // top left
      Assert.equal(111, result.color.topLeft[0], "R color value");
      Assert.equal(111, result.color.topLeft[1], "G color value");
      Assert.equal(111, result.color.topLeft[2], "B color value");

      // top right
      Assert.equal(111, result.color.topRight[0], "R color value");
      Assert.equal(111, result.color.topRight[1], "G color value");
      Assert.equal(111, result.color.topRight[2], "B color value");

      // bottom left
      Assert.equal(111, result.color.bottomLeft[0], "R color value");
      Assert.equal(111, result.color.bottomLeft[1], "G color value");
      Assert.equal(111, result.color.bottomLeft[2], "B color value");

      // bottom right
      Assert.equal(111, result.color.bottomRight[0], "R color value");
      Assert.equal(111, result.color.bottomRight[1], "G color value");
      Assert.equal(111, result.color.bottomRight[2], "B color value");
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

      // let result = await helper.getImageSizeAndColorFromClipboard();
      // debugger;

      info("result: " + JSON.stringify(result, null, 2));
      info("contentInfo: " + JSON.stringify(contentInfo, null, 2));

      Assert.equal(result.width, expectedWidth, "Widths should be equal");
      Assert.equal(result.height, expectedHeight, "Heights should be equal");

      // top left
      Assert.equal(105, result.color.topLeft[0], "R color value");
      Assert.equal(55, result.color.topLeft[1], "G color value");
      Assert.equal(105, result.color.topLeft[2], "B color value");

      // top right
      Assert.equal(105, result.color.topRight[0], "R color value");
      Assert.equal(55, result.color.topRight[1], "G color value");
      Assert.equal(105, result.color.topRight[2], "B color value");

      // bottom left
      Assert.equal(105, result.color.bottomLeft[0], "R color value");
      Assert.equal(55, result.color.bottomLeft[1], "G color value");
      Assert.equal(105, result.color.bottomLeft[2], "B color value");

      // bottom right
      Assert.equal(105, result.color.bottomRight[0], "R color value");
      Assert.equal(55, result.color.bottomRight[1], "G color value");
      Assert.equal(105, result.color.bottomRight[2], "B color value");
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

      // top left
      Assert.equal(55, result.color.topLeft[0], "R color value");
      Assert.equal(155, result.color.topLeft[1], "G color value");
      Assert.equal(155, result.color.topLeft[2], "B color value");

      // top right
      Assert.equal(55, result.color.topRight[0], "R color value");
      Assert.equal(155, result.color.topRight[1], "G color value");
      Assert.equal(155, result.color.topRight[2], "B color value");

      // bottom left
      Assert.equal(55, result.color.bottomLeft[0], "R color value");
      Assert.equal(155, result.color.bottomLeft[1], "G color value");
      Assert.equal(155, result.color.bottomLeft[2], "B color value");

      // bottom right
      Assert.equal(55, result.color.bottomRight[0], "R color value");
      Assert.equal(155, result.color.bottomRight[1], "G color value");
      Assert.equal(155, result.color.bottomRight[2], "B color value");
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

      // top left
      Assert.equal(52, result.color.topLeft[0], "R color value");
      Assert.equal(127, result.color.topLeft[1], "G color value");
      Assert.equal(152, result.color.topLeft[2], "B color value");

      // top right
      Assert.equal(52, result.color.topRight[0], "R color value");
      Assert.equal(127, result.color.topRight[1], "G color value");
      Assert.equal(152, result.color.topRight[2], "B color value");

      // bottom left
      Assert.equal(52, result.color.bottomLeft[0], "R color value");
      Assert.equal(127, result.color.bottomLeft[1], "G color value");
      Assert.equal(152, result.color.bottomLeft[2], "B color value");

      // bottom right
      Assert.equal(52, result.color.bottomRight[0], "R color value");
      Assert.equal(127, result.color.bottomRight[1], "G color value");
      Assert.equal(152, result.color.bottomRight[2], "B color value");
    }
  );
});
