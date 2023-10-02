/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests ime composition handling on searchbar.

add_setup(async function () {
  await gCUITestUtils.addSearchBar();

  await SearchTestUtils.installSearchExtension({}, { setAsDefault: true });

  registerCleanupFunction(async function () {
    gCUITestUtils.removeSearchBar();
  });
});

add_task(async function test_composition_with_focus() {
  info("Open a page");
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com");

  info("Focus on the search bar");
  const searchBarTextBox = BrowserSearch.searchBar.textbox;
  EventUtils.synthesizeMouseAtCenter(searchBarTextBox, {});
  is(
    document.activeElement,
    BrowserSearch.searchBar.textbox,
    "The text box of search bar has focus"
  );

  info("Do search with new tab");
  EventUtils.synthesizeKey("x");
  EventUtils.synthesizeKey("KEY_Enter", { altKey: true, type: "keydown" });
  is(gBrowser.tabs.length, 3, "Alt+Return key added new tab");
  await TestUtils.waitForCondition(
    () => document.activeElement === gBrowser.selectedBrowser,
    "Wait for focus to be moved to the browser"
  );
  info("The focus is moved to the browser");

  info("Focus on the search bar again");
  EventUtils.synthesizeMouseAtCenter(searchBarTextBox, {});
  is(
    document.activeElement,
    BrowserSearch.searchBar.textbox,
    "The textbox of search bar has focus again"
  );

  info("Type some characters during composition");
  const string = "ex";
  EventUtils.synthesizeCompositionChange({
    composition: {
      string,
      clauses: [
        {
          length: string.length,
          attr: Ci.nsITextInputProcessor.ATTR_RAW_CLAUSE,
        },
      ],
    },
    caret: { start: string.length, length: 0 },
    key: { key: string[string.length - 1] },
  });

  info("Commit the composition");
  EventUtils.synthesizeComposition({
    type: "compositioncommitasis",
    key: { key: "KEY_Enter" },
  });
  is(
    document.activeElement,
    BrowserSearch.searchBar.textbox,
    "The search bar still has focus"
  );

  // Close all open tabs
  await BrowserTestUtils.removeTab(gBrowser.tabs[2]);
  await BrowserTestUtils.removeTab(gBrowser.tabs[1]);
});
