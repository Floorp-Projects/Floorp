/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests search mode preview.
 */

"use strict";

const TEST_ENGINE_NAME = "Test";

let win;

add_task(async function setup() {
  win = await BrowserTestUtils.openNewBrowserWindow();

  await SearchTestUtils.installSearchExtension({
    name: TEST_ENGINE_NAME,
    keyword: "@test",
  });

  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
    // Avoid memory leaking, need to set null explicitly.
    win = null;
    await PlacesUtils.history.clear();
  });
});

/**
 * @param {Node} button
 *   A one-off button.
 * @param {boolean} [isPreview]
 *  Whether the expected search mode should be a preview. Defaults to true.
 * @returns {object}
 *   The search mode object expected when that one-off is selected.
 */
function getExpectedSearchMode(button, isPreview = true) {
  let expectedSearchMode = {
    entry: "oneoff",
    isPreview,
  };
  if (button.engine) {
    expectedSearchMode.engineName = button.engine.name;
    let engine = Services.search.getEngineByName(button.engine.name);
    if (engine.isGeneralPurposeEngine) {
      expectedSearchMode.source = UrlbarUtils.RESULT_SOURCE.SEARCH;
    }
  } else {
    expectedSearchMode.source = button.source;
  }

  return expectedSearchMode;
}

// Tests that cycling through token alias engines enters search mode preview.
add_task(async function tokenAlias() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "@",
  });

  let result;
  while (win.gURLBar.searchMode?.engineName != TEST_ENGINE_NAME) {
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    let index = UrlbarTestUtils.getSelectedRowIndex(win);
    result = await UrlbarTestUtils.getDetailsOfResultAt(win, index);
    let expectedSearchMode = {
      engineName: result.searchParams.engine,
      isPreview: true,
      entry: "keywordoffer",
    };
    let engine = Services.search.getEngineByName(result.searchParams.engine);
    if (engine.isGeneralPurposeEngine) {
      expectedSearchMode.source = UrlbarUtils.RESULT_SOURCE.SEARCH;
    }
    await UrlbarTestUtils.assertSearchMode(win, expectedSearchMode);
  }
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(win);
  EventUtils.synthesizeKey("KEY_Enter", {}, win);
  await searchPromise;
  // Test that we are in confirmed search mode.
  await UrlbarTestUtils.assertSearchMode(win, {
    engineName: result.searchParams.engine,
    entry: "keywordoffer",
  });
  await UrlbarTestUtils.exitSearchMode(win);
});

// Tests that starting to type a query exits search mode preview in favour of
// full search mode.
add_task(async function startTyping() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "@",
  });
  while (win.gURLBar.searchMode?.engineName != TEST_ENGINE_NAME) {
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  }

  await UrlbarTestUtils.assertSearchMode(win, {
    engineName: TEST_ENGINE_NAME,
    isPreview: true,
    entry: "keywordoffer",
  });

  let searchPromise = UrlbarTestUtils.promiseSearchComplete(win);
  EventUtils.synthesizeKey("M", {}, win);
  await searchPromise;
  await UrlbarTestUtils.assertSearchMode(win, {
    engineName: TEST_ENGINE_NAME,
    entry: "keywordoffer",
  });
  await UrlbarTestUtils.exitSearchMode(win);
});

// Tests that highlighting a search shortcut Top Site enters search mode
// preview.
add_task(async function topSites() {
  // Enable search shortcut Top Sites.
  await PlacesUtils.history.clear();
  await updateTopSites(
    sites => sites && sites[0] && sites[0].searchTopSite,
    true
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "",
    fireInputEvent: true,
  });

  // We previously verified that the first Top Site is a search shortcut.
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  let searchTopSite = await UrlbarTestUtils.getDetailsOfResultAt(win, 0);
  await UrlbarTestUtils.assertSearchMode(win, {
    engineName: searchTopSite.searchParams.engine,
    isPreview: true,
    entry: "topsites_urlbar",
  });
  await UrlbarTestUtils.exitSearchMode(win);
});

// Tests that search mode preview is exited when the view is closed.
add_task(async function closeView() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "@",
  });

  while (win.gURLBar.searchMode?.engineName != TEST_ENGINE_NAME) {
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  }
  await UrlbarTestUtils.assertSearchMode(win, {
    engineName: TEST_ENGINE_NAME,
    isPreview: true,
    entry: "keywordoffer",
  });

  // We should close search mode when closing the view.
  await UrlbarTestUtils.promisePopupClose(win, () => win.gURLBar.blur());
  await UrlbarTestUtils.assertSearchMode(win, null);

  // Check search mode isn't re-entered when re-opening the view.
  await UrlbarTestUtils.promisePopupOpen(win, () => {
    if (win.gURLBar.getAttribute("pageproxystate") == "invalid") {
      win.gURLBar.handleRevert();
    }
    EventUtils.synthesizeMouseAtCenter(win.gURLBar.inputField, {}, win);
  });
  await UrlbarTestUtils.assertSearchMode(win, null);
  await UrlbarTestUtils.promisePopupClose(win);
});

