/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the details view hides the memory buttons when `enable-memory` is toggled,
 * and that it switches to default panel if toggling while a memory panel is selected.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, OverviewView, DetailsView } = panel.panelWin;
  let { $, WaterfallView, MemoryCallTreeView, MemoryFlameGraphView } = panel.panelWin;

  ok(DetailsView.isViewSelected(WaterfallView),
    "The waterfall view is selected by default in the details view.");

  // The toolbar buttons will always be hidden when a recording isn't available,
  // so make sure we have one that's finished.
  yield startRecording(panel);
  yield stopRecording(panel);

  let flameBtn = $("toolbarbutton[data-view='memory-flamegraph']");
  let callBtn = $("toolbarbutton[data-view='memory-calltree']");

  Services.prefs.setBoolPref(MEMORY_PREF, false);
  is(flameBtn.hidden, true, "memory-flamegraph button hidden when enable-memory=false");
  is(callBtn.hidden, true, "memory-calltree button hidden when enable-memory=false");

  Services.prefs.setBoolPref(MEMORY_PREF, true);
  is(flameBtn.hidden, false, "memory-flamegraph button shown when enable-memory=true");
  is(callBtn.hidden, false, "memory-calltree button shown when enable-memory=true");

  let selected = DetailsView.whenViewSelected(MemoryCallTreeView);
  let notified = DetailsView.once(EVENTS.DETAILS_VIEW_SELECTED);
  DetailsView.selectView("memory-calltree");
  yield Promise.all([selected, notified]);

  ok(DetailsView.isViewSelected(MemoryCallTreeView),
    "The memory call tree view can now be selected.");

  selected = DetailsView.whenViewSelected(MemoryFlameGraphView);
  notified = DetailsView.once(EVENTS.DETAILS_VIEW_SELECTED);
  DetailsView.selectView("memory-flamegraph");
  yield Promise.all([selected, notified]);

  ok(DetailsView.isViewSelected(MemoryFlameGraphView),
    "The memory flamegraph view can now be selected.");

  selected = DetailsView.whenViewSelected(WaterfallView);
  notified = DetailsView.once(EVENTS.DETAILS_VIEW_SELECTED);
  Services.prefs.setBoolPref(MEMORY_PREF, false);
  yield Promise.all([selected, notified]);

  ok(DetailsView.isViewSelected(WaterfallView),
    "The waterfall view is now selected when toggling off enable-memory when a memory panel is selected.");

  Services.prefs.setBoolPref(MEMORY_PREF, true);

  selected = DetailsView.whenViewSelected(MemoryCallTreeView);
  notified = DetailsView.once(EVENTS.DETAILS_VIEW_SELECTED);
  DetailsView.selectView("memory-calltree");
  yield Promise.all([selected, notified]);

  ok(DetailsView.isViewSelected(MemoryCallTreeView),
    "The memory call tree view can be selected again after re-enabling memory.");

  selected = DetailsView.whenViewSelected(MemoryFlameGraphView);
  notified = DetailsView.once(EVENTS.DETAILS_VIEW_SELECTED);
  DetailsView.selectView("memory-flamegraph");
  yield Promise.all([selected, notified]);

  ok(DetailsView.isViewSelected(MemoryFlameGraphView),
    "The memory flamegraph view can be selected again after re-enabling memory.");

  selected = DetailsView.whenViewSelected(WaterfallView);
  notified = DetailsView.once(EVENTS.DETAILS_VIEW_SELECTED);
  Services.prefs.setBoolPref(MEMORY_PREF, false);
  yield Promise.all([selected, notified]);

  ok(DetailsView.isViewSelected(WaterfallView),
    "The waterfall view is now selected when toggling off enable-memory when a memory panel is selected.");

  yield teardown(panel);
  finish();
}
