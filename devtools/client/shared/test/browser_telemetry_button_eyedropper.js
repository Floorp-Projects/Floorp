/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>browser_telemetry_button_eyedropper.js</p><div>test</div>";
const EYEDROPPER_OPENED = "devtools.toolbar.eyedropper.opened";

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

  checkTelemetryResults(Telemetry);
}

function checkTelemetryResults(Telemetry) {
  let data = Telemetry.prototype.telemetryInfo;
  let results = new Map();

  for (let key in data) {
    if (key.toLowerCase() === key) {
      let pings = data[key].length;

      results.set(key, pings);
    }
  }

  is(results.size, 1, "The correct number of scalars were logged");

  let pings = checkPings(EYEDROPPER_OPENED, results);
  is(pings, 1, `${EYEDROPPER_OPENED} has just 1 ping`);
}

function checkPings(scalarId, results) {
  return results.get(scalarId);
}
