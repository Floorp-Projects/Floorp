"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm");

function popPrefs() {
  return SpecialPowers.popPrefEnv();
}
function pushPrefs(...prefs) {
  return SpecialPowers.pushPrefEnv({set: prefs});
}

// Activity Stream tests expect it to be enabled, and make sure to clear out any
// preloaded browsers that might have about:newtab that we don't want to test
const ACTIVITY_STREAM_PREF = "browser.newtabpage.activity-stream.enabled";
pushPrefs([ACTIVITY_STREAM_PREF, true]);
gBrowser.removePreloadedBrowser();

async function clearHistoryAndBookmarks() { // eslint-disable-line no-unused-vars
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
}

/**
 * Helper to wait for potentially preloaded browsers to "load" where a preloaded
 * page has already loaded and won't trigger "load", and a "load"ed page might
 * not necessarily have had all its javascript/render logic executed.
 */
async function waitForPreloaded(browser) {
  let readyState = await ContentTask.spawn(browser, {}, () => content.document.readyState);
  if (readyState !== "complete") {
    await BrowserTestUtils.browserLoaded(browser);
  }
}

/**
 * Helper to force the HighlightsFeed to update.
 */
function refreshHighlightsFeed() {
  // Toggling the pref will clear the feed cache and force a places query.
  Services.prefs.setBoolPref("browser.newtabpage.activity-stream.feeds.section.highlights", false);
  Services.prefs.setBoolPref("browser.newtabpage.activity-stream.feeds.section.highlights", true);
}

/**
 * Helper to populate the Highlights section with bookmark cards.
 * @param count Number of items to add.
 */
async function addHighlightsBookmarks(count) { // eslint-disable-line no-unused-vars
  const bookmarks = new Array(count).fill(null).map((entry, i) => ({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "foo",
    url: `https://mozilla${i}.com/nowNew`
  }));

  for (let placeInfo of bookmarks) {
    await PlacesUtils.bookmarks.insert(placeInfo);
    // Bookmarks need at least one visit to show up as highlights.
    await PlacesTestUtils.addVisits(placeInfo.url);
  }

  // Force HighlightsFeed to make a request for the new items.
  refreshHighlightsFeed();
}

/**
 * Helper to run Activity Stream about:newtab test tasks in content.
 *
 * @param testInfo {Function|Object}
 *   {Function} This parameter will be used as if the function were called with
 *              an Object with this parameter as "test" key's value.
 *   {Object} The following keys are expected:
 *     before {Function} Optional. Runs before and returns an arg for "test"
 *     test   {Function} The test to run in the about:newtab content task taking
 *                       an arg from "before" and returns a result to "after"
 *     after  {Function} Optional. Runs after and with the result of "test"
 */
function test_newtab(testInfo) { // eslint-disable-line no-unused-vars
  // Extract any test parts or default to just the single content task
  let {before, test: contentTask, after} = testInfo;
  if (!before) {
    before = () => ({});
  }
  if (!contentTask) {
    contentTask = testInfo;
  }
  if (!after) {
    after = () => {};
  }

  // Helper to push prefs for just this test and pop them when done
  let needPopPrefs = false;
  let scopedPushPrefs = async (...args) => {
    needPopPrefs = true;
    await pushPrefs(...args);
  };
  let scopedPopPrefs = async () => {
    if (needPopPrefs) {
      await popPrefs();
    }
  };

  // Make the test task with optional before/after and content task to run in a
  // new tab that opens and closes.
  let testTask = async () => {
    // Open about:newtab without using the default load listener
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:newtab", false);

    // Specially wait for potentially preloaded browsers
    let browser = tab.linkedBrowser;
    await waitForPreloaded(browser);

    // Wait for React to render something
    await BrowserTestUtils.waitForCondition(() => ContentTask.spawn(browser, {},
      () => content.document.getElementById("root").children.length),
      "Should render activity stream content");

    // Chain together before -> contentTask -> after data passing
    try {
      let contentArg = await before({pushPrefs: scopedPushPrefs, tab});
      let contentResult = await ContentTask.spawn(browser, contentArg, contentTask);
      await after(contentResult);
    } finally {
      // Clean up for next tests
      await scopedPopPrefs();
      await BrowserTestUtils.removeTab(tab);
    }
  };

  // Copy the name of the content task to identify the test
  Object.defineProperty(testTask, "name", {value: contentTask.name});
  add_task(testTask);
}
