/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_DFPI_ENABLED_BY_DEFAULT =
  "privacy.restrict3rdpartystorage.rollout.enabledByDefault";
const COOKIE_BEHAVIOR_PREF = "network.cookie.cookieBehavior";

const defaultPrefs = Services.prefs.getDefaultBranch("");
const previousDefaultCB = defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF);

registerCleanupFunction(function() {
  defaultPrefs.setIntPref(COOKIE_BEHAVIOR_PREF, previousDefaultCB);
});

// Tests that the dFPI rollout pref updates the default cookieBehavior to 5
add_task(async function testdFPIRolloutPref() {
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
});
