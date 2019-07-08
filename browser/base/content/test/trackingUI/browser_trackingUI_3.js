/*
 * Test that the Tracking Protection is correctly enabled / disabled
 * in both normal and private windows given all possible states of the prefs:
 *   privacy.trackingprotection.enabled
 *   privacy.trackingprotection.pbmode.enabled
 * See also Bug 1178985.
 */

const PREF = "privacy.trackingprotection.enabled";
const PB_PREF = "privacy.trackingprotection.pbmode.enabled";

registerCleanupFunction(function() {
  Services.prefs.clearUserPref(PREF);
  Services.prefs.clearUserPref(PB_PREF);
});

add_task(async function testNormalBrowsing() {
  let TrackingProtection = gBrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the browser window");

  Services.prefs.setBoolPref(PREF, true);
  Services.prefs.setBoolPref(PB_PREF, false);
  ok(TrackingProtection.enabled, "TP is enabled (ENABLED=true,PB=false)");
  Services.prefs.setBoolPref(PB_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled (ENABLED=true,PB=true)");

  Services.prefs.setBoolPref(PREF, false);
  Services.prefs.setBoolPref(PB_PREF, false);
  ok(!TrackingProtection.enabled, "TP is disabled (ENABLED=false,PB=false)");
  Services.prefs.setBoolPref(PB_PREF, true);
  ok(!TrackingProtection.enabled, "TP is disabled (ENABLED=false,PB=true)");
});

add_task(async function testPrivateBrowsing() {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  let TrackingProtection = privateWin.gBrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the browser window");

  Services.prefs.setBoolPref(PREF, true);
  Services.prefs.setBoolPref(PB_PREF, false);
  ok(TrackingProtection.enabled, "TP is enabled (ENABLED=true,PB=false)");
  Services.prefs.setBoolPref(PB_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled (ENABLED=true,PB=true)");

  Services.prefs.setBoolPref(PREF, false);
  Services.prefs.setBoolPref(PB_PREF, false);
  ok(!TrackingProtection.enabled, "TP is disabled (ENABLED=false,PB=false)");
  Services.prefs.setBoolPref(PB_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled (ENABLED=false,PB=true)");

  privateWin.close();
});
