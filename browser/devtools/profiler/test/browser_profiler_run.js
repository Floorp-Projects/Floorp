/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "data:text/html;charset=utf8,<p>JavaScript Profiler test</p>";

let gTab, gPanel;

function test() {
  waitForExplicitFinish();

  setUp(URL, function onSetUp(tab, browser, panel) {
    gTab = tab;
    gPanel = panel;

    function done() {
      tearDown(gTab, () => { gPanel = null; gTab = null; });
    }

    startRecording()
      .then(stopRecording)
      .then(startRecordingAgain)
      .then(stopRecording)
      .then(switchBackToTheFirstOne)
      .then(done);
  });
}

function startRecording() {
  let deferred = Promise.defer();

  ok(gPanel, "Profiler panel exists");
  ok(!gPanel.activeProfile, "Active profile doesn't exist");
  ok(!gPanel.recordingProfile, "Recording profile doesn't exist");

  let record = gPanel.controls.record;
  ok(record, "Record button exists.");
  ok(!record.getAttribute("checked"), "Record button is unchecked");

  gPanel.once("started", () => {
    let item = gPanel.sidebar.getItemByProfile(gPanel.recordingProfile);
    is(item.attachment.name, "Profile 1");
    is(item.attachment.state, PROFILE_RUNNING);

    gPanel.controller.isActive(function (err, isActive) {
      ok(isActive, "Profiler is running");
      deferred.resolve();
    });
  });

  record.click();
  return deferred.promise;
}

function stopRecording() {
  let deferred = Promise.defer();

  gPanel.once("parsed", () => {
    let item = gPanel.sidebar.getItemByProfile(gPanel.activeProfile);
    is(item.attachment.state, PROFILE_COMPLETED);

    function assertSample() {
      let [ win, doc ] = getProfileInternals();
      let sample = doc.getElementsByClassName("samplePercentage");

      if (sample.length <= 0) {
        return void setTimeout(assertSample, 100);
      }

      ok(sample.length > 0, "We have some items displayed");
      is(sample[0].innerHTML, "100.0%", "First percentage is 100%");

      deferred.resolve();
    }

    assertSample();
  });

  setTimeout(function () gPanel.controls.record.click(), 100);
  return deferred.promise;
}

function startRecordingAgain() {
  let deferred = Promise.defer();

  let record = gPanel.controls.record;
  ok(!record.getAttribute("checked"), "Record button is unchecked");

  gPanel.once("started", () => {
    ok(gPanel.activeProfile !== gPanel.recordingProfile);

    let item = gPanel.sidebar.getItemByProfile(gPanel.recordingProfile);
    is(item.attachment.name, "Profile 2");
    is(item.attachment.state, PROFILE_RUNNING);

    deferred.resolve();
  });

  record.click();
  return deferred.promise;
}

function switchBackToTheFirstOne() {
  let deferred = Promise.defer();
  let button = gPanel.sidebar.getElementByProfile({ uid: 1 });
  let item = gPanel.sidebar.getItemByProfile({ uid: 1 });

  gPanel.once("profileSwitched", () => {
    is(gPanel.activeProfile.uid, 1, "activeProfile is correct");
    is(gPanel.sidebar.selectedItem, item, "selectedItem is correct");
    deferred.resolve();
  });

  button.click();
  return deferred.promise;
}