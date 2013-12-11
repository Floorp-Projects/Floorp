/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  runTests();
}

gTests.push({
  desc: "deck offset",
  run: function run() {
    yield addTab("about:mozilla");
    yield hideContextUI();
    yield waitForMs(5000);

    let shiftDataSet = new Array();
    let paintDataSet = new Array();
    let stopwatch = new StopWatch();

    var paintCount = 0;
    function onPaint() {
      paintCount++;
    }

    for (let idx = 0; idx < 10; idx++) {
      window.addEventListener("MozAfterPaint", onPaint, true);
      stopwatch.start();
      let promise = waitForEvent(window, "MozDeckOffsetChanged");
      ContentAreaObserver.shiftBrowserDeck(300);
      yield promise;
      promise = waitForEvent(window, "MozDeckOffsetChanged");
      ContentAreaObserver.shiftBrowserDeck(0);
      yield promise;
      stopwatch.stop();
      yield waitForMs(500);
      window.removeEventListener("MozAfterPaint", onPaint, true);
      shiftDataSet.push(stopwatch.time());
      paintDataSet.push(paintCount);
      paintCount = 0;
    }
    
    PerfTest.declareTest("ecb5fbec-0b3d-490f-8d4a-13fa8963e54a",
                         "shift browser deck", "browser", "ux",
                         "Triggers multiple SKB deck shifting operations using an offset " +
                         "value of 300px. Measures total time in milliseconds for a up/down " +
                         "shift operation plus the total number of paints. Strips outliers.");
    let shiftAverage = PerfTest.computeAverage(shiftDataSet, { stripOutliers: true });
    let paintAverage = PerfTest.computeAverage(paintDataSet, { stripOutliers: true });
    PerfTest.declareNumericalResults([
      { value: shiftAverage, desc: "msec" },
      { value: paintAverage, desc: "paint count" },
    ]);
  }
});

