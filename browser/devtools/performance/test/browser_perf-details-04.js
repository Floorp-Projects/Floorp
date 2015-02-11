/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the details view hides the toolbar buttons when a recording
 * doesn't exist or is in progress.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, $, $$, PerformanceController, RecordingsView, DetailsView } = panel.panelWin;

  let waterfallBtn = $("toolbarbutton[data-view='waterfall']");
  let jsFlameBtn = $("toolbarbutton[data-view='js-flamegraph']");
  let jsCallBtn = $("toolbarbutton[data-view='js-calltree']");
  let memFlameBtn = $("toolbarbutton[data-view='memory-flamegraph']");
  let memCallBtn = $("toolbarbutton[data-view='memory-calltree']");

  is(waterfallBtn.hidden, true, "waterfall button hidden when tool starts.");
  is(jsFlameBtn.hidden, true, "js-flamegraph button hidden when tool starts.");
  is(jsCallBtn.hidden, true, "js-calltree button hidden when tool starts.");
  is(memFlameBtn.hidden, true, "memory-flamegraph button hidden when tool starts.");
  is(memCallBtn.hidden, true, "memory-calltree button hidden when tool starts.");

  yield startRecording(panel);

  is(waterfallBtn.hidden, true, "waterfall button hidden when recording starts.");
  is(jsFlameBtn.hidden, true, "js-flamegraph button hidden when recording starts.");
  is(jsCallBtn.hidden, true, "js-calltree button hidden when recording starts.");
  is(memFlameBtn.hidden, true, "memory-flamegraph button hidden when recording starts.");
  is(memCallBtn.hidden, true, "memory-calltree button hidden when recording starts.");

  yield stopRecording(panel);

  is(waterfallBtn.hidden, false, "waterfall button visible when recording ends.");
  is(jsFlameBtn.hidden, false, "js-flamegraph button visible when recording ends.");
  is(jsCallBtn.hidden, false, "js-calltree button visible when recording ends.");
  is(memFlameBtn.hidden, true, "memory-flamegraph button hidden when recording ends.");
  is(memCallBtn.hidden, true, "memory-calltree button hidden when recording ends.");

  yield startRecording(panel);

  is(waterfallBtn.hidden, true, "waterfall button hidden when another recording starts.");
  is(jsFlameBtn.hidden, true, "js-flamegraph button hidden when another recording starts.");
  is(jsCallBtn.hidden, true, "js-calltree button hidden when another recording starts.");
  is(memFlameBtn.hidden, true, "memory-flamegraph button hidden when another recording starts.");
  is(memCallBtn.hidden, true, "memory-calltree button hidden when another recording starts.");

  let select = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  mousedown(panel.panelWin, $$(".recording-item")[0]);
  yield select;

  is(RecordingsView.selectedIndex, 0,
    "The first recording was selected again.");

  is(waterfallBtn.hidden, false, "waterfall button visible when first recording selected.");
  is(jsFlameBtn.hidden, false, "js-flamegraph button visible when first recording selected.");
  is(jsCallBtn.hidden, false, "js-calltree button visible when first recording selected.");
  is(memFlameBtn.hidden, true, "memory-flamegraph button hidden when first recording selected.");
  is(memCallBtn.hidden, true, "memory-calltree button hidden when first recording selected.");

  select = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  mousedown(panel.panelWin, $$(".recording-item")[1]);
  yield select;

  is(RecordingsView.selectedIndex, 1,
    "The second recording was selected again.");

  is(waterfallBtn.hidden, true, "waterfall button still hidden when second recording selected.");
  is(jsFlameBtn.hidden, true, "js-flamegraph button still hidden when second recording selected.");
  is(jsCallBtn.hidden, true, "js-calltree button still hidden when second recording selected.");
  is(memFlameBtn.hidden, true, "memory-flamegraph button still hidden when second recording selected.");
  is(memCallBtn.hidden, true, "memory-calltree button still hidden when second recording selected.");

  yield stopRecording(panel);

  is(RecordingsView.selectedIndex, 1,
    "The second recording is still selected.");

  is(waterfallBtn.hidden, false, "waterfall button visible when second recording finished.");
  is(jsFlameBtn.hidden, false, "js-flamegraph button visible when second recording finished.");
  is(jsCallBtn.hidden, false, "js-calltree button visible when second recording finished.");
  is(memFlameBtn.hidden, true, "memory-flamegraph button hidden when second recording finished.");
  is(memCallBtn.hidden, true, "memory-calltree button hidden when second recording finished.");

  yield teardown(panel);
  finish();
}
