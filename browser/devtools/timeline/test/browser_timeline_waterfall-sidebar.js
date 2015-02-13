/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the sidebar is properly updated when a marker is selected.
 */

add_task(function*() {
  let { target, panel } = yield initTimelinePanel(SIMPLE_URL);
  let { $, $$, EVENTS, TimelineController, TimelineView, TIMELINE_BLUEPRINT} = panel.panelWin;
  let { L10N } = devtools.require("devtools/shared/timeline/global");

  yield TimelineController.toggleRecording();
  ok(true, "Recording has started.");

  yield waitUntil(() => {
    // Wait until we get 3 different markers.
    let markers = TimelineController.getMarkers();
    return markers.some(m => m.name == "Styles") &&
           markers.some(m => m.name == "Reflow") &&
           markers.some(m => m.name == "Paint");
  });

  yield TimelineController.toggleRecording();
  ok(true, "Recording has ended.");

  // Select everything
  TimelineView.markersOverview.setSelection({ start: 0, end: TimelineView.markersOverview.width })


  let bars = $$(".waterfall-marker-item:not(spacer) > .waterfall-marker-bar");
  let markers = TimelineController.getMarkers();

  ok(bars.length > 2, "got at least 3 markers");

  let sidebar = $("#timeline-waterfall-details");
  for (let i = 0; i < bars.length; i++) {
    let bar = bars[i];
    bar.click();
    let m = markers[i];

    let name = TIMELINE_BLUEPRINT[m.name].label;

    is($("#timeline-waterfall-details .marker-details-type").getAttribute("value"), name,
      "sidebar title matches markers name");

    let printedStartTime = $(".marker-details-start .marker-details-labelvalue").getAttribute("value");
    let printedEndTime = $(".marker-details-end .marker-details-labelvalue").getAttribute("value");
    let printedDuration= $(".marker-details-duration .marker-details-labelvalue").getAttribute("value");

    let toMs = ms => L10N.getFormatStrWithNumbers("timeline.tick", ms);

    // Values are rounded. We don't use a strict equality.
    is(toMs(m.start), printedStartTime, "sidebar start time is valid");
    is(toMs(m.end), printedEndTime, "sidebar end time is valid");
    is(toMs(m.end - m.start), printedDuration, "sidebar duration is valid");
  }
});
