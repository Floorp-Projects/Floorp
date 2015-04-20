/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>browser_telemetry_button_scratchpad.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

add_task(function*() {
  yield promiseTab(TEST_URI);
  let Telemetry = loadTelemetryAndRecordLogs();

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = yield gDevTools.showToolbox(target, "inspector");
  info("inspector opened");

  let onAllWindowsOpened = trackScratchpadWindows();

  info("testing the scratchpad button");
  yield testButton(toolbox, Telemetry);
  yield onAllWindowsOpened;

  checkResults("_SCRATCHPAD_", Telemetry);

  stopRecordingTelemetryLogs(Telemetry);
  yield gDevTools.closeToolbox(target);
  gBrowser.removeCurrentTab();
});

function trackScratchpadWindows() {
  info("register the window observer to track when scratchpad windows open");

  let numScratchpads = 0;

  return new Promise(resolve => {
    Services.ww.registerNotification(function observer(subject, topic) {
      if (topic == "domwindowopened") {
        let win = subject.QueryInterface(Ci.nsIDOMWindow);
        win.addEventListener("load", function onLoad() {
          win.removeEventListener("load", onLoad, false);

          if (win.Scratchpad) {
            win.Scratchpad.addObserver({
              onReady: function() {
                win.Scratchpad.removeObserver(this);
                numScratchpads++;
                win.close();

                info("another scratchpad was opened and closed, count is now " + numScratchpads);

                if (numScratchpads === 4) {
                  Services.ww.unregisterNotification(observer);
                  info("4 scratchpads have been opened and closed, checking results");
                  resolve();
                }
              },
            });
          }
        }, false);
      }
    });
  });
}

function* testButton(toolbox, Telemetry) {
  info("Testing command-button-scratchpad");
  let button = toolbox.doc.querySelector("#command-button-scratchpad");
  ok(button, "Captain, we have the button");

  yield delayedClicks(button, 4);
}

function delayedClicks(node, clicks) {
  return new Promise(resolve => {
    let clicked = 0;

    // See TOOL_DELAY for why we need setTimeout here
    setTimeout(function delayedClick() {
      info("Clicking button " + node.id);
      node.click();
      clicked++;

      if (clicked >= clicks) {
        resolve(node);
      } else {
        setTimeout(delayedClick, TOOL_DELAY);
      }
    }, TOOL_DELAY);
  });
}

function checkResults(histIdFocus, Telemetry) {
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
}
