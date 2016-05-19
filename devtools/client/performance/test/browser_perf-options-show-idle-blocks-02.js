/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the memory flamegraphs get rerendered when toggling `show-idle-blocks`.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_ENABLE_ALLOCATIONS_PREF, UI_SHOW_IDLE_BLOCKS_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { EVENTS, DetailsView, MemoryFlameGraphView } = panel.panelWin;

  // Enable allocations to test.
  Services.prefs.setBoolPref(UI_ENABLE_ALLOCATIONS_PREF, true);
  Services.prefs.setBoolPref(UI_SHOW_IDLE_BLOCKS_PREF, true);

  yield startRecording(panel);
  yield stopRecording(panel);

  let rendered = once(MemoryFlameGraphView, EVENTS.UI_MEMORY_FLAMEGRAPH_RENDERED);
  yield DetailsView.selectView("memory-flamegraph");
  yield rendered;

  rendered = once(MemoryFlameGraphView, EVENTS.UI_MEMORY_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(UI_SHOW_IDLE_BLOCKS_PREF, false);
  yield rendered;
  ok(true, "MemoryFlameGraphView rerendered when toggling show-idle-blocks.");

  rendered = once(MemoryFlameGraphView, EVENTS.UI_MEMORY_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(UI_SHOW_IDLE_BLOCKS_PREF, true);
  yield rendered;
  ok(true, "MemoryFlameGraphView rerendered when toggling back show-idle-blocks.");

  yield teardownToolboxAndRemoveTab(panel);
});
