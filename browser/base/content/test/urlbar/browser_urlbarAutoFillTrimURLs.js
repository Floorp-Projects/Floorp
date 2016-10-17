// This test ensures that autoFilled values are not trimmed, unless the user
// selects from the autocomplete popup.

add_task(function* setup() {
  const PREF_TRIMURL = "browser.urlbar.trimURLs";
  const PREF_AUTOFILL = "browser.urlbar.autoFill";

  registerCleanupFunction(function* () {
    Services.prefs.clearUserPref(PREF_TRIMURL);
    Services.prefs.clearUserPref(PREF_AUTOFILL);
    yield PlacesTestUtils.clearHistory();
    gURLBar.handleRevert();
  });
  Services.prefs.setBoolPref(PREF_TRIMURL, true);
  Services.prefs.setBoolPref(PREF_AUTOFILL, true);

  // Adding a tab would hit switch-to-tab, so it's safer to just add a visit.
  yield PlacesTestUtils.addVisits({
    uri: "http://www.autofilltrimurl.com/whatever",
    transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
  });
});

function* promiseSearch(searchtext) {
  gURLBar.focus();
  gURLBar.inputField.value = searchtext.substr(0, searchtext.length -1);
  EventUtils.synthesizeKey(searchtext.substr(-1, 1), {});
  yield promiseSearchComplete();
}

add_task(function* () {
  yield promiseSearch("http://");
  is(gURLBar.inputField.value, "http://", "Autofilled value is as expected");
});

add_task(function* () {
  yield promiseSearch("http://au");
  is(gURLBar.inputField.value, "http://autofilltrimurl.com/", "Autofilled value is as expected");
});

add_task(function* () {
  yield promiseSearch("http://www.autofilltrimurl.com");
  is(gURLBar.inputField.value, "http://www.autofilltrimurl.com/", "Autofilled value is as expected");

  // Now ensure selecting from the popup correctly trims.
  is(gURLBar.controller.matchCount, 2, "Found the expected number of matches");
  EventUtils.synthesizeKey("VK_DOWN", {});
  is(gURLBar.inputField.value, "www.autofilltrimurl.com/whatever", "trim was applied correctly");
});
