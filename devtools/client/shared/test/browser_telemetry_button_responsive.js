/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>browser_telemetry_button_responsive.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

const asyncStorage = require("devtools/shared/async-storage");

// Toggling the RDM UI involves several docShell swap operations, which are somewhat slow
// on debug builds. Usually we are just barely over the limit, so a blanket factor of 2
// should be enough.
requestLongerTimeout(2);

Services.prefs.setBoolPref("devtools.testing", true);
Services.prefs.clearUserPref("devtools.responsive.html.displayedDeviceList");
Services.prefs.setCharPref("devtools.devices.url",
  "http://example.com/browser/devtools/client/responsive.html/test/browser/devices.json");

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.testing");
  Services.prefs.clearUserPref("devtools.devices.url");
  Services.prefs.clearUserPref("devtools.responsive.html.displayedDeviceList");
  asyncStorage.removeItem("devtools.devices.url_cache");
  asyncStorage.removeItem("devtools.devices.local");
});

loader.lazyRequireGetter(this, "ResponsiveUIManager", "devtools/client/responsive.html/manager", true);

add_task(async function() {
  await addTab(TEST_URI);
  let Telemetry = loadTelemetryAndRecordLogs();

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = await gDevTools.showToolbox(target, "inspector");
  info("inspector opened");

  info("testing the responsivedesign button");
  await testButton(toolbox, Telemetry);

  stopRecordingTelemetryLogs(Telemetry);
  await gDevTools.closeToolbox(target);
  gBrowser.removeCurrentTab();
});

async function testButton(toolbox, Telemetry) {
  info("Testing command-button-responsive");

  let button = toolbox.doc.querySelector("#command-button-responsive");
  ok(button, "Captain, we have the button");

  await delayedClicks(button, 4);

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

var delayedClicks = async function(node, clicks) {
  for (let i = 0; i < clicks; i++) {
    info("Clicking button " + node.id);
    let toggled = waitForToggle();
    node.click();
    await toggled;
    // See TOOL_DELAY for why we need setTimeout here
    await DevToolsUtils.waitForTime(TOOL_DELAY);
  }
};

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

      let okay = value.every(function(element) {
        return element === true;
      });

      ok(okay, "All " + histId + " entries are === true");
    } else if (histId.endsWith("TIME_ACTIVE_SECONDS")) {
      ok(value.length > 1, histId + " has more than one entry");

      let okay = value.every(function(element) {
        return element > 0;
      });

      ok(okay, "All " + histId + " entries have time > 0");
    }
  }
}
