/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests markers filtering mechanism.
 */

add_task(function*() {
  let { target, panel } = yield initTimelinePanel(SIMPLE_URL);
  let { $, $$, TimelineController, TimelineView } = panel.panelWin;

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

  let overview = TimelineView.markersOverview;
  let waterfall = TimelineView.waterfall;

  // Select everything
  overview.setSelection({ start: 0, end: overview.width })

  $("#filter-button").click();

  yield waitUntil(() => !waterfall._outstandingMarkers.length);

  let menuItem1 = $("menuitem[marker-type=Styles]");
  let menuItem2 = $("menuitem[marker-type=Reflow]");
  let menuItem3 = $("menuitem[marker-type=Paint]");

  let originalHeight = overview.fixedHeight;

  ok($(".waterfall-marker-bar[type=Styles]"), "Found at least one 'Styles' marker (1)");
  ok($(".waterfall-marker-bar[type=Reflow]"), "Found at least one 'Reflow' marker (1)");
  ok($(".waterfall-marker-bar[type=Paint]"), "Found at least one 'Paint' marker (1)");

  let heightBefore = overview.fixedHeight;
  EventUtils.synthesizeMouseAtCenter(menuItem1, {type: "mouseup"}, panel.panelWin);
  yield once(menuItem1, "command");

  yield waitUntil(() => !waterfall._outstandingMarkers.length);

  // A row is 11px. See markers-overview.js
  is(overview.fixedHeight, heightBefore - 11, "Overview is smaller");
  ok(!$(".waterfall-marker-bar[type=Styles]"), "No 'Styles' marker (2)");
  ok($(".waterfall-marker-bar[type=Reflow]"), "Found at least one 'Reflow' marker (2)");
  ok($(".waterfall-marker-bar[type=Paint]"), "Found at least one 'Paint' marker (2)");

  heightBefore = overview.fixedHeight;
  EventUtils.synthesizeMouseAtCenter(menuItem2, {type: "mouseup"}, panel.panelWin);
  yield once(menuItem2, "command");

  yield waitUntil(() => !waterfall._outstandingMarkers.length);

  is(overview.fixedHeight, heightBefore - 11, "Overview is smaller");
  ok(!$(".waterfall-marker-bar[type=Styles]"), "No 'Styles' marker (3)");
  ok(!$(".waterfall-marker-bar[type=Reflow]"), "No 'Reflow' marker (3)");
  ok($(".waterfall-marker-bar[type=Paint]"), "Found at least one 'Paint' marker (3)");

  heightBefore = overview.fixedHeight;
  EventUtils.synthesizeMouseAtCenter(menuItem3, {type: "mouseup"}, panel.panelWin);
  yield once(menuItem3, "command");

  yield waitUntil(() => !waterfall._outstandingMarkers.length);

  is(overview.fixedHeight, heightBefore - 11, "Overview is smaller");
  ok(!$(".waterfall-marker-bar[type=Styles]"), "No 'Styles' marker (4)");
  ok(!$(".waterfall-marker-bar[type=Reflow]"), "No 'Reflow' marker (4)");
  ok(!$(".waterfall-marker-bar[type=Paint]"), "No 'Paint' marker (4)");

  for (let item of [menuItem1, menuItem2, menuItem3]) {
    EventUtils.synthesizeMouseAtCenter(item, {type: "mouseup"}, panel.panelWin);
    yield once(item, "command");
  }

  yield waitUntil(() => !waterfall._outstandingMarkers.length);

  ok($(".waterfall-marker-bar[type=Styles]"), "Found at least one 'Styles' marker (5)");
  ok($(".waterfall-marker-bar[type=Reflow]"), "Found at least one 'Reflow' marker (5)");
  ok($(".waterfall-marker-bar[type=Paint]"), "Found at least one 'Paint' marker (5)");

  is(overview.fixedHeight, originalHeight, "Overview restored");

  $(".waterfall-marker-bar[type=Styles]");
});
