"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("identity.fxaccounts.remote.signup.uri");
  Services.prefs.clearUserPref("identity.fxaccounts.remote.email.uri");
  Services.prefs.clearUserPref("services.sync.username");
});

add_task(setup_UITourTest);

add_task(async function setup() {
  Services.prefs.setCharPref("identity.fxaccounts.remote.signup.uri",
                             "https://example.com/signup");
  Services.prefs.setCharPref("identity.fxaccounts.remote.email.uri",
                             "https://example.com/?action=email");
})

add_UITour_task(async function test_checkSyncSetup_disabled() {
  let result = await getConfigurationPromise("sync");
  is(result.setup, false, "Sync shouldn't be setup by default");
});

add_UITour_task(async function test_checkSyncSetup_enabled() {
  Services.prefs.setCharPref("services.sync.username", "uitour@tests.mozilla.org");
  let result = await getConfigurationPromise("sync");
  is(result.setup, true, "Sync should be setup");
});

add_UITour_task(async function test_checkSyncCounts() {
  Services.prefs.setIntPref("services.sync.clients.devices.desktop", 4);
  Services.prefs.setIntPref("services.sync.clients.devices.mobile", 5);
  Services.prefs.setIntPref("services.sync.numClients", 9);
  let result = await getConfigurationPromise("sync");
  is(result.mobileDevices, 5, "mobileDevices should be set");
  is(result.desktopDevices, 4, "desktopDevices should be set");
  is(result.totalDevices, 9, "totalDevices should be set");

  Services.prefs.clearUserPref("services.sync.clients.devices.desktop");
  result = await getConfigurationPromise("sync");
  is(result.mobileDevices, 5, "mobileDevices should be set");
  is(result.desktopDevices, 0, "desktopDevices should be 0");
  is(result.totalDevices, 9, "totalDevices should be set");

  Services.prefs.clearUserPref("services.sync.clients.devices.mobile");
  result = await getConfigurationPromise("sync");
  is(result.mobileDevices, 0, "mobileDevices should be 0");
  is(result.desktopDevices, 0, "desktopDevices should be 0");
  is(result.totalDevices, 9, "totalDevices should be set");

  Services.prefs.clearUserPref("services.sync.numClients");
  result = await getConfigurationPromise("sync");
  is(result.mobileDevices, 0, "mobileDevices should be 0");
  is(result.desktopDevices, 0, "desktopDevices should be 0");
  is(result.totalDevices, 0, "totalDevices should be 0");
});

// The showFirefoxAccounts API is sync related, so we test that here too...
add_UITour_task(async function test_firefoxAccountsNoParams() {
  info("Load https://accounts.firefox.com");
  await gContentAPI.showFirefoxAccounts();
  await BrowserTestUtils.browserLoaded(gTestTab.linkedBrowser, false,
                                       "https://example.com/signup?entrypoint=uitour");
});


add_UITour_task(async function test_firefoxAccountsValidParams() {
  info("Load https://accounts.firefox.com");
  await gContentAPI.showFirefoxAccounts({ utm_foo: "foo", utm_bar: "bar" });
  await BrowserTestUtils.browserLoaded(gTestTab.linkedBrowser, false,
                                       "https://example.com/signup?entrypoint=uitour&utm_foo=foo&utm_bar=bar");
});

add_UITour_task(async function test_firefoxAccountsWithEmail() {
  info("Load https://accounts.firefox.com");
  await gContentAPI.showFirefoxAccounts(null, "foo@bar.com");
  await BrowserTestUtils.browserLoaded(gTestTab.linkedBrowser, false,
                                       "https://example.com/?action=email&email=foo%40bar.com&entrypoint=uitour");
});

add_UITour_task(async function test_firefoxAccountsNonAlphaValue() {
  // All characters in the value are allowed, but they must be automatically escaped.
  // (we throw a unicode character in there too - it's not auto-utf8 encoded,
  // but that's ok, so long as it is escaped correctly.)
  let value = "foo& /=?:\\\xa9";
  // encodeURIComponent encodes spaces to %20 but we want "+"
  let expected = encodeURIComponent(value).replace(/%20/g, "+");
  info("Load https://accounts.firefox.com");
  await gContentAPI.showFirefoxAccounts({ utm_foo: value });
  await BrowserTestUtils.browserLoaded(gTestTab.linkedBrowser, false,
                                       "https://example.com/signup?entrypoint=uitour&utm_foo=" + expected);
});

// A helper to check the request was ignored due to invalid params.
async function checkFxANotLoaded() {
  try {
    await waitForConditionPromise(() => {
      return gBrowser.selectedBrowser.currentURI.spec.startsWith("https://example.com");
    }, "Check if FxA opened");
    ok(false, "No FxA tab should have opened");
  } catch (ex) {
    ok(true, "No FxA tab opened");
  }
}

add_UITour_task(async function test_firefoxAccountsNonObject() {
  // non-string should be rejected.
  await gContentAPI.showFirefoxAccounts(99);
  await checkFxANotLoaded();
});

add_UITour_task(async function test_firefoxAccountsNonUtmPrefix() {
  // Any non "utm_" name should should be rejected.
  await gContentAPI.showFirefoxAccounts({ utm_foo: "foo", bar: "bar" });
  await checkFxANotLoaded();
});

add_UITour_task(async function test_firefoxAccountsNonAlphaName() {
  // Any "utm_" name which includes non-alpha chars should be rejected.
  await gContentAPI.showFirefoxAccounts({ utm_foo: "foo", "utm_bar=": "bar" });
  await checkFxANotLoaded();
});
