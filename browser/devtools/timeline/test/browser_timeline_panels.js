/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the timeline panels are correctly shown and hidden when
 * recording starts and stops.
 */

let test = Task.async(function*() {
  let { target, panel } = yield initTimelinePanel(SIMPLE_URL);
  let { $, EVENTS } = panel.panelWin;

  is($("#record-button").hasAttribute("checked"), false,
    "The record button should not be checked yet.");
  is($("#timeline-pane").selectedPanel, $("#empty-notice"),
    "An empty notice is initially displayed instead of the waterfall view.");

  let whenRecStarted = panel.panelWin.once(EVENTS.RECORDING_STARTED);
  EventUtils.synthesizeMouseAtCenter($("#record-button"), {}, panel.panelWin);
  yield whenRecStarted;

  ok(true, "Recording has started.");

  is($("#record-button").getAttribute("checked"), "true",
    "The record button should be checked now.");
  is($("#timeline-pane").selectedPanel, $("#recording-notice"),
    "A recording notice is now displayed instead of the waterfall view.");

  let whenRecEnded = panel.panelWin.once(EVENTS.RECORDING_ENDED);
  EventUtils.synthesizeMouseAtCenter($("#record-button"), {}, panel.panelWin);
  yield whenRecEnded;

  ok(true, "Recording has ended.");

  is($("#record-button").hasAttribute("checked"), false,
    "The record button should be unchecked again.");
  is($("#timeline-pane").selectedPanel, $("#timeline-waterfall"),
    "A waterfall view is now displayed.");

  yield teardown(panel);
  finish();
});
