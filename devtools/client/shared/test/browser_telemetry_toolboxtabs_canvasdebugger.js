/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>browser_telemetry_toolboxtabs_canvasdebugger.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

add_task(async function() {
  info("Activate the canvasdebugger");
  let originalPref = Services.prefs.getBoolPref("devtools.canvasdebugger.enabled");
  Services.prefs.setBoolPref("devtools.canvasdebugger.enabled", true);

  await addTab(TEST_URI);
  startTelemetry();

  await openAndCloseToolbox(2, TOOL_DELAY, "canvasdebugger");
  checkResults();

  gBrowser.removeCurrentTab();

  info("De-activate the canvasdebugger");
  Services.prefs.setBoolPref("devtools.canvasdebugger.enabled", originalPref);
});

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_CANVASDEBUGGER")
  // here.
  checkTelemetry("DEVTOOLS_CANVASDEBUGGER_OPENED_COUNT", "", [2, 0, 0], "array");
  checkTelemetry("DEVTOOLS_CANVASDEBUGGER_TIME_ACTIVE_SECONDS", "", null, "hasentries");
}
