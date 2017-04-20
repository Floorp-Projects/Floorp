/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that even if the user has set their tabs to restore
 * immediately on session start, that background tabs after a
 * content process crash restore on demand.
 */

"use strict";

const PAGE_1 = "data:text/html,<html><body>A%20regular,%20everyday,%20normal%20page.";
const PAGE_2 = "data:text/html,<html><body>Another%20regular,%20everyday,%20normal%20page.";

add_task(function* setup() {
  yield pushPrefs(["dom.ipc.processCount", 1],
                  ["toolkit.cosmeticAnimations.enabled", false],
                  ["browser.sessionstore.restore_on_demand", false]);
});

add_task(function* test_revive_bg_tabs_on_demand() {
  let newTab1 = gBrowser.addTab(PAGE_1);
  let browser1 = newTab1.linkedBrowser;
  gBrowser.selectedTab = newTab1;

  let newTab2 = gBrowser.addTab(PAGE_2);
  let browser2 = newTab2.linkedBrowser;

  yield BrowserTestUtils.browserLoaded(browser1);
  yield BrowserTestUtils.browserLoaded(browser2);

  yield TabStateFlusher.flush(browser2);

  // Now crash the selected tab
  let windowReady = BrowserTestUtils.waitForEvent(window, "SSWindowStateReady");
  yield BrowserTestUtils.crashBrowser(browser1);

  ok(newTab1.hasAttribute("crashed"), "Selected tab should be crashed");
  ok(!newTab2.hasAttribute("crashed"), "Background tab should not be crashed");

  // Wait until we've had a chance to restore all tabs immediately
  yield windowReady;

  // But we should not have restored the background tab
  ok(newTab2.hasAttribute("pending"), "Background tab should be pending");

  // Now select newTab2 to make sure it restores.
  let newTab2Restored = promiseTabRestored(newTab2);
  gBrowser.selectedTab = newTab2;
  yield newTab2Restored;

  ok(browser2.isRemoteBrowser, "Restored browser should be remote");

  yield BrowserTestUtils.removeTab(newTab1);
  yield BrowserTestUtils.removeTab(newTab2);
});
