/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the details view hides the toolbar buttons when a recording
 * doesn't exist or is in progress.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { EVENTS, $, PerformanceController, RecordingsView, WaterfallView } = panel.panelWin;

  let waterfallBtn = $("toolbarbutton[data-view='waterfall']");
  let jsFlameBtn = $("toolbarbutton[data-view='js-flamegraph']");
  let jsCallBtn = $("toolbarbutton[data-view='js-calltree']");
  let memFlameBtn = $("toolbarbutton[data-view='memory-flamegraph']");
  let memCallBtn = $("toolbarbutton[data-view='memory-calltree']");

  is(waterfallBtn.hidden, true,
    "The `waterfall` button is hidden when tool starts.");
  is(jsFlameBtn.hidden, true,
    "The `js-flamegraph` button is hidden when tool starts.");
  is(jsCallBtn.hidden, true,
    "The `js-calltree` button is hidden when tool starts.");
  is(memFlameBtn.hidden, true,
    "The `memory-flamegraph` button is hidden when tool starts.");
  is(memCallBtn.hidden, true,
    "The `memory-calltree` button is hidden when tool starts.");

  yield startRecording(panel);

  is(waterfallBtn.hidden, true,
    "The `waterfall` button is hidden when recording starts.");
  is(jsFlameBtn.hidden, true,
    "The `js-flamegraph` button is hidden when recording starts.");
  is(jsCallBtn.hidden, true,
    "The `js-calltree` button is hidden when recording starts.");
  is(memFlameBtn.hidden, true,
    "The `memory-flamegraph` button is hidden when recording starts.");
  is(memCallBtn.hidden, true,
    "The `memory-calltree` button is hidden when recording starts.");

  yield stopRecording(panel);

  is(waterfallBtn.hidden, false,
    "The `waterfall` button is visible when recording ends.");
  is(jsFlameBtn.hidden, false,
    "The `js-flamegraph` button is visible when recording ends.");
  is(jsCallBtn.hidden, false,
    "The `js-calltree` button is visible when recording ends.");
  is(memFlameBtn.hidden, true,
    "The `memory-flamegraph` button is hidden when recording ends.");
  is(memCallBtn.hidden, true,
    "The `memory-calltree` button is hidden when recording ends.");

  yield startRecording(panel);

  is(waterfallBtn.hidden, true,
    "The `waterfall` button is hidden when another recording starts.");
  is(jsFlameBtn.hidden, true,
    "The `js-flamegraph` button is hidden when another recording starts.");
  is(jsCallBtn.hidden, true,
    "The `js-calltree` button is hidden when another recording starts.");
  is(memFlameBtn.hidden, true,
    "The `memory-flamegraph` button is hidden when another recording starts.");
  is(memCallBtn.hidden, true,
    "The `memory-calltree` button is hidden when another recording starts.");

  let selected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  let rendered = once(WaterfallView, EVENTS.UI_WATERFALL_RENDERED);
  RecordingsView.selectedIndex = 0;
  yield selected;
  yield rendered;

  is(RecordingsView.selectedIndex, 0,
    "The first recording was selected again.");

  is(waterfallBtn.hidden, false,
    "The `waterfall` button is visible when first recording selected.");
  is(jsFlameBtn.hidden, false,
    "The `js-flamegraph` button is visible when first recording selected.");
  is(jsCallBtn.hidden, false,
    "The `js-calltree` button is visible when first recording selected.");
  is(memFlameBtn.hidden, true,
    "The `memory-flamegraph` button is hidden when first recording selected.");
  is(memCallBtn.hidden, true,
    "The `memory-calltree` button is hidden when first recording selected.");

  selected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  RecordingsView.selectedIndex = 1;
  yield selected;

  is(RecordingsView.selectedIndex, 1,
    "The second recording was selected again.");

  is(waterfallBtn.hidden, true,
    "The `waterfall button` still is hidden when second recording selected.");
  is(jsFlameBtn.hidden, true,
    "The `js-flamegraph button` still is hidden when second recording selected.");
  is(jsCallBtn.hidden, true,
    "The `js-calltree button` still is hidden when second recording selected.");
  is(memFlameBtn.hidden, true,
    "The `memory-flamegraph button` still is hidden when second recording selected.");
  is(memCallBtn.hidden, true,
    "The `memory-calltree button` still is hidden when second recording selected.");

  rendered = once(WaterfallView, EVENTS.UI_WATERFALL_RENDERED);
  yield stopRecording(panel);
  yield rendered;

  is(RecordingsView.selectedIndex, 1,
    "The second recording is still selected.");

  is(waterfallBtn.hidden, false,
    "The `waterfall` button is visible when second recording finished.");
  is(jsFlameBtn.hidden, false,
    "The `js-flamegraph` button is visible when second recording finished.");
  is(jsCallBtn.hidden, false,
    "The `js-calltree` button is visible when second recording finished.");
  is(memFlameBtn.hidden, true,
    "The `memory-flamegraph` button is hidden when second recording finished.");
  is(memCallBtn.hidden, true,
    "The `memory-calltree` button is hidden when second recording finished.");

  yield teardownToolboxAndRemoveTab(panel);
});
