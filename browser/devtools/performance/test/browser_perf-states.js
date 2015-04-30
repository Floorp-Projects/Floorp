/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that view states and lazy component intialization works.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceView, OverviewView, DetailsView } = panel.panelWin;

  is(PerformanceView.getState(), "empty",
    "The intial state of the performance panel view is correct.");

  ok(!(OverviewView.graphs.get("timeline")),
    "The markers graph should not have been created yet.");
  ok(!(OverviewView.graphs.get("memory")),
    "The memory graph should not have been created yet.");
  ok(!(OverviewView.graphs.get("framerate")),
    "The framerate graph should not have been created yet.");

  ok(!DetailsView.components["waterfall"].initialized,
    "The waterfall detail view should not have been created yet.");
  ok(!DetailsView.components["js-calltree"].initialized,
    "The js-calltree detail view should not have been created yet.");
  ok(!DetailsView.components["js-flamegraph"].initialized,
    "The js-flamegraph detail view should not have been created yet.");
  ok(!DetailsView.components["memory-calltree"].initialized,
    "The memory-calltree detail view should not have been created yet.");
  ok(!DetailsView.components["memory-flamegraph"].initialized,
    "The memory-flamegraph detail view should not have been created yet.");

  Services.prefs.setBoolPref(MEMORY_PREF, true);

  ok(!(OverviewView.graphs.get("timeline")),
    "The markers graph should still not have been created yet.");
  ok(!(OverviewView.graphs.get("memory")),
    "The memory graph should still not have been created yet.");
  ok(!(OverviewView.graphs.get("framerate")),
    "The framerate graph should still not have been created yet.");

  let stateChanged = once(PerformanceView, EVENTS.UI_STATE_CHANGED);
  yield startRecording(panel);
  yield stateChanged;

  is(PerformanceView.getState(), "recording",
    "The current state of the performance panel view is 'recording'.");
  ok(OverviewView.graphs.get("memory"),
    "The memory graph should have been created now.");
  ok(OverviewView.graphs.get("framerate"),
    "The framerate graph should have been created now.");

  stateChanged = once(PerformanceView, EVENTS.UI_STATE_CHANGED);
  yield stopRecording(panel);
  yield stateChanged;

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

  yield DetailsView.selectView("js-calltree");

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

  yield DetailsView.selectView("memory-calltree");

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

  yield teardown(panel);
  finish();
}
