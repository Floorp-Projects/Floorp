/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests markers filtering mechanism.
 */

let test = Task.async(function*() {
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

  // Select everything
  overview.setSelection({ start: 0, end: overview.width })

  $("#filter-button").click();

  let menuItem1 = $("menuitem[marker-type=Styles]");
  let menuItem2 = $("menuitem[marker-type=Reflow]");
  let menuItem3 = $("menuitem[marker-type=Paint]");

  let originalHeight = overview.fixedHeight;

  ok($(".waterfall-marker-bar[type=Styles]"), "Found at least one 'Styles' marker");
  ok($(".waterfall-marker-bar[type=Reflow]"), "Found at least one 'Reflow' marker");
  ok($(".waterfall-marker-bar[type=Paint]"), "Found at least one 'Paint' marker");

  let heightBefore = overview.fixedHeight;
  EventUtils.synthesizeMouseAtCenter(menuItem1, {type: "mouseup"}, panel.panelWin);
  yield once(menuItem1, "command");
  // A row is 11px. See markers-overview.js
  is(overview.fixedHeight, heightBefore - 11, "Overview is smaller");
  ok(!$(".waterfall-marker-bar[type=Styles]"), "No 'Styles' marker");
  ok($(".waterfall-marker-bar[type=Reflow]"), "Found at least one 'Reflow' marker");
  ok($(".waterfall-marker-bar[type=Paint]"), "Found at least one 'Paint' marker");

  heightBefore = overview.fixedHeight;
  EventUtils.synthesizeMouseAtCenter(menuItem2, {type: "mouseup"}, panel.panelWin);
  yield once(menuItem2, "command");
  is(overview.fixedHeight, heightBefore - 11, "Overview is smaller");
  ok(!$(".waterfall-marker-bar[type=Styles]"), "No 'Styles' marker");
  ok(!$(".waterfall-marker-bar[type=Reflow]"), "No 'Reflow' marker");
  ok($(".waterfall-marker-bar[type=Paint]"), "Found at least one 'Paint' marker");

  heightBefore = overview.fixedHeight;
  EventUtils.synthesizeMouseAtCenter(menuItem3, {type: "mouseup"}, panel.panelWin);
  yield once(menuItem3, "command");
  is(overview.fixedHeight, heightBefore - 11, "Overview is smaller");
  ok(!$(".waterfall-marker-bar[type=Styles]"), "No 'Styles' marker");
  ok(!$(".waterfall-marker-bar[type=Reflow]"), "No 'Reflow' marker");
  ok(!$(".waterfall-marker-bar[type=Paint]"), "No 'Paint' marker");

  for (let item of [menuItem1, menuItem2, menuItem3]) {
    EventUtils.synthesizeMouseAtCenter(item, {type: "mouseup"}, panel.panelWin);
    yield once(item, "command");
  }

  ok($(".waterfall-marker-bar[type=Styles]"), "Found at least one 'Styles' marker");
  ok($(".waterfall-marker-bar[type=Reflow]"), "Found at least one 'Reflow' marker");
  ok($(".waterfall-marker-bar[type=Paint]"), "Found at least one 'Paint' marker");

  is(overview.fixedHeight, originalHeight, "Overview restored");

  $(".waterfall-marker-bar[type=Styles]");

  yield teardown(panel);
  finish();
});
