/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_VALUE = "example.com/\xF7?\xF7";
const START_VALUE = "example.com/%C3%B7?%C3%B7";

add_task(async function setup() {
  const engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "searchSuggestionEngine.xml"
  );

  const defaultEngine = Services.search.defaultEngine;
  Services.search.defaultEngine = engine;

  registerCleanupFunction(async function() {
    Services.search.defaultEngine = defaultEngine;
  });
});

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

  // Why is BrowserTestUtils.openNewForegroundTab not causing the bug?
  let promiseTabOpened = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabOpen"
  );
  EventUtils.synthesizeMouseAtCenter(gBrowser.tabContainer.newTabButton, {});
  let openEvent = await promiseTabOpened;
  let tab = openEvent.target;

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

  // Cleanup.
  BrowserTestUtils.removeTab(tab);
});

add_task(async function searchOnEnterSoon() {
  info("Search on Enter as soon as typing a char");
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    START_VALUE
  );

  const onLoad = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  const onBeforeUnload = SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => {
      return new Promise(resolve => {
        content.window.addEventListener("beforeunload", () => {
          resolve();
        });
      });
    }
  );
  const onResult = SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    return new Promise(resolve => {
      content.window.addEventListener("keyup", () => {
        resolve("keyup");
      });
      content.window.addEventListener("unload", () => {
        resolve("unload");
      });
    });
  });

  // Focus on the input field in urlbar.
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  const ownerDocument = gBrowser.selectedBrowser.ownerDocument;
  is(
    ownerDocument.activeElement,
    gURLBar.inputField,
    "The input field in urlbar has focus"
  );

  info("Keydown a char and Enter");
  EventUtils.synthesizeKey("x", { type: "keydown" });
  EventUtils.synthesizeKey("KEY_Enter", { type: "keydown" });

  // Wait for beforeUnload event in the content.
  await onBeforeUnload;
  is(
    ownerDocument.activeElement,
    gURLBar.inputField,
    "The input field in urlbar still has focus"
  );

  // Keyup both key as soon as beforeUnload event happens.
  EventUtils.synthesizeKey("x", { type: "keyup" });
  EventUtils.synthesizeKey("KEY_Enter", { type: "keyup" });

  // Wait for moving the focus.
  await TestUtils.waitForCondition(
    () => ownerDocument.activeElement === gBrowser.selectedBrowser
  );
  info("The focus is moved to the browser");

  // Check whether keyup event is not captured before unload event happens.
  const result = await onResult;
  is(result, "unload", "Keyup event is not captured.");

  // Cleanup.
  await onLoad;
  BrowserTestUtils.removeTab(tab);
});
