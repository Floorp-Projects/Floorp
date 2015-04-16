/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8,<p>browser_telemetry_misc.js</p>";
const TOOL_DELAY = 0;

add_task(function*() {
  yield promiseTab(TEST_URI);

  startTelemetry();

  yield openAndCloseToolbox(1, TOOL_DELAY, "inspector");
  checkResults();

  gBrowser.removeCurrentTab();
});

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_")
  // here.
  checkTelemetry("DEVTOOLS_OS_ENUMERATED_PER_USER", null, "hasentries");
  checkTelemetry("DEVTOOLS_OS_IS_64_BITS_PER_USER", null, "hasentries");
  checkTelemetry("DEVTOOLS_SCREEN_RESOLUTION_ENUMERATED_PER_USER", null, "hasentries");
}
