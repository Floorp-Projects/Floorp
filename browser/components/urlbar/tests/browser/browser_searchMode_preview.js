/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests search mode preview.
 */

"use strict";

const TEST_ENGINE_NAME = "Test";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.update2", true]],
  });
  let testEngine = await Services.search.addEngineWithDetails(
    TEST_ENGINE_NAME,
    {
      alias: "@test",
      template: "http://example.com/?search={searchTerms}",
    }
  );

  registerCleanupFunction(async () => {
    await Services.search.removeEngine(testEngine);
  });
});

// Tests that cycling through token alias engines enters search mode preview.
add_task(async function tokenAlias() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "@",
  });

  let result;
  while (gURLBar.searchMode?.engineName != TEST_ENGINE_NAME) {
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
    let index = UrlbarTestUtils.getSelectedRowIndex(window);
    result = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
    let expectedSearchMode = {
      engineName: result.searchParams.engine,
      isPreview: true,
      entry: "keywordoffer",
    };
    if (UrlbarUtils.WEB_ENGINE_NAMES.has(result.searchParams.engine)) {
      expectedSearchMode.source = UrlbarUtils.RESULT_SOURCE.SEARCH;
    }
    await UrlbarTestUtils.assertSearchMode(window, expectedSearchMode);
  }
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("KEY_Enter");
  await searchPromise;
  // Test that we are in non-preview search mode.
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: result.searchParams.engine,
    entry: "keywordoffer",
  });
  await UrlbarTestUtils.exitSearchMode(window);
});

// Tests that starting to type a query exits search mode preview in favour of
// full search mode.
add_task(async function startTyping() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "@",
  });
  while (gURLBar.searchMode?.engineName != TEST_ENGINE_NAME) {
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
  }

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: TEST_ENGINE_NAME,
    isPreview: true,
    entry: "keywordoffer",
  });

  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("M");
  await searchPromise;
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: TEST_ENGINE_NAME,
    entry: "keywordoffer",
  });
  await UrlbarTestUtils.exitSearchMode(window);
});

// Tests that highlighting a search shortcut Top Site enters search mode
// preview.
add_task(async function topSites() {
  // Enable search shortcut Top Sites.
  await updateTopSites(
    sites => sites && sites[0] && sites[0].searchTopSite,
    true
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
    fireInputEvent: true,
  });

  // We previously verified that the first Top Site is a search shortcut.
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
  let searchTopSite = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: searchTopSite.searchParams.engine,
    isPreview: true,
    entry: "topsites_urlbar",
  });
  await UrlbarTestUtils.exitSearchMode(window);
});

// Tests that search mode preview is exited when the view is closed.
add_task(async function closeView() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "@",
  });

  while (gURLBar.searchMode?.engineName != TEST_ENGINE_NAME) {
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
  }
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: TEST_ENGINE_NAME,
    isPreview: true,
    entry: "keywordoffer",
  });

  // We should close search mode when closing the view.
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  await UrlbarTestUtils.assertSearchMode(window, null);

  // Check search mode isn't re-entered when re-opening the view.
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    if (gURLBar.getAttribute("pageproxystate") == "invalid") {
      gURLBar.handleRevert();
    }
    EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  });
  await UrlbarTestUtils.assertSearchMode(window, null);
  await UrlbarTestUtils.promisePopupClose(window);
});
