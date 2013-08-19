/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
  Services.scriptloader.loadSubScript(testDir + "/perfhelpers.js", this);
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

    let stopwatch = new StopWatch();

    let win = Browser.selectedTab.browser.contentWindow;

    PerfTest.declareTest("79235F74-037C-4F6B-AE47-FDCCC13458C3",
                         "scrollBy scroll", "graphics", "content",
                         "Measures performance of single line scrolls using scrollBy for a large page of text.");

    let mozpaints = 0;
    function onPaint() {
      mozpaints++;
    }
    win.addEventListener("MozAfterPaint", onPaint, true);

    let deferExit = Promise.defer();

    function step() {
      if (stopwatch.time() < 10000) {
        win.scrollBy(0, 2);
        // XXX slaves won't paint without this
        win.mozRequestAnimationFrame(step);
        return;
      }
      win.removeEventListener("MozAfterPaint", onPaint, true);
      let msec = stopwatch.stop();
      PerfTest.declareFrameRateResult(mozpaints, msec, "fps");
      deferExit.resolve();
    }

    stopwatch.start();
    win.mozRequestAnimationFrame(step);
    yield deferExit.promise;
  }
});

gTests.push({
  desc: "scoll touch",
  run: function run() {
    yield addTab(chromeRoot + "res/scroll_test.html");
    yield hideContextUI();

    let stopwatch = new StopWatch();

    let win = Browser.selectedTab.browser.contentWindow;

    PerfTest.declareTest("14C693E5-3ED3-4A5D-93BC-A31F130A8CDE",
                         "touch scroll", "graphics", "content",
                         "Measures performance of single line scrolls using touch input for a large page of text.");

    let y = win.innerHeight - 10;
    EventUtils.synthesizeTouchAtPoint(100, y, { type: "touchstart" }, win);
    EventUtils.synthesizeTouchAtPoint(100, y, { type: "touchmove" }, win);
    y -= tapRadius() + 5;
    EventUtils.synthesizeTouchAtPoint(100, y, { type: "touchmove" }, win);

    let mozpaints = 0;
    function onPaint() {
      mozpaints++;
    }
    win.addEventListener("MozAfterPaint", onPaint, true);

    let deferExit = Promise.defer();

    function step() {
      if (stopwatch.time() < 10000) {
        y -= 2;
        EventUtils.synthesizeTouchAtPoint(100, y, { type: "touchmove" }, win);
        win.mozRequestAnimationFrame(step);
        return;
      }
      win.removeEventListener("MozAfterPaint", onPaint, true);
      let msec = stopwatch.stop();
      EventUtils.synthesizeTouchAtPoint(100, y, { type: "touchend" }, win);
      PerfTest.declareFrameRateResult(mozpaints, msec, "fps");
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
    let win = Browser.selectedTab.browser.contentWindow;
    yield hideContextUI();
    PerfTest.declareTest("6A455F96-2B2C-4B3C-B387-1AF2F1747CCF",
                         "ripples", "graphics", "canvas",
                         "Measures animation frames during a computationally " +
                         "heavy graphics demo using canvas.");
    let stopwatch = new StopWatch(true);
    let event = yield waitForEvent(win, "test", 20000);
    let msec = stopwatch.stop();
    PerfTest.declareFrameRateResult(event.detail.frames, msec, "fps");

  }
});

gTests.push({
  desc: "video perf test",
  run: function run() {
    yield addTab(chromeRoot + "res/tidevideo.html");
    let win = Browser.selectedTab.browser.contentWindow;
    let video = win.document.getElementById("videoelement");
    video.pause();
    yield hideContextUI();
    yield waitForMs(1000);
    PerfTest.declareTest("7F55F9C4-0ECF-4A13-9A9C-A38D46922C0B",
                         "video playback (moz paints)", "graphics", "video",
                         "Measures MozAfterPaints per second during five seconds of playback of an mp4.");

    var paintCount = 0;
    function onPaint() {
      paintCount++;
    }
    let stopwatch = new StopWatch(true);
    window.addEventListener("MozAfterPaint", onPaint, true);
    video.play();
    yield waitForMs(5000);
    video.pause();
    window.removeEventListener("MozAfterPaint", onPaint, true);
    let msec = stopwatch.stop();

    PerfTest.declareNumericalResult((paintCount / msec) * 1000.0, "pps");

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

