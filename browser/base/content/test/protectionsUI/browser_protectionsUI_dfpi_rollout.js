/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const PREF_DFPI_ENABLED_BY_DEFAULT =
  "privacy.restrict3rdpartystorage.rollout.enabledByDefault";
const COOKIE_BEHAVIOR_PREF = "network.cookie.cookieBehavior";

const defaultPrefs = Services.prefs.getDefaultBranch("");
const previousDefaultCB = defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF);

const SEARCH_PREFS_OPT_IN = [
  ["browser.search.param.google_channel_us", "tus7"],
  ["browser.search.param.google_channel_row", "trow7"],
  ["browser.search.param.bing_ptag", "MOZZ0000000031"],
];

const SEARCH_PREFS_OPT_OUT = [
  ["browser.search.param.google_channel_us", "xus7"],
  ["browser.search.param.google_channel_row", "xrow7"],
  ["browser.search.param.bing_ptag", "MOZZ0000000032"],
];

registerCleanupFunction(function() {
  defaultPrefs.setIntPref(COOKIE_BEHAVIOR_PREF, previousDefaultCB);
  [
    PREF_DFPI_ENABLED_BY_DEFAULT,
    ...SEARCH_PREFS_OPT_IN,
    ...SEARCH_PREFS_OPT_OUT,
  ].forEach(([key]) => Services.prefs.clearUserPref(key));
});

function testSearchPrefState(optIn) {
  let expectedPrefs = optIn ? SEARCH_PREFS_OPT_IN : SEARCH_PREFS_OPT_OUT;

  expectedPrefs.forEach(([key, value]) => {
    ok(
      Services.prefs.prefHasUserValue(key),
      `Pref '${key}' should have user value.'`
    );
    is(
      Services.prefs.getStringPref(key),
      value,
      `Pref '${key}' should have correct value.`
    );
  });
}

function testTelemetryState(optIn) {
  let expectedValue;
  if (optIn == null) {
    expectedValue = 2;
  } else {
    expectedValue = optIn ? 1 : 0;
  }

  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    "privacy.dfpi_rollout_enabledByDefault",
    expectedValue,
    "Scalar should have correct value"
  );
}

// Tests that the dFPI rollout pref updates the default cookieBehavior to 5,
// sets the correct search prefs and records telemetry.
add_task(async function testdFPIRolloutPref() {
  defaultPrefs.setIntPref(
    COOKIE_BEHAVIOR_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );

  // Test the unset state of the pref.
  testTelemetryState(null);

  Services.prefs.setBoolPref(PREF_DFPI_ENABLED_BY_DEFAULT, false);
  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );
  testSearchPrefState(false);
  testTelemetryState(false);

  Services.prefs.setBoolPref(PREF_DFPI_ENABLED_BY_DEFAULT, true);
  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
  );
  testSearchPrefState(true);
  testTelemetryState(true);

  Services.prefs.setBoolPref(PREF_DFPI_ENABLED_BY_DEFAULT, false);
  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );
  testSearchPrefState(false);
  testTelemetryState(false);

  Services.prefs.setBoolPref(PREF_DFPI_ENABLED_BY_DEFAULT, true);
  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
  );
  testSearchPrefState(true);
  testTelemetryState(true);
});
