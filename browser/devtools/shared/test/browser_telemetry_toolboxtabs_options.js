/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8,<p>browser_telemetry_toolboxtabs_options.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

let {Promise: promise} = Cu.import("resource://gre/modules/devtools/deprecated-sync-thenables.js", {});

function init() {
  startTelemetry();
  openToolboxTabTwice("options", false);
}

function openToolboxTabTwice(id, secondPass) {
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gDevTools.showToolbox(target, id).then(function(toolbox) {
    info("Toolbox tab " + id + " opened");

    toolbox.once("destroyed", function() {
      if (secondPass) {
        checkResults();
      } else {
        openToolboxTabTwice(id, true);
      }
    });
    // We use a timeout to check the tools active time
    setTimeout(function() {
      gDevTools.closeToolbox(target);
    }, TOOL_DELAY);
  }).then(null, reportError);
}

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_")
  // here.
  checkTelemetry("DEVTOOLS_DEBUGGER_RDP_LOCAL_LISTTABS_MS", null, "hasentries");
  checkTelemetry("DEVTOOLS_DEBUGGER_RDP_LOCAL_RECONFIGURETAB_MS", null, "hasentries");
  checkTelemetry("DEVTOOLS_DEBUGGER_RDP_LOCAL_TABDETACH_MS", null, "hasentries");

  checkTelemetry("DEVTOOLS_OPTIONS_OPENED_BOOLEAN", [0,2,0]);
  checkTelemetry("DEVTOOLS_OPTIONS_OPENED_PER_USER_FLAG", [0,1,0]);
  checkTelemetry("DEVTOOLS_OPTIONS_TIME_ACTIVE_SECONDS", null, "hasentries");

  checkTelemetry("DEVTOOLS_TOOLBOX_OPENED_BOOLEAN", [0,2,0]);
  checkTelemetry("DEVTOOLS_TOOLBOX_OPENED_PER_USER_FLAG", [0,1,0]);
  checkTelemetry("DEVTOOLS_TOOLBOX_TIME_ACTIVE_SECONDS", null, "hasentries");

  finishUp();
}

function finishUp() {
  gBrowser.removeCurrentTab();

  TargetFactory = promise = null;

  finish();
}

function test() {
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    waitForFocus(init, content);
  }, true);

  content.location = TEST_URI;
}
