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
const { setSelectedRecording, getSelectedRecordingIndex } = require("devtools/client/performance/test/helpers/recording-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const {
    EVENTS,
    $,
    PerformanceController,
    WaterfallView
  } = panel.panelWin;

  const waterfallBtn = $("toolbarbutton[data-view='waterfall']");
  const jsFlameBtn = $("toolbarbutton[data-view='js-flamegraph']");
  const jsCallBtn = $("toolbarbutton[data-view='js-calltree']");
  const memFlameBtn = $("toolbarbutton[data-view='memory-flamegraph']");
  const memCallBtn = $("toolbarbutton[data-view='memory-calltree']");

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

  await startRecording(panel);

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

  await stopRecording(panel);

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

  await startRecording(panel);

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
  setSelectedRecording(panel, 0);
  await selected;
  await rendered;

  let selectedIndex = getSelectedRecordingIndex(panel);
  is(selectedIndex, 0,
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
  setSelectedRecording(panel, 1);
  await selected;

  selectedIndex = getSelectedRecordingIndex(panel);
  is(selectedIndex, 1,
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
  await stopRecording(panel);
  await rendered;

  selectedIndex = getSelectedRecordingIndex(panel);
  is(selectedIndex, 1,
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

  await teardownToolboxAndRemoveTab(panel);
});
