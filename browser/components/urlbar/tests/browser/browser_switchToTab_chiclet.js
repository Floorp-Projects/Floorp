/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for chiclet upon switching tab mode.
 */

"use strict";

const TEST_URL = `${TEST_BASE_URL}dummy_page.html`;

add_task(async function test_with_oneoff_button() {
  info("Loading test page into first tab");
  let promiseLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    TEST_URL
  );
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, TEST_URL);
  await promiseLoad;

  info("Opening a new tab");
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  info("Wait for autocomplete");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });

  info("Enter Tabs mode");
  await UrlbarTestUtils.enterSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.TABS,
  });

  info("Select first popup entry");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "dummy",
  });
  EventUtils.synthesizeKey("KEY_ArrowDown");
  const result = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    UrlbarTestUtils.getSelectedRowIndex(window)
  );
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.TAB_SWITCH);

  info("Enter escape key");
  EventUtils.synthesizeKey("KEY_Escape");

  info("Check label visibility");
  const searchModeTitle = document.getElementById(
    "urlbar-search-mode-indicator-title"
  );
  const switchTabLabel = document.getElementById("urlbar-label-switchtab");
  await BrowserTestUtils.waitForCondition(
    () =>
      BrowserTestUtils.isVisible(searchModeTitle) &&
      searchModeTitle.textContent === "Tabs",
    "Waiting until the search mode title will be visible"
  );
  await BrowserTestUtils.waitForCondition(
    () => BrowserTestUtils.isHidden(switchTabLabel),
    "Waiting until the switch tab label will be hidden"
  );

  await PlacesUtils.history.clear();
  gBrowser.removeTab(tab);
});

add_task(async function test_with_keytype() {
  info("Loading test page into first tab");
  let promiseLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    TEST_URL
  );
  BrowserTestUtils.startLoadingURIString(gBrowser, TEST_URL);
  await promiseLoad;

  info("Opening a new tab");
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  info("Enter Tabs mode with keytype");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "%",
  });

  info("Select second popup entry");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "dummy",
  });
  EventUtils.synthesizeKey("KEY_ArrowDown");
  const result = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    UrlbarTestUtils.getSelectedRowIndex(window)
  );
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.TAB_SWITCH);

  info("Enter escape key");
  EventUtils.synthesizeKey("KEY_Escape");

  info("Check label visibility");
  const searchModeTitle = document.getElementById(
    "urlbar-search-mode-indicator-title"
  );
  const switchTabLabel = document.getElementById("urlbar-label-switchtab");
  await BrowserTestUtils.waitForCondition(
    () => BrowserTestUtils.isHidden(searchModeTitle),
    "Waiting until the search mode title will be hidden"
  );
  await BrowserTestUtils.waitForCondition(
    () => BrowserTestUtils.isVisible(switchTabLabel),
    "Waiting until the switch tab label will be visible"
  );

  await PlacesUtils.history.clear();
  gBrowser.removeTab(tab);
});
