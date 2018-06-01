/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that view states and lazy component intialization works.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_ENABLE_MEMORY_PREF, UI_ENABLE_ALLOCATIONS_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { PerformanceView, OverviewView, DetailsView } = panel.panelWin;

  is(PerformanceView.getState(), "empty",
    "The intial state of the performance panel view is correct.");

  ok(!(OverviewView.graphs.get("timeline")),
    "The markers graph should not have been created yet.");
  ok(!(OverviewView.graphs.get("memory")),
    "The memory graph should not have been created yet.");
  ok(!(OverviewView.graphs.get("framerate")),
    "The framerate graph should not have been created yet.");

  ok(!DetailsView.components.waterfall.initialized,
    "The waterfall detail view should not have been created yet.");
  ok(!DetailsView.components["js-calltree"].initialized,
    "The js-calltree detail view should not have been created yet.");
  ok(!DetailsView.components["js-flamegraph"].initialized,
    "The js-flamegraph detail view should not have been created yet.");
  ok(!DetailsView.components["memory-calltree"].initialized,
    "The memory-calltree detail view should not have been created yet.");
  ok(!DetailsView.components["memory-flamegraph"].initialized,
    "The memory-flamegraph detail view should not have been created yet.");

  Services.prefs.setBoolPref(UI_ENABLE_MEMORY_PREF, true);
  Services.prefs.setBoolPref(UI_ENABLE_ALLOCATIONS_PREF, true);

  ok(!(OverviewView.graphs.get("timeline")),
    "The markers graph should still not have been created yet.");
  ok(!(OverviewView.graphs.get("memory")),
    "The memory graph should still not have been created yet.");
  ok(!(OverviewView.graphs.get("framerate")),
    "The framerate graph should still not have been created yet.");

  await startRecording(panel);

  is(PerformanceView.getState(), "recording",
    "The current state of the performance panel view is 'recording'.");
  ok(OverviewView.graphs.get("memory"),
    "The memory graph should have been created now.");
  ok(OverviewView.graphs.get("framerate"),
    "The framerate graph should have been created now.");

  await stopRecording(panel);

  is(PerformanceView.getState(), "recorded",
    "The current state of the performance panel view is 'recorded'.");
  ok(!DetailsView.components["js-calltree"].initialized,
    "The js-calltree detail view should still not have been created yet.");
  ok(!DetailsView.components["js-flamegraph"].initialized,
    "The js-flamegraph detail view should still not have been created yet.");
  ok(!DetailsView.components["memory-calltree"].initialized,
    "The memory-calltree detail view should still not have been created yet.");
  ok(!DetailsView.components["memory-flamegraph"].initialized,
    "The memory-flamegraph detail view should still not have been created yet.");

  await DetailsView.selectView("js-calltree");

  is(PerformanceView.getState(), "recorded",
    "The current state of the performance panel view is still 'recorded'.");
  ok(DetailsView.components["js-calltree"].initialized,
    "The js-calltree detail view should still have been created now.");
  ok(!DetailsView.components["js-flamegraph"].initialized,
    "The js-flamegraph detail view should still not have been created yet.");
  ok(!DetailsView.components["memory-calltree"].initialized,
    "The memory-calltree detail view should still not have been created yet.");
  ok(!DetailsView.components["memory-flamegraph"].initialized,
    "The memory-flamegraph detail view should still not have been created yet.");

  await DetailsView.selectView("memory-calltree");

  is(PerformanceView.getState(), "recorded",
    "The current state of the performance panel view is still 'recorded'.");
  ok(DetailsView.components["js-calltree"].initialized,
    "The js-calltree detail view should still register as being created.");
  ok(!DetailsView.components["js-flamegraph"].initialized,
    "The js-flamegraph detail view should still not have been created yet.");
  ok(DetailsView.components["memory-calltree"].initialized,
    "The memory-calltree detail view should still have been created now.");
  ok(!DetailsView.components["memory-flamegraph"].initialized,
    "The memory-flamegraph detail view should still not have been created yet.");

  await teardownToolboxAndRemoveTab(panel);
});
