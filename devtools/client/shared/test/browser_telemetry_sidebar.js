/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<p>browser_telemetry_sidebar.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

add_task(async function() {
  await addTab(TEST_URI);
  startTelemetry();

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = await gDevTools.showToolbox(target, "inspector");
  info("inspector opened");

  await testSidebar(toolbox);
  checkResults();

  await gDevTools.closeToolbox(target);
  gBrowser.removeCurrentTab();
});

function testSidebar(toolbox) {
  info("Testing sidebar");

  let inspector = toolbox.getCurrentPanel();
  let sidebarTools = ["computedview", "layoutview", "fontinspector",
                      "animationinspector"];

  // Concatenate the array with itself so that we can open each tool twice.
  sidebarTools = [...sidebarTools, ...sidebarTools];

  return new Promise(resolve => {
    // See TOOL_DELAY for why we need setTimeout here
    setTimeout(function selectSidebarTab() {
      let tool = sidebarTools.pop();
      if (tool) {
        inspector.sidebar.select(tool);
        setTimeout(function() {
          setTimeout(selectSidebarTab, TOOL_DELAY);
        }, TOOL_DELAY);
      } else {
        resolve();
      }
    }, TOOL_DELAY);
  });
}

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_")
  // here.
  checkTelemetry("DEVTOOLS_INSPECTOR_OPENED_COUNT", "", [1, 0, 0], "array");
  checkTelemetry("DEVTOOLS_RULEVIEW_OPENED_COUNT", "", [1, 0, 0], "array");
  checkTelemetry("DEVTOOLS_COMPUTEDVIEW_OPENED_COUNT", "", [3, 0, 0], "array");
  checkTelemetry("DEVTOOLS_LAYOUTVIEW_OPENED_COUNT", "", [2, 0, 0], "array");
  checkTelemetry("DEVTOOLS_FONTINSPECTOR_OPENED_COUNT", "", [2, 0, 0], "array");
  checkTelemetry("DEVTOOLS_COMPUTEDVIEW_TIME_ACTIVE_SECONDS", "", null, "hasentries");
  checkTelemetry("DEVTOOLS_LAYOUTVIEW_TIME_ACTIVE_SECONDS", "", null, "hasentries");
  checkTelemetry("DEVTOOLS_FONTINSPECTOR_TIME_ACTIVE_SECONDS", "", null, "hasentries");
}
