/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
  Services.scriptloader.loadSubScript(testDir + "/perfhelpers.js", this);
  runTests();
}

function timeTab(aUrl) {
  return Task.spawn(function() {
    let stopwatch = new StopWatch(true);
    let tab = Browser.addTab(aUrl, true);
    yield tab.pageShowPromise;
    stopwatch.stop();
    Browser.closeTab(tab)
    yield waitForMs(500);
    throw new Task.Result(stopwatch.time());
  });
}

gTests.push({
  desc: "tab open",
  run: function run() {
    yield addTab("about:blank");
    yield hideContextUI();
    yield waitForMs(5000);

    let openDataSet = new Array();
    for (let idx = 0; idx < 20; idx++) {
      let time = yield timeTab("about:blank");
      openDataSet.push(time);
    }
    
    PerfTest.declareTest("FBD7A532-D63A-44B5-9744-5CB07CFD131A",
                         "tab open", "browser", "ux",
                         "Open twenty tabs in succession, closing each before the next is opened. " +
                         "Gives the browser time to settle in between. Lets the ui react however it " +
                         "is designed to. Strips outliers.");
    let result = PerfTest.computeAverage(openDataSet, { stripOutliers: true });
    PerfTest.declareNumericalResult(result, "msec");
  }
});

