/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "data:text/html;charset=utf8,<p>JavaScript Profiler test</p>";

let gTab, gPanel;

function test() {
  waitForExplicitFinish();

  setUp(URL, function onSetUp(tab, browser, panel) {
    gTab = tab;
    gPanel = panel;

    testInactive(startFirstProfile);
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

function startFirstProfile() {
  gPanel.controller.start("Profile 1", function (err) {
    ok(!err, "Profile 1 started without errors");
    testActive(startSecondProfile);
  });
}

function startSecondProfile() {
  gPanel.controller.start("Profile 2", function (err) {
    ok(!err, "Profile 2 started without errors");
    testActive(stopFirstProfile);
  });
}

function stopFirstProfile() {
  gPanel.controller.stop("Profile 1", function (err, data) {
    ok(!err, "Profile 1 stopped without errors");
    ok(data, "Profiler returned some data");

    testActive(stopSecondProfile);
  });
}

function stopSecondProfile() {
  gPanel.controller.stop("Profile 2", function (err, data) {
    ok(!err, "Profile 2 stopped without errors");
    ok(data, "Profiler returned some data");
    testInactive(tearDown.call(null, gTab, function () gTab = gPanel = null));
  });
}