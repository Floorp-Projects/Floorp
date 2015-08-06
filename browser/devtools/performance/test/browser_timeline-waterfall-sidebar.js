/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the sidebar is properly updated when a marker is selected.
 */

function* spawnTest() {
  let { target, panel } = yield initPerformance(SIMPLE_URL);
  let { $, $$, PerformanceController, WaterfallView } = panel.panelWin;
  let { L10N } = require("devtools/performance/global");
  let { getMarkerLabel } = require("devtools/performance/marker-utils");

  // Hijack the markers massaging part of creating the waterfall view,
  // to prevent collapsing markers and allowing this test to verify
  // everything individually. A better solution would be to just expand
  // all markers first and then skip the meta nodes, but I'm lazy.
  WaterfallView._prepareWaterfallTree = markers => {
    return { submarkers: markers };
  };

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

  info("No need to select everything in the timeline.");
  info("All the markers should be displayed by default.");

  let bars = $$(".waterfall-marker-bar");
  let markers = PerformanceController.getCurrentRecording().getMarkers();

  info(`Got ${bars.length} bars and ${markers.length} markers.`);
  info("Markers types from datasrc: " + Array.map(markers, e => e.name));
  info("Markers names from sidebar: " + Array.map(bars, e => e.parentNode.parentNode.querySelector(".waterfall-marker-name").getAttribute("value")));

  ok(bars.length > 2, "Got at least 3 markers (1)");
  ok(markers.length > 2, "Got at least 3 markers (2)");

  let toMs = ms => L10N.getFormatStrWithNumbers("timeline.tick", ms);

  for (let i = 0; i < bars.length; i++) {
    let bar = bars[i];
    let mkr = markers[i];
    EventUtils.sendMouseEvent({ type: "mousedown" }, bar);

    let type = $(".marker-details-type").getAttribute("value");
    let tooltip = $(".marker-details-duration").getAttribute("tooltiptext");
    let duration = $(".marker-details-duration .marker-details-labelvalue").getAttribute("value");

    info("Current marker data: " + mkr.toSource());
    info("Current marker output: " + $("#waterfall-details").innerHTML);

    is(type, getMarkerLabel(mkr), "Sidebar title matches markers name.");

    // Values are rounded. We don't use a strict equality.
    is(toMs(mkr.end - mkr.start), duration, "Sidebar duration is valid.");

    // For some reason, anything that creates "→" here turns it into a "â" for some reason.
    // So just check that start and end time are in there somewhere.
    ok(tooltip.indexOf(toMs(mkr.start)) !== -1, "Tooltip has start time.");
    ok(tooltip.indexOf(toMs(mkr.end)) !== -1, "Tooltip has end time.");
  }

  yield teardown(panel);
  finish();
}
