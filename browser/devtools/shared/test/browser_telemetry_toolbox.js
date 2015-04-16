/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>browser_telemetry_toolbox.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

add_task(function*() {
  yield promiseTab(TEST_URI);

  startTelemetry();

  yield openAndCloseToolbox(3, TOOL_DELAY, "inspector");
  checkResults();

  gBrowser.removeCurrentTab();
});

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_")
  // here.
  checkTelemetry("DEVTOOLS_INSPECTOR_OPENED_BOOLEAN", [0,3,0]);
  checkTelemetry("DEVTOOLS_INSPECTOR_OPENED_PER_USER_FLAG", [0,1,0]);
  checkTelemetry("DEVTOOLS_INSPECTOR_TIME_ACTIVE_SECONDS", null, "hasentries");
  checkTelemetry("DEVTOOLS_RULEVIEW_OPENED_BOOLEAN", [0,3,0]);
  checkTelemetry("DEVTOOLS_RULEVIEW_OPENED_PER_USER_FLAG", [0,1,0]);
  checkTelemetry("DEVTOOLS_RULEVIEW_TIME_ACTIVE_SECONDS", null, "hasentries");
  checkTelemetry("DEVTOOLS_TOOLBOX_OPENED_BOOLEAN", [0,3,0]);
  checkTelemetry("DEVTOOLS_TOOLBOX_OPENED_PER_USER_FLAG", [0,1,0]);
  checkTelemetry("DEVTOOLS_TOOLBOX_TIME_ACTIVE_SECONDS", null, "hasentries");
}
