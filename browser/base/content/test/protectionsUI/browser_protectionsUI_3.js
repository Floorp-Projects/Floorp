/*
 * Test that the Tracking Protection is correctly enabled / disabled
 * in both normal and private windows given all possible states of the prefs:
 *   privacy.trackingprotection.enabled
 *   privacy.trackingprotection.pbmode.enabled
 *   privacy.trackingprotection.emailtracking.enabled
 *   privacy.trackingprotection.emailtracking.pbmode.enabled
 * See also Bug 1178985, Bug 1819662.
 */

const PREF = "privacy.trackingprotection.enabled";
const PB_PREF = "privacy.trackingprotection.pbmode.enabled";
const EMAIL_PREF = "privacy.trackingprotection.emailtracking.enabled";
const EMAIL_PB_PREF = "privacy.trackingprotection.emailtracking.pbmode.enabled";

registerCleanupFunction(function () {
  Services.prefs.clearUserPref(PREF);
  Services.prefs.clearUserPref(PB_PREF);
  Services.prefs.clearUserPref(EMAIL_PREF);
  Services.prefs.clearUserPref(EMAIL_PB_PREF);
});

add_task(async function testNormalBrowsing() {
  let { TrackingProtection } =
    gBrowser.ownerGlobal.gProtectionsHandler.blockers;
  ok(
    TrackingProtection,
    "Normal window gProtectionsHandler should have TrackingProtection blocker."
  );

  Services.prefs.setBoolPref(PREF, true);
  Services.prefs.setBoolPref(PB_PREF, false);
  Services.prefs.setBoolPref(EMAIL_PREF, false);
  Services.prefs.setBoolPref(EMAIL_PB_PREF, false);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=true,PB=false,EmailEnabled=false,EmailPB=false)"
  );
  Services.prefs.setBoolPref(PB_PREF, true);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=true,PB=true,EmailEnabled=false,EmailPB=false)"
  );
  Services.prefs.setBoolPref(EMAIL_PREF, true);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=true,PB=true,EmailEnabled=true,EmailPB=false)"
  );
  Services.prefs.setBoolPref(EMAIL_PB_PREF, true);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=true,PB=true,EmailEnabled=true,EmailPB=true)"
  );
  Services.prefs.setBoolPref(EMAIL_PREF, false);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=true,PB=true,EmailEnabled=false,EmailPB=true)"
  );
  Services.prefs.setBoolPref(PB_PREF, false);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=true,PB=false,EmailEnabled=false,EmailPB=true)"
  );
  Services.prefs.setBoolPref(EMAIL_PREF, true);
  Services.prefs.setBoolPref(EMAIL_PB_PREF, false);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=true,PB=false,EmailEnabled=true,EmailPB=false)"
  );
  Services.prefs.setBoolPref(EMAIL_PB_PREF, true);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=true,PB=false,EmailEnabled=true,EmailPB=true)"
  );

  Services.prefs.setBoolPref(PREF, false);
  Services.prefs.setBoolPref(PB_PREF, false);
  Services.prefs.setBoolPref(EMAIL_PREF, false);
  Services.prefs.setBoolPref(EMAIL_PB_PREF, false);
  ok(
    !TrackingProtection.enabled,
    "TP is disabled (ENABLED=false,PB=false,EmailEnabled=false,EmailPB=false)"
  );
  Services.prefs.setBoolPref(PB_PREF, true);
  ok(
    !TrackingProtection.enabled,
    "TP is disabled (ENABLED=false,PB=true,EmailEnabled=false,EmailPB=false)"
  );
  Services.prefs.setBoolPref(EMAIL_PREF, true);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=false,PB=true,EmailEnabled=true,EmailPB=false)"
  );
  Services.prefs.setBoolPref(EMAIL_PB_PREF, true);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=false,PB=true,EmailEnabled=true,EmailPB=true)"
  );
  Services.prefs.setBoolPref(EMAIL_PREF, false);
  ok(
    !TrackingProtection.enabled,
    "TP is disabled (ENABLED=false,PB=true,EmailEnabled=false,EmailPB=true)"
  );
  Services.prefs.setBoolPref(PB_PREF, false);
  ok(
    !TrackingProtection.enabled,
    "TP is disabled (ENABLED=false,PB=false,EmailEnabled=false,EmailPB=true)"
  );
  Services.prefs.setBoolPref(EMAIL_PREF, true);
  Services.prefs.setBoolPref(EMAIL_PB_PREF, false);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=false,PB=false,EmailEnabled=true,EmailPB=false)"
  );
  Services.prefs.setBoolPref(EMAIL_PB_PREF, true);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=false,PB=false,EmailEnabled=true,EmailPB=true)"
  );
});

