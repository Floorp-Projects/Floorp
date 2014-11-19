/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the overview graphs cannot be selected during recording
 * and that they're cleared upon rerecording.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, OverviewView } = panel.panelWin;

  yield startRecording(panel);

  ok("selectionEnabled" in OverviewView.framerateGraph,
    "The selection should not be enabled for the framerate overview (1).");
  is(OverviewView.framerateGraph.selectionEnabled, false,
    "The selection should not be enabled for the framerate overview (2).");
  is(OverviewView.framerateGraph.hasSelection(), false,
    "The framerate overview shouldn't have a selection before recording.");

  let updated = 0;
  OverviewView.on(EVENTS.OVERVIEW_RENDERED, () => updated++);

  ok((yield waitUntil(() => updated > 10)),
    "The overviews were updated several times.");

  ok("selectionEnabled" in OverviewView.framerateGraph,
    "The selection should still not be enabled for the framerate overview (1).");
  is(OverviewView.framerateGraph.selectionEnabled, false,
    "The selection should still not be enabled for the framerate overview (2).");
  is(OverviewView.framerateGraph.hasSelection(), false,
    "The framerate overview still shouldn't have a selection before recording.");

  yield stopRecording(panel);

  is(OverviewView.framerateGraph.selectionEnabled, true,
    "The selection should now be enabled for the framerate overview.");

  yield teardown(panel);
  finish();
}
