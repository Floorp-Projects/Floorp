/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that toggling preferences during a recording does not throw.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, DetailsView, JsCallTreeView } = panel.panelWin;

  yield DetailsView.selectView("js-calltree");

  yield startRecording(panel);

  // Manually call the _onPrefChanged function so we can catch an error
  try {
    JsCallTreeView._onPrefChanged(null, "invert-call-tree", true);
    ok(true, "Toggling preferences during a recording should not fail.");
  } catch (e) {
    ok(false, "Toggling preferences during a recording should not fail.");
  }

  yield stopRecording(panel);

  yield teardown(panel);
  finish();
}
