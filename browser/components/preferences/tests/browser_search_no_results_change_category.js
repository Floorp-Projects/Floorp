"use strict";

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.search", true]],
  });
});

add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });

  let searchInput = gBrowser.contentDocument.getElementById("searchInput");
  is(
    searchInput,
    gBrowser.contentDocument.activeElement.closest("#searchInput"),
    "Search input should be focused when visiting preferences"
  );
  let query = "ffff____noresults____ffff";
  let searchCompletedPromise = BrowserTestUtils.waitForEvent(
    gBrowser.contentWindow,
    "PreferencesSearchCompleted",
    evt => evt.detail == query
  );
  EventUtils.sendString(query);
  await searchCompletedPromise;

  let noResultsEl = gBrowser.contentDocument.querySelector(
    "#no-results-message"
  );
  is_element_visible(
    noResultsEl,
    "Should be reporting no results for this query"
  );

  await gBrowser.contentWindow.gotoPref("panePrivacy");
  is_element_hidden(
    noResultsEl,
    "Should not be showing the 'no results' message after selecting a category"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
