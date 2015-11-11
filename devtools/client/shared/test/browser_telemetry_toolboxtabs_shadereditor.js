/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("Error: Shader Editor is still waiting for a WebGL context to be created.");

const TEST_URI = "data:text/html;charset=utf-8,<p>browser_telemetry_toolboxtabs_shadereditor.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

add_task(function*() {
  info("Active the sharer editor");
  let originalPref = Services.prefs.getBoolPref("devtools.shadereditor.enabled");
  Services.prefs.setBoolPref("devtools.shadereditor.enabled", true);

  yield addTab(TEST_URI);
  let Telemetry = loadTelemetryAndRecordLogs();

  yield openAndCloseToolbox(2, TOOL_DELAY, "shadereditor");
  checkTelemetryResults(Telemetry);

  stopRecordingTelemetryLogs(Telemetry);
  gBrowser.removeCurrentTab();

  info("De-activate the sharer editor");
  Services.prefs.setBoolPref("devtools.shadereditor.enabled", originalPref);
});
