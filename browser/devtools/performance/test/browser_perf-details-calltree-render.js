/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the call tree view renders content after recording.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, DetailsView, WaterfallView, JsCallTreeView } = panel.panelWin;

  yield startRecording(panel);
  yield busyWait(100);
  yield stopRecording(panel);

  let rendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  yield DetailsView.selectView("js-calltree");
  ok(DetailsView.isViewSelected(JsCallTreeView), "The call tree is now selected.");
  yield rendered;

  ok(true, "JsCallTreeView rendered after recording is stopped.");

  yield DetailsView.selectView("waterfall");

  yield startRecording(panel);
  yield busyWait(100);
  let waterfallRenderedOnWF = once(WaterfallView, EVENTS.WATERFALL_RENDERED);
  let waterfallRenderedOnJS = once(JsCallTreeView, EVENTS.WATERFALL_RENDERED);

  rendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  yield stopRecording(panel);

  // The waterfall should render by default, and we want to make
  // sure that the render events don't bleed between detail views
  // via bug 1173393, so test that's the case after both views have been
  // created
  waterfallRenderedOnJS.then(() =>
    ok(false, "JsCallTreeView should not receive WATERFALL_RENDERED event"));
  yield waterfallRenderedOnWF;

  yield DetailsView.selectView("js-calltree");
  yield rendered;

  ok(true, "JsCallTreeView rendered again after recording completed a second time.");

  yield teardown(panel);
  finish();
}
