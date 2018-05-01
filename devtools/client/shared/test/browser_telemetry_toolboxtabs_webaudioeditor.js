/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>browser_telemetry_toolboxtabs_webaudioeditor.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

add_task(async function() {
  info("Activating the webaudioeditor");
  let originalPref = Services.prefs.getBoolPref("devtools.webaudioeditor.enabled");
  Services.prefs.setBoolPref("devtools.webaudioeditor.enabled", true);

  await addTab(TEST_URI);
  startTelemetry();

  await openAndCloseToolbox(2, TOOL_DELAY, "webaudioeditor");
  checkResults();

  gBrowser.removeCurrentTab();

  info("De-activating the webaudioeditor");
  Services.prefs.setBoolPref("devtools.webaudioeditor.enabled", originalPref);
});

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_WEBAUDIOEDITOR")
  // here.
  checkTelemetry("DEVTOOLS_WEBAUDIOEDITOR_OPENED_COUNT", "", [2, 0, 0], "array");
  checkTelemetry("DEVTOOLS_WEBAUDIOEDITOR_TIME_ACTIVE_SECONDS", "", null, "hasentries");
}
