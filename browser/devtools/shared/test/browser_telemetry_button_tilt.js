/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>browser_telemetry_button_tilt.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

add_task(function*() {
  yield promiseTab(TEST_URI);
  let Telemetry = loadTelemetryAndRecordLogs();

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = yield gDevTools.showToolbox(target, "inspector");

  // Wait for the inspector to be initialized
  yield toolbox.getPanel("inspector").once("inspector-updated");

  info("inspector opened");

  info("testing the tilt button");
  yield testButton(toolbox, Telemetry);

  stopRecordingTelemetryLogs(Telemetry);

  yield gDevTools.closeToolbox(target);
  gBrowser.removeCurrentTab();
});

function* testButton(toolbox, Telemetry) {
  info("Testing command-button-tilt");

  let button = toolbox.doc.querySelector("#command-button-tilt");
  ok(button, "Captain, we have the button");

  yield delayedClicks(button, 4)

  checkResults("_TILT_", Telemetry);
}

function delayedClicks(node, clicks) {
  return new Promise(resolve => {
    let clicked = 0;

    // See TOOL_DELAY for why we need setTimeout here
    setTimeout(function delayedClick() {
      if (clicked >= clicks) {
        resolve();
        return;
      }
      info("Clicking button " + node.id);

      // Depending on odd/even click we are either opening
      // or closing tilt
      let event;
      if (clicked % 2 == 0) {
        info("Waiting for opening\n");
        event = "tilt-initialized";
      } else {
        dump("Waiting for closing\n");
        event = "tilt-destroyed";
      }
      let f = function () {
        Services.obs.removeObserver(f, event, false);
        setTimeout(delayedClick, 200);
      };
      Services.obs.addObserver(f, event, false);

      clicked++;
      node.click();
    }, TOOL_DELAY);
  });
}

function checkResults(histIdFocus, Telemetry) {
  let result = Telemetry.prototype.telemetryInfo;

  for (let [histId, value] of Iterator(result)) {
    if (histId.startsWith("DEVTOOLS_INSPECTOR_") ||
        !histId.includes(histIdFocus)) {
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
}
