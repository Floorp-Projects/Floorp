/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the overview graphs share the exact same width and scaling.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_ENABLE_MEMORY_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { waitUntil } = require("devtools/client/performance/test/helpers/wait-utils");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { PerformanceController, OverviewView } = panel.panelWin;

  // Enable memory to test.
  Services.prefs.setBoolPref(UI_ENABLE_MEMORY_PREF, true);

  let doChecks = () => {
    let markers = OverviewView.graphs.get("timeline");
    let framerate = OverviewView.graphs.get("framerate");
    let memory = OverviewView.graphs.get("memory");

    ok(markers.width > 0,
      "The overview's markers graph has a width.");
    ok(markers.dataScaleX > 0,
      "The overview's markers graph has a data scale factor.");

    ok(memory.width > 0,
      "The overview's memory graph has a width.");
    ok(memory.dataDuration > 0,
      "The overview's memory graph has a data duration.");
    ok(memory.dataScaleX > 0,
      "The overview's memory graph has a data scale factor.");

    ok(framerate.width > 0,
      "The overview's framerate graph has a width.");
    ok(framerate.dataDuration > 0,
      "The overview's framerate graph has a data duration.");
    ok(framerate.dataScaleX > 0,
      "The overview's framerate graph has a data scale factor.");

    is(markers.width, memory.width,
      "The markers and memory graphs widths are the same.");
    is(markers.width, framerate.width,
      "The markers and framerate graphs widths are the same.");

    is(memory.dataDuration, framerate.dataDuration,
      "The memory and framerate graphs data duration are the same.");

    is(markers.dataScaleX, memory.dataScaleX,
      "The markers and memory graphs data scale are the same.");
    is(markers.dataScaleX, framerate.dataScaleX,
      "The markers and framerate graphs data scale are the same.");
  };

  yield startRecording(panel);
  doChecks();

  yield waitUntil(() => PerformanceController.getCurrentRecording().getMarkers().length);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getMemory().length);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getTicks().length);
  doChecks();

  yield stopRecording(panel);
  doChecks();

  yield teardownToolboxAndRemoveTab(panel);
});
