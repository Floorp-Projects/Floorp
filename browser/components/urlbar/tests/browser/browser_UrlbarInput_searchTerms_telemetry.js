/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * These tests check that we record the number of times search terms
 * persist in the Urlbar, and when search terms are cleared due to a
 * PopupNotification.
 *
 * This is different from existing telemetry that tracks whether users
 * interacted with the Urlbar or made another search while the search
 * terms were peristed.
 */

let defaultTestEngine;

// The main search string used in tests
const SEARCH_STRING = "chocolate cake";

// Telemetry keys.
const PERSISTED_VIEWED = "urlbar.persistedsearchterms.view_count";
const PERSISTED_REVERTED = "urlbar.persistedsearchterms.revert_by_popup_count";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });
  await SearchTestUtils.installSearchExtension(
    {
      name: "MozSearch",
      search_url: "https://www.example.com/",
      search_url_get_params: "q={searchTerms}&pc=fake_code",
    },
    { setAsDefault: true }
  );
  defaultTestEngine = Services.search.getEngineByName("MozSearch");
  Services.telemetry.clearScalars();

  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
    Services.telemetry.clearScalars();
  });
});

// Starts a search with a tab and asserts that
// the state of the Urlbar contains the search term.
async function searchWithTab(
  searchString,
  tab = null,
  engine = defaultTestEngine,
  assertSearchString = true
) {
  if (!tab) {
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  }

  let [expectedSearchUrl] = UrlbarUtils.getSearchQueryUrl(engine, searchString);
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    expectedSearchUrl
  );

  gURLBar.focus();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: searchString,
    fireInputEvent: true,
  });
  EventUtils.synthesizeKey("KEY_Enter");
  await browserLoadedPromise;

  if (assertSearchString) {
    assertSearchStringIsInUrlbar(searchString);
  }

  return { tab, expectedSearchUrl };
}

add_task(async function load_page_with_persisted_search() {
  let { tab } = await searchWithTab(SEARCH_STRING);
  const scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_VIEWED, 1);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_REVERTED, undefined);

  // Clean up.
  BrowserTestUtils.removeTab(tab);
});

add_task(async function load_page_without_persisted_search() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", false]],
  });

  let { tab } = await searchWithTab(
    SEARCH_STRING,
    null,
    defaultTestEngine,
    false
  );

  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_VIEWED, undefined);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_REVERTED, undefined);

  // Clean up.
  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

// Multiple searches should result in the correct number of recorded views.
add_task(async function load_page_n_times() {
  let N = 5;
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  for (let index = 0; index < N; ++index) {
    await searchWithTab(SEARCH_STRING, tab);
  }

  const scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_VIEWED, N);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_REVERTED, undefined);

  // Clean up.
  BrowserTestUtils.removeTab(tab);
});

// A persisted search view event should not be recorded when unfocusing the urlbar.
add_task(async function focus_and_unfocus() {
  let { tab } = await searchWithTab(SEARCH_STRING);

  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, false);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_VIEWED, 1);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_REVERTED, undefined);

  gURLBar.focus();
  gURLBar.select();
  gURLBar.blur();

  // Focusing and unfocusing the urlbar shouldn't change the persisted view count.
  scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_VIEWED, 1);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_REVERTED, undefined);

  // Clean up.
  BrowserTestUtils.removeTab(tab);
});

// A persisted search view event should not be recorded by a
// pushState event after a page has been loaded.
add_task(async function history_api() {
  let { tab } = await searchWithTab(SEARCH_STRING);

  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, false);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_VIEWED, 1);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_REVERTED, undefined);

  await SpecialPowers.spawn(tab.linkedBrowser, [], function () {
    let url = new URL(content.window.location);
    let someState = { value: true };
    url.searchParams.set("pc", "fake_code_2");
    content.history.pushState(someState, "", url);
    someState.value = false;
    content.history.replaceState(someState, "", url);
  });

  scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_VIEWED, 1);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_REVERTED, undefined);

  // Clean up.
  BrowserTestUtils.removeTab(tab);
});

// A persisted search view event should be recorded when switching back to a tab
// that contains search terms.
add_task(async function switch_tabs() {
  let { tab } = await searchWithTab(SEARCH_STRING);

  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, false);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_VIEWED, 1);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_REVERTED, undefined);

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await BrowserTestUtils.switchTab(gBrowser, tab);

  scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_VIEWED, 2);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_REVERTED, undefined);

  // Clean up.
  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(tab2);
});

