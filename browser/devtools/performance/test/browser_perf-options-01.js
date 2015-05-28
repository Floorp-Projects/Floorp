/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that toggling preferences before there are any recordings does not throw.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, DetailsView, JsCallTreeView } = panel.panelWin;

  yield DetailsView.selectView("js-calltree");

  // Manually call the _onPrefChanged function so we can catch an error
  try {
    JsCallTreeView._onPrefChanged(null, "invert-call-tree", true);
    ok(true, "Toggling preferences before there are any recordings should not fail.");
  } catch (e) {
    ok(false, "Toggling preferences before there are any recordings should not fail.");
  }

  yield teardown(panel);
  finish();
}
