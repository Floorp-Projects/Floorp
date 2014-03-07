/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  requestLongerTimeout(2);
  runTests();
}

function setup() {
  PanelUI.hide();

  if (!BrowserUI.isStartTabVisible) {
    let tab = yield addTab("about:start");

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
  Browser.selectedTab
         .browser
         .contentWindow
         .QueryInterface(Ci.nsIInterfaceRequestor)
         .getInterface(Ci.nsIDOMWindowUtils).clearNativeTouchSequence();
}

/*
 * short up/down touch scroll. This test isn't affected by
 * skate or stationary apzc prefs provided the display port
 * is twice the height of the screen. Measures apzc/composition
 * perf since the display port (should) only render once.
 */
gTests.push({
  desc: "short up/down touch scroll",
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
    for (let count = 0; count < 5; count++) {
      yield touchdrag.start(win, 100, (win.innerHeight - 50), 100, 50);
      touchdrag.end();
      yield touchdrag.start(win, 100, 50, 100, (win.innerHeight - 50));
      touchdrag.end();
    }
    let intervals = domUtils.stopFrameTimeRecording(recordingHandle);
    let msec = stopwatch.stop();


    PerfTest.declareTest("14C693E5-3ED3-4A5D-93BC-A31F130A8CDE",
                          "touch scroll window", "graphics", "apzc",
                          "Measures performance of up/down apzc scrolling within a window using FTR.");
    PerfTest.declareFrameRateResult(intervals.length, msec, "fps");
  }
});

/*
 * Long scroll a page of text, which will include repainting
 * content.
 */
gTests.push({
  desc: "touch scroll",
  run: function run() {
    yield addTab(chromeRoot + "res/scroll_test_tall.html");
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
    for (let count = 0; count < 5; count++) {
      yield touchdrag.start(win, 100, (win.innerHeight - 50), 100, 50);
      touchdrag.end();
    }
    let intervals = domUtils.stopFrameTimeRecording(recordingHandle);
    let msec = stopwatch.stop();


    PerfTest.declareTest("4546F318-DC1B-4225-9196-D5196C87982A",
                          "touch scroll text", "graphics", "apzc",
                          "Measures performance of apzc scrolling for a large page of text using FTR.");
    PerfTest.declareFrameRateResult(intervals.length, msec, "fps");
  }
});

/*
 * Fling a page of text downward over a set of iterations
 * taking measurements for each fling. Content will repaint
 * and includes measurements that cover fling logic/prefs.
 */
gTests.push({
  desc: "fling scroll",
  run: function run() {
    yield addTab(chromeRoot + "res/scroll_test_tall.html");
    yield hideContextUI();
    yield hideNavBar();

    let stopwatch = new StopWatch();
    let win = Browser.selectedTab.browser.contentWindow;
    let domUtils = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    var touchdrag = new TouchDragAndHold();
    touchdrag.useNativeEvents = true;
    touchdrag.stepTimeout = 5;
    touchdrag.numSteps = 10;

    let iterations = 3;
    let sets = [];
    for (let count = 0; count < iterations; count++) {
      let obsPromise = waitForObserver("apzc-transform-end", 30000);
      stopwatch.start();
      let recordingHandle = domUtils.startFrameTimeRecording();
      yield touchdrag.start(win, 100, (win.innerHeight - 50), 100, 50);
      touchdrag.end();
      yield obsPromise;
      let intervals = domUtils.stopFrameTimeRecording(recordingHandle);
      let msec = stopwatch.stop();
      sets[count] = {
        bands: PerfTest.computeHighLowBands(intervals, .1),
        frameRate: intervals.length / (msec / 1000.0)
      };
      yield waitForMs(300);
    }

    let frameRate = 0, low = 0, ave = 0, high = 0;
    for (let count = 0; count < iterations; count++) {
      frameRate += sets[count].frameRate;
      low += sets[count].bands.low;
      ave += sets[count].bands.ave;
      high += sets[count].bands.high;

    }
    frameRate /= iterations;
    low /= iterations;
    ave /= iterations;
    high /= iterations;

    PerfTest.declareTest("A8EDF0D6-562B-4C4A-AC6B-1E4900FE0EE9",
                          "fling text", "graphics", "apzc",
                          "Measures frame rate of apzc fling for a large page of text using FTR.");
    PerfTest.declareNumericalResult(frameRate, "fps");

    PerfTest.declareTest("F5E83238-383F-4665-8415-878AA027B4A3",
                          "fling jank", "graphics", "apzc",
                          "Displays the low, high, and average FTR frame intervals when flinging a page of text.");
    PerfTest.declareNumericalResults([
      { value: low, desc: "low" },
      { value: high, desc: "high", shareAxis: 0 },
      { value: ave, desc: "average", shareAxis: 0 }
    ]);
  }
});

/*
 * touch scroll the about:start page back and forth averaging
 * values across the entire set. Generally about:start should
 * only need to be painted once. Exercises composition and apzc.
 */
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
    for (let count = 0; count < 5; count++) {
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
    PerfTest.declareNumericalResults([
      { value: results.low, desc: "low" },
      { value: results.high, desc: "high", shareAxis: 0 },
      { value: results.ave, desc: "average", shareAxis: 0 }
    ]);
  }
});
