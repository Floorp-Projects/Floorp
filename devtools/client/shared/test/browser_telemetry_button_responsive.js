/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8," +
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
Services.prefs.setCharPref(
  "devtools.devices.url",
  "http://example.com/browser/devtools/client/responsive/test/browser/devices.json"
);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.testing");
  Services.prefs.clearUserPref("devtools.devices.url");
  Services.prefs.clearUserPref("devtools.responsive.html.displayedDeviceList");
  asyncStorage.removeItem("devtools.devices.url_cache");
  asyncStorage.removeItem("devtools.devices.local");
});

loader.lazyRequireGetter(
  this,
  "ResponsiveUIManager",
  "devtools/client/responsive/manager",
  true
);

add_task(async function() {
  await addTab(TEST_URI);
  startTelemetry();

  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  const toolbox = await gDevTools.showToolbox(target, "inspector");
  info("inspector opened");

  info("testing the responsivedesign button");
  await testButton(toolbox);

  await toolbox.destroy();
  gBrowser.removeCurrentTab();
});

async function testButton(toolbox) {
  info("Testing command-button-responsive");

  const button = toolbox.doc.querySelector("#command-button-responsive");
  ok(button, "Captain, we have the button");

  await delayedClicks(button, 4);

  checkResults();
}

function waitForToggle() {
  return new Promise(resolve => {
    const handler = () => {
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
    const toggled = waitForToggle();
    node.click();
    await toggled;
    // See TOOL_DELAY for why we need setTimeout here
    await DevToolsUtils.waitForTime(TOOL_DELAY);
  }
};

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_RESPONSIVE_")
  // here.
  checkTelemetry(
    "DEVTOOLS_RESPONSIVE_OPENED_COUNT",
    "",
    { 0: 2, 1: 0 },
    "array"
  );
  checkTelemetry(
    "DEVTOOLS_RESPONSIVE_TIME_ACTIVE_SECONDS",
    "",
    null,
    "hasentries"
  );
}
