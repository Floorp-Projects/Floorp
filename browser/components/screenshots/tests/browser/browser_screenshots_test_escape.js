/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SCREENSHOTS_EVENTS = [
  { category: "screenshots", method: "started", object: "toolbar_button" },
  { category: "screenshots", method: "canceled", object: "escape" },
];

add_task(async function test_fullpageScreenshot() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await clearAllTelemetryEvents();
      let helper = new ScreenshotsHelper(browser);

      // click toolbar button so panel shows
      helper.triggerUIFromToolbar();

      await helper.waitForOverlay();

      EventUtils.synthesizeKey("KEY_F6", { shiftKey: true });

      EventUtils.synthesizeKey("KEY_Escape");

      await helper.waitForOverlayClosed();

      await assertScreenshotsEvents(SCREENSHOTS_EVENTS);
    }
  );
});
