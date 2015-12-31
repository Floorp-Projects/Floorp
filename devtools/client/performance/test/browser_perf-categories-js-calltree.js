/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

requestLongerTimeout(2);

/**
 * Tests that the categories are shown in the js call tree when platform data
 * is enabled.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, $, $$, DetailsView, JsCallTreeView } = panel.panelWin;

  // Enable platform data to show the categories.
  Services.prefs.setBoolPref(PLATFORM_DATA_PREF, true);

  yield startRecording(panel);
  yield busyWait(100);

  let rendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  yield stopRecording(panel);
  yield DetailsView.selectView("js-calltree");
  yield rendered;

  is($(".call-tree-cells-container").hasAttribute("categories-hidden"), false,
    "The call tree cells container should show the categories now.");
  ok(geckoCategoryPresent($$),
    "A category node with the text `Gecko` is displayed in the tree.");

  // Disable platform data to show the categories.
  Services.prefs.setBoolPref(PLATFORM_DATA_PREF, false);

  is($(".call-tree-cells-container").getAttribute("categories-hidden"), "",
    "The call tree cells container should hide the categories now.");
  ok(!geckoCategoryPresent($$),
    "A category node with the text `Gecko` doesn't exist in the tree anymore.");

  yield teardown(panel);
  finish();
}

function geckoCategoryPresent($$) {
  for (let elem of $$('.call-tree-category')) {
    if (elem.textContent.trim() == 'Gecko') {
      return true
    }
  }
  return false
}
