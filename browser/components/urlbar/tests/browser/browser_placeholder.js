/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test ensures the placeholder is set correctly for different search
 * engines.
 */

"use strict";

const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";
const TEST_PRIVATE_ENGINE_BASENAME = "searchSuggestionEngine2.xml";

var originalEngine, extraEngine, extraPrivateEngine, expectedString;
var tabs = [];

add_task(async function setup() {
  originalEngine = await Services.search.getDefault();
  expectedString = gBrowserBundle.formatStringFromName("urlbar.placeholder", [
    originalEngine.name,
  ]);

  let rootDir = getRootDirectory(gTestPath);
  extraEngine = await SearchTestUtils.promiseNewSearchEngine(
    rootDir + TEST_ENGINE_BASENAME
  );
  extraPrivateEngine = await SearchTestUtils.promiseNewSearchEngine(
    rootDir + TEST_PRIVATE_ENGINE_BASENAME
  );

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
    ],
  });

  registerCleanupFunction(async () => {
    await Services.search.setDefault(originalEngine);
    for (let tab of tabs) {
      BrowserTestUtils.removeTab(tab);
    }
  });
});

add_task(async function test_change_default_engine_updates_placeholder() {
  tabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser));

  await Services.search.setDefault(extraEngine);

  await TestUtils.waitForCondition(
    () => gURLBar.placeholder == gURLBar.getAttribute("defaultPlaceholder"),
    "The placeholder should match the default placeholder for non-built-in engines."
  );
  Assert.equal(gURLBar.placeholder, gURLBar.getAttribute("defaultPlaceholder"));

  await Services.search.setDefault(originalEngine);

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

  await Services.search.setDefault(extraEngine);
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
    () => gURLBar.placeholder == gURLBar.getAttribute("defaultPlaceholder"),
    "The placeholder should have updated in the background."
  );

  // Do it the other way to check both named engine and fallback code paths.
  await BrowserTestUtils.switchTab(gBrowser, blankTab);

  await Services.search.setDefault(originalEngine);
  BrowserSearch._updateURLBarPlaceholder(originalEngine.name, false, true);

  Assert.equal(
    gURLBar.placeholder,
    gURLBar.getAttribute("defaultPlaceholder"),
    "Placeholder should be unchanged."
  );

  await BrowserTestUtils.switchTab(gBrowser, urlTab);

  await TestUtils.waitForCondition(
    () => gURLBar.placeholder == expectedString,
    "The placeholder should include the engine name for built-in engines."
  );

  // Now check when we have a URL displayed, the placeholder is updated straight away.
  BrowserSearch._updateURLBarPlaceholder(extraEngine.name, false);

  Assert.equal(
    gURLBar.placeholder,
    gURLBar.getAttribute("defaultPlaceholder"),
    "Placeholder should be the default."
  );

  Services.obs.addObserver(BrowserSearch, "browser-search-engine-modified");
});

add_task(async function test_private_window_no_separate_engine() {
  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  await Services.search.setDefault(extraEngine);

  await TestUtils.waitForCondition(
    () =>
      win.gURLBar.placeholder == win.gURLBar.getAttribute("defaultPlaceholder"),
    "The placeholder should match the default placeholder for non-built-in engines."
  );
  Assert.equal(
    win.gURLBar.placeholder,
    win.gURLBar.getAttribute("defaultPlaceholder")
  );

  await Services.search.setDefault(originalEngine);

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
    await Services.search.setDefaultPrivate(originalPrivateEngine);
  });

  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  // Keep the normal default as a different string to the private, so that we
  // can be sure we're testing the right thing.
  await Services.search.setDefault(originalEngine);
  await Services.search.setDefaultPrivate(extraPrivateEngine);

  await TestUtils.waitForCondition(
    () =>
      win.gURLBar.placeholder == win.gURLBar.getAttribute("defaultPlaceholder"),
    "The placeholder should match the default placeholder for non-built-in engines."
  );
  Assert.equal(
    win.gURLBar.placeholder,
    win.gURLBar.getAttribute("defaultPlaceholder")
  );

  await Services.search.setDefault(extraEngine);
  await Services.search.setDefaultPrivate(originalEngine);

  await TestUtils.waitForCondition(
    () => win.gURLBar.placeholder == expectedString,
    "The placeholder should include the engine name for built-in engines."
  );
  Assert.equal(win.gURLBar.placeholder, expectedString);

  await BrowserTestUtils.closeWindow(win);
});
