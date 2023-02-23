/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testPanelFocused() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);

      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();

      EventUtils.synthesizeKey("KEY_Tab");

      let screenshotsButtons = gBrowser.selectedBrowser.ownerDocument
        .querySelector("#screenshotsPagePanel")
        .querySelector("screenshots-buttons").shadowRoot;

      let focusedElement = screenshotsButtons.querySelector(".visible-page");

      is(
        focusedElement,
        screenshotsButtons.activeElement,
        "Visible button is focused"
      );
    }
  );
});
