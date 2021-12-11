/*
 * Test telemetry for Tracking Protection
 */

const PREF = "privacy.trackingprotection.enabled";
const DTSCBN_PREF = "dom.testing.sync-content-blocking-notifications";
const BENIGN_PAGE =
  "http://tracking.example.org/browser/browser/base/content/test/protectionsUI/benignPage.html";
const TRACKING_PAGE =
  "http://tracking.example.org/browser/browser/base/content/test/protectionsUI/trackingPage.html";

/**
 * Enable local telemetry recording for the duration of the tests.
 */
var oldCanRecord = Services.telemetry.canRecordExtended;
Services.telemetry.canRecordExtended = true;
registerCleanupFunction(function() {
  UrlClassifierTestUtils.cleanupTestTrackers();
  Services.telemetry.canRecordExtended = oldCanRecord;
  Services.prefs.clearUserPref(PREF);
  Services.prefs.clearUserPref(DTSCBN_PREF);
});

function getShieldHistogram() {
  return Services.telemetry.getHistogramById("TRACKING_PROTECTION_SHIELD");
}

function getShieldCounts() {
  return getShieldHistogram().snapshot().values;
}

add_task(async function setup() {
  await UrlClassifierTestUtils.addTestTrackers();
  Services.prefs.setBoolPref(DTSCBN_PREF, true);

  let TrackingProtection =
    gBrowser.ownerGlobal.gProtectionsHandler.blockers.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the browser window");
  ok(!TrackingProtection.enabled, "TP is not enabled");

  let enabledCounts = Services.telemetry
    .getHistogramById("TRACKING_PROTECTION_ENABLED")
    .snapshot().values;
  is(enabledCounts[0], 1, "TP was not enabled on start up");
});

add_task(async function testShieldHistogram() {
  Services.prefs.setBoolPref(PREF, true);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  // Reset these to make counting easier
  getShieldHistogram().clear();

  await promiseTabLoadEvent(tab, BENIGN_PAGE);
  is(getShieldCounts()[0], 1, "Page loads without tracking");

  await promiseTabLoadEvent(tab, TRACKING_PAGE);
  is(getShieldCounts()[0], 2, "Adds one more page load");
  is(getShieldCounts()[2], 1, "Counts one instance of the shield being shown");

  info("Disable TP for the page (which reloads the page)");
  let tabReloadPromise = promiseTabLoadEvent(tab);
  gProtectionsHandler.disableForCurrentPage();
  await tabReloadPromise;
  is(getShieldCounts()[0], 3, "Adds one more page load");
  is(
    getShieldCounts()[1],
    1,
    "Counts one instance of the shield being crossed out"
  );

  info("Re-enable TP for the page (which reloads the page)");
  tabReloadPromise = promiseTabLoadEvent(tab);
  gProtectionsHandler.enableForCurrentPage();
  await tabReloadPromise;
  is(getShieldCounts()[0], 4, "Adds one more page load");
  is(
    getShieldCounts()[2],
    2,
    "Adds one more instance of the shield being shown"
  );

  gBrowser.removeCurrentTab();

  // Reset these to make counting easier for the next test
  getShieldHistogram().clear();
});
