/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
  Services.scriptloader.loadSubScript(testDir + "/perfhelpers.js", this);
  runTests();
}

var EchoServer = {
  _deferred: null,
  _stopwatch: null,
  _browser: null,

  timeAsyncMessage: function timeAsyncMessage(aJson) {
    if (this._stopwatch == null) {
      this._stopwatch = new StopWatch(false);
    }
    this._deferred = Promise.defer();
    messageManager.addMessageListener("Test:EchoResponse", this);
    this._stopwatch.start();
    this._browser.messageManager.sendAsyncMessage("Test:EchoRequest", aJson);
    return this._deferred.promise;
  },

  receiveMessage: function receiveMessage(aMessage) {
    let json = aMessage.json;
    switch (aMessage.name) {
      case "Test:EchoResponse":
        let msec = this._stopwatch.stop();
        messageManager.removeMessageListener("Test:EchoResponse", this);
        this._deferred.resolve(msec);
        break;
    }
  },
};


gTests.push({
  desc: "msg manager 1",
  run: function run() {
    yield addTab("about:blank");
    yield hideContextUI();

    let browser = Browser.selectedTab.browser;
    EchoServer._browser = browser;

    browser.messageManager.loadFrameScript(chromeRoot + "msgmanagerecho.js", true);

    yield waitForMs(1000);

    let openDataSet = new Array();

    for (let idx = 0; idx < 100; idx++) {
      let msec = yield EchoServer.timeAsyncMessage({});
      openDataSet.push(msec);
    }

    PerfTest.declareTest("A4354ACB-34DE-4796-914F-FD01EF298B1D",
                         "msg manager 01", "platform", "",
                         "Measures the time it takes to send/recv 100 message manager messages with an empty payload. " +
                         "Browser displays about:blank during the test.");
    let result = PerfTest.computeAverage(openDataSet, { stripOutliers: false });
    PerfTest.declareNumericalResult(result, "msec");
  }
});

gTests.push({
  desc: "msg manager 1",
  run: function run() {
    yield addTab("about:blank");
    yield hideContextUI();

    let browser = Browser.selectedTab.browser;
    EchoServer._browser = browser;

    browser.messageManager.loadFrameScript(chromeRoot + "msgmanagerecho.js", true);

    let ds = {};

    for (let i = 0; i < 100; i++) {
      ds["i" + i] = { idx: i };
      let table = ds["i" + i];
      for (let j = 0; j < 100; j++) {
        table["j" + i] = { rnd: Math.random() };
      }
    }

    yield waitForMs(1000);

    let openDataSet = new Array();

    for (let idx = 0; idx < 100; idx++) {
      let msec = yield EchoServer.timeAsyncMessage(ds);
      openDataSet.push(msec);
    }

    PerfTest.declareTest("B4354ACB-34DE-4796-914F-FD01EF298B1D",
                         "msg manager 02", "platform", "",
                         "Measures the time it takes to send/recv 100 message manager messages with a heavy payload. " +
                         "Browser displays about:blank during the test.");
    let result = PerfTest.computeAverage(openDataSet, { stripOutliers: false });
    PerfTest.declareNumericalResult(result, "msec");
  }
});

gTests.push({
  desc: "msg manager 3",
  run: function run() {
    yield addTab(chromeRoot + "res/ripples.html");
    yield hideContextUI();

    let browser = Browser.selectedTab.browser;
    EchoServer._browser = browser;

    browser.messageManager.loadFrameScript(chromeRoot + "msgmanagerecho.js", true);

    let ds = {};

    for (let i = 0; i < 100; i++) {
      ds["i" + i] = { idx: i };
      let table = ds["i" + i];
      for (let j = 0; j < 100; j++) {
        table["j" + i] = { rnd: Math.random() };
      }
    }

    yield waitForMs(1000);

    let openDataSet = new Array();

    for (let idx = 0; idx < 100; idx++) {
      let msec = yield EchoServer.timeAsyncMessage(ds);
      openDataSet.push(msec);
    }

    PerfTest.declareTest("C4354ACB-34DE-4796-914F-FD01EF298B1D",
                         "msg manager 03", "platform", "",
                         "Measures the time it takes to send/recv 100 message manager messages with a heavy payload. " +
                         "Browser displays ripples during the test.");
    let result = PerfTest.computeAverage(openDataSet, { stripOutliers: false });
    PerfTest.declareNumericalResult(result, "msec");
  }
});
