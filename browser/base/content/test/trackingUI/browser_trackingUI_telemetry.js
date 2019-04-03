/*
 * Test telemetry for Tracking Protection
 */

const PREF = "privacy.trackingprotection.enabled";
const DTSCBN_PREF = "dom.testing.sync-content-blocking-notifications";
const BENIGN_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/benignPage.html";
const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";

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

  let TrackingProtection = gBrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the browser window");
  ok(!TrackingProtection.enabled, "TP is not enabled");

  let enabledCounts =
    Services.telemetry.getHistogramById("TRACKING_PROTECTION_ENABLED").snapshot().values;
  is(enabledCounts[0], 1, "TP was not enabled on start up");

  let scalars = Services.telemetry.getSnapshotForScalars("main", false).parent;

  is(scalars["contentblocking.exceptions"], 0, "no CB exceptions at startup");
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
  document.querySelector("#tracking-action-unblock").doCommand();
  await tabReloadPromise;
  is(getShieldCounts()[0], 3, "Adds one more page load");
  is(getShieldCounts()[1], 1, "Counts one instance of the shield being crossed out");

  info("Re-enable TP for the page (which reloads the page)");
  tabReloadPromise = promiseTabLoadEvent(tab);
  document.querySelector("#tracking-action-block").doCommand();
  await tabReloadPromise;
  is(getShieldCounts()[0], 4, "Adds one more page load");
  is(getShieldCounts()[2], 2, "Adds one more instance of the shield being shown");

  gBrowser.removeCurrentTab();

  // Reset these to make counting easier for the next test
  getShieldHistogram().clear();
});

add_task(async function testIdentityPopupEvents() {
  Services.prefs.setBoolPref(PREF, true);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  await promiseTabLoadEvent(tab, BENIGN_PAGE);

  Services.telemetry.clearEvents();

  await openIdentityPopup();

  let events = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS, true).parent;
  let openEvents = events.filter(
    e => e[1] == "security.ui.identitypopup" && e[2] == "open" && e[3] == "identity_popup");
  is(openEvents.length, 1, "recorded telemetry for opening the identity popup");
  is(openEvents[0][4], "shield-hidden", "recorded the shield as hidden");

  await promiseTabLoadEvent(tab, TRACKING_PAGE);

  await openIdentityPopup();

  events = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS, true).parent;
  openEvents = events.filter(
    e => e[1] == "security.ui.identitypopup" && e[2] == "open" && e[3] == "identity_popup");
  is(openEvents.length, 1, "recorded telemetry for opening the identity popup");
  is(openEvents[0][4], "shield-showing", "recorded the shield as showing");

  info("Disable TP for the page (which reloads the page)");
  let tabReloadPromise = promiseTabLoadEvent(tab);
  document.querySelector("#tracking-action-unblock").doCommand();
  await tabReloadPromise;

  events = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS, true).parent;
  let clickEvents = events.filter(
    e => e[1] == "security.ui.identitypopup" && e[2] == "click" && e[3] == "unblock");
  is(clickEvents.length, 1, "recorded telemetry for the click");

  info("Re-enable TP for the page (which reloads the page)");
  tabReloadPromise = promiseTabLoadEvent(tab);
  document.querySelector("#tracking-action-block").doCommand();
  await tabReloadPromise;

  events = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS, true).parent;
  clickEvents = events.filter(
    e => e[1] == "security.ui.identitypopup" && e[2] == "click" && e[3] == "block");
  is(clickEvents.length, 1, "recorded telemetry for the click");

  gBrowser.removeCurrentTab();
});
