/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<p>browser_telemetry_misc.js</p>";
const TOOL_DELAY = 0;

add_task(async function() {
  await addTab(TEST_URI);

  startTelemetry();

  await openAndCloseToolbox(1, TOOL_DELAY, "inspector");
  checkResults();

  gBrowser.removeCurrentTab();
});

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_")
  // here.
  checkTelemetry("DEVTOOLS_TOOLBOX_OPENED_COUNT", "", [1, 0, 0], "array");
  checkTelemetry("DEVTOOLS_INSPECTOR_OPENED_COUNT", "", [1, 0, 0], "array");
  checkTelemetry("DEVTOOLS_RULEVIEW_OPENED_COUNT", "", [1, 0, 0], "array");
  checkTelemetry("DEVTOOLS_TOOLBOX_TIME_ACTIVE_SECONDS", "", null, "hasentries");
  checkTelemetry("DEVTOOLS_INSPECTOR_TIME_ACTIVE_SECONDS", "", null, "hasentries");
  checkTelemetry("DEVTOOLS_RULEVIEW_TIME_ACTIVE_SECONDS", "", null, "hasentries");
  checkTelemetry("DEVTOOLS_TOOLBOX_HOST", "", null, "hasentries");
}
