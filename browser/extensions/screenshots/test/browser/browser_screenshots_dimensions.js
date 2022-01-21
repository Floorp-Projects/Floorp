add_task(async function test_fullPageScreenshot() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_GREEN_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);
      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");

      await helper.triggerUIFromToolbar();

      await helper.clickUIElement(
        helper.selector.preselectIframe,
        helper.selector.fullPageButton
      );

      info("Waiting for the preview UI and the copy button");
      await helper.waitForUIContent(
        helper.selector.previewIframe,
        helper.selector.copyButton
      );

      let deactivatedPromise = helper.waitForToolbarButtonDeactivation();
      let clipboardChanged = waitForRawClipboardChange();

      await helper.clickUIElement(
        helper.selector.previewIframe,
        helper.selector.copyButton
      );

      info("Waiting for clipboard change");
      await clipboardChanged;

      await deactivatedPromise;
      info("Screenshots is deactivated");

      let result = await getImageSizeFromClipboard(browser);
      is(
        result.width,
        contentInfo.documentWidth,
        "Got expected screenshot width"
      );
      is(
        result.height,
        contentInfo.documentHeight,
        "Got expected screenshot height"
      );
    }
  );
});

add_task(async function test_visiblePageScreenshot() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_GREEN_PAGE,
    },
    async browser => {
      const DEVICE_PIXEL_RATIO = window.devicePixelRatio;
      let helper = new ScreenshotsHelper(browser);
      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");

      await helper.triggerUIFromToolbar();

      await helper.clickUIElement(
        helper.selector.preselectIframe,
        helper.selector.visiblePageButton
      );

      info("Waiting for the preview UI and the copy button");
      await helper.waitForUIContent(
        helper.selector.previewIframe,
        helper.selector.copyButton
      );

      let deactivatedPromise = helper.waitForToolbarButtonDeactivation();
      let clipboardChanged = waitForRawClipboardChange();

      await helper.clickUIElement(
        helper.selector.previewIframe,
        helper.selector.copyButton
      );

      info("Waiting for clipboard change");
      await clipboardChanged;

      await deactivatedPromise;
      info("Screenshots is deactivated");

      let result = await getImageSizeFromClipboard(browser);
      info("result: " + JSON.stringify(result, null, 2));
      info("contentInfo: " + JSON.stringify(contentInfo, null, 2));

      is(
        result.width,
        contentInfo.documentWidth * DEVICE_PIXEL_RATIO,
        "Got expected screenshot width"
      );
      is(
        result.height,
        contentInfo.clientHeight * DEVICE_PIXEL_RATIO,
        "Got expected screenshot height"
      );
    }
  );
});
