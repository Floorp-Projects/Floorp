/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>browser_telemetry_button_responsive.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

const { ResponsiveUIManager } = Cu.import("resource://devtools/client/responsivedesign/responsivedesign.jsm", {});

add_task(function* () {
  yield addTab(TEST_URI);
  let Telemetry = loadTelemetryAndRecordLogs();

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = yield gDevTools.showToolbox(target, "inspector");
  info("inspector opened");

  info("testing the responsivedesign button");
  yield testButton(toolbox, Telemetry);

  stopRecordingTelemetryLogs(Telemetry);
  yield gDevTools.closeToolbox(target);
  gBrowser.removeCurrentTab();
});

function* testButton(toolbox, Telemetry) {
  info("Testing command-button-responsive");

  let button = toolbox.doc.querySelector("#command-button-responsive");
  ok(button, "Captain, we have the button");

  yield delayedClicks(button, 4);

  checkResults("_RESPONSIVE_", Telemetry);
}

function waitForToggle() {
  return new Promise(resolve => {
    let handler = () => {
      ResponsiveUIManager.off("on", handler);
      ResponsiveUIManager.off("off", handler);
      resolve();
    };
    ResponsiveUIManager.on("on", handler);
    ResponsiveUIManager.on("off", handler);
  });
}

var delayedClicks = Task.async(function* (node, clicks) {
  for (let i = 0; i < clicks; i++) {
    info("Clicking button " + node.id);
    let toggled = waitForToggle();
    node.click();
    yield toggled;
    // See TOOL_DELAY for why we need setTimeout here
    yield DevToolsUtils.waitForTime(TOOL_DELAY);
  }
});

function checkResults(histIdFocus, Telemetry) {
  let result = Telemetry.prototype.telemetryInfo;

  for (let [histId, value] of Iterator(result)) {
    if (histId.startsWith("DEVTOOLS_INSPECTOR_") ||
        !histId.includes(histIdFocus)) {
      // Inspector stats are tested in
      // browser_telemetry_toolboxtabs_{toolname}.js so we skip them here
      // because we only open the inspector once for this test.
      continue;
    }

    if (histId.endsWith("OPENED_PER_USER_FLAG")) {
      ok(value.length === 1 && value[0] === true,
         "Per user value " + histId + " has a single value of true");
    } else if (histId.endsWith("OPENED_COUNT")) {
      ok(value.length > 1, histId + " has more than one entry");

      let okay = value.every(function (element) {
        return element === true;
      });

      ok(okay, "All " + histId + " entries are === true");
    } else if (histId.endsWith("TIME_ACTIVE_SECONDS")) {
      ok(value.length > 1, histId + " has more than one entry");

      let okay = value.every(function (element) {
        return element > 0;
      });

      ok(okay, "All " + histId + " entries have time > 0");
    }
  }
}
