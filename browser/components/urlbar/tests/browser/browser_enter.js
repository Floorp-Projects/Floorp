/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_VALUE = "example.com/\xF7?\xF7";
const START_VALUE = "example.com/%C3%B7?%C3%B7";

add_task(async function returnKeypress() {
  info("Simple return keypress");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, START_VALUE);

  gURLBar.focus();
  EventUtils.synthesizeKey("KEY_Enter");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  // Check url bar and selected tab.
  is(
    gURLBar.value,
    TEST_VALUE,
    "Urlbar should preserve the value on return keypress"
  );
  is(gBrowser.selectedTab, tab, "New URL was loaded in the current tab");

  // Cleanup.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function altReturnKeypress() {
  info("Alt+Return keypress");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, START_VALUE);

  let tabOpenPromise = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabOpen"
  );
  gURLBar.focus();
  EventUtils.synthesizeKey("KEY_Enter", { altKey: true });

  // wait for the new tab to appear.
  await tabOpenPromise;

  // Check url bar and selected tab.
  is(
    gURLBar.value,
    TEST_VALUE,
    "Urlbar should preserve the value on return keypress"
  );
  isnot(gBrowser.selectedTab, tab, "New URL was loaded in a new tab");

  // Cleanup.
  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function altGrReturnKeypress() {
  info("AltGr+Return keypress");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, START_VALUE);

  let tabOpenPromise = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabOpen"
  );
  gURLBar.focus();
  EventUtils.synthesizeKey("KEY_Enter", { altGraphKey: true });

  // wait for the new tab to appear.
  await tabOpenPromise;

  // Check url bar and selected tab.
  is(
    gURLBar.value,
    TEST_VALUE,
    "Urlbar should preserve the value on return keypress"
  );
  isnot(gBrowser.selectedTab, tab, "New URL was loaded in a new tab");

  // Cleanup.
  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function searchOnEnterNoPick() {
  info("Search on Enter without picking a urlbar result");
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "searchSuggestionEngine.xml"
  );
  let defaultEngine = Services.search.defaultEngine;
  Services.search.defaultEngine = engine;
  // Why is BrowserTestUtils.openNewForegroundTab not causing the bug?
  let promiseTabOpened = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabOpen"
  );
  EventUtils.synthesizeMouseAtCenter(gBrowser.tabContainer.newTabButton, {});
  let openEvent = await promiseTabOpened;
  let tab = openEvent.target;
  registerCleanupFunction(async function() {
    Services.search.defaultEngine = defaultEngine;
    BrowserTestUtils.removeTab(tab);
  });

  let loadPromise = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    null,
    true
  );
  gURLBar.focus();
  gURLBar.value = "test test";
  EventUtils.synthesizeKey("KEY_Enter");
  await loadPromise;

  Assert.ok(
    gBrowser.selectedBrowser.currentURI.spec.endsWith("test+test"),
    "Should have loaded the correct page"
  );
  Assert.equal(
    gBrowser.selectedBrowser.currentURI.spec,
    gURLBar.untrimmedValue,
    "The location should have changed"
  );
});
