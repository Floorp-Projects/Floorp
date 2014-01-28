/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  requestLongerTimeout(2);
  runTests();
}

function tapRadius() {
  let dpi = Util.displayDPI;
  return Services.prefs.getIntPref("ui.dragThresholdX") / 240 * dpi; // 27.999998728434246
}

gTests.push({
  desc: "scrollBy",
  run: function run() {
    yield addTab(chromeRoot + "res/scroll_test.html");
    yield hideContextUI();
    yield hideNavBar();

    let stopwatch = new StopWatch();
    let win = Browser.selectedTab.browser.contentWindow;
    let domUtils = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    let deferExit = Promise.defer();
    let recordingHandle = domUtils.startFrameTimeRecording();

    function step() {
      if (stopwatch.time() < 5000) {
        win.scrollBy(0, 2);
        win.mozRequestAnimationFrame(step);
        return;
      }

      let intervals = domUtils.stopFrameTimeRecording(recordingHandle);
      let msec = stopwatch.stop();
      PerfTest.declareTest("79235F74-037C-4F6B-AE47-FDCCC13458C3",
                           "scrollBy scroll", "graphics", "content",
                           "Measures performance of single line scrolls for a large page of text using FTR.");
      PerfTest.declareFrameRateResult(intervals.length, msec, "fps");
      deferExit.resolve();
    }

    stopwatch.start();
    win.mozRequestAnimationFrame(step);
    yield deferExit.promise;
  }
});

gTests.push({
  desc: "canvas perf test",
  run: function run() {
    yield addTab(chromeRoot + "res/ripples.html");
    yield hideContextUI();
    yield hideNavBar();

    let win = Browser.selectedTab.browser.contentWindow;
    let domUtils = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    let recordingHandle = domUtils.startFrameTimeRecording();
    PerfTest.declareTest("6A455F96-2B2C-4B3C-B387-1AF2F1747CCF",
                         "ripples", "graphics", "canvas",
                         "Measures animation frames during a computationally " +
                         "heavy graphics demo using canvas using FTR.");
    let stopwatch = new StopWatch(true);
    // test page runs for 5 seconds
    let event = yield waitForEvent(win, "test", 10000);
    let intervals = domUtils.stopFrameTimeRecording(recordingHandle);
    let msec = stopwatch.stop();
    PerfTest.declareFrameRateResult(intervals.length, msec, "fps");
  }
});

gTests.push({
  desc: "video perf test",
  run: function run() {
    yield addTab(chromeRoot + "res/tidevideo.html");
    let win = Browser.selectedTab.browser.contentWindow;
    let domUtils = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    let video = win.document.getElementById("videoelement");
    video.pause();
    yield hideContextUI();
    yield hideNavBar();
    yield waitForMs(1000);
    PerfTest.declareTest("7F55F9C4-0ECF-4A13-9A9C-A38D46922C0B",
                         "video playback (moz paints)", "graphics", "video",
                         "Measures frames per second during five seconds of playback of an mp4.");

    let recordingHandle = domUtils.startFrameTimeRecording();
    let stopwatch = new StopWatch(true);
    video.play();
    yield waitForMs(5000);
    video.pause();
    let intervals = domUtils.stopFrameTimeRecording(recordingHandle);
    let msec = stopwatch.stop();
    PerfTest.declareFrameRateResult(intervals.length, msec, "fps");

    PerfTest.declareTest("E132D333-4642-4597-B1F0-1E74B625DBD7",
                         "video playback (moz stats)", "graphics", "video",
                         "Report moz* related video performance stats during five seconds of playback of an mp4.");
    let results = [];
    results.push({ value: (video.mozDecodedFrames / msec) * 1000.0, desc: "mozDecodedFrames" });
    results.push({ value: (video.mozParsedFrames / msec) * 1000.0, desc: "mozParsedFrames", shareAxis: 0 });
    results.push({ value: (video.mozPresentedFrames / msec) * 1000.0, desc: "mozPresentedFrames", shareAxis: 0 });
    results.push({ value: (video.mozPaintedFrames / msec) * 1000.0, desc: "mozPaintedFrames", shareAxis: 0 });
    PerfTest.declareNumericalResults(results);
  }
});
