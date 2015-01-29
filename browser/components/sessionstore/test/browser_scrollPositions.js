/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = ROOT + "browser_scrollPositions_sample.html";
const URL_FRAMESET = ROOT + "browser_scrollPositions_sample_frameset.html";

// Randomized set of scroll positions we will use in this test.
const SCROLL_X = Math.round(100 * (1 + Math.random()));
const SCROLL_Y = Math.round(200 * (1 + Math.random()));
const SCROLL_STR = SCROLL_X + "," + SCROLL_Y;

const SCROLL2_X = Math.round(300 * (1 + Math.random()));
const SCROLL2_Y = Math.round(400 * (1 + Math.random()));
const SCROLL2_STR = SCROLL2_X + "," + SCROLL2_Y;

/**
 * This test ensures that we properly serialize and restore scroll positions
 * for an average page without any frames.
 */
add_task(function test_scroll() {
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Scroll down a little.
  yield sendMessage(browser, "ss-test:setScrollPosition", {x: SCROLL_X, y: SCROLL_Y});
  checkScroll(tab, {scroll: SCROLL_STR}, "scroll is fine");

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
  checkScroll(tab2, {scroll: SCROLL_STR}, "reloading retains scroll positions");

  // Check that a force-reload resets scroll positions.
  browser2.reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE);
  yield promiseBrowserLoaded(browser2);
  checkScroll(tab2, null, "force-reload resets scroll positions");

  // Scroll back to the top and check that the position has been reset. We
  // expect the scroll position to be "null" here because there is no data to
  // be stored if the frame is in its default scroll position.
  yield sendMessage(browser, "ss-test:setScrollPosition", {x: 0, y: 0});
  checkScroll(tab, null, "no scroll stored");

  // Cleanup.
  gBrowser.removeTab(tab);
  gBrowser.removeTab(tab2);
});

/**
 * This tests ensures that we properly serialize and restore scroll positions
 * for multiple frames of pages with framesets.
 */
add_task(function test_scroll_nested() {
  let tab = gBrowser.addTab(URL_FRAMESET);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Scroll the first child frame down a little.
  yield sendMessage(browser, "ss-test:setScrollPosition", {x: SCROLL_X, y: SCROLL_Y, frame: 0});
  checkScroll(tab, {children: [{scroll: SCROLL_STR}]}, "scroll is fine");

  // Scroll the second child frame down a little.
  yield sendMessage(browser, "ss-test:setScrollPosition", {x: SCROLL2_X, y: SCROLL2_Y, frame: 1});
  checkScroll(tab, {children: [{scroll: SCROLL_STR}, {scroll: SCROLL2_STR}]}, "scroll is fine");

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
  checkScroll(tab, {children: [null, {scroll: SCROLL2_STR}]}, "scroll is fine");

  // Check the resetting all frames' scroll positions nulls the stored value.
  yield sendMessage(browser, "ss-test:setScrollPosition", {x: 0, y: 0, frame: 1});
  checkScroll(tab, null, "no scroll stored");

  // Cleanup.
  gBrowser.removeTab(tab);
  gBrowser.removeTab(tab2);
});

/**
 * This test ensures that by moving scroll positions out of tabData.entries[]
 * we still support the old scroll data format stored per shistory entry.
 */
add_task(function test_scroll_old_format() {
  const TAB_STATE = { entries: [{url: URL, scroll: SCROLL_STR}] };

  // Add a blank tab.
  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Apply the tab state with the old format.
  yield promiseTabState(tab, TAB_STATE);

  // Check that the scroll positions has been applied.
  let scroll = yield sendMessage(browser, "ss-test:getScrollPosition");
  is(JSON.stringify(scroll), JSON.stringify({x: SCROLL_X, y: SCROLL_Y}),
    "scroll position has been restored correctly");

  // Cleanup.
  gBrowser.removeTab(tab);
});

function checkScroll(tab, expected, msg) {
  let browser = tab.linkedBrowser;
  TabState.flush(browser);

  let scroll = JSON.parse(ss.getTabState(tab)).scroll || null;
  is(JSON.stringify(scroll), JSON.stringify(expected), msg);
}
