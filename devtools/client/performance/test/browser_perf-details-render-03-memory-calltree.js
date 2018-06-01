/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the memory call tree view renders content after recording.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_ENABLE_ALLOCATIONS_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { EVENTS, DetailsView, MemoryCallTreeView } = panel.panelWin;

  // Enable allocations to test.
  Services.prefs.setBoolPref(UI_ENABLE_ALLOCATIONS_PREF, true);

  await startRecording(panel);
  await stopRecording(panel);

  const rendered = once(MemoryCallTreeView, EVENTS.UI_MEMORY_CALL_TREE_RENDERED);
  await DetailsView.selectView("memory-calltree");
  await rendered;

  ok(true, "MemoryCallTreeView rendered after recording is stopped.");

  await startRecording(panel);
  await stopRecording(panel, {
    expectedViewClass: "MemoryCallTreeView",
    expectedViewEvent: "UI_MEMORY_CALL_TREE_RENDERED"
  });

  ok(true, "MemoryCallTreeView rendered again after recording completed a second time.");

  await teardownToolboxAndRemoveTab(panel);
});
