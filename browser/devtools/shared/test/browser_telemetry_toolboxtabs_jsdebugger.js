/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8,<p>browser_telemetry_toolboxtabs_jsdebugger.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

let {Promise: promise} = Cu.import("resource://gre/modules/devtools/deprecated-sync-thenables.js", {});
let {Services} = Cu.import("resource://gre/modules/Services.jsm", {});

let require = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools.require;
let Telemetry = require("devtools/shared/telemetry");

function init() {
  Telemetry.prototype.telemetryInfo = {};
  Telemetry.prototype._oldlog = Telemetry.prototype.log;
  Telemetry.prototype.log = function(histogramId, value) {
    if (!this.telemetryInfo) {
      // Can be removed when Bug 992911 lands (see Bug 1011652 Comment 10)
      return;
    }
    if (histogramId) {
      if (!this.telemetryInfo[histogramId]) {
        this.telemetryInfo[histogramId] = [];
      }

      this.telemetryInfo[histogramId].push(value);
    }
  }

  openToolboxTabTwice("jsdebugger", false);
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
    }
  }

  finishUp();
}

function reportError(error) {
  let stack = "    " + error.stack.replace(/\n?.*?@/g, "\n    JS frame :: ");

  ok(false, "ERROR: " + error + " at " + error.fileName + ":" +
            error.lineNumber + "\n\nStack trace:" + stack);
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
