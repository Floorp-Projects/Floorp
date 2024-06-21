/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the behavior for enter key.

add_setup(async function () {
  await gCUITestUtils.addSearchBar();

  await SearchTestUtils.installSearchExtension({}, { setAsDefault: true });

  registerCleanupFunction(async function () {
    gCUITestUtils.removeSearchBar();
  });
});

add_task(async function searchOnEnterSoon() {
  info("Search on Enter as soon as typing a char");
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const browser = win.gBrowser.selectedBrowser;
  const browserSearch = win.BrowserSearch;

  const onPageHide = SpecialPowers.spawn(browser, [], () => {
    return new Promise(resolve => {
      content.addEventListener("pagehide", () => {
        resolve();
      });
    });
  });
  const onResult = SpecialPowers.spawn(browser, [], () => {
    return new Promise(resolve => {
      content.addEventListener("keyup", () => {
        resolve("keyup");
      });
      content.addEventListener("unload", () => {
        resolve("unload");
      });
    });
  });

  info("Focus on the search bar");
  const searchBarTextBox = browserSearch.searchBar.textbox;
  EventUtils.synthesizeMouseAtCenter(searchBarTextBox, {}, win);
  const ownerDocument = browser.ownerDocument;
  is(ownerDocument.activeElement, searchBarTextBox, "The search bar has focus");

  info("Keydown a char and Enter");
  EventUtils.synthesizeKey("x", { type: "keydown" }, win);
  EventUtils.synthesizeKey("KEY_Enter", { type: "keydown" }, win);

  info("Wait for pagehide event in the content");
  await onPageHide;
  is(
    ownerDocument.activeElement,
    searchBarTextBox,
    "The search bar still has focus"
  );

  // Keyup both key as soon as pagehide event happens.
  EventUtils.synthesizeKey("x", { type: "keyup" }, win);
  EventUtils.synthesizeKey("KEY_Enter", { type: "keyup" }, win);

  await TestUtils.waitForCondition(
    () => ownerDocument.activeElement === browser,
    "Wait for focus to be moved to the browser"
  );
  info("The focus is moved to the browser");

  // Check whether keyup event is not captured before unload event happens.
  const result = await onResult;
  is(result, "unload", "Keyup event is not captured");

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function typeCharWhileProcessingEnter() {
  info("Typing a char while processing enter key");
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const browser = win.gBrowser.selectedBrowser;
  const searchBar = win.BrowserSearch.searchBar;

  const SEARCH_WORD = "test";
  const onLoad = BrowserTestUtils.browserLoaded(
    browser,
    false,
    `https://example.com/?q=${SEARCH_WORD}`
  );
  searchBar.textbox.focus();
  searchBar.textbox.value = SEARCH_WORD;

  info("Keydown Enter");
  EventUtils.synthesizeKey("KEY_Enter", { type: "keydown" }, win);
  await TestUtils.waitForCondition(
    () => searchBar._needBrowserFocusAtEnterKeyUp,
    "Wait for starting process for the enter key"
  );

  info("Keydown a char");
  EventUtils.synthesizeKey("x", { type: "keydown" }, win);

  info("Keyup both");
  EventUtils.synthesizeKey("x", { type: "keyup" }, win);
  EventUtils.synthesizeKey("KEY_Enter", { type: "keyup" }, win);

  Assert.equal(
    searchBar.textbox.value,
    SEARCH_WORD,
    "The value of searchbar is correct"
  );

  await onLoad;
  Assert.ok("Browser loaded the correct url");

  // Cleanup.
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function keyupEnterWhilePressingMeta() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const browser = win.gBrowser.selectedBrowser;
  const searchBar = win.BrowserSearch.searchBar;

  info("Keydown Meta+Enter");
  searchBar.textbox.focus();
  searchBar.textbox.value = "a";
  EventUtils.synthesizeKey(
    "KEY_Enter",
    { type: "keydown", metaKey: true },
    win
  );

  // Pressing Enter key while pressing Meta key, and next, even when releasing
  // Enter key before releasing Meta key, the keyup event is not fired.
  // Therefor, we fire Meta keyup event only.
  info("Keyup Meta");
  EventUtils.synthesizeKey("KEY_Meta", { type: "keyup" }, win);

  await TestUtils.waitForCondition(
    () => browser.ownerDocument.activeElement === browser,
    "Wait for focus to be moved to the browser"
  );
  info("The focus is moved to the browser");

  info("Check whether we can input on the search bar");
  searchBar.textbox.focus();
  EventUtils.synthesizeKey("a", {}, win);
  is(searchBar.textbox.value, "aa", "Can input a char");

  // Cleanup.
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function enterOnEmptySearchBar() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const browser = win.gBrowser.selectedBrowser;
  const searchBar = win.BrowserSearch.searchBar;

  // Enter should be ignored if the searchbar is empty.
  info("Pressing Enter");
  searchBar.textbox.focus();
  searchBar.textbox.value = "";
  EventUtils.synthesizeKey("KEY_Enter", {}, win);

  await TestUtils.waitForTick();
  is(
    browser.ownerDocument.activeElement,
    searchBar.textbox,
    "Focus stays in the searchbar"
  );

  info("Check whether we can input on the search bar");
  EventUtils.synthesizeKey("a", {}, win);
  is(searchBar.textbox.value, "a", "Can input a char");

  // Cleanup.
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_open_settings_with_enter() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const searchBar = win.BrowserSearch.searchBar;
  const searchPopup = win.document.getElementById("PopupSearchAutoComplete");
  const searchButton = searchBar.querySelector(".searchbar-search-button");

  let shownPromise = promiseEvent(searchPopup, "popupshown");
  info("Clicking icon");
  EventUtils.synthesizeMouseAtCenter(searchButton, {}, win);
  await shownPromise;
  info("Popup shown");

  EventUtils.synthesizeKey("KEY_ArrowUp", {}, win);

  await TestUtils.waitForCondition(
    () =>
      searchBar.textbox.selectedButton?.classList.contains(
        "search-setting-button"
      ),
    "Wait for settings button to get selected"
  );

  let promise = TestUtils.topicObserved("sync-pane-loaded");
  EventUtils.synthesizeKey("KEY_Enter", {}, win);
  await promise;

  Assert.equal(
    win.gBrowser.contentWindow.history.state,
    "paneSearch",
    "Should have opened the search preferences pane"
  );

  await BrowserTestUtils.closeWindow(win);
});
