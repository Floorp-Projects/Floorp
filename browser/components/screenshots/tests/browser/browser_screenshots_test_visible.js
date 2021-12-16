/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_visibleScreenshot() {
  CustomizableUI.addWidgetToArea(
    "screenshot-button",
    CustomizableUI.AREA_NAVBAR
  );
  let screenshotBtn = document.getElementById("screenshot-button");
  Assert.ok(screenshotBtn, "The screenshots button was added to the nav bar");

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

      // click the visible page button in panel
      let visiblePageButton = panel
        .querySelector("screenshots-buttons")
        .shadowRoot.querySelector(".visible-page");
      visiblePageButton.click();

      let dialog = helper.getDialog();

      let img;
      await TestUtils.waitForCondition(() => {
        img = dialog._frame.contentDocument.querySelector(
          "img#placeholder-image"
        );
        if (img) {
          return true;
        }
        return false;
      }, "Waiting for img to be added to dialog box");

      ok(img, "Screenshot exists and is in the dialog box");

      let copyButton = dialog._frame.contentDocument.querySelector(
        ".highlight-button-copy"
      );
      ok(copyButton, "Got the copy button");

      let clipboardChanged = helper.waitForRawClipboardChange();

      // click copy button on dialog box
      copyButton.click();

      info("Waiting for clipboard change");
      await clipboardChanged;

      let result = await helper.getImageSizeFromClipboard();
      info("result: " + JSON.stringify(result, null, 2));
      info("contentInfo: " + JSON.stringify(contentInfo, null, 2));

      Assert.equal(
        contentInfo.width,
        result.clientWidth,
        "Widths should be equal"
      );

      Assert.equal(
        contentInfo.height,
        result.clientHeight,
        "Heights should be equal"
      );
    }
  );
});
