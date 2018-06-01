/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the details view hides the allocations buttons when a recording
 * does not have allocations data ("withAllocations": false), and that when an
 * allocations panel is selected to a panel that does not have allocations goes
 * to a default panel instead.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_ENABLE_ALLOCATIONS_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");
const { setSelectedRecording } = require("devtools/client/performance/test/helpers/recording-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const {
    EVENTS,
    $,
    DetailsView,
    WaterfallView,
    MemoryCallTreeView,
    MemoryFlameGraphView
  } = panel.panelWin;

  const flameBtn = $("toolbarbutton[data-view='memory-flamegraph']");
  const callBtn = $("toolbarbutton[data-view='memory-calltree']");

  // Disable allocations to prevent recording them.
  Services.prefs.setBoolPref(UI_ENABLE_ALLOCATIONS_PREF, false);

  await startRecording(panel);
  await stopRecording(panel);

  ok(DetailsView.isViewSelected(WaterfallView),
    "The waterfall view is selected by default in the details view.");

  // Re-enable allocations to test.
  Services.prefs.setBoolPref(UI_ENABLE_ALLOCATIONS_PREF, true);

  // The toolbar buttons will always be hidden when a recording isn't available,
  // so make sure we have one that's finished.
  await startRecording(panel);
  await stopRecording(panel);

  ok(DetailsView.isViewSelected(WaterfallView),
    "The waterfall view is still selected in the details view.");

  is(callBtn.hidden, false,
    "The `memory-calltree` button is shown when recording has memory data.");
  is(flameBtn.hidden, false,
    "The `memory-flamegraph` button is shown when recording has memory data.");

  let selected = once(DetailsView, EVENTS.UI_DETAILS_VIEW_SELECTED);
  let rendered = once(MemoryCallTreeView, EVENTS.UI_MEMORY_CALL_TREE_RENDERED);
  DetailsView.selectView("memory-calltree");
  await selected;
  await rendered;

  ok(DetailsView.isViewSelected(MemoryCallTreeView),
    "The memory call tree view can now be selected.");

  selected = once(DetailsView, EVENTS.UI_DETAILS_VIEW_SELECTED);
  rendered = once(MemoryFlameGraphView, EVENTS.UI_MEMORY_FLAMEGRAPH_RENDERED);
  DetailsView.selectView("memory-flamegraph");
  await selected;
  await rendered;

  ok(DetailsView.isViewSelected(MemoryFlameGraphView),
    "The memory flamegraph view can now be selected.");

  // Select the first recording with no memory data.
  selected = once(DetailsView, EVENTS.UI_DETAILS_VIEW_SELECTED);
  rendered = once(WaterfallView, EVENTS.UI_WATERFALL_RENDERED);
  setSelectedRecording(panel, 0);
  await selected;
  await rendered;

  ok(DetailsView.isViewSelected(WaterfallView), "The waterfall view is now selected " +
    "when switching back to a recording that does not have memory data.");

  is(callBtn.hidden, true,
    "The `memory-calltree` button is hidden when recording has no memory data.");
  is(flameBtn.hidden, true,
    "The `memory-flamegraph` button is hidden when recording has no memory data.");

  // Go back to the recording with memory data.
  rendered = once(WaterfallView, EVENTS.UI_WATERFALL_RENDERED);
  setSelectedRecording(panel, 1);
  await rendered;

  ok(DetailsView.isViewSelected(WaterfallView),
    "The waterfall view is still selected in the details view.");

  is(callBtn.hidden, false,
    "The `memory-calltree` button is shown when recording has memory data.");
  is(flameBtn.hidden, false,
    "The `memory-flamegraph` button is shown when recording has memory data.");

  selected = once(DetailsView, EVENTS.UI_DETAILS_VIEW_SELECTED);
  rendered = once(MemoryCallTreeView, EVENTS.UI_MEMORY_CALL_TREE_RENDERED);
  DetailsView.selectView("memory-calltree");
  await selected;
  await rendered;

  ok(DetailsView.isViewSelected(MemoryCallTreeView), "The memory call tree view can be " +
    "selected again after going back to the view with memory data.");

  selected = once(DetailsView, EVENTS.UI_DETAILS_VIEW_SELECTED);
  rendered = once(MemoryFlameGraphView, EVENTS.UI_MEMORY_FLAMEGRAPH_RENDERED);
  DetailsView.selectView("memory-flamegraph");
  await selected;
  await rendered;

  ok(DetailsView.isViewSelected(MemoryFlameGraphView), "The memory flamegraph view can " +
    "be selected again after going back to the view with memory data.");

  await teardownToolboxAndRemoveTab(panel);
});
