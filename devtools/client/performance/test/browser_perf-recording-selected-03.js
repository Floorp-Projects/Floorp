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

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { $, EVENTS, PerformanceController, RecordingsView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel);

  yield startRecording(panel);

  info("Selecting recording #0 and waiting for it to be displayed.");

  let selected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  RecordingsView.selectedIndex = 0;
  yield selected;

  ok($("#main-record-button").hasAttribute("checked"),
    "Button is still checked after selecting another item.");
  ok(!$("#main-record-button").hasAttribute("locked"),
    "Button is not locked after selecting another item.");

  yield stopRecording(panel);

  yield teardownToolboxAndRemoveTab(panel);
});
