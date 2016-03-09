"use strict";

const SLOW_PAGE = "http://www.example.com/browser/browser/base/content/test/urlbar/slow-page.sjs";
const SLOW_PAGE2 = "http://mochi.test:8888/browser/browser/base/content/test/urlbar/slow-page.sjs?faster";

/**
 * Check that if we:
 * 1) have a loaded page
 * 2) load a separate URL
 * 3) before the URL for step 2 has finished loading, load a third URL
 * we don't revert to the URL from (1).
 */
add_task(function*() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com", true, true);

  let expectedURLBarChange = SLOW_PAGE;
  let sawChange = false;
  let handler = e => {
    sawChange = true;
    is(gURLBar.value, expectedURLBarChange, "Should not change URL bar value!");
  };

  let obs = new MutationObserver(handler);

  obs.observe(gURLBar, {attributes: true});
  gURLBar.value = SLOW_PAGE;
  gURLBar.handleCommand();

  // If this ever starts going intermittent, we've broken this.
  yield new Promise(resolve => setTimeout(resolve, 200));
  expectedURLBarChange = SLOW_PAGE2;
  let pageLoadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  gURLBar.value = expectedURLBarChange;
  gURLBar.handleCommand();
  is(gURLBar.value, expectedURLBarChange, "Should not have changed URL bar value synchronously.");
  yield pageLoadPromise;
  ok(sawChange, "The URL bar change handler should have been called by the time the page was loaded");
  obs.disconnect();
  obs = null;
  yield BrowserTestUtils.removeTab(tab);
});

