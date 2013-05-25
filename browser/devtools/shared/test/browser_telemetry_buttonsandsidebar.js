/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8,<p>browser_telemetry_buttonsandsidebar.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

let {Promise} = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js", {});
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
  }

  testButtons();
}

function testButtons() {
  info("Testing buttons");

  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    let container = toolbox.doc.getElementById("toolbox-buttons");
    let buttons = container.getElementsByTagName("toolbarbutton");

    // Copy HTMLCollection to array.
    buttons = Array.prototype.slice.call(buttons);

    (function testButton() {
      let button = buttons.pop();

      if (button) {
        info("Clicking button " + button.id);
        button.click();
        delayedClicks(button, 3).then(function(button) {
          if (buttons.length == 0) {
            // Remove scratchpads
            let wins = Services.wm.getEnumerator("devtools:scratchpad");
            while (wins.hasMoreElements()) {
              let win = wins.getNext();
              info("Closing scratchpad window");
              win.close();
            }

            testSidebar();
          } else {
            setTimeout(testButton, TOOL_DELAY);
          }
        });
      }
    })();
  }).then(null, reportError);
}

function delayedClicks(node, clicks) {
  let deferred = Promise.defer();
  let clicked = 0;

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

function testSidebar() {
  info("Testing sidebar");

  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    let inspector = toolbox.getCurrentPanel();
    let sidebarTools = ["ruleview", "computedview", "fontinspector", "layoutview"];

    // Concatenate the array with itself so that we can open each tool twice.
    sidebarTools.push.apply(sidebarTools, sidebarTools);

    setTimeout(function selectSidebarTab() {
      let tool = sidebarTools.pop();
      if (tool) {
        inspector.sidebar.select(tool);
        setTimeout(function() {
          setTimeout(selectSidebarTab, TOOL_DELAY);
        }, TOOL_DELAY);
      } else {
        checkResults();
      }
    }, TOOL_DELAY);
  });
}

function checkResults() {
  let result = Telemetry.prototype.telemetryInfo;

  for (let [histId, value] of Iterator(result)) {
    if (histId.startsWith("DEVTOOLS_INSPECTOR_")) {
      // Inspector stats are tested in browser_telemetry_toolboxtabs.js so we
      // skip them here because we only open the inspector once for this test.
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

  TargetFactory = Services = Promise = require = null;

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
