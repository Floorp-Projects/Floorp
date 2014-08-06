/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler can correctly handle simultaneous console and manual
 * recordings (via `console.profile` and clicking the record button).
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { $, EVENTS, gFront, RecordingsListView, ProfileView } = panel.panelWin;

  info("Starting a manual recording...");
  yield startRecording(panel);

  info("Starting two console recordings, one without a label, one with.");
  yield consoleProfile(gFront);
  yield consoleProfile(gFront, true, "hello world");

  is(RecordingsListView.itemCount, 3,
    "There should be three recordings visible now.");
  is(RecordingsListView.selectedIndex, 0,
    "The first recording item was selected in the list.");

  testListItem(RecordingsListView, 0, undefined, true);
  testListItem(RecordingsListView, 1, undefined, true);
  testListItem(RecordingsListView, 2, "hello world", true);

  info("Stopping the manual recording...");
  yield stopRecording(panel, { waitForDisplay: true });

  is(RecordingsListView.itemCount, 3,
    "There should still be three recordings visible now.");
  is(RecordingsListView.selectedIndex, 0,
    "The first recording item is still selected in the list.");

  testListItem(RecordingsListView, 0, undefined, false);
  testListItem(RecordingsListView, 1, undefined, true);
  testListItem(RecordingsListView, 2, "hello world", true);

  info("Stopping the unlabeled console recording...");
  yield consoleProfileEnd(gFront);

  is(RecordingsListView.itemCount, 3,
    "There should still be three recordings visible now.");
  is(RecordingsListView.selectedIndex, 1,
    "The second recording item was selected in the list.");

  testListItem(RecordingsListView, 0, undefined, false);
  testListItem(RecordingsListView, 1, undefined, false);
  testListItem(RecordingsListView, 2, "hello world", true);

  info("Stopping the labeled console recording...");
  yield consoleProfileEnd(gFront);

  is(RecordingsListView.itemCount, 3,
    "There should still be three recordings visible now.");
  is(RecordingsListView.selectedIndex, 2,
    "The third recording item was selected in the list.");

  testListItem(RecordingsListView, 0, undefined, false);
  testListItem(RecordingsListView, 1, undefined, false);
  testListItem(RecordingsListView, 2, "hello world", false);

  yield teardown(panel);
  finish();
});

function testListItem(view, index, profileLabel, isRecording) {
  is(view.items[index].attachment.profilerData.profileLabel, profileLabel,
    "The recording item at index " + index + " has a correct profile label.");
  is(!!view.items[index].isRecording, isRecording,
    "The recording item at index " + index + " has a correct `isRecording` flag.");
}

function* consoleProfile(front, withLabel, labelValue) {
  let notified = front.once("profile");
  if (!withLabel) {
    console.profile();
  } else {
    console.profile(labelValue);
  }
  yield notified;
}

function* consoleProfileEnd(front) {
  let notified = front.once("profileEnd");
  console.profileEnd();
  yield notified;
}
