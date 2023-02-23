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
      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");

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

      let copyButton = dialog._frame.contentDocument.querySelector(
        ".highlight-button-copy"
      );
      ok(copyButton, "Got the copy button");

      let clipboardChanged = helper.waitForRawClipboardChange();

      // click copy button on dialog box
      copyButton.click();

      info("Waiting for clipboard change");
      await clipboardChanged;

      let result = await helper.getImageSizeAndColorFromClipboard();
      info("result: " + JSON.stringify(result, null, 2));
      info("contentInfo: " + JSON.stringify(contentInfo, null, 2));

      Assert.equal(
        contentInfo.scrollWidth,
        result.width,
        "Widths should be equal"
      );

      Assert.equal(
        contentInfo.scrollHeight,
        result.height,
        "Heights should be equal"
      );

      // top left
      Assert.equal(111, result.color.topLeft[0], "R color value");
      Assert.equal(111, result.color.topLeft[1], "G color value");
      Assert.equal(111, result.color.topLeft[2], "B color value");

      // top right
      Assert.equal(55, result.color.topRight[0], "R color value");
      Assert.equal(155, result.color.topRight[1], "G color value");
      Assert.equal(155, result.color.topRight[2], "B color value");

      // bottom left
      Assert.equal(105, result.color.bottomLeft[0], "R color value");
      Assert.equal(55, result.color.bottomLeft[1], "G color value");
      Assert.equal(105, result.color.bottomLeft[2], "B color value");

      // bottom right
      Assert.equal(52, result.color.bottomRight[0], "R color value");
      Assert.equal(127, result.color.bottomRight[1], "G color value");
      Assert.equal(152, result.color.bottomRight[2], "B color value");
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
      await SpecialPowers.spawn(browser, [], () => {
        content.scrollTo(0, 2008);
      });

      let helper = new ScreenshotsHelper(browser);
      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");

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

      let copyButton = dialog._frame.contentDocument.querySelector(
        ".highlight-button-copy"
      );
      ok(copyButton, "Got the copy button");

      let clipboardChanged = helper.waitForRawClipboardChange();

      // click copy button on dialog box
      copyButton.click();

      info("Waiting for clipboard change");
      await clipboardChanged;

      let result = await helper.getImageSizeAndColorFromClipboard();
      info("result: " + JSON.stringify(result, null, 2));
      info("contentInfo: " + JSON.stringify(contentInfo, null, 2));

      Assert.equal(
        contentInfo.scrollWidth,
        result.width,
        "Widths should be equal"
      );

      Assert.equal(
        contentInfo.scrollHeight,
        result.height,
        "Heights should be equal"
      );

      // top left
      Assert.equal(111, result.color.topLeft[0], "R color value");
      Assert.equal(111, result.color.topLeft[1], "G color value");
      Assert.equal(111, result.color.topLeft[2], "B color value");

      // top right
      Assert.equal(55, result.color.topRight[0], "R color value");
      Assert.equal(155, result.color.topRight[1], "G color value");
      Assert.equal(155, result.color.topRight[2], "B color value");

      // bottom left
      Assert.equal(105, result.color.bottomLeft[0], "R color value");
      Assert.equal(55, result.color.bottomLeft[1], "G color value");
      Assert.equal(105, result.color.bottomLeft[2], "B color value");

      // bottom right
      Assert.equal(52, result.color.bottomRight[0], "R color value");
      Assert.equal(127, result.color.bottomRight[1], "G color value");
      Assert.equal(152, result.color.bottomRight[2], "B color value");
    }
  );
});
