/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the flamegraph view renders content after recording.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, FlameGraphView } = panel.panelWin;

  yield startRecording(panel);
  yield busyWait(100);

  let rendered = once(FlameGraphView, EVENTS.FLAMEGRAPH_RENDERED);
  yield stopRecording(panel);
  yield rendered;

  ok(true, "FlameGraphView rendered after recording is stopped.");

  yield startRecording(panel);
  yield busyWait(100);

  rendered = once(FlameGraphView, EVENTS.FLAMEGRAPH_RENDERED);
  yield stopRecording(panel);
  yield rendered;

  ok(true, "FlameGraphView rendered again after recording completed a second time.");

  yield teardown(panel);
  finish();
}
