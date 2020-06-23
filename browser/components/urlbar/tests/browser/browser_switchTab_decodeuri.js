/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test ensures that switch to tab still works when the URI contains an
 * encoded part.
 */

"use strict";

const TEST_URL = `${TEST_BASE_URL}dummy_page.html#test%7C1`;

add_task(async function test_switchtab_decodeuri() {
  info("Opening first tab");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  info("Opening and selecting second tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  info("Wait for autocomplete");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "dummy_page",
  });

  info("Select autocomplete popup entry");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  let result = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    UrlbarTestUtils.getSelectedRowIndex(window)
  );
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.TAB_SWITCH);

  info("switch-to-tab");
  let tabSelectPromise = BrowserTestUtils.waitForEvent(
    window,
    "TabSelect",
    false
  );
  EventUtils.synthesizeKey("KEY_Enter");
  await tabSelectPromise;

  Assert.equal(
    gBrowser.selectedTab,
    tab,
    "Should have switched to the right tab"
  );

  gBrowser.removeCurrentTab();
  await PlacesUtils.history.clear();
});
