/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the profiler UI does not forget that recording is active when
 * selected recording changes.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");
const { setSelectedRecording } = require("devtools/client/performance/test/helpers/recording-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { $, EVENTS, PerformanceController } = panel.panelWin;

  await startRecording(panel);
  await stopRecording(panel);

  await startRecording(panel);

  info("Selecting recording #0 and waiting for it to be displayed.");

  const selected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  setSelectedRecording(panel, 0);
  await selected;

  ok($("#main-record-button").classList.contains("checked"),
    "Button is still checked after selecting another item.");
  ok(!$("#main-record-button").hasAttribute("disabled"),
    "Button is not locked after selecting another item.");

  await stopRecording(panel);

  await teardownToolboxAndRemoveTab(panel);
});
