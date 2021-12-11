/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8," +
  "<p>browser_telemetry_button_paintflashing.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

add_task(async function() {
  await addTab(TEST_URI);
  startTelemetry();

  await pushPref("devtools.command-button-paintflashing.enabled", true);

  const tab = gBrowser.selectedTab;
  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "inspector",
  });
  info("inspector opened");

  info("testing the paintflashing button");
  await testButton(toolbox);

  await toolbox.destroy();
  gBrowser.removeCurrentTab();
});

async function testButton(toolbox) {
  info("Testing command-button-paintflashing");

  const button = toolbox.doc.querySelector("#command-button-paintflashing");
  ok(button, "Captain, we have the button");

  await delayedClicks(toolbox, button, 4);
  await toolbox.commands.client.waitForRequestsToSettle();
  checkResults();
}

async function delayedClicks(toolbox, node, clicks) {
  for (let i = 0; i < clicks; i++) {
    await new Promise(resolve => {
      // See TOOL_DELAY for why we need setTimeout here
      setTimeout(() => resolve(), TOOL_DELAY);
    });
    info("Clicking button " + node.id);
    node.click();
  }
}

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_PAINTFLASHING_")
  // here.
  checkTelemetry(
    "DEVTOOLS_PAINTFLASHING_OPENED_COUNT",
    "",
    { 0: 2, 1: 0 },
    "array"
  );
  checkTelemetry(
    "DEVTOOLS_PAINTFLASHING_TIME_ACTIVE_SECONDS",
    "",
    null,
    "hasentries"
  );
}
