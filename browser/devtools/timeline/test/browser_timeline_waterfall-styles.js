/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the waterfall is properly built after making a selection
 * and the child nodes are styled correctly.
 */

var gRGB_TO_HSL = {
 "rgb(193, 132, 214)": "hsl(285,50%,68%)",
 "rgb(152, 61, 183)": "hsl(285,50%,48%)",
 "rgb(161, 223, 138)": "hsl(104,57%,71%)",
 "rgb(96, 201, 58)": "hsl(104,57%,51%)",
 "rgb(240, 195, 111)": "hsl(39,82%,69%)",
 "rgb(227, 155, 22)": "hsl(39,82%,49%)",
};

let test = Task.async(function*() {
  let { target, panel } = yield initTimelinePanel(SIMPLE_URL);
  let { TIMELINE_BLUEPRINT } = devtools.require("devtools/timeline/global");
  let { $, $$, EVENTS, TimelineController } = panel.panelWin;

  yield TimelineController.toggleRecording();
  ok(true, "Recording has started.");

  let updated = 0;
  panel.panelWin.on(EVENTS.OVERVIEW_UPDATED, () => updated++);

  ok((yield waitUntil(() => updated > 0)),
    "The overview graphs were updated a bunch of times.");
  ok((yield waitUntil(() => TimelineController.getMarkers().length > 0)),
    "There are some markers available.");

  yield TimelineController.toggleRecording();
  ok(true, "Recording has ended.");

  // Test the table sidebars.

  for (let sidebar of [
    ...$$(".timeline-header-sidebar"),
    ...$$(".timeline-marker-sidebar")
  ]) {
    is(sidebar.getAttribute("width"), "150",
      "The table's sidebar width is correct.");
  }

  // Test the table ticks.

  for (let tick of $$(".timeline-header-tick")) {
    ok(tick.getAttribute("value").match(/^\d+ ms$/),
      "The table's timeline ticks appear to have correct labels.");
    ok(tick.style.transform.match(/^translateX\(.*px\)$/),
      "The table's timeline ticks appear to have proper translations.");
  }

  // Test the marker bullets.

  for (let bullet of $$(".timeline-marker-bullet")) {
    let type = bullet.getAttribute("type");

    ok(type in TIMELINE_BLUEPRINT,
      "The bullet type is present in the timeline blueprint.");
    is(gRGB_TO_HSL[bullet.style.backgroundColor], TIMELINE_BLUEPRINT[type].fill,
      "The bullet's background color is correct.");
    is(gRGB_TO_HSL[bullet.style.borderColor], TIMELINE_BLUEPRINT[type].stroke,
      "The bullet's border color is correct.");
  }

  // Test the marker bars.

  for (let bar of $$(".timeline-marker-bar")) {
    let type = bar.getAttribute("type");

    ok(type in TIMELINE_BLUEPRINT,
      "The bar type is present in the timeline blueprint.");
    is(gRGB_TO_HSL[bar.style.backgroundColor], TIMELINE_BLUEPRINT[type].fill,
      "The bar's background color is correct.");
    is(gRGB_TO_HSL[bar.style.borderColor], TIMELINE_BLUEPRINT[type].stroke,
      "The bar's border color is correct.");

    ok(bar.getAttribute("width") > 0,
      "The bar appears to have a proper width.");
    ok(bar.style.transform.match(/^translateX\(.*px\)$/),
      "The bar appears to have proper translations.");
  }

  yield teardown(panel);
  finish();
});
