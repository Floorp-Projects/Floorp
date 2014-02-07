/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;

Components.utils.import("resource:///modules/UITour.jsm");
Components.utils.import("resource:///modules/BrowserUITelemetry.jsm");

function test() {
  registerCleanupFunction(function() {
    UITour.seenPageIDs.clear();
    BrowserUITelemetry.setBucket(null);
    delete window.BrowserUITelemetry;
  });
  UITourTest();
}

let tests = [
  function test_seenPageIDs_1(done) {
    gContentAPI.registerPageID("testpage1");

    is(UITour.seenPageIDs.size, 1, "Should be 1 seen page ID");
    ok(UITour.seenPageIDs.has("testpage1"), "Should have seen 'testpage1' page ID");

    const PREFIX = BrowserUITelemetry.BUCKET_PREFIX;
    const SEP = BrowserUITelemetry.BUCKET_SEPARATOR;

    let bucket = PREFIX + "UITour" + SEP + "testpage1";
    is(BrowserUITelemetry.currentBucket, bucket, "Bucket should have correct name");

    gBrowser.selectedTab = gBrowser.addTab("about:blank");
    bucket = PREFIX + "UITour" + SEP + "testpage1" + SEP + "inactive" + SEP + "1m";
    is(BrowserUITelemetry.currentBucket, bucket,
       "After switching tabs, bucket should be expiring");

    gBrowser.removeTab(gBrowser.selectedTab);
    gBrowser.selectedTab = gTestTab;
    BrowserUITelemetry.setBucket(null);
    done();
  },
  function test_seenPageIDs_2(done) {
    gContentAPI.registerPageID("testpage2");

    is(UITour.seenPageIDs.size, 2, "Should be 2 seen page IDs");
    ok(UITour.seenPageIDs.has("testpage1"), "Should have seen 'testpage1' page ID");
    ok(UITour.seenPageIDs.has("testpage2"), "Should have seen 'testpage2' page ID");

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
    done();
  },
];
