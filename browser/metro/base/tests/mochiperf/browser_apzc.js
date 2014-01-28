/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gStartView = null;

function test() {
  requestLongerTimeout(2);
  runTests();
}

function setup() {
  PanelUI.hide();

  if (!BrowserUI.isStartTabVisible) {
    let tab = yield addTab("about:start");
    gStartView = tab.browser.contentWindow.BookmarksStartView._view;

    yield waitForCondition(() => BrowserUI.isStartTabVisible);

    yield hideContextUI();
  }

  BookmarksTestHelper.setup();
  HistoryTestHelper.setup();
}

function tearDown() {
  PanelUI.hide();
  BookmarksTestHelper.restore();
  HistoryTestHelper.restore();
}

gTests.push({
  desc: "touch scroll",
  run: function run() {
    yield addTab(chromeRoot + "res/scroll_test.html");
    yield hideContextUI();
    yield hideNavBar();

    let stopwatch = new StopWatch();
    let win = Browser.selectedTab.browser.contentWindow;
    let domUtils = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);

    var touchdrag = new TouchDragAndHold();
    touchdrag.useNativeEvents = true;
    touchdrag.stepTimeout = 5;
    touchdrag.numSteps = 20;

    stopwatch.start();
    let recordingHandle = domUtils.startFrameTimeRecording();
    for (let count = 0; count < 3; count++) {
      yield touchdrag.start(win, 100, (win.innerHeight - 50), 100, 50);
      touchdrag.end();
      yield touchdrag.start(win, 100, 50, 100, (win.innerHeight - 50));
      touchdrag.end();
    }
    let intervals = domUtils.stopFrameTimeRecording(recordingHandle);
    let msec = stopwatch.stop();


    PerfTest.declareTest("14C693E5-3ED3-4A5D-93BC-A31F130A8CDE",
                          "touch scroll text", "graphics", "apzc",
                          "Measures performance of apzc scrolling for a large page of text using FTR.");
    PerfTest.declareFrameRateResult(intervals.length, msec, "fps");
  }
});

gTests.push({
  desc: "touch scroll start",
  setUp: setup,
  tearDown: tearDown,
  run: function run() {
    let stopwatch = new StopWatch();
    let win = Browser.selectedTab.browser.contentWindow;
    let domUtils = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);

    var touchdrag = new TouchDragAndHold();
    touchdrag.useNativeEvents = true;
    touchdrag.stepTimeout = 5;
    touchdrag.numSteps = 20;

    stopwatch.start();
    let recordingHandle = domUtils.startFrameTimeRecording();
    for (let count = 0; count < 3; count++) {
      yield touchdrag.start(win, (win.innerWidth - 50), (win.innerHeight - 50), 50, (win.innerHeight - 50));
      touchdrag.end();
      yield touchdrag.start(win, 50, (win.innerHeight - 50), (win.innerWidth - 50), (win.innerHeight - 50));
      touchdrag.end();
    }
    let intervals = domUtils.stopFrameTimeRecording(recordingHandle);
    let msec = stopwatch.stop();

    PerfTest.declareTest("24C693E5-3ED3-4A5D-93BC-A31F130A8CDE",
                          "touch scroll about:start", "graphics", "apzc",
                          "Measures performance of apzc scrolling for about:start using FTR.");
    PerfTest.declareFrameRateResult(intervals.length, msec, "fps");

    let results = PerfTest.computeHighLowBands(intervals, .1);
    PerfTest.declareTest("2E60F8B5-8925-4628-988E-E4C0BC6B34C7",
                          "about:start jank", "graphics", "apzc",
                          "Displays the low, high, and average FTR frame intervals for about:start.");
    info("results.low:" + results.low);
    PerfTest.declareNumericalResults([
      { value: results.low, desc: "low" },
      { value: results.high, desc: "high" },
      { value: results.ave, desc: "average" }
    ]);
  }
});