// A telemetry event should be recorded when returning to a persisted SERP via tabhistory.
add_task(async function tabhistory() {
  let { tab } = await searchWithTab(SEARCH_STRING);

  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, false);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_VIEWED, 1);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_REVERTED, undefined);

  let browserLoadedPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    "https://www.example.com/some_url"
  );
  await browserLoadedPromise;

  let pageShowPromise = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "pageshow"
  );
  tab.linkedBrowser.goBack();
  await pageShowPromise;

  scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_VIEWED, 2);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_REVERTED, undefined);

  // Clean up.
  BrowserTestUtils.removeTab(tab);
});

// PopupNotification's that rely on an anchor element in the urlbar should trigger
// an increment of the revert counter.
// This assumes the anchor element is present in the Urlbar during a valid pageproxystate.
add_task(async function popup_in_urlbar() {
  let { tab } = await searchWithTab(SEARCH_STRING);
  let promisePopupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  PopupNotifications.show(
    gBrowser.selectedBrowser,
    "test-notification",
    "This is a sample popup."
  );
  await promisePopupShown;

  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_VIEWED, 1);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_REVERTED, 1);

  // Clean up.
  BrowserTestUtils.removeTab(tab);
});

// Non-persistent PopupNotifications won't re-appear if a user switches
// tabs and returns to the tab that had the Popup.
add_task(async function non_persistent_popup_in_urlbar_switch_tab() {
  let { tab } = await searchWithTab(SEARCH_STRING);
  let promisePopupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  PopupNotifications.show(
    gBrowser.selectedBrowser,
    "test-notification",
    "This is a sample popup.",
    "geo-notification-icon"
  );
  await promisePopupShown;

  let scalars = TelemetryTestUtils.getProcessScalars("parent", false);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_VIEWED, 1);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_REVERTED, 1);

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await BrowserTestUtils.switchTab(gBrowser, tab);

  scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_VIEWED, 2);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_REVERTED, 1);

  // Clean up.
  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(tab2);
});

// Persistent PopupNotifications will constantly appear to users
// even if they switch to another tab and switch back.
add_task(async function persistent_popup_in_urlbar_switch_tab() {
  let { tab } = await searchWithTab(SEARCH_STRING);
  let promisePopupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  PopupNotifications.show(
    gBrowser.selectedBrowser,
    "test-notification",
    "This is a sample popup.",
    "geo-notification-icon",
    null,
    null,
    { persistent: true }
  );
  await promisePopupShown;

  let scalars = TelemetryTestUtils.getProcessScalars("parent", false);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_VIEWED, 1);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_REVERTED, 1);

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  promisePopupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  await BrowserTestUtils.switchTab(gBrowser, tab);
  await promisePopupShown;

  scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_VIEWED, 2);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_REVERTED, 2);

  // Clean up.
  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(tab2);
});

// If the persist feature is not enabled, a telemetry event should not be recorded
// if a PopupNotification uses an anchor in the Urlbar.
add_task(async function popup_in_urlbar_without_feature() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", false]],
  });

  let { tab } = await searchWithTab(
    SEARCH_STRING,
    null,
    defaultTestEngine,
    false
  );
  let promisePopupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  PopupNotifications.show(
    gBrowser.selectedBrowser,
    "test-notification",
    "This is a sample popup."
  );
  await promisePopupShown;

  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_VIEWED, undefined);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_REVERTED, undefined);

  // Clean up.
  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

// If the anchor element for the PopupNotification is not located in the Urlbar,
// a telemetry event should not be recorded.
add_task(async function popup_not_in_urlbar() {
  let { tab } = await searchWithTab(SEARCH_STRING);

  let promisePopupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  PopupNotifications.show(
    gBrowser.selectedBrowser,
    "test-notification",
    "This is a sample popup that uses the unified extensions button.",
    gUnifiedExtensions.getPopupAnchorID(gBrowser.selectedBrowser, window)
  );
  await promisePopupShown;

  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_VIEWED, 1);
  TelemetryTestUtils.assertScalar(scalars, PERSISTED_REVERTED, undefined);

  // Clean up.
  BrowserTestUtils.removeTab(tab);
});
