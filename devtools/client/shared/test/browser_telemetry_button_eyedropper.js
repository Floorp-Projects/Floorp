/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>browser_telemetry_button_eyedropper.js</p><div>test</div>";

add_task(function* () {
  yield addTab(TEST_URI);
  let Telemetry = loadTelemetryAndRecordLogs();

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = yield gDevTools.showToolbox(target, "inspector");
  info("inspector opened");

  info("testing the eyedropper button");
  yield testButton(toolbox, Telemetry);

  stopRecordingTelemetryLogs(Telemetry);
  yield gDevTools.closeToolbox(target);
  gBrowser.removeCurrentTab();
});

function* testButton(toolbox, Telemetry) {
  info("Calling the eyedropper button's callback");
  // We call the button callback directly because we don't need to test the UI here, we're
  // only concerned about testing the telemetry probe.
  yield toolbox.getPanel("inspector").showEyeDropper();

  checkResults("_EYEDROPPER_", Telemetry);
}

function checkResults(histIdFocus, Telemetry) {
  let result = Telemetry.prototype.telemetryInfo;

  for (let [histId, value] of Object.entries(result)) {
    if (histId.startsWith("DEVTOOLS_INSPECTOR_") ||
        !histId.includes(histIdFocus)) {
      // Inspector stats are tested in
      // browser_telemetry_toolboxtabs_{toolname}.js so we skip them here
      // because we only open the inspector once for this test.
      continue;
    }

    if (histId.endsWith("OPENED_COUNT")) {
      is(value.length, 1, histId + " has one entry");

      let okay = value.every(element => element === true);
      ok(okay, "All " + histId + " entries are === true");
    }
  }
}
