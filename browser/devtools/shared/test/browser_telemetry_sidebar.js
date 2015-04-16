/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8,<p>browser_telemetry_sidebar.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

add_task(function*() {
  yield promiseTab(TEST_URI);

  startTelemetry();

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = yield gDevTools.showToolbox(target, "inspector");
  info("inspector opened");

  yield testSidebar(toolbox);
  checkResults();

  yield gDevTools.closeToolbox(target);
  gBrowser.removeCurrentTab();
});

function* testSidebar(toolbox) {
  info("Testing sidebar");

  let inspector = toolbox.getCurrentPanel();
  let sidebarTools = ["ruleview", "computedview", "fontinspector",
                      "layoutview", "animationinspector"];

  // Concatenate the array with itself so that we can open each tool twice.
  sidebarTools.push.apply(sidebarTools, sidebarTools);

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
  checkTelemetry("DEVTOOLS_ANIMATIONINSPECTOR_OPENED_BOOLEAN", [0,2,0]);
  checkTelemetry("DEVTOOLS_ANIMATIONINSPECTOR_OPENED_PER_USER_FLAG", [0,1,0]);
  checkTelemetry("DEVTOOLS_ANIMATIONINSPECTOR_TIME_ACTIVE_SECONDS", null, "hasentries");
  checkTelemetry("DEVTOOLS_COMPUTEDVIEW_OPENED_BOOLEAN", [0,2,0]);
  checkTelemetry("DEVTOOLS_COMPUTEDVIEW_OPENED_PER_USER_FLAG", [0,1,0]);
  checkTelemetry("DEVTOOLS_COMPUTEDVIEW_TIME_ACTIVE_SECONDS", null, "hasentries");
  checkTelemetry("DEVTOOLS_FONTINSPECTOR_OPENED_BOOLEAN", [0,2,0]);
  checkTelemetry("DEVTOOLS_FONTINSPECTOR_OPENED_PER_USER_FLAG", [0,1,0]);
  checkTelemetry("DEVTOOLS_FONTINSPECTOR_TIME_ACTIVE_SECONDS", null, "hasentries");
}
