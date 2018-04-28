/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test ensures the placeholder is set correctly for different search
 * engines.
 */

"use strict";

const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

const originalEngine = Services.search.currentEngine;
const expectedString = gBrowserBundle.formatStringFromName("urlbar.placeholder",
  [originalEngine.name], 1);
var extraEngine;
var tabs = [];

add_task(async function setup() {
  let rootDir = getRootDirectory(gTestPath);
  extraEngine = await SearchTestUtils.promiseNewSearchEngine(rootDir + TEST_ENGINE_BASENAME);

  // Force display of a tab with a URL bar, to clear out any possible placeholder
  // initialization listeners that happen on startup.
  let urlTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");
  BrowserTestUtils.removeTab(urlTab);

  registerCleanupFunction(() => {
    Services.search.currentEngine = originalEngine;
    for (let tab of tabs) {
      BrowserTestUtils.removeTab(tab);
    }
  });
});

add_task(async function test_change_default_engine_updates_placeholder() {
  tabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser));

  Services.search.currentEngine = extraEngine;

  await TestUtils.waitForCondition(
    () => gURLBar.getAttribute("placeholder") == gURLBar.getAttribute("defaultPlaceholder"),
    "The placeholder should match the default placeholder for non-built-in engines.");

  Services.search.currentEngine = originalEngine;

  await TestUtils.waitForCondition(
    () => gURLBar.getAttribute("placeholder") == expectedString,
    "The placeholder should include the engine name for built-in engines.");
});

add_task(async function test_delayed_update_placeholder() {
  // Since we can't easily test for startup changes, we'll at least test the delay
  // of update for the placeholder works.
  let urlTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");
  tabs.push(urlTab);

  // Open a tab with a blank URL bar.
  let blankTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  tabs.push(blankTab);

  // Pretend we've "initialized".
  BrowserSearch._updateURLBarPlaceholder(extraEngine, true);

  Assert.equal(gURLBar.getAttribute("placeholder"), expectedString,
    "Placeholder should be unchanged.");

  // Now switch to a tab with something in the URL Bar.
  await BrowserTestUtils.switchTab(gBrowser, urlTab);

  await TestUtils.waitForCondition(
    () => gURLBar.getAttribute("placeholder") == gURLBar.getAttribute("defaultPlaceholder"),
    "The placeholder should have updated in the background.");

  // Do it the other way to check both named engine and fallback code paths.
  await BrowserTestUtils.switchTab(gBrowser, blankTab);

  BrowserSearch._updateURLBarPlaceholder(originalEngine, true);

  Assert.equal(gURLBar.getAttribute("placeholder"), gURLBar.getAttribute("defaultPlaceholder"),
    "Placeholder should be unchanged.");

  await BrowserTestUtils.switchTab(gBrowser, urlTab);

  await TestUtils.waitForCondition(
    () => gURLBar.getAttribute("placeholder") == expectedString,
    "The placeholder should include the engine name for built-in engines.");

  // Now check when we have a URL displayed, the placeholder is updated straight away.
  BrowserSearch._updateURLBarPlaceholder(extraEngine);

  Assert.equal(gURLBar.getAttribute("placeholder"), gURLBar.getAttribute("defaultPlaceholder"),
    "Placeholder should be the default.");
});
