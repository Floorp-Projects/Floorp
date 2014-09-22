/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the waterfall is properly built after finishing a recording.
 */

let test = Task.async(function*() {
  let { target, panel } = yield initTimelinePanel(SIMPLE_URL);
  let { $, $$, EVENTS, TimelineController } = panel.panelWin;

  yield TimelineController.toggleRecording();
  ok(true, "Recording has started.");

  let updated = 0;
  panel.panelWin.on(EVENTS.OVERVIEW_UPDATED, () => updated++);

  ok((yield waitUntil(() => updated > 0)),
    "The overview graph was updated a bunch of times.");
  ok((yield waitUntil(() => TimelineController.getMarkers().length > 0)),
    "There are some markers available.");

  yield TimelineController.toggleRecording();
  ok(true, "Recording has ended.");

  // Test the header container.

  ok($(".timeline-header-container"),
    "A header container should have been created.");

  // Test the header sidebar (left).

  ok($(".timeline-header-sidebar"),
    "A header sidebar node should have been created.");
  ok($(".timeline-header-sidebar > .timeline-header-name"),
    "A header name label should have been created inside the sidebar.");

  // Test the header ticks (right).

  ok($(".timeline-header-ticks"),
    "A header ticks node should have been created.");
  ok($$(".timeline-header-ticks > .timeline-header-tick").length > 0,
    "Some header tick labels should have been created inside the tick node.");

  // Test the markers container.

  ok($(".timeline-marker-container"),
    "A marker container should have been created.");

  // Test the markers sidebar (left).

  ok($$(".timeline-marker-sidebar").length,
    "Some marker sidebar nodes should have been created.");
  ok($$(".timeline-marker-sidebar:not(spacer) > .timeline-marker-bullet").length,
    "Some marker color bullets should have been created inside the sidebar.");
  ok($$(".timeline-marker-sidebar:not(spacer) > .timeline-marker-name").length,
    "Some marker name labels should have been created inside the sidebar.");

  // Test the markers waterfall (right).

  ok($$(".timeline-marker-waterfall").length,
    "Some marker waterfall nodes should have been created.");
  ok($$(".timeline-marker-waterfall:not(spacer) > .timeline-marker-bar").length,
    "Some marker color bars should have been created inside the waterfall.");

  yield teardown(panel);
  finish();
});
