/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the overview renders content when recording.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, OverviewView } = panel.panelWin;

  yield startRecording(panel);
 
  yield once(OverviewView, EVENTS.OVERVIEW_RENDERED);
  yield once(OverviewView, EVENTS.OVERVIEW_RENDERED);

  yield stopRecording(panel);

  yield teardown(panel);
  finish();
}
