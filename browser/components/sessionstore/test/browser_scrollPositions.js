/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BASE = "http://example.com/browser/browser/components/sessionstore/test/"
const URL = BASE + "browser_scrollPositions_sample.html";
const URL2 = BASE + "browser_scrollPositions_sample2.html";
const URL_FRAMESET = BASE + "browser_scrollPositions_sample_frameset.html";

// Randomized set of scroll positions we will use in this test.
const SCROLL_X = Math.round(100 * (1 + Math.random()));
const SCROLL_Y = Math.round(200 * (1 + Math.random()));
const SCROLL_STR = SCROLL_X + "," + SCROLL_Y;

const SCROLL2_X = Math.round(300 * (1 + Math.random()));
const SCROLL2_Y = Math.round(400 * (1 + Math.random()));
const SCROLL2_STR = SCROLL2_X + "," + SCROLL2_Y;

requestLongerTimeout(2);

/**
 * This test ensures that we properly serialize and restore scroll positions
 * for an average page without any frames.
 */
add_task(function* test_scroll() {
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Scroll down a little.
  yield sendMessage(browser, "ss-test:setScrollPosition", {x: SCROLL_X, y: SCROLL_Y});
  yield checkScroll(tab, {scroll: SCROLL_STR}, "scroll is fine");

  // Duplicate and check that the scroll position is restored.
  let tab2 = ss.duplicateTab(window, tab);
  let browser2 = tab2.linkedBrowser;
  yield promiseTabRestored(tab2);

  let scroll = yield sendMessage(browser2, "ss-test:getScrollPosition");
  is(JSON.stringify(scroll), JSON.stringify({x: SCROLL_X, y: SCROLL_Y}),
    "scroll position has been duplicated correctly");

  // Check that reloading retains the scroll positions.
  browser2.reload();
  yield promiseBrowserLoaded(browser2);
  yield checkScroll(tab2, {scroll: SCROLL_STR}, "reloading retains scroll positions");

  // Check that a force-reload resets scroll positions.
  browser2.reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE);
  yield promiseBrowserLoaded(browser2);
  yield checkScroll(tab2, null, "force-reload resets scroll positions");

  // Scroll back to the top and check that the position has been reset. We
  // expect the scroll position to be "null" here because there is no data to
  // be stored if the frame is in its default scroll position.
  yield sendMessage(browser, "ss-test:setScrollPosition", {x: 0, y: 0});
  yield checkScroll(tab, null, "no scroll stored");

  // Cleanup.
  yield promiseRemoveTab(tab);
  yield promiseRemoveTab(tab2);
});

/**
 * This tests ensures that we properly serialize and restore scroll positions
 * for multiple frames of pages with framesets.
 */
add_task(function* test_scroll_nested() {
  let tab = gBrowser.addTab(URL_FRAMESET);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Scroll the first child frame down a little.
  yield sendMessage(browser, "ss-test:setScrollPosition", {x: SCROLL_X, y: SCROLL_Y, frame: 0});
  yield checkScroll(tab, {children: [{scroll: SCROLL_STR}]}, "scroll is fine");

  // Scroll the second child frame down a little.
  yield sendMessage(browser, "ss-test:setScrollPosition", {x: SCROLL2_X, y: SCROLL2_Y, frame: 1});
  yield checkScroll(tab, {children: [{scroll: SCROLL_STR}, {scroll: SCROLL2_STR}]}, "scroll is fine");

  // Duplicate and check that the scroll position is restored.
  let tab2 = ss.duplicateTab(window, tab);
  let browser2 = tab2.linkedBrowser;
  yield promiseTabRestored(tab2);

  let scroll = yield sendMessage(browser2, "ss-test:getScrollPosition", {frame: 0});
  is(JSON.stringify(scroll), JSON.stringify({x: SCROLL_X, y: SCROLL_Y}),
    "scroll position #1 has been duplicated correctly");

  scroll = yield sendMessage(browser2, "ss-test:getScrollPosition", {frame: 1});
  is(JSON.stringify(scroll), JSON.stringify({x: SCROLL2_X, y: SCROLL2_Y}),
    "scroll position #2 has been duplicated correctly");

  // Check that resetting one frame's scroll position removes it from the
  // serialized value.
  yield sendMessage(browser, "ss-test:setScrollPosition", {x: 0, y: 0, frame: 0});
  yield checkScroll(tab, {children: [null, {scroll: SCROLL2_STR}]}, "scroll is fine");

  // Check the resetting all frames' scroll positions nulls the stored value.
  yield sendMessage(browser, "ss-test:setScrollPosition", {x: 0, y: 0, frame: 1});
  yield checkScroll(tab, null, "no scroll stored");

  // Cleanup.
  yield promiseRemoveTab(tab);
  yield promiseRemoveTab(tab2);
});

/**
 * Test that scroll positions persist after restoring background tabs in
 * a restored window (bug 1228518).
 * Also test that scroll positions for previous session history entries
 * are preserved as well (bug 1265818).
 */
add_task(function* test_scroll_background_tabs() {
  pushPrefs(["browser.sessionstore.restore_on_demand", true]);

  let newWin = yield BrowserTestUtils.openNewBrowserWindow();
  let tab = newWin.gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield BrowserTestUtils.browserLoaded(browser);

  // Scroll down a little.
  yield sendMessage(browser, "ss-test:setScrollPosition", {x: SCROLL_X, y: SCROLL_Y});
  yield checkScroll(tab, {scroll: SCROLL_STR}, "scroll on first page is fine");

  // Navigate to a different page and scroll there as well.
  browser.loadURI(URL2);
  yield BrowserTestUtils.browserLoaded(browser);

  // Scroll down a little.
  yield sendMessage(browser, "ss-test:setScrollPosition", {x: SCROLL2_X, y: SCROLL2_Y});
  yield checkScroll(tab, {scroll: SCROLL2_STR}, "scroll on second page is fine");

  // Close the window
  yield BrowserTestUtils.closeWindow(newWin);

  yield forceSaveState();

  // Now restore the window
  newWin = ss.undoCloseWindow(0);

  // Make sure to wait for the window to be restored.
  yield BrowserTestUtils.waitForEvent(newWin, "SSWindowStateReady");

  is(newWin.gBrowser.tabs.length, 2, "There should be two tabs");

  // The second tab should be the one we loaded URL at still
  tab = newWin.gBrowser.tabs[1];

  ok(tab.hasAttribute("pending"), "Tab should be pending");
  browser = tab.linkedBrowser;

  // Ensure there are no pending queued messages in the child.
  yield TabStateFlusher.flush(browser);

  // Now check to see if the background tab remembers where it
  // should be scrolled to.
  newWin.gBrowser.selectedTab = tab;
  yield promiseTabRestored(tab);

  yield checkScroll(tab, {scroll: SCROLL2_STR}, "scroll is still fine");

  // Now go back in history and check that the scroll position
  // is restored there as well.
  is(browser.canGoBack, true, "can go back");
  browser.goBack();

  yield BrowserTestUtils.browserLoaded(browser);
  yield TabStateFlusher.flush(browser);

  yield checkScroll(tab, {scroll: SCROLL_STR}, "scroll is still fine after navigating back");

  yield BrowserTestUtils.closeWindow(newWin);
});
