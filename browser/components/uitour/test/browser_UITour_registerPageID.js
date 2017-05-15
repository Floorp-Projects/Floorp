"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;

Components.utils.import("resource://gre/modules/UITelemetry.jsm");
Components.utils.import("resource:///modules/BrowserUITelemetry.jsm");

add_task(async function setup_telemetry() {
  UITelemetry._enabled = true;

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("browser.uitour.seenPageIDs");
    resetSeenPageIDsLazyGetter();
    UITelemetry._enabled = undefined;
    BrowserUITelemetry.setBucket(null);
    delete window.UITelemetry;
    delete window.BrowserUITelemetry;
  });
});

add_task(setup_UITourTest);

function resetSeenPageIDsLazyGetter() {
  delete UITour.seenPageIDs;
  // This should be kept in sync with how UITour.init() sets this.
  Object.defineProperty(UITour, "seenPageIDs", {
    get: UITour.restoreSeenPageIDs.bind(UITour),
    configurable: true,
  });
}

function checkExpectedSeenPageIDs(expected) {
  is(UITour.seenPageIDs.size, expected.length, "Should be " + expected.length + " total seen page IDs");

  for (let id of expected)
    ok(UITour.seenPageIDs.has(id), "Should have seen '" + id + "' page ID");

  let prefData = Services.prefs.getCharPref("browser.uitour.seenPageIDs");
  prefData = new Map(JSON.parse(prefData));

  is(prefData.size, expected.length, "Should be " + expected.length + " total seen page IDs persisted");

  for (let id of expected)
    ok(prefData.has(id), "Should have seen '" + id + "' page ID persisted");
}


add_UITour_task(function test_seenPageIDs_restore() {
  info("Setting up seenPageIDs to be restored from pref");
  let data = JSON.stringify([
    ["savedID1", { lastSeen: Date.now() }],
    ["savedID2", { lastSeen: Date.now() }],
    // 9 weeks ago, should auto expire.
    ["savedID3", { lastSeen: Date.now() - 9 * 7 * 24 * 60 * 60 * 1000 }],
  ]);
  Services.prefs.setCharPref("browser.uitour.seenPageIDs",
                             data);

  resetSeenPageIDsLazyGetter();
  checkExpectedSeenPageIDs(["savedID1", "savedID2"]);
});

add_UITour_task(async function test_seenPageIDs_set_1() {
  await gContentAPI.registerPageID("testpage1");

  await waitForConditionPromise(() => UITour.seenPageIDs.size == 3, "Waiting for page to be registered.");

  checkExpectedSeenPageIDs(["savedID1", "savedID2", "testpage1"]);

  const PREFIX = BrowserUITelemetry.BUCKET_PREFIX;
  const SEP = BrowserUITelemetry.BUCKET_SEPARATOR;

  let bucket = PREFIX + "UITour" + SEP + "testpage1";
  is(BrowserUITelemetry.currentBucket, bucket, "Bucket should have correct name");

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  bucket = PREFIX + "UITour" + SEP + "testpage1" + SEP + "inactive" + SEP + "1m";
  is(BrowserUITelemetry.currentBucket, bucket,
     "After switching tabs, bucket should be expiring");

  gBrowser.removeTab(gBrowser.selectedTab);
  gBrowser.selectedTab = gTestTab;
  BrowserUITelemetry.setBucket(null);
});

add_UITour_task(async function test_seenPageIDs_set_2() {
  await gContentAPI.registerPageID("testpage2");

  await waitForConditionPromise(() => UITour.seenPageIDs.size == 4, "Waiting for page to be registered.");

  checkExpectedSeenPageIDs(["savedID1", "savedID2", "testpage1", "testpage2"]);

  const PREFIX = BrowserUITelemetry.BUCKET_PREFIX;
  const SEP = BrowserUITelemetry.BUCKET_SEPARATOR;

  let bucket = PREFIX + "UITour" + SEP + "testpage2";
  is(BrowserUITelemetry.currentBucket, bucket, "Bucket should have correct name");

  gBrowser.removeTab(gTestTab);
  gTestTab = null;
  bucket = PREFIX + "UITour" + SEP + "testpage2" + SEP + "closed" + SEP + "1m";
  is(BrowserUITelemetry.currentBucket, bucket,
     "After closing tab, bucket should be expiring");

  BrowserUITelemetry.setBucket(null);
});