// Tests that search more preview is exited when the user switches tabs.
add_task(async function tabSwitch() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "@",
  });

  while (win.gURLBar.searchMode?.engineName != TEST_ENGINE_NAME) {
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  }
  await UrlbarTestUtils.assertSearchMode(win, {
    engineName: TEST_ENGINE_NAME,
    isPreview: true,
    entry: "keywordoffer",
  });

  // Open a new tab then switch back to the original tab.
  let tab1 = win.gBrowser.selectedTab;
  let tab2 = await BrowserTestUtils.openNewForegroundTab(win.gBrowser);
  await BrowserTestUtils.switchTab(win.gBrowser, tab1);

  await UrlbarTestUtils.assertSearchMode(win, null);
  BrowserTestUtils.removeTab(tab2);
});

// Tests that search mode is previewed when the user down arrows through the
// one-offs.
add_task(async function oneOff_downArrow() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "",
  });
  let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(win);
  await TestUtils.waitForCondition(
    () => !oneOffs._rebuilding,
    "Waiting for one-offs to finish rebuilding"
  );
  let resultCount = UrlbarTestUtils.getResultCount(win);

  // Key down through all results.
  for (let i = 0; i < resultCount; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  }

  // Key down again. The first one-off should be selected.
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);

  // Check for the one-off's search mode previews.
  while (oneOffs.selectedButton != oneOffs.settingsButton) {
    await UrlbarTestUtils.assertSearchMode(
      win,
      getExpectedSearchMode(oneOffs.selectedButton)
    );
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  }

  // Check that selecting the search settings button leaves search mode preview.
  Assert.equal(
    oneOffs.selectedButton,
    oneOffs.settingsButton,
    "The settings button is selected."
  );
  await UrlbarTestUtils.assertSearchMode(win, null);

  // Closing the view should also exit search mode preview.
  await UrlbarTestUtils.promisePopupClose(win);
  await UrlbarTestUtils.assertSearchMode(win, null);
});

// Tests that search mode is previewed when the user Alt+down arrows through the
// one-offs. This subtest also highlights a keywordoffer result (the first Top
// Site) before Alt+Arrowing to the one-offs. This checks that the search mode
// previews from keywordoffer results are overwritten by selected one-offs.
add_task(async function oneOff_alt_downArrow() {
  // Add some visits to a URL so we have multiple Top Sites.
  await PlacesUtils.history.clear();
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits("https://example.com/");
  }
  await updateTopSites(
    sites =>
      sites &&
      sites[0]?.searchTopSite &&
      sites[1]?.url == "https://example.com/",
    true
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "",
  });
  let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(win);
  await TestUtils.waitForCondition(
    () => !oneOffs._rebuilding,
    "Waiting for one-offs to finish rebuilding"
  );

  // Key down to the first result and check that it enters search mode preview.
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  let searchTopSite = await UrlbarTestUtils.getDetailsOfResultAt(win, 0);
  await UrlbarTestUtils.assertSearchMode(win, {
    engineName: searchTopSite.searchParams.engine,
    isPreview: true,
    entry: "topsites_urlbar",
  });

  // Alt+down. The first one-off should be selected.
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true }, win);
  // Check for the one-offs' search mode previews.
  while (oneOffs.selectedButton) {
    await UrlbarTestUtils.assertSearchMode(
      win,
      getExpectedSearchMode(oneOffs.selectedButton)
    );
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true }, win);
  }

  // Now key down without a modifier. We should move to the second result and
  // have no search mode preview.
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(win),
    1,
    "The second result is selected."
  );
  await UrlbarTestUtils.assertSearchMode(win, null);

  // Arrow back up to the keywordoffer result and check for search mode preview.
  EventUtils.synthesizeKey("KEY_ArrowUp", {}, win);
  await UrlbarTestUtils.assertSearchMode(win, {
    engineName: searchTopSite.searchParams.engine,
    isPreview: true,
    entry: "topsites_urlbar",
  });

  await PlacesUtils.history.clear();
  await UrlbarTestUtils.promisePopupClose(win);
  await UrlbarTestUtils.assertSearchMode(win, null);
});

