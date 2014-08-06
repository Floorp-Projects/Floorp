/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler goes through the appropriate steps while displaying
 * a finished recording.
 */

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { EVENTS } = panel.panelWin;

  yield startRecording(panel);

  let loadingNoticeDisplayed = panel.panelWin.once(EVENTS.LOADING_NOTICE_SHOWN);
  let recordingDisplayed = panel.panelWin.once(EVENTS.RECORDING_DISPLAYED);
  yield stopRecording(panel, { waitForDisplay: false });

  yield loadingNoticeDisplayed;
  ok(true, "The loading noticed was briefly displayed.");

  yield recordingDisplayed;
  ok(true, "The recording was finally displayed.");

  yield teardown(panel);
  finish();
});
