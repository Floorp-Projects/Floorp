/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the overview graphs do not render when realtime rendering is off
 * due to lack of e10s.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { $, EVENTS, PerformanceController, OverviewView, RecordingsView } = panel.panelWin;

  let updated = 0;
  OverviewView.on(EVENTS.OVERVIEW_RENDERED, () => updated++);
  OverviewView.OVERVIEW_UPDATE_INTERVAL = 1;

  // Set realtime rendering off.
  OverviewView.isRealtimeRenderingEnabled = () => false;

  yield startRecording(panel, { waitForOverview: false, waitForStateChange: true });
  is(isVisible($("#overview-pane")), false, "overview graphs hidden");
  is(updated, 0, "Overview graphs have still not been updated");
  yield waitUntil(() => PerformanceController.getCurrentRecording().getMarkers().length);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getTicks().length);
  is(updated, 0, "Overview graphs have still not been updated");

  yield stopRecording(panel);

  let markers = OverviewView.graphs.get("timeline");
  let framerate = OverviewView.graphs.get("framerate");

  ok(markers.width > 0,
    "The overview's markers graph has a width.");
  ok(framerate.width > 0,
    "The overview's framerate graph has a width.");

  is(updated, 1, "Overview graphs rendered upon completion.");
  is(isVisible($("#overview-pane")), true, "overview graphs no longer hidden");

  yield startRecording(panel, { waitForOverview: false, waitForStateChange: true });
  is(isVisible($("#overview-pane")), false, "overview graphs hidden again when starting new recording");

  RecordingsView.selectedIndex = 0;
  is(isVisible($("#overview-pane")), true, "overview graphs no longer hidden when switching back to complete recording.");
  RecordingsView.selectedIndex = 1;
  is(isVisible($("#overview-pane")), false, "overview graphs hidden again when going back to inprogress recording.");

  yield stopRecording(panel);
  is(isVisible($("#overview-pane")), true, "overview graphs no longer hidden when recording finishes");

  yield teardown(panel);
  finish();
}
