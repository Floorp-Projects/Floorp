/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "data:text/html;charset=utf8,<p>JavaScript Profiler test</p>";

let gTab, gPanel, gAttempts = 0;

function test() {
  waitForExplicitFinish();

  setUp(URL, function onSetUp(tab, browser, panel) {
    gTab = tab;
    gPanel = panel;

    panel.once("started", onStart);
    panel.once("stopped", onStop);
    panel.once("parsed", onParsed);

    testUI();
  });
}

function attemptTearDown() {
  gAttempts += 1;

  if (gAttempts < 2) {
    return;
  }

  tearDown(gTab, function onTearDown() {
    gPanel = null;
    gTab = null;
  });
}

function getProfileInternals() {
  let win = gPanel.activeProfile.iframe.contentWindow;
  let doc = win.document;

  return [win, doc];
}

function testUI() {
  ok(gPanel, "Profiler panel exists");
  ok(gPanel.activeProfile, "Active profile exists");

  let [win, doc] = getProfileInternals();
  let startButton = doc.querySelector(".controlPane #startWrapper button");
  let stopButton = doc.querySelector(".controlPane #stopWrapper button");

  ok(startButton, "Start button exists");
  ok(stopButton, "Stop button exists");

  startButton.click();
}

function onStart() {
  gPanel.controller.isActive(function (err, isActive) {
    ok(isActive, "Profiler is running");

    let [win, doc] = getProfileInternals();
    let stopButton = doc.querySelector(".controlPane #stopWrapper button");

    stopButton.click();
  });
}

function onStop() {
  gPanel.controller.isActive(function (err, isActive) {
    ok(!isActive, "Profiler is idle");
    attemptTearDown();
  });
}

function onParsed() {
  function assertSample() {
    let [win,doc] = getProfileInternals();
    let sample = doc.getElementsByClassName("samplePercentage");

    if (sample.length <= 0) {
      return void setTimeout(assertSample, 100);
    }

    ok(sample.length > 0, "We have some items displayed");
    is(sample[0].innerHTML, "100.0%", "First percentage is 100%");
    attemptTearDown();
  }

  assertSample();
}
