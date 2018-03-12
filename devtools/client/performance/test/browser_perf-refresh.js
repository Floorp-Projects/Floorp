/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Rough test that the recording still continues after a refresh.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording, reload } = require("devtools/client/performance/test/helpers/actions");
const { waitUntil } = require("devtools/client/performance/test/helpers/wait-utils");

add_task(function* () {
  let { panel, target } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { PerformanceController } = panel.panelWin;

  yield startRecording(panel);
  yield reload(target);

  let recording = PerformanceController.getCurrentRecording();
  let markersLength = recording.getAllData().markers.length;

  ok(recording.isRecording(),
    "RecordingModel should still be recording after reload");

  yield waitUntil(() => recording.getMarkers().length > markersLength);
  ok("Markers continue after reload.");

  yield stopRecording(panel);
  yield teardownToolboxAndRemoveTab(panel);
});
