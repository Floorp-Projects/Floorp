/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SCREENSHOTS_EVENTS = [
  { category: "screenshots", method: "started", object: "toolbar_button" },
  { category: "screenshots", method: "canceled", object: "escape" },
];

add_task(async function testPanelFocused() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await clearAllTelemetryEvents();
      let helper = new ScreenshotsHelper(browser);

      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();

      let screenshotsButtons = gBrowser.selectedBrowser.ownerDocument
        .querySelector("#screenshotsPagePanel")
        .querySelector("screenshots-buttons").shadowRoot;

      let focusedElement = screenshotsButtons.querySelector(".visible-page");

      is(
        focusedElement,
        screenshotsButtons.activeElement,
        "Visible button is focused"
      );

      EventUtils.synthesizeKey("KEY_Escape");

      await helper.waitForOverlayClosed();

      await assertScreenshotsEvents(SCREENSHOTS_EVENTS);
    }
  );
});
