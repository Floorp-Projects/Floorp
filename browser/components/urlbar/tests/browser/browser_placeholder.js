/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test ensures the placeholder is set correctly for different search
 * engines.
 */

"use strict";

var originalEngine, extraEngine, extraPrivateEngine, expectedString;
var tabs = [];

var noEngineString;

add_setup(async function () {
  originalEngine = await Services.search.getDefault();
  [noEngineString, expectedString] = (
    await document.l10n.formatMessages([
      { id: "urlbar-placeholder" },
      {
        id: "urlbar-placeholder-with-name",
        args: { name: originalEngine.name },
      },
    ])
  ).map(msg => msg.attributes[0].value);

  let rootUrl = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://mochi.test:8888/"
  );
  await SearchTestUtils.installSearchExtension({
    name: "extraEngine",
    search_url: "https://mochi.test:8888/",
    suggest_url: `${rootUrl}/searchSuggestionEngine.sjs`,
  });
  extraEngine = Services.search.getEngineByName("extraEngine");
  await SearchTestUtils.installSearchExtension({
    name: "extraPrivateEngine",
    search_url: "https://mochi.test:8888/",
    suggest_url: `${rootUrl}/searchSuggestionEngine.sjs`,
  });
  extraPrivateEngine = Services.search.getEngineByName("extraPrivateEngine");

  // Force display of a tab with a URL bar, to clear out any possible placeholder
  // initialization listeners that happen on startup.
  let urlTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:mozilla"
  );
  BrowserTestUtils.removeTab(urlTab);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", true],
      ["browser.search.separatePrivateDefault", false],
      ["browser.urlbar.suggest.quickactions", false],
    ],
  });

  registerCleanupFunction(async () => {
    await Services.search.setDefault(
      originalEngine,
      Ci.nsISearchService.CHANGE_REASON_UNKNOWN
    );
    for (let tab of tabs) {
      BrowserTestUtils.removeTab(tab);
    }
  });
});

add_task(async function test_change_default_engine_updates_placeholder() {
  tabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser));

  await Services.search.setDefault(
    extraEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  await TestUtils.waitForCondition(
    () => gURLBar.placeholder == noEngineString,
    "The placeholder should match the default placeholder for non-built-in engines."
  );
  Assert.equal(gURLBar.placeholder, noEngineString);

  await Services.search.setDefault(
    originalEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  await TestUtils.waitForCondition(
    () => gURLBar.placeholder == expectedString,
    "The placeholder should include the engine name for built-in engines."
  );
  Assert.equal(gURLBar.placeholder, expectedString);
});

