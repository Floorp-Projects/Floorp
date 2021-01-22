/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* global browser */

// This tests the browser.experiments.urlbar.suggestionsEnabled WebExtension
// Experiment API.

"use strict";

add_task(async function init() {
  // Change the default engine so we can enable suggestions.
  let oldDefaultEngine = await Services.search.getDefault();
  let defaultEngine = await Services.search.addEngineWithDetails("TestEngine", {
    template: "http://example.com/?s=%S",
  });
  registerCleanupFunction(async () => {
    Services.search.defaultEngine = oldDefaultEngine;
    await Services.search.removeEngine(defaultEngine);
  });
  Services.search.defaultEngine = defaultEngine;
});

add_task(async function test() {
  let ext = await loadExtension({
    manifest: {
      incognito: "spanning",
    },
    background: async () => {
      browser.urlbar.onBehaviorRequested.addListener(query => {
        return "active";
      }, "test");
      browser.urlbar.onResultsRequested.addListener(async queryContext => {
        if (await browser.experiments.urlbar.suggestionsEnabled(queryContext)) {
          browser.test.sendMessage("enabled");
        } else {
          browser.test.sendMessage("disabled");
        }

        return [];
      }, "test");
    },
  });

  async function runTest(win, searchString, suggestionsExpected) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: searchString,
      waitForFocus: SimpleTest.waitForFocus,
    });
    if (suggestionsExpected) {
      await ext.awaitMessage("enabled");
      Assert.ok("Suggestions are enabled");
    } else {
      await ext.awaitMessage("disabled");
      Assert.ok("Suggestions are disabled");
    }
    await UrlbarTestUtils.promisePopupClose(window);
  }

  // Wait for the provider to be registered before continuing.
  await TestUtils.waitForCondition(
    () => UrlbarProvidersManager.getProvider("test"),
    "Waiting for provider to be registered"
  );
  Assert.ok(
    UrlbarProvidersManager.getProvider("test"),
    "Provider should be registered"
  );

  info("Expect suggestions to be enabled.");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.suggest.enabled", true],
      ["browser.urlbar.suggest.searches", true],
    ],
  });
  await runTest(window, "test", true);
  await SpecialPowers.popPrefEnv();

  info(
    "Expect suggestions to be disabled after disabling address bar suggestions."
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.suggest.enabled", true],
      ["browser.urlbar.suggest.searches", false],
    ],
  });
  await runTest(window, "test", false);
  await SpecialPowers.popPrefEnv();

  info(
    "Expect suggestions to be disabled after disabling suggestions globally."
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.suggest.enabled", false],
      ["browser.urlbar.suggest.searches", true],
    ],
  });
  await runTest(window, "test", false);
  await SpecialPowers.popPrefEnv();

  info(
    "Expect suggestions to be disabled after disabling all suggestions prefs."
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.suggest.enabled", false],
      ["browser.urlbar.suggest.searches", false],
    ],
  });
  await runTest(window, "test", false);
  await SpecialPowers.popPrefEnv();

  info(
    "Expect suggestions to be disabled after using the bookmarks restriction token."
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.suggest.enabled", true],
      ["browser.urlbar.suggest.searches", true],
    ],
  });
  // Note we are using the bookmark restriction token.
  await runTest(window, "* test", false);
  await UrlbarTestUtils.exitSearchMode(window);
  // Exiting search mode fires a new empty search, for which suggestions are
  // enabled.
  await ext.awaitMessage("enabled");
  await SpecialPowers.popPrefEnv();

  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  info(
    "Expect suggestions to be enabled in PBM with the PBM suggestion pref enabled."
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.suggest.enabled", true],
      ["browser.urlbar.suggest.searches", true],
      ["browser.search.suggest.enabled.private", true],
    ],
  });
  await runTest(privateWindow, "test", true);
  await SpecialPowers.popPrefEnv();

  info(
    "Expect suggestions to be disabled in PBM with the PBM suggestion pref disabled."
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.suggest.enabled", true],
      ["browser.urlbar.suggest.searches", true],
      ["browser.search.suggest.enabled.private", false],
    ],
  });
  await runTest(privateWindow, "test", false);
  await SpecialPowers.popPrefEnv();

  await BrowserTestUtils.closeWindow(privateWindow);

  await ext.unload();
});
