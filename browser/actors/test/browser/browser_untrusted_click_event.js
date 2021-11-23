/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

/**
 * Ensure that click handlers that prevent the default action but fire
 * a different click event that does open a tab are allowed to work.
 */
add_task(async function test_untrusted_click_opens_tab() {
  await BrowserTestUtils.withNewTab(TEST_PATH + "click.html", async browser => {
    let newTabOpened = BrowserTestUtils.waitForNewTab(
      gBrowser,
      "https://example.org/",
      true
    );
    info("clicking link with modifier pressed.");
    let eventObj = {};
    eventObj[AppConstants.platform == "macosx" ? "metaKey" : "ctrlKey"] = true;
    await BrowserTestUtils.synthesizeMouseAtCenter("#test", eventObj, browser);
    info("Waiting for new tab to open; if we timeout the test is broken.");
    let newTab = await newTabOpened;
    ok(newTab, "New tab should be opened.");
    BrowserTestUtils.removeTab(newTab);
  });
});
