/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8,<p>browser_telemetry_toolbox.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

const TOOLBOX_THEME_PREF = "devtools.theme";
const DEVEDITION_BROWSER_THEME_PREF = "browser.devedition.theme.enabled";

let {Promise: promise} = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js", {});

function init() {
  startTelemetry();
  openToolboxFourTimes();
}

let pass = 0;
function openToolboxFourTimes() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    info("Toolbox opened");

    toolbox.once("destroyed", function() {
      if (pass++ === 3) {
        switchThemes();
        checkResults();
        finishUp();
      } else {
        openToolboxFourTimes();
      }
    });
    // We use a timeout to check the toolbox's active time
    setTimeout(function() {
      gDevTools.closeToolbox(target);
    }, TOOL_DELAY);
  }).then(null, console.error);
}

function switchThemes() {
  let currentToolboxTheme = Services.prefs.getCharPref(TOOLBOX_THEME_PREF);
  let usingDevEditionTheme = Services.prefs.getBoolPref(DEVEDITION_BROWSER_THEME_PREF);
  let toolboxThemeArray =
    currentToolboxTheme === "light" ? ["dark", "light"] : ["light", "dark"];
  let usingBrowserThemeArray =[!usingDevEditionTheme, usingDevEditionTheme];

  for (let i = 0; i < 3; i++) {
    for (let theme of toolboxThemeArray) {
      Services.prefs.setCharPref(TOOLBOX_THEME_PREF, theme);
    }

    for (let using of usingBrowserThemeArray) {
      Services.prefs.setBoolPref(DEVEDITION_BROWSER_THEME_PREF, using);
    }
  }
}

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_")
  // here.
  checkTelemetry("DEVTOOLS_DEBUGGER_RDP_LOCAL_LISTTABS_MS", null, "hasentries");
  checkTelemetry("DEVTOOLS_DEBUGGER_RDP_LOCAL_RECONFIGURETAB_MS", null, "hasentries");
  checkTelemetry("DEVTOOLS_DEBUGGER_RDP_LOCAL_TABDETACH_MS", null, "hasentries");

  checkTelemetry("DEVTOOLS_INSPECTOR_OPENED_BOOLEAN", [0,4,0]);
  checkTelemetry("DEVTOOLS_INSPECTOR_OPENED_PER_USER_FLAG", [0,1,0]);
  checkTelemetry("DEVTOOLS_INSPECTOR_TIME_ACTIVE_SECONDS", null, "hasentries");

  checkTelemetry("DEVTOOLS_RULEVIEW_OPENED_BOOLEAN", [0,4,0]);
  checkTelemetry("DEVTOOLS_RULEVIEW_OPENED_PER_USER_FLAG", [0,1,0]);
  checkTelemetry("DEVTOOLS_RULEVIEW_TIME_ACTIVE_SECONDS", null, "hasentries");

  checkTelemetry("DEVTOOLS_SELECTED_BROWSER_THEME_BOOLEAN", [3,3,0]);
  checkTelemetry("DEVTOOLS_SELECTED_TOOLBOX_THEME_ENUMERATED", [3,3,0,0]);

  checkTelemetry("DEVTOOLS_TOOLBOX_OPENED_BOOLEAN", [0,4,0]);
  checkTelemetry("DEVTOOLS_TOOLBOX_OPENED_PER_USER_FLAG", [0,1,0]);
  checkTelemetry("DEVTOOLS_TOOLBOX_TIME_ACTIVE_SECONDS", null, "hasentries");
}

function finishUp() {
  gBrowser.removeCurrentTab();

  Services.prefs.clearUserPref(TOOLBOX_THEME_PREF);

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
