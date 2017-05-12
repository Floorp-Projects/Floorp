/*
 * Test telemetry for Tracking Protection
 */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
const PREF = "privacy.trackingprotection.enabled";
const BENIGN_PAGE = "http://tracking.example.org/browser/browser/base/content/test/general/benignPage.html";
const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/general/trackingPage.html";
const {UrlClassifierTestUtils} = Cu.import("resource://testing-common/UrlClassifierTestUtils.jsm", {});

/**
 * Enable local telemetry recording for the duration of the tests.
 */
var oldCanRecord = Services.telemetry.canRecordExtended;
Services.telemetry.canRecordExtended = true;
Services.prefs.setBoolPref(PREF, false);
Services.telemetry.getHistogramById("TRACKING_PROTECTION_ENABLED").clear();
registerCleanupFunction(function() {
  UrlClassifierTestUtils.cleanupTestTrackers();
  Services.telemetry.canRecordExtended = oldCanRecord;
  Services.prefs.clearUserPref(PREF);
});

function getShieldHistogram() {
  return Services.telemetry.getHistogramById("TRACKING_PROTECTION_SHIELD");
}

function getEnabledHistogram() {
  return Services.telemetry.getHistogramById("TRACKING_PROTECTION_ENABLED");
}

function getEventsHistogram() {
  return Services.telemetry.getHistogramById("TRACKING_PROTECTION_EVENTS");
}

function getShieldCounts() {
  return getShieldHistogram().snapshot().counts;
}

function getEnabledCounts() {
  return getEnabledHistogram().snapshot().counts;
}

function getEventCounts() {
  return getEventsHistogram().snapshot().counts;
}

add_task(async function setup() {
  await UrlClassifierTestUtils.addTestTrackers();

  let TrackingProtection = gBrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the browser window");
  ok(!TrackingProtection.enabled, "TP is not enabled");

  // Open a window with TP disabled to make sure 'enabled' is logged correctly.
  let newWin = await promiseOpenAndLoadWindow({}, true);
  await promiseWindowClosed(newWin);

  is(getEnabledCounts()[0], 1, "TP was disabled once on start up");
  is(getEnabledCounts()[1], 0, "TP was not enabled on start up");

  // Enable TP so the next browser to open will log 'enabled'
  Services.prefs.setBoolPref(PREF, true);
});


add_task(async function testNewWindow() {
  let newWin = await promiseOpenAndLoadWindow({}, true);
  let tab = await BrowserTestUtils.openNewForegroundTab(newWin.gBrowser);
  let TrackingProtection = newWin.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the browser window");

  is(getEnabledCounts()[0], 1, "TP was disabled once on start up");
  is(getEnabledCounts()[1], 1, "TP was enabled once on start up");

  // Reset these to make counting easier
  getEventsHistogram().clear();
  getShieldHistogram().clear();

  await promiseTabLoadEvent(tab, BENIGN_PAGE);
  is(getEventCounts()[0], 1, "Total page loads");
  is(getEventCounts()[1], 0, "Disable actions");
  is(getEventCounts()[2], 0, "Enable actions");
  is(getShieldCounts()[0], 1, "Page loads without tracking");

  await promiseTabLoadEvent(tab, TRACKING_PAGE);
  // Note that right now the events and shield histogram is not measuring what
  // you might think.  Since onSecurityChange fires twice for a tracking page,
  // the total page loads count is double counting, and the shield count
  // (which is meant to measure times when the shield wasn't shown) fires even
  // when tracking elements exist on the page.
  todo_is(getEventCounts()[0], 2, "FIXME: TOTAL PAGE LOADS IS DOUBLE COUNTING");
  is(getEventCounts()[1], 0, "Disable actions");
  is(getEventCounts()[2], 0, "Enable actions");
  todo_is(getShieldCounts()[0], 1, "FIXME: TOTAL PAGE LOADS WITHOUT TRACKING IS DOUBLE COUNTING");

  info("Disable TP for the page (which reloads the page)");
  let tabReloadPromise = promiseTabLoadEvent(tab);
  newWin.document.querySelector("#tracking-action-unblock").doCommand();
  await tabReloadPromise;
  todo_is(getEventCounts()[0], 3, "FIXME: TOTAL PAGE LOADS IS DOUBLE COUNTING");
  is(getEventCounts()[1], 1, "Disable actions");
  is(getEventCounts()[2], 0, "Enable actions");
  todo_is(getShieldCounts()[0], 1, "FIXME: TOTAL PAGE LOADS WITHOUT TRACKING IS DOUBLE COUNTING");

  info("Re-enable TP for the page (which reloads the page)");
  tabReloadPromise = promiseTabLoadEvent(tab);
  newWin.document.querySelector("#tracking-action-block").doCommand();
  await tabReloadPromise;
  todo_is(getEventCounts()[0], 4, "FIXME: TOTAL PAGE LOADS IS DOUBLE COUNTING");
  is(getEventCounts()[1], 1, "Disable actions");
  is(getEventCounts()[2], 1, "Enable actions");
  todo_is(getShieldCounts()[0], 1, "FIXME: TOTAL PAGE LOADS WITHOUT TRACKING IS DOUBLE COUNTING");

  await promiseWindowClosed(newWin);

  // Reset these to make counting easier for the next test
  getEventsHistogram().clear();
  getShieldHistogram().clear();
  getEnabledHistogram().clear();
});

add_task(async function testPrivateBrowsing() {
  let privateWin = await promiseOpenAndLoadWindow({private: true}, true);
  let tab = await BrowserTestUtils.openNewForegroundTab(privateWin.gBrowser);
  let TrackingProtection = privateWin.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the browser window");

  // Do a bunch of actions and make sure that no telemetry data is gathered
  await promiseTabLoadEvent(tab, BENIGN_PAGE);
  await promiseTabLoadEvent(tab, TRACKING_PAGE);
  let tabReloadPromise = promiseTabLoadEvent(tab);
  privateWin.document.querySelector("#tracking-action-unblock").doCommand();
  await tabReloadPromise;
  tabReloadPromise = promiseTabLoadEvent(tab);
  privateWin.document.querySelector("#tracking-action-block").doCommand();
  await tabReloadPromise;

  // Sum up all the counts to make sure that nothing got logged
  is(getEnabledCounts().reduce((p, c) => p + c), 0, "Telemetry logging off in PB mode");
  is(getEventCounts().reduce((p, c) => p + c), 0, "Telemetry logging off in PB mode");
  is(getShieldCounts().reduce((p, c) => p + c), 0, "Telemetry logging off in PB mode");

  await promiseWindowClosed(privateWin);
});
