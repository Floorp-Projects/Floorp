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
add_UITour_task(function* test_firefoxAccountsNoParams() {
  yield gContentAPI.showFirefoxAccounts();
  yield BrowserTestUtils.browserLoaded(gTestTab.linkedBrowser, false,
                                       "about:accounts?action=signup&entrypoint=uitour");
});

add_UITour_task(function* test_firefoxAccountsValidParams() {
  yield gContentAPI.showFirefoxAccounts({ utm_foo: "foo", utm_bar: "bar" });
  yield BrowserTestUtils.browserLoaded(gTestTab.linkedBrowser, false,
                                       "about:accounts?action=signup&entrypoint=uitour&utm_foo=foo&utm_bar=bar");
});

add_UITour_task(function* test_firefoxAccountsNonAlphaValue() {
  // All characters in the value are allowed, but they must be automatically escaped.
  // (we throw a unicode character in there too - it's not auto-utf8 encoded,
  // but that's ok, so long as it is escaped correctly.)
  let value = "foo& /=?:\\\xa9";
  // encodeURIComponent encodes spaces to %20 but we want "+"
  let expected = encodeURIComponent(value).replace(/%20/g, "+");
  yield gContentAPI.showFirefoxAccounts({ utm_foo: value });
  yield BrowserTestUtils.browserLoaded(gTestTab.linkedBrowser, false,
                                       "about:accounts?action=signup&entrypoint=uitour&utm_foo=" + expected);
});

// A helper to check the request was ignored due to invalid params.
function* checkAboutAccountsNotLoaded() {
  try {
    yield waitForConditionPromise(() => {
      return gBrowser.selectedBrowser.currentURI.spec.startsWith("about:accounts");
    }, "Check if about:accounts opened");
    ok(false, "No about:accounts tab should have opened");
  } catch (ex) {
    ok(true, "No about:accounts tab opened");
  }
}

add_UITour_task(function* test_firefoxAccountsNonObject() {
  // non-string should be rejected.
  yield gContentAPI.showFirefoxAccounts(99);
  yield checkAboutAccountsNotLoaded();
});

add_UITour_task(function* test_firefoxAccountsNonUtmPrefix() {
  // Any non "utm_" name should should be rejected.
  yield gContentAPI.showFirefoxAccounts({ utm_foo: "foo", bar: "bar" });
  yield checkAboutAccountsNotLoaded();
});

add_UITour_task(function* test_firefoxAccountsNonAlphaName() {
  // Any "utm_" name which includes non-alpha chars should be rejected.
  yield gContentAPI.showFirefoxAccounts({ utm_foo: "foo", "utm_bar=": "bar" });
  yield checkAboutAccountsNotLoaded();
});
