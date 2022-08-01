/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "BrowserGlue",
  "resource:///modules/BrowserGlue.jsm"
);

const PREF_DFPI_ENABLED_BY_DEFAULT =
  "privacy.restrict3rdpartystorage.rollout.enabledByDefault";
const COOKIE_BEHAVIOR_PREF = "network.cookie.cookieBehavior";

const defaultPrefs = Services.prefs.getDefaultBranch("");
const previousDefaultCB = defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF);

function cleanup() {
  [COOKIE_BEHAVIOR_PREF, PREF_DFPI_ENABLED_BY_DEFAULT].forEach(
    Services.prefs.clearUserPref
  );

  BrowserGlue._defaultCookieBehaviorAtStartup = previousDefaultCB;
  defaultPrefs.setIntPref(COOKIE_BEHAVIOR_PREF, previousDefaultCB);
}

// Tests that the dFPI rollout pref updates the default cookieBehavior to 5,
// sets the correct search prefs and records telemetry.
add_task(async function testdFPIRolloutPref() {
  // The BrowserGlue code which computes this flag runs before we can set the
  // default cookie behavior for this test. Thus we need to overwrite it in
  // order for the opt-out to work correctly.
  BrowserGlue._defaultCookieBehaviorAtStartup =
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;
  defaultPrefs.setIntPref(
    COOKIE_BEHAVIOR_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );

  Services.prefs.setBoolPref(PREF_DFPI_ENABLED_BY_DEFAULT, false);
  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );

  Services.prefs.setBoolPref(PREF_DFPI_ENABLED_BY_DEFAULT, true);
  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
  );

  Services.prefs.setBoolPref(PREF_DFPI_ENABLED_BY_DEFAULT, false);
  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );

  Services.prefs.setBoolPref(PREF_DFPI_ENABLED_BY_DEFAULT, true);
  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
  );

  cleanup();
});

/**
 * Setting the rollout pref to false should revert to the initial default cookie
 * behavior, not always BEHAVIOR_REJECT_TRACKER.
 */
add_task(async function testdFPIRolloutPrefNonDefaultCookieBehavior() {
  BrowserGlue._defaultCookieBehaviorAtStartup =
    Ci.nsICookieService.BEHAVIOR_ACCEPT;
  defaultPrefs.setIntPref(
    COOKIE_BEHAVIOR_PREF,
    Ci.nsICookieService.BEHAVIOR_ACCEPT
  );

  is(
    Services.prefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_ACCEPT,
    "Initial cookie behavior should be BEHAVIOR_ACCEPT"
  );

  Services.prefs.setBoolPref(PREF_DFPI_ENABLED_BY_DEFAULT, true);
  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    "Default cookie behavior should be set to dFPI."
  );

  Services.prefs.setBoolPref(PREF_DFPI_ENABLED_BY_DEFAULT, false);
  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_ACCEPT,
    "Default cookie behavior should be set to BEHAVIOR_ACCEPT."
  );

  cleanup();
});

/**
 * When a client already ships with dFPI enabled, toggling the rollout pref
 * should not change cookie behavior.
 */
add_task(async function testdFPIRolloutPrefDFPIAlreadyEnabled() {
  // Simulate TCP enabled by default.
  BrowserGlue._defaultCookieBehaviorAtStartup =
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;
  defaultPrefs.setIntPref(
    COOKIE_BEHAVIOR_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
  );

  is(
    Services.prefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    "Initial cookie behavior should be BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN"
  );

  Services.prefs.setBoolPref(PREF_DFPI_ENABLED_BY_DEFAULT, true);
  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    "Default cookie behavior should still be BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN."
  );

  Services.prefs.setBoolPref(PREF_DFPI_ENABLED_BY_DEFAULT, false);
  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    "Default cookie behavior should still be BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN."
  );

  cleanup();
});
