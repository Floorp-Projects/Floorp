/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8,<p>browser_telemetry_button_tilt.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

let promise = Cu.import("resource://gre/modules/devtools/deprecated-sync-thenables.js", {}).Promise;

function init() {
  startTelemetry();
  testButton("command-button-tilt");
}

function testButton(id) {
  info("Testing " + id);

  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    info("inspector opened");

    let button = toolbox.doc.querySelector("#" + id);
    ok(button, "Captain, we have the button");

    delayedClicks(button, 4).then(function() {
      checkResults();
    });
  }).then(null, console.error);
}

function delayedClicks(node, clicks) {
  let deferred = promise.defer();
  let clicked = 0;

  // See TOOL_DELAY for why we need setTimeout here
  setTimeout(function delayedClick() {
    info("Clicking button " + node.id);
    node.click();
    clicked++;

    if (clicked >= clicks) {
      deferred.resolve(node);
    } else {
      setTimeout(delayedClick, TOOL_DELAY);
    }
  }, TOOL_DELAY);

  return deferred.promise;
}

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_")
  // here.
  checkTelemetry("DEVTOOLS_DEBUGGER_RDP_LOCAL_LISTTABS_MS", null, "hasentries");
  checkTelemetry("DEVTOOLS_DEBUGGER_RDP_LOCAL_RECONFIGURETAB_MS", null, "hasentries");

  checkTelemetry("DEVTOOLS_INSPECTOR_OPENED_BOOLEAN", [0,1,0]);
  checkTelemetry("DEVTOOLS_INSPECTOR_OPENED_PER_USER_FLAG", [0,1,0]);

  checkTelemetry("DEVTOOLS_RULEVIEW_OPENED_BOOLEAN", [0,1,0]);
  checkTelemetry("DEVTOOLS_RULEVIEW_OPENED_PER_USER_FLAG", [0,1,0]);

  checkTelemetry("DEVTOOLS_TILT_OPENED_BOOLEAN", [0,2,0]);
  checkTelemetry("DEVTOOLS_TILT_OPENED_PER_USER_FLAG", [0,1,0]);
  checkTelemetry("DEVTOOLS_TILT_TIME_ACTIVE_SECONDS", null, "hasentries");

  checkTelemetry("DEVTOOLS_TOOLBOX_OPENED_BOOLEAN", [0,1,0]);
  checkTelemetry("DEVTOOLS_TOOLBOX_OPENED_PER_USER_FLAG", [0,1,0]);

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
