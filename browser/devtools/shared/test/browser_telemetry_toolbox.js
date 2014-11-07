/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8,<p>browser_telemetry_toolbox.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

const THEME_HISTOGRAM_TOOLBOX = "DEVTOOLS_SELECTED_TOOLBOX_THEME_ENUMERATED";
const THEME_HISTOGRAM_BROWSER = "DEVTOOLS_SELECTED_BROWSER_THEME_BOOLEAN";

let {Promise: promise} = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js", {});
let {Services} = Cu.import("resource://gre/modules/Services.jsm", {});

let require = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools.require;
let Telemetry = require("devtools/shared/telemetry");

function init() {
  Telemetry.prototype.telemetryInfo = {};
  Telemetry.prototype._oldlog = Telemetry.prototype.log;
  Telemetry.prototype.log = function(histogramId, value) {
    if (histogramId) {
      if (!this.telemetryInfo[histogramId]) {
        this.telemetryInfo[histogramId] = [];
      }

      this.telemetryInfo[histogramId].push(value);
    }
  };

  openToolboxThreeTimes();
}

let pass = 0;
function openToolboxThreeTimes() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    info("Toolbox opened");

    toolbox.once("destroyed", function() {
      if (pass++ === 3) {
        checkResults();
      } else {
        openToolboxThreeTimes();
      }
    });
    // We use a timeout to check the toolbox's active time
    setTimeout(function() {
      gDevTools.closeToolbox(target);
    }, TOOL_DELAY);
  }).then(null, console.error);
}

function checkResults() {
  let result = Telemetry.prototype.telemetryInfo;

  for (let [histId, value] of Iterator(result)) {
    if (histId.endsWith("OPENED_PER_USER_FLAG")) {
      ok(value.length === 1 && value[0] === true,
         "Per user value " + histId + " has a single value of true");
    } else if (histId.endsWith("OPENED_BOOLEAN")) {
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
    } else if (histId === THEME_HISTOGRAM_TOOLBOX) {
      let item = result[THEME_HISTOGRAM_TOOLBOX];
      let expected = isInDevEdition() ? "1,1,1,1" : "0,0,0,0";

      ok(item.length === 4 && item.join(",") === expected,
        "Devtools toolbox theme correctly logged");
    } else if (histId === THEME_HISTOGRAM_BROWSER) {
      let item = result[THEME_HISTOGRAM_BROWSER];
      let expected = isInDevEdition() ? "true,true,true,true" :
                                        "false,false,false,false";
      ok(item.length === 4 && item.join(",") === expected,
        "DevTools browser theme correctly logged");
    }
  }

  finishUp();
}

function finishUp() {
  gBrowser.removeCurrentTab();

  Telemetry.prototype.log = Telemetry.prototype._oldlog;
  delete Telemetry.prototype._oldlog;
  delete Telemetry.prototype.telemetryInfo;

  TargetFactory = Services = promise = require = null;

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
