"use strict";

ChromeUtils.defineESModuleGetters(this, {
  FeatureCallout: "resource:///modules/FeatureCallout.sys.mjs",
  FeatureCalloutBroker:
    "resource://activity-stream/lib/FeatureCalloutBroker.sys.mjs",
  FeatureCalloutMessages:
    "resource://activity-stream/lib/FeatureCalloutMessages.sys.mjs",
  ObjectUtils: "resource://gre/modules/ObjectUtils.sys.mjs",
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
});
XPCOMUtils.defineLazyModuleGetters(this, {
  ASRouter: "resource://activity-stream/lib/ASRouter.jsm",
  QueryCache: "resource://activity-stream/lib/ASRouterTargeting.jsm",
});
const { FxAccounts } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccounts.sys.mjs"
);
// We import sinon here to make it available across all mochitest test files
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { DiscoveryStreamFeed } = ChromeUtils.import(
  "resource://activity-stream/lib/DiscoveryStreamFeed.jsm"
);

// Feature callout constants
const calloutId = "feature-callout";
const calloutSelector = `#${calloutId}.featureCallout`;
const calloutCTASelector = `#${calloutId} :is(.primary, .secondary)`;
const calloutDismissSelector = `#${calloutId} .dismiss-button`;

function popPrefs() {
  return SpecialPowers.popPrefEnv();
}
function pushPrefs(...prefs) {
  return SpecialPowers.pushPrefEnv({ set: prefs });
}

// Toggle the feed off and on as a workaround to read the new prefs.
async function toggleTopsitesPref() {
  await pushPrefs([
    "browser.newtabpage.activity-stream.feeds.system.topsites",
    false,
  ]);
  await pushPrefs([
    "browser.newtabpage.activity-stream.feeds.system.topsites",
    true,
  ]);
}

async function setDefaultTopSites() {
  // The pref for TopSites is empty by default.
  await pushPrefs([
    "browser.newtabpage.activity-stream.default.sites",
    "https://www.youtube.com/,https://www.facebook.com/,https://www.amazon.com/,https://www.reddit.com/,https://www.wikipedia.org/,https://twitter.com/",
  ]);
  await toggleTopsitesPref();
  await pushPrefs([
    "browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts",
    true,
  ]);
}

async function setTestTopSites() {
  await pushPrefs([
    "browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts",
    false,
  ]);
  // The pref for TopSites is empty by default.
  // Using a topsite with example.com allows us to open the topsite without a network request.
  await pushPrefs([
    "browser.newtabpage.activity-stream.default.sites",
    "https://example.com/",
  ]);
  await toggleTopsitesPref();
}

async function clearHistoryAndBookmarks() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
  QueryCache.expireAll();
}

/**
 * Helper to wait for potentially preloaded browsers to "load" where a preloaded
 * page has already loaded and won't trigger "load", and a "load"ed page might
 * not necessarily have had all its javascript/render logic executed.
 */
async function waitForPreloaded(browser) {
  let readyState = await ContentTask.spawn(
    browser,
    null,
    () => content.document.readyState
  );
  if (readyState !== "complete") {
    await BrowserTestUtils.browserLoaded(browser);
  }
}

/**
 * Helper function to navigate and wait for page to load
 * https://searchfox.org/mozilla-central/rev/314b4297e899feaf260e7a7d1a9566a218216e7a/testing/mochitest/BrowserTestUtils/BrowserTestUtils.sys.mjs#404
 */
async function waitForUrlLoad(url) {
  let browser = gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, url);
  await BrowserTestUtils.browserLoaded(browser, false, url);
}

/**
 * Helper to force the HighlightsFeed to update.
 */
function refreshHighlightsFeed() {
  // Toggling the pref will clear the feed cache and force a places query.
  Services.prefs.setBoolPref(
    "browser.newtabpage.activity-stream.feeds.section.highlights",
    false
  );
  Services.prefs.setBoolPref(
    "browser.newtabpage.activity-stream.feeds.section.highlights",
    true
  );
}

/**
 * Helper to populate the Highlights section with bookmark cards.
 * @param count Number of items to add.
 */
async function addHighlightsBookmarks(count) {
  const bookmarks = new Array(count).fill(null).map((entry, i) => ({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "foo",
    url: `https://mozilla${i}.com/nowNew`,
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
 * Helper to add various helpers to the content process by injecting variables
 * and functions to the `content` global.
 */
function addContentHelpers() {
  const { document } = content;
  Object.assign(content, {
    /**
     * Click the context menu button for an item and get its options list.
     *
     * @param selector {String} Selector to get an item (e.g., top site, card)
     * @return {Array} The nodes for the options.
     */
    async openContextMenuAndGetOptions(selector) {
      const item = document.querySelector(selector);
      const contextButton = item.querySelector(".context-menu-button");
      contextButton.click();
      // Gives fluent-dom the time to render strings
      await new Promise(r => content.requestAnimationFrame(r));

      const contextMenu = item.querySelector(".context-menu");
      const contextMenuList = contextMenu.querySelector(".context-menu-list");
      return [...contextMenuList.getElementsByClassName("context-menu-item")];
    },
  });
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
 * @param browserURL {optional String}
 *   {String} This parameter is used to explicitly specify URL opened in new tab
 */
function test_newtab(testInfo, browserURL = "about:newtab") {
  // Extract any test parts or default to just the single content task
  let { before, test: contentTask, after } = testInfo;
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
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      browserURL,
      false
    );

    // Specially wait for potentially preloaded browsers
    let browser = tab.linkedBrowser;
    await waitForPreloaded(browser);

    // Add shared helpers to the content process
    SpecialPowers.spawn(browser, [], addContentHelpers);

    // Wait for React to render something
    await BrowserTestUtils.waitForCondition(
      () =>
        SpecialPowers.spawn(
          browser,
          [],
          () => content.document.getElementById("root").children.length
        ),
      "Should render activity stream content"
    );

    // Chain together before -> contentTask -> after data passing
    try {
      let contentArg = await before({ pushPrefs: scopedPushPrefs, tab });
      let contentResult = await SpecialPowers.spawn(
        browser,
        [contentArg],
        contentTask
      );
      await after(contentResult);
    } finally {
      // Clean up for next tests
      BrowserTestUtils.removeTab(tab);
      await scopedPopPrefs();
    }
  };

  // Copy the name of the content task to identify the test
  Object.defineProperty(testTask, "name", { value: contentTask.name });
  add_task(testTask);
}

async function waitForCalloutScreen(target, screenId) {
  await BrowserTestUtils.waitForMutationCondition(
    target,
    { childList: true, subtree: true, attributeFilter: ["class"] },
    () => target.querySelector(`${calloutSelector}:not(.hidden) .${screenId}`)
  );
}

async function waitForCalloutRemoved(target) {
  await BrowserTestUtils.waitForMutationCondition(
    target,
    { childList: true, subtree: true },
    () => !target.querySelector(calloutSelector)
  );
}
