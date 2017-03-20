/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>browser_telemetry_button_paintflashing.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

add_task(function* () {
  yield addTab(TEST_URI);
  let Telemetry = loadTelemetryAndRecordLogs();
  yield pushPref("devtools.command-button-paintflashing.enabled", true);

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = yield gDevTools.showToolbox(target, "inspector");
  info("inspector opened");

  info("testing the paintflashing button");
  yield testButton(toolbox, Telemetry);

  stopRecordingTelemetryLogs(Telemetry);
  yield gDevTools.closeToolbox(target);
  gBrowser.removeCurrentTab();
});

function* testButton(toolbox, Telemetry) {
  info("Testing command-button-paintflashing");

  let button = toolbox.doc.querySelector("#command-button-paintflashing");
  ok(button, "Captain, we have the button");

  yield* delayedClicks(toolbox, button, 4);
  checkResults("_PAINTFLASHING_", Telemetry);
}

function* delayedClicks(toolbox, node, clicks) {
  for (let i = 0; i < clicks; i++) {
    yield new Promise(resolve => {
      // See TOOL_DELAY for why we need setTimeout here
      setTimeout(() => resolve(), TOOL_DELAY);
    });

    let { CommandState } = require("devtools/shared/gcli/command-state");
    let clicked = new Promise(resolve => {
      CommandState.on("changed", function changed(type, { command }) {
        if (command === "paintflashing") {
          CommandState.off("changed", changed);
          resolve();
        }
      });
    });

    info("Clicking button " + node.id);
    node.click();

    yield clicked;
  }
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
