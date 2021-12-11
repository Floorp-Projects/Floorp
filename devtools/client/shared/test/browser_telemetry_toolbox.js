/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8," + "<p>browser_telemetry_toolbox.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

add_task(async function() {
  await addTab(TEST_URI);
  startTelemetry();

  await openAndCloseToolbox(3, TOOL_DELAY, "inspector");
  checkResults();

  gBrowser.removeCurrentTab();
});

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_TOOLBOX_")
  // here.
  checkTelemetry("DEVTOOLS_TOOLBOX_OPENED_COUNT", "", { 0: 3, 1: 0 }, "array");
  checkTelemetry(
    "DEVTOOLS_TOOLBOX_TIME_ACTIVE_SECONDS",
    "",
    null,
    "hasentries"
  );
  checkTelemetry("DEVTOOLS_TOOLBOX_HOST", "", null, "hasentries");
}