add_task(async function testPrivateBrowsing() {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  let { TrackingProtection } =
    privateWin.gBrowser.ownerGlobal.gProtectionsHandler.blockers;
  ok(
    TrackingProtection,
    "Private window gProtectionsHandler should have TrackingProtection blocker."
  );

  Services.prefs.setBoolPref(PREF, true);
  Services.prefs.setBoolPref(PB_PREF, false);
  Services.prefs.setBoolPref(EMAIL_PREF, false);
  Services.prefs.setBoolPref(EMAIL_PB_PREF, false);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=true,PB=false,EmailEnabled=false,EmailPB=false)"
  );
  Services.prefs.setBoolPref(PB_PREF, true);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=true,PB=true,EmailEnabled=false,EmailPB=false)"
  );
  Services.prefs.setBoolPref(EMAIL_PREF, true);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=true,PB=true,EmailEnabled=true,EmailPB=false)"
  );
  Services.prefs.setBoolPref(EMAIL_PB_PREF, true);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=true,PB=true,EmailEnabled=true,EmailPB=true)"
  );
  Services.prefs.setBoolPref(EMAIL_PREF, false);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=true,PB=true,EmailEnabled=false,EmailPB=true)"
  );
  Services.prefs.setBoolPref(PB_PREF, false);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=true,PB=false,EmailEnabled=false,EmailPB=true)"
  );
  Services.prefs.setBoolPref(EMAIL_PREF, true);
  Services.prefs.setBoolPref(EMAIL_PB_PREF, false);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=true,PB=false,EmailEnabled=true,EmailPB=false)"
  );
  Services.prefs.setBoolPref(EMAIL_PB_PREF, true);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=true,PB=false,EmailEnabled=true,EmailPB=true)"
  );

  Services.prefs.setBoolPref(PREF, false);
  Services.prefs.setBoolPref(PB_PREF, false);
  Services.prefs.setBoolPref(EMAIL_PREF, false);
  Services.prefs.setBoolPref(EMAIL_PB_PREF, false);
  ok(
    !TrackingProtection.enabled,
    "TP is disabled (ENABLED=false,PB=false,EmailEnabled=false,EmailPB=false)"
  );
  Services.prefs.setBoolPref(PB_PREF, true);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=false,PB=true,EmailEnabled=false,EmailPB=false)"
  );
  Services.prefs.setBoolPref(EMAIL_PREF, true);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=false,PB=true,EmailEnabled=true,EmailPB=false)"
  );
  Services.prefs.setBoolPref(EMAIL_PB_PREF, true);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=false,PB=true,EmailEnabled=true,EmailPB=true)"
  );
  Services.prefs.setBoolPref(EMAIL_PREF, false);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=false,PB=true,EmailEnabled=false,EmailPB=true)"
  );
  Services.prefs.setBoolPref(PB_PREF, false);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=false,PB=false,EmailEnabled=false,EmailPB=true)"
  );
  Services.prefs.setBoolPref(EMAIL_PREF, true);
  Services.prefs.setBoolPref(EMAIL_PB_PREF, false);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=false,PB=false,EmailEnabled=true,EmailPB=false)"
  );
  Services.prefs.setBoolPref(EMAIL_PB_PREF, true);
  ok(
    TrackingProtection.enabled,
    "TP is enabled (ENABLED=false,PB=false,EmailEnabled=true,EmailPB=true)"
  );

  privateWin.close();
});
