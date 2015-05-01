/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the sidebar is properly updated when a marker is selected.
 */

function spawnTest () {
  let { target, panel } = yield initPerformance(SIMPLE_URL);
  let { $, $$, EVENTS, PerformanceController, OverviewView } = panel.panelWin;
  let { L10N, TIMELINE_BLUEPRINT } = devtools.require("devtools/shared/timeline/global");

  yield startRecording(panel);
  ok(true, "Recording has started.");

  yield waitUntil(() => {
    // Wait until we get 3 different markers.
    let markers = PerformanceController.getCurrentRecording().getMarkers();
    return markers.some(m => m.name == "Styles") &&
           markers.some(m => m.name == "Reflow") &&
           markers.some(m => m.name == "Paint");
  });

  yield stopRecording(panel);
  ok(true, "Recording has ended.");

  // Select everything
  OverviewView.graphs.get("timeline").setSelection({ start: 0, end: OverviewView.graphs.get("timeline").width })

  let bars = $$(".waterfall-marker-item:not(spacer) > .waterfall-marker-bar");
  let markers = PerformanceController.getCurrentRecording().getMarkers();

  ok(bars.length > 2, "got at least 3 markers");

  let sidebar = $("#waterfall-details");
  for (let i = 0; i < bars.length; i++) {
    let bar = bars[i];
    bar.click();
    let m = markers[i];

    let name = TIMELINE_BLUEPRINT[m.name].label;

    is($("#waterfall-details .marker-details-type").getAttribute("value"), name,
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
  yield teardown(panel);
  finish();
}
