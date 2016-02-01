"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("services.sync.username");
});

add_task(setup_UITourTest);

add_UITour_task(function* test_checkSyncSetup_disabled() {
  let result = yield getConfigurationPromise("sync");
  is(result.setup, false, "Sync shouldn't be setup by default");
});

add_UITour_task(function* test_checkSyncSetup_enabled() {
  Services.prefs.setCharPref("services.sync.username", "uitour@tests.mozilla.org");
  let result = yield getConfigurationPromise("sync");
  is(result.setup, true, "Sync should be setup");
});

// The showFirefoxAccounts API is sync related, so we test that here too...
add_UITour_task(function* test_firefoxAccounts() {
  yield gContentAPI.showFirefoxAccounts();
  yield BrowserTestUtils.browserLoaded(gTestTab.linkedBrowser, false,
                                       "about:accounts?action=signup&entrypoint=uitour");
});
