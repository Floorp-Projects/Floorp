/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the waterfall background is a 1px high canvas stretching across
 * the container bounds.
 */

function* spawnTest() {
  let { target, panel } = yield initPerformance(SIMPLE_URL);
  let { $, EVENTS, PerformanceController, OverviewView, DetailsView, WaterfallView } = panel.panelWin;

  yield startRecording(panel);
  ok(true, "Recording has started.");

  let updated = 0;
  OverviewView.on(EVENTS.OVERVIEW_RENDERED, () => updated++);

  ok((yield waitUntil(() => updated > 0)),
    "The overview graphs were updated a bunch of times.");
  ok((yield waitUntil(() => PerformanceController.getCurrentRecording().getMarkers().length > 0)),
    "There are some markers available.");

  let rendered = Promise.all([
    DetailsView.selectView("waterfall"),
    once(WaterfallView, EVENTS.WATERFALL_RENDERED)
  ]);

  yield stopRecording(panel);
  ok(true, "Recording has ended.");

  yield rendered;
  ok(true, "Recording has rendered.");

  // Test the waterfall background.

  let parentWidth = $("#waterfall-view").getBoundingClientRect().width;
  let sidebarWidth = $(".waterfall-sidebar").getBoundingClientRect().width;
  let detailsWidth = $("#waterfall-details").getBoundingClientRect().width;
  let waterfallWidth = WaterfallView._markersRoot._waterfallWidth;
  is(waterfallWidth, parentWidth - sidebarWidth - detailsWidth,
    "The waterfall width is correct.")

  ok(WaterfallView._waterfallHeader._canvas,
    "A canvas should be created after the recording ended.");
  ok(WaterfallView._waterfallHeader._ctx,
    "A 2d context should be created after the recording ended.");

  is(WaterfallView._waterfallHeader._canvas.width, waterfallWidth,
    "The canvas width is correct.");
  is(WaterfallView._waterfallHeader._canvas.height, 1,
    "The canvas height is correct.");

  yield teardown(panel);
  finish();
}