// Tests that search mode is previewed when the user is in full search mode
// and down arrows through the one-offs.
add_task(async function fullSearchMode_oneOff_downArrow() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "",
  });
  let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(win);
  await TestUtils.waitForCondition(
    () => !oneOffs._rebuilding,
    "Waiting for one-offs to finish rebuilding"
  );
  let oneOffButtons = oneOffs.getSelectableButtons(true);

  await UrlbarTestUtils.enterSearchMode(win);
  let expectedSearchMode = getExpectedSearchMode(oneOffButtons[0], false);
  // Sanity check: we are in the correct search mode.
  await UrlbarTestUtils.assertSearchMode(win, expectedSearchMode);

  // Key down through all results.
  let resultCount = UrlbarTestUtils.getResultCount(win);
  for (let i = 0; i < resultCount; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    // If the result is a shortcut, it will enter preview mode.
    let result = await UrlbarTestUtils.getDetailsOfResultAt(win, i);
    await UrlbarTestUtils.assertSearchMode(
      win,
      Object.assign(expectedSearchMode, {
        isPreview: !!result.searchParams.keyword,
      })
    );
  }

  // Key down again. The first one-off should be selected.
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  // Check that we show the correct preview as we cycle through the one-offs.
  while (oneOffs.selectedButton != oneOffs.settingsButton) {
    await UrlbarTestUtils.assertSearchMode(
      win,
      getExpectedSearchMode(oneOffs.selectedButton, true)
    );
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  }

  // We should still be in the same search mode after cycling through all the
  // one-offs.
  await UrlbarTestUtils.assertSearchMode(win, expectedSearchMode);
  await UrlbarTestUtils.exitSearchMode(win);
  await UrlbarTestUtils.promisePopupClose(win);
});

// Tests that search mode is previewed when the user is in full search mode
// and alt+down arrows through the one-offs. This subtest also checks that
// exiting full search mode while alt+arrowing through the one-offs enters
// search mode preview for subsequent one-offs.
add_task(async function fullSearchMode_oneOff_alt_downArrow() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "",
  });
  let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(win);
  let oneOffButtons = oneOffs.getSelectableButtons(true);
  await TestUtils.waitForCondition(
    () => !oneOffs._rebuilding,
    "Waiting for one-offs to finish rebuilding"
  );

  await UrlbarTestUtils.enterSearchMode(win);
  let expectedSearchMode = getExpectedSearchMode(oneOffButtons[0], false);
  await UrlbarTestUtils.assertSearchMode(win, expectedSearchMode);

  // Key down to the first result.
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);

  // Alt+down. The first one-off should be selected.
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true }, win);
  // Cycle through the first half of the one-offs and verify that search mode
  // preview is entered.
  Assert.greater(
    oneOffButtons.length,
    1,
    "Sanity check: We should have at least two one-offs."
  );
  for (let i = 1; i < oneOffButtons.length / 2; i++) {
    await UrlbarTestUtils.assertSearchMode(
      win,
      getExpectedSearchMode(oneOffs.selectedButton, true)
    );
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true }, win);
  }
  // Now click out of search mode.
  await UrlbarTestUtils.exitSearchMode(win, { clickClose: true });
  // Now check for the remaining one-offs' search mode previews.
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true }, win);
  while (oneOffs.selectedButton) {
    await UrlbarTestUtils.assertSearchMode(
      win,
      getExpectedSearchMode(oneOffs.selectedButton, true)
    );
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true }, win);
  }

  await UrlbarTestUtils.promisePopupClose(win);
  await UrlbarTestUtils.assertSearchMode(win, null);
});

// Tests that the original search mode is preserved when going through some
// one-off buttons and then back down in the results list.
add_task(async function fullSearchMode_oneOff_restore_on_down() {
  info("Add a few visits to top sites");
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits([
      "https://1.example.com/",
      "https://2.example.com/",
      "https://3.example.com/",
    ]);
  }
  await updateTopSites(sites => sites?.length > 2, false);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "",
  });
  let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(win);
  let oneOffButtons = oneOffs.getSelectableButtons(true);
  await TestUtils.waitForCondition(
    () => !oneOffs._rebuilding,
    "Waiting for one-offs to finish rebuilding"
  );

  await UrlbarTestUtils.enterSearchMode(win, {
    source: UrlbarUtils.RESULT_SOURCE.HISTORY,
  });
  let expectedSearchMode = getExpectedSearchMode(
    oneOffButtons.find(b => b.source == UrlbarUtils.RESULT_SOURCE.HISTORY),
    false
  );
  await UrlbarTestUtils.assertSearchMode(win, expectedSearchMode);
  info("Down to the first result");
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  await UrlbarTestUtils.assertSearchMode(win, expectedSearchMode);
  info("Alt+down to the first one-off.");
  Assert.greater(
    oneOffButtons.length,
    1,
    "Sanity check: We should have at least two one-offs."
  );
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true }, win);
  await UrlbarTestUtils.assertSearchMode(
    win,
    getExpectedSearchMode(oneOffs.selectedButton, true)
  );
  info("Go again down through the list of results");
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  await UrlbarTestUtils.assertSearchMode(win, expectedSearchMode);

  // Now do a similar test without initial search mode.
  info("Exit search mode.");
  await UrlbarTestUtils.exitSearchMode(win);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "",
  });
  info("Down to the first result");
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  await UrlbarTestUtils.assertSearchMode(win, null);
  info("select a one-off to start preview");
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true }, win);
  await UrlbarTestUtils.assertSearchMode(
    win,
    getExpectedSearchMode(oneOffs.selectedButton, true)
  );
  info("Go again through the list of results");
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  await UrlbarTestUtils.assertSearchMode(win, null);

  await UrlbarTestUtils.promisePopupClose(win);
  await PlacesUtils.history.clear();
});
