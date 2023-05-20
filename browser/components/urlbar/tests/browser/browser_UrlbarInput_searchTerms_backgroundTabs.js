/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// These tests check the behavior of the Urlbar when search terms are
// expected to be shown and tabs are opened in the background.

let defaultTestEngine;

// The main search string used in tests
const SEARCH_STRING = "chocolate cake";

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

  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
  });
});

// If a user opens background tab search from the Urlbar,
// the search term should show when the tab is focused.
add_task(async function ctrl_open() {
  let [expectedSearchUrl] = UrlbarUtils.getSearchQueryUrl(
    defaultTestEngine,
    SEARCH_STRING
  );
  // Search for the term in a new background tab.
  let newTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    expectedSearchUrl
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: SEARCH_STRING,
    fireInputEvent: true,
  });
  gURLBar.focus();

  EventUtils.synthesizeKey("KEY_Enter", {
    altKey: true,
    shiftKey: true,
  });

  // Find the background tab that was created, and switch to it.
  let backgroundTab = await newTabPromise;
  await BrowserTestUtils.switchTab(gBrowser, backgroundTab);
  assertSearchStringIsInUrlbar(SEARCH_STRING);

  BrowserTestUtils.removeTab(backgroundTab);
});
