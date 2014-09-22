/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the timeline can properly start and stop a recording.
 */

let test = Task.async(function*() {
  let { target, panel } = yield initTimelinePanel(SIMPLE_URL);
  let { gFront, TimelineController } = panel.panelWin;

  is((yield gFront.isRecording()), false,
    "The timeline actor should not be recording when the tool starts.");
  is(TimelineController.getMarkers().length, 0,
    "There should be no markers available when the tool starts.");

  yield TimelineController.toggleRecording();

  is((yield gFront.isRecording()), true,
    "The timeline actor should be recording now.");
  ok((yield waitUntil(() => TimelineController.getMarkers().length > 0)),
    "There are some markers available now.");

  ok("startTime" in TimelineController.getMarkers(),
    "A `startTime` field was set on the markers array.");
  ok("endTime" in TimelineController.getMarkers(),
    "An `endTime` field was set on the markers array.");
  ok(TimelineController.getMarkers().endTime >
     TimelineController.getMarkers().startTime,
    "Some time has passed since the recording started.");

  yield teardown(panel);
  finish();
});
