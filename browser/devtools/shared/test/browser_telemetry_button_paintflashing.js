/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8,<p>browser_telemetry_button_paintflashing.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

let promise = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js", {}).Promise;
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

  testButton("command-button-paintflashing");
}

function testButton(id) {
  info("Testing " + id);

  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    info("inspector opened");

    let button = toolbox.doc.querySelector("#" + id);
    ok(button, "Captain, we have the button");

    delayedClicks(button, 4).then(function() {
      checkResults("_PAINTFLASHING_");
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

function checkResults(histIdFocus) {
  let result = Telemetry.prototype.telemetryInfo;

  for (let [histId, value] of Iterator(result)) {
    if (histId.startsWith("DEVTOOLS_INSPECTOR_") ||
        !histId.contains(histIdFocus)) {
      // Inspector stats are tested in
      // browser_telemetry_toolboxtabs_{toolname}.js so we skip them here
      // because we only open the inspector once for this test.
      continue;
    }

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
