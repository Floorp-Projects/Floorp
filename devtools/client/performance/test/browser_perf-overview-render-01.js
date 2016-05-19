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

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { EVENTS, OverviewView } = panel.panelWin;

  yield startRecording(panel);

  // Ensure overview keeps rendering.
  yield times(OverviewView, EVENTS.UI_OVERVIEW_RENDERED, 3, {
    expectedArgs: { "1": Constants.FRAMERATE_GRAPH_LOW_RES_INTERVAL }
  });

  ok(true, "Overview was rendered while recording.");

  yield stopRecording(panel);
  yield teardownToolboxAndRemoveTab(panel);
});
