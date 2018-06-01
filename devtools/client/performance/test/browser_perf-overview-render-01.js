/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the overview continuously renders content when recording.
 */

const { Constants } = require("devtools/client/performance/modules/constants");
const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { times } = require("devtools/client/performance/test/helpers/event-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { EVENTS, OverviewView } = panel.panelWin;

  await startRecording(panel);

  // Ensure overview keeps rendering.
  await times(OverviewView, EVENTS.UI_OVERVIEW_RENDERED, 3, {
    expectedArgs: [Constants.FRAMERATE_GRAPH_LOW_RES_INTERVAL]
  });

  ok(true, "Overview was rendered while recording.");

  await stopRecording(panel);
  await teardownToolboxAndRemoveTab(panel);
});
