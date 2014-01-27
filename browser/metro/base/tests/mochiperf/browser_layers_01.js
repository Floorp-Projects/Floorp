/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  runTests();
}

gTests.push({
  desc: "rotating divs",
  run: function run() {
    yield addTab(chromeRoot + "res/divs_test.html", true);
    yield hideContextUI();
    yield hideNavBar();

    let stopwatch = new StopWatch();
    let win = Browser.selectedTab.browser.contentWindow;
    let domUtils = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    yield waitForEvent(win, "teststarted", 5000);
    // the test runs for five seconds
    let recordingHandle = domUtils.startFrameTimeRecording();
    stopwatch.start();
    let event = yield waitForEvent(win, "testfinished", 10000);
    let intervals = domUtils.stopFrameTimeRecording(recordingHandle);
    let msec = stopwatch.stop();

    PerfTest.declareTest("B924F3FA-4CB5-4453-B131-53E3611E0765",
                         "rotating divs w/text", "graphics", "content",
                         "Measures animation frames for rotating translucent divs on top of a background of text.");
    PerfTest.declareFrameRateResult(intervals.length, msec, "fps");
  }
});