add_task(async function test_delayed_update_placeholder() {
  // We remove the change of engine listener here as that is set so that
  // if the engine is changed by the user then the placeholder is always updated
  // straight away. As we want to test the delay update here, we remove the
  // listener and call the placeholder update manually with the delay flag.
  Services.obs.removeObserver(BrowserSearch, "browser-search-engine-modified");

  // Since we can't easily test for startup changes, we'll at least test the delay
  // of update for the placeholder works.
  let urlTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:mozilla"
  );
  tabs.push(urlTab);

  // Open a tab with a blank URL bar.
  let blankTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  tabs.push(blankTab);

  await Services.search.setDefault(
    extraEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  // Pretend we've "initialized".
  BrowserSearch._updateURLBarPlaceholder(extraEngine.name, false, true);

  Assert.equal(
    gURLBar.placeholder,
    expectedString,
    "Placeholder should be unchanged."
  );

  // Now switch to a tab with something in the URL Bar.
  await BrowserTestUtils.switchTab(gBrowser, urlTab);

  await TestUtils.waitForCondition(
    () => gURLBar.placeholder == noEngineString,
    "The placeholder should have updated in the background."
  );

  // Do it the other way to check both named engine and fallback code paths.
  await BrowserTestUtils.switchTab(gBrowser, blankTab);

  await Services.search.setDefault(
    originalEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  BrowserSearch._updateURLBarPlaceholder(originalEngine.name, false, true);

  Assert.equal(
    gURLBar.placeholder,
    noEngineString,
    "Placeholder should be unchanged."
  );
  Assert.deepEqual(
    document.l10n.getAttributes(gURLBar.inputField),
    { id: "urlbar-placeholder", args: null },
    "Placeholder data should be unchanged."
  );

  await BrowserTestUtils.switchTab(gBrowser, urlTab);

  await TestUtils.waitForCondition(
    () => gURLBar.placeholder == expectedString,
    "The placeholder should include the engine name for built-in engines."
  );

  // Now check when we have a URL displayed, the placeholder is updated straight away.
  BrowserSearch._updateURLBarPlaceholder(extraEngine.name, false);

  await TestUtils.waitForCondition(
    () => gURLBar.placeholder == noEngineString,
    "The placeholder should go back to the default"
  );
  Assert.equal(
    gURLBar.placeholder,
    noEngineString,
    "Placeholder should be the default."
  );

  Services.obs.addObserver(BrowserSearch, "browser-search-engine-modified");
});

add_task(async function test_private_window_no_separate_engine() {
  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  await Services.search.setDefault(
    extraEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  await TestUtils.waitForCondition(
    () => win.gURLBar.placeholder == noEngineString,
    "The placeholder should match the default placeholder for non-built-in engines."
  );
  Assert.equal(win.gURLBar.placeholder, noEngineString);

  await Services.search.setDefault(
    originalEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  await TestUtils.waitForCondition(
    () => win.gURLBar.placeholder == expectedString,
    "The placeholder should include the engine name for built-in engines."
  );
  Assert.equal(win.gURLBar.placeholder, expectedString);

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_private_window_separate_engine() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.separatePrivateDefault", true]],
  });
  const originalPrivateEngine = await Services.search.getDefaultPrivate();
  registerCleanupFunction(async () => {
    await Services.search.setDefaultPrivate(
      originalPrivateEngine,
      Ci.nsISearchService.CHANGE_REASON_UNKNOWN
    );
  });

  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  // Keep the normal default as a different string to the private, so that we
  // can be sure we're testing the right thing.
  await Services.search.setDefault(
    originalEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  await Services.search.setDefaultPrivate(
    extraPrivateEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  await TestUtils.waitForCondition(
    () => win.gURLBar.placeholder == noEngineString,
    "The placeholder should match the default placeholder for non-built-in engines."
  );
  Assert.equal(win.gURLBar.placeholder, noEngineString);

  await Services.search.setDefault(
    extraEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  await Services.search.setDefaultPrivate(
    originalEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  await TestUtils.waitForCondition(
    () => win.gURLBar.placeholder == expectedString,
    "The placeholder should include the engine name for built-in engines."
  );
  Assert.equal(win.gURLBar.placeholder, expectedString);

  await BrowserTestUtils.closeWindow(win);

  // Verify that the placeholder for private windows is updated even when no
  // private window is visible (https://bugzilla.mozilla.org/1792816).
  await Services.search.setDefault(
    originalEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  await Services.search.setDefaultPrivate(
    extraPrivateEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  const win2 = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  Assert.equal(win2.gURLBar.placeholder, noEngineString);
  await BrowserTestUtils.closeWindow(win2);

  // And ensure this doesn't affect the placeholder for non private windows.
  tabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser));
  Assert.equal(win.gURLBar.placeholder, expectedString);
});

add_task(async function test_search_mode_engine_web() {
  // Add our test engine to WEB_ENGINE_NAMES so that it's recognized as a web
  // engine.
  SearchUtils.GENERAL_SEARCH_ENGINE_IDS.add(
    extraEngine.wrappedJSObject._extensionID
  );

  await doSearchModeTest(
    {
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      engineName: extraEngine.name,
    },
    {
      id: "urlbar-placeholder-search-mode-web-2",
      args: { name: extraEngine.name },
    }
  );

  SearchUtils.GENERAL_SEARCH_ENGINE_IDS.delete(
    extraEngine.wrappedJSObject._extensionID
  );
});

add_task(async function test_search_mode_engine_other() {
  await doSearchModeTest(
    { engineName: extraEngine.name },
    {
      id: "urlbar-placeholder-search-mode-other-engine",
      args: { name: extraEngine.name },
    }
  );
});

add_task(async function test_search_mode_bookmarks() {
  await doSearchModeTest(
    { source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS },
    { id: "urlbar-placeholder-search-mode-other-bookmarks", args: null }
  );
});

add_task(async function test_search_mode_tabs() {
  await doSearchModeTest(
    { source: UrlbarUtils.RESULT_SOURCE.TABS },
    { id: "urlbar-placeholder-search-mode-other-tabs", args: null }
  );
});

add_task(async function test_search_mode_history() {
  await doSearchModeTest(
    { source: UrlbarUtils.RESULT_SOURCE.HISTORY },
    { id: "urlbar-placeholder-search-mode-other-history", args: null }
  );
});

add_task(async function test_change_default_engine_updates_placeholder() {
  tabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser));

  info(`Set engine to ${extraEngine.name}`);
  await Services.search.setDefault(
    extraEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  await TestUtils.waitForCondition(
    () => gURLBar.placeholder == noEngineString,
    "The placeholder should match the default placeholder for non-built-in engines."
  );
  Assert.equal(gURLBar.placeholder, noEngineString);

  info(`Set engine to ${originalEngine.name}`);
  await Services.search.setDefault(
    originalEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  await TestUtils.waitForCondition(
    () => gURLBar.placeholder == expectedString,
    "The placeholder should include the engine name for built-in engines."
  );

  // Simulate the placeholder not having changed due to the delayed update
  // on startup.
  BrowserSearch._setURLBarPlaceholder("");
  await TestUtils.waitForCondition(
    () => gURLBar.placeholder == noEngineString,
    "The placeholder should have been reset."
  );

  info("Show search engine removal info bar");
  await BrowserSearch.removalOfSearchEngineNotificationBox(
    extraEngine.name,
    originalEngine.name
  );
  const notificationBox = gNotificationBox.getNotificationWithValue(
    "search-engine-removal"
  );
  Assert.ok(notificationBox, "Search engine removal should be shown.");

  await TestUtils.waitForCondition(
    () => gURLBar.placeholder == expectedString,
    "The placeholder should include the engine name for built-in engines."
  );

  Assert.equal(gURLBar.placeholder, expectedString);

  notificationBox.close();
});

/**
 * Opens the view, clicks a one-off button to enter search mode, and asserts
 * that the placeholder is corrrect.
 *
 * @param {object} expectedSearchMode
 *   The expected search mode object for the one-off.
 * @param {object} expectedPlaceholderL10n
 *   The expected l10n object for the one-off.
 */
async function doSearchModeTest(expectedSearchMode, expectedPlaceholderL10n) {
  // Click the urlbar to open the top-sites view.
  if (gURLBar.getAttribute("pageproxystate") == "invalid") {
    gURLBar.handleRevert();
  }
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  });

  // Enter search mode.
  await UrlbarTestUtils.enterSearchMode(window, expectedSearchMode);

  // Check the placeholder.
  Assert.deepEqual(
    document.l10n.getAttributes(gURLBar.inputField),
    expectedPlaceholderL10n,
    "Placeholder has expected l10n"
  );

  await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
  await UrlbarTestUtils.promisePopupClose(window);
}
