"use strict";

var gTestTab;
var gContentAPI;

const MOCK_FLOW_ID =
  "5445b28b8b7ba6cf71e345f8fff4bc59b2a514f78f3e2cc99b696449427fd445";
const MOCK_FLOW_BEGIN_TIME = 1590780440325;
const MOCK_DEVICE_ID = "7e450f3337d3479b8582ea1c9bb5ba6c";

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("identity.fxaccounts.remote.root");
  Services.prefs.clearUserPref("services.sync.username");
});

add_task(setup_UITourTest);

add_task(async function setup() {
  Services.prefs.setCharPref(
    "identity.fxaccounts.remote.root",
    "https://example.com"
  );
});

add_UITour_task(async function test_checkSyncSetup_disabled() {
  let result = await getConfigurationPromise("sync");
  is(result.setup, false, "Sync shouldn't be setup by default");
});

add_UITour_task(async function test_checkSyncSetup_enabled() {
  Services.prefs.setCharPref(
    "services.sync.username",
    "uitour@tests.mozilla.org"
  );
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
  await BrowserTestUtils.browserLoaded(
    gTestTab.linkedBrowser,
    false,
    "https://example.com/?context=fx_desktop_v3&entrypoint=uitour&action=email&service=sync"
  );
});

add_UITour_task(async function test_firefoxAccountsValidParams() {
  info("Load https://accounts.firefox.com");
  await gContentAPI.showFirefoxAccounts({ utm_foo: "foo", utm_bar: "bar" });
  await BrowserTestUtils.browserLoaded(
    gTestTab.linkedBrowser,
    false,
    "https://example.com/?context=fx_desktop_v3&entrypoint=uitour&action=email&service=sync&utm_foo=foo&utm_bar=bar"
  );
});

add_UITour_task(async function test_firefoxAccountsWithEmail() {
  info("Load https://accounts.firefox.com");
  await gContentAPI.showFirefoxAccounts(null, null, "foo@bar.com");
  await BrowserTestUtils.browserLoaded(
    gTestTab.linkedBrowser,
    false,
    "https://example.com/?context=fx_desktop_v3&entrypoint=uitour&email=foo%40bar.com&service=sync"
  );
});

add_UITour_task(async function test_firefoxAccountsWithEmailAndFlowParams() {
  info("Load https://accounts.firefox.com with flow params");
  const flowParams = {
    flow_id: MOCK_FLOW_ID,
    flow_begin_time: MOCK_FLOW_BEGIN_TIME,
    device_id: MOCK_DEVICE_ID,
  };
  await gContentAPI.showFirefoxAccounts(flowParams, null, "foo@bar.com");
  await BrowserTestUtils.browserLoaded(
    gTestTab.linkedBrowser,
    false,
    "https://example.com/?context=fx_desktop_v3&entrypoint=uitour&email=foo%40bar.com&service=sync&" +
      `flow_id=${MOCK_FLOW_ID}&flow_begin_time=${MOCK_FLOW_BEGIN_TIME}&device_id=${MOCK_DEVICE_ID}`
  );
});

add_UITour_task(
  async function test_firefoxAccountsWithEmailAndBadFlowParamValues() {
    info("Load https://accounts.firefox.com with bad flow params");
    const BAD_MOCK_FLOW_ID = "1";
    const BAD_MOCK_FLOW_BEGIN_TIME = 100;

    await gContentAPI.showFirefoxAccounts(
      {
        flow_id: BAD_MOCK_FLOW_ID,
        flow_begin_time: MOCK_FLOW_BEGIN_TIME,
        device_id: MOCK_DEVICE_ID,
      },
      null,
      "foo@bar.com"
    );
    await checkFxANotLoaded();

    await gContentAPI.showFirefoxAccounts(
      {
        flow_id: MOCK_FLOW_ID,
        flow_begin_time: BAD_MOCK_FLOW_BEGIN_TIME,
        device_id: MOCK_DEVICE_ID,
      },
      null,
      "foo@bar.com"
    );
    await checkFxANotLoaded();
  }
);

add_UITour_task(
  async function test_firefoxAccountsWithEmailAndMissingFlowParamValues() {
    info("Load https://accounts.firefox.com with missing flow params");

    await gContentAPI.showFirefoxAccounts(
      {
        flow_id: MOCK_FLOW_ID,
        flow_begin_time: MOCK_FLOW_BEGIN_TIME,
      },
      null,
      "foo@bar.com"
    );
    await BrowserTestUtils.browserLoaded(
      gTestTab.linkedBrowser,
      false,
      "https://example.com/?context=fx_desktop_v3&entrypoint=uitour&email=foo%40bar.com&service=sync&" +
        `flow_id=${MOCK_FLOW_ID}&flow_begin_time=${MOCK_FLOW_BEGIN_TIME}`
    );
  }
);

add_UITour_task(async function test_firefoxAccountsWithEmailAndEntrypoints() {
  info("Load https://accounts.firefox.com with entrypoint parameters");

  await gContentAPI.showFirefoxAccounts(
    {
      entrypoint_experiment: "exp",
      entrypoint_variation: "var",
    },
    "entry",
    "foo@bar.com"
  );
  await BrowserTestUtils.browserLoaded(
    gTestTab.linkedBrowser,
    false,
    "https://example.com/?context=fx_desktop_v3&entrypoint=entry&email=foo%40bar.com&service=sync&" +
      `entrypoint_experiment=exp&entrypoint_variation=var`
  );
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
  await BrowserTestUtils.browserLoaded(
    gTestTab.linkedBrowser,
    false,
    "https://example.com/?context=fx_desktop_v3&entrypoint=uitour&action=email&service=sync&utm_foo=" +
      expected
  );
});

// A helper to check the request was ignored due to invalid params.
async function checkFxANotLoaded() {
  try {
    await waitForConditionPromise(() => {
      return gBrowser.selectedBrowser.currentURI.spec.startsWith(
        "https://example.com"
      );
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
