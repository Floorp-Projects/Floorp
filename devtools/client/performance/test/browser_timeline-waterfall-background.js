/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the waterfall background is a 1px high canvas stretching across
 * the container bounds.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording, waitForOverviewRenderedWithMarkers } = require("devtools/client/performance/test/helpers/actions");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { WaterfallView } = panel.panelWin;

  yield startRecording(panel);
  ok(true, "Recording has started.");

  // Ensure overview is rendering and some markers were received.
  yield waitForOverviewRenderedWithMarkers(panel);

  yield stopRecording(panel);
  ok(true, "Recording has ended.");

  // Test the waterfall background.

  ok(WaterfallView.canvas, "A canvas should be created after the recording ended.");

  is(WaterfallView.canvas.width, WaterfallView.waterfallWidth,
    "The canvas width is correct.");
  is(WaterfallView.canvas.height, 1,
    "The canvas height is correct.");

  yield teardownToolboxAndRemoveTab(panel);
});
