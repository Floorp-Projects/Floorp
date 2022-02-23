/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the focus timing for the search bar.

add_task(async function setup() {
  await gCUITestUtils.addSearchBar();

  await SearchTestUtils.installSearchExtension();

  const defaultEngine = Services.search.defaultEngine;
  Services.search.defaultEngine = Services.search.getEngineByName("Example");

  registerCleanupFunction(async function() {
    Services.search.defaultEngine = defaultEngine;
    gCUITestUtils.removeSearchBar();
  });
});

add_task(async function() {
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
