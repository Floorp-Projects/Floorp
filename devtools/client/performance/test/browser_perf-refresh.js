/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Rough test that the recording still continues after a refresh.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const {
  initPerformanceInNewTab,
  teardownToolboxAndRemoveTab,
} = require("devtools/client/performance/test/helpers/panel-utils");
const {
  startRecording,
  stopRecording,
  reload,
} = require("devtools/client/performance/test/helpers/actions");
const {
  waitUntil,
} = require("devtools/client/performance/test/helpers/wait-utils");

add_task(async function() {
  // Run this test without server targets as reload would introduce a target switch
  // with a new profile record which would clear previous page markers.
  // This panel has now been deprecated, we might later get rid of this test
  // once removing client side targets (bug 1721852)
  await SpecialPowers.pushPrefEnv({
    set: [["devtools.target-switching.server.enabled", false]],
  });

  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window,
  });

  const { PerformanceController } = panel.panelWin;

  await startRecording(panel);
  await reload(panel);

  const recording = PerformanceController.getCurrentRecording();
  const markersLength = recording.getAllData().markers.length;

  ok(
    recording.isRecording(),
    "RecordingModel should still be recording after reload"
  );

  await waitUntil(() => recording.getMarkers().length > markersLength);
  ok(true, "Markers continue after reload.");

  await stopRecording(panel);
  await teardownToolboxAndRemoveTab(panel);
});
