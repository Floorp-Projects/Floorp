"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "ObjectUtils",
  "resource://gre/modules/ObjectUtils.jsm"
);
ChromeUtils.defineESModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
});
ChromeUtils.defineModuleGetter(
  this,
  "QueryCache",
  "resource://activity-stream/lib/ASRouterTargeting.jsm"
);
// eslint-disable-next-line no-unused-vars
const { FxAccounts } = ChromeUtils.import(
  "resource://gre/modules/FxAccounts.jsm"
);
// We import sinon here to make it available across all mochitest test files
// eslint-disable-next-line no-unused-vars
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
// Set the content pref to make it available across tests
const ABOUT_WELCOME_OVERRIDE_CONTENT_PREF = "browser.aboutwelcome.screens";
// Test differently for windows 7 as theme screens are removed.
// eslint-disable-next-line no-unused-vars
const win7Content = AppConstants.isPlatformAndVersionAtMost("win", "6.1");

function popPrefs() {
  return SpecialPowers.popPrefEnv();
}
function pushPrefs(...prefs) {
  return SpecialPowers.pushPrefEnv({ set: prefs });
}
// eslint-disable-next-line no-unused-vars
async function getAboutWelcomeParent(browser) {
  let windowGlobalParent = browser.browsingContext.currentWindowGlobal;
  return windowGlobalParent.getActor("AboutWelcome");
}
// eslint-disable-next-line no-unused-vars
async function setAboutWelcomeMultiStage(value = "") {
  return pushPrefs([ABOUT_WELCOME_OVERRIDE_CONTENT_PREF, value]);
}

/**
 * Setup functions to test welcome UI
 */
// eslint-disable-next-line no-unused-vars
async function test_screen_content(
  browser,
  experiment,
  expectedSelectors = [],
  unexpectedSelectors = []
) {
  await ContentTask.spawn(
    browser,
    { expectedSelectors, experiment, unexpectedSelectors },
    async ({
      expectedSelectors: expected,
      experiment: experimentName,
      unexpectedSelectors: unexpected,
    }) => {
      for (let selector of expected) {
        await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector(selector),
          `Should render ${selector} in ${experimentName}`
        );
      }
      for (let selector of unexpected) {
        ok(
          !content.document.querySelector(selector),
          `Should not render ${selector} in ${experimentName}`
        );
      }

      if (experimentName === "home") {
        Assert.equal(
          content.document.location.href,
          "about:home",
          "Navigated to about:home"
        );
      } else {
        Assert.equal(
          content.document.location.href,
          "about:welcome",
          "Navigated to a welcome screen"
        );
      }
    }
  );
}

// eslint-disable-next-line no-unused-vars
async function test_element_styles(
  browser,
  elementSelector,
  expectedStyles = {},
  unexpectedStyles = {}
) {
  await ContentTask.spawn(
    browser,
    [elementSelector, expectedStyles, unexpectedStyles],
    async ([selector, expected, unexpected]) => {
      const element = await ContentTaskUtils.waitForCondition(() =>
        content.document.querySelector(selector)
      );
      const computedStyles = content.window.getComputedStyle(element);
      Object.entries(expected).forEach(([attr, val]) =>
        is(
          computedStyles[attr],
          val,
          `${selector} should have computed ${attr} of ${val}`
        )
      );
      Object.entries(unexpected).forEach(([attr, val]) =>
        isnot(
          computedStyles[attr],
          val,
          `${selector} should not have computed ${attr} of ${val}`
        )
      );
    }
  );
}

// eslint-disable-next-line no-unused-vars
async function onButtonClick(browser, elementId) {
  await ContentTask.spawn(
    browser,
    { elementId },
    async ({ elementId: buttonId }) => {
      await ContentTaskUtils.waitForCondition(
        () => content.document.querySelector(buttonId),
        buttonId
      );
      let button = content.document.querySelector(buttonId);
      button.click();
    }
  );
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

// eslint-disable-next-line no-unused-vars
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

// eslint-disable-next-line no-unused-vars
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

// eslint-disable-next-line no-unused-vars
async function setAboutWelcomePref(value) {
  return pushPrefs(["browser.aboutwelcome.enabled", value]);
}

// eslint-disable-next-line no-unused-vars
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
 * https://searchfox.org/mozilla-central/rev/b2716c233e9b4398fc5923cbe150e7f83c7c6c5b/testing/mochitest/BrowserTestUtils/BrowserTestUtils.jsm#383
 */
// eslint-disable-next-line no-unused-vars
async function waitForUrlLoad(url) {
  let browser = gBrowser.selectedBrowser;
  BrowserTestUtils.loadURI(browser, url);
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
// eslint-disable-next-line no-unused-vars
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
// eslint-disable-next-line no-unused-vars
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
      await scopedPopPrefs();
      BrowserTestUtils.removeTab(tab);
    }
  };

  // Copy the name of the content task to identify the test
  Object.defineProperty(testTask, "name", { value: contentTask.name });
  add_task(testTask);
}

/**
 * The firefox view code and the utility funcitons here are cribbed from (mostly)
 * browser/components/firefoxview/test/browser/head.js
 *
 * https://bugzilla.mozilla.org/show_bug.cgi?id=1784979 has been filed to move
 * these to some place publically accessible, after which we will be able to
 * a bunch of code from this file.
 */
// eslint-disable-next-line no-unused-vars
async function assertFirefoxViewTabSelected(w) {
  ok(w.FirefoxViewHandler.tab, "Firefox View tab exists");
  ok(w.FirefoxViewHandler.tab?.hidden, "Firefox View tab is hidden");
  is(
    w.gBrowser.visibleTabs.indexOf(w.FirefoxViewHandler.tab),
    -1,
    "Firefox View tab is not in the list of visible tabs"
  );
  ok(w.FirefoxViewHandler.tab.selected, "Firefox View tab is selected");
  await BrowserTestUtils.browserLoaded(w.FirefoxViewHandler.tab.linkedBrowser);
}

// eslint-disable-next-line no-unused-vars
function closeFirefoxViewTab(w = window) {
  w.gBrowser.removeTab(w.FirefoxViewHandler.tab);
  ok(
    !w.FirefoxViewHandler.tab,
    "Reference to Firefox View tab got removed when closing the tab"
  );
}
