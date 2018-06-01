/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the memory flamegraphs views get rerendered when toggling
 * `invert-flame-graph`.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_ENABLE_ALLOCATIONS_PREF, UI_INVERT_FLAME_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { EVENTS, DetailsView, MemoryFlameGraphView } = panel.panelWin;

  // Enable allocations to test.
  Services.prefs.setBoolPref(UI_ENABLE_ALLOCATIONS_PREF, true);
  Services.prefs.setBoolPref(UI_INVERT_FLAME_PREF, true);

  await startRecording(panel);
  await stopRecording(panel);

  let rendered = once(MemoryFlameGraphView, EVENTS.UI_MEMORY_FLAMEGRAPH_RENDERED);
  await DetailsView.selectView("memory-flamegraph");
  await rendered;

  rendered = once(MemoryFlameGraphView, EVENTS.UI_MEMORY_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(UI_INVERT_FLAME_PREF, false);
  await rendered;
  ok(true, "MemoryFlameGraphView rerendered when toggling invert-call-tree.");

  rendered = once(MemoryFlameGraphView, EVENTS.UI_MEMORY_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(UI_INVERT_FLAME_PREF, true);
  await rendered;
  ok(true, "MemoryFlameGraphView rerendered when toggling back invert-call-tree.");

  await teardownToolboxAndRemoveTab(panel);
});
