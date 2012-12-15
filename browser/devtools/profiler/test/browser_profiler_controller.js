/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "data:text/html;charset=utf8,<p>JavaScript Profiler test</p>";

let gTab, gPanel;

function test() {
  waitForExplicitFinish();

  setUp(URL, function onSetUp(tab, browser, panel) {
    gTab = tab;
    gPanel = panel;

    testInactive(testStart);
  });
}

function testInactive(next=function(){}) {
  gPanel.controller.isActive(function (err, isActive) {
    ok(!err, "isActive didn't return any errors");
    ok(!isActive, "Profiler is not active");
    next();
  });
}

function testActive(next=function(){}) {
  gPanel.controller.isActive(function (err, isActive) {
    ok(!err, "isActive didn't return any errors");
    ok(isActive, "Profiler is active");
    next();
  });
}

function testStart() {
  gPanel.controller.start(function (err) {
    ok(!err, "Profiler started without errors");
    testActive(testStop);
  });
}

function testStop() {
  gPanel.controller.stop(function (err, data) {
    ok(!err, "Profiler stopped without errors");
    ok(data, "Profiler returned some data");

    testInactive(function () {
      tearDown(gTab, function () {
        gTab = null;
        gPanel = null;
      });
    });
  });
}
