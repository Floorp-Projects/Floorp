/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>browser_telemetry_button_scratchpad.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

add_task(async function() {
  await addTab(TEST_URI);
  startTelemetry();

  await pushPref("devtools.command-button-scratchpad.enabled", true);

  const target = TargetFactory.forTab(gBrowser.selectedTab);
  const toolbox = await gDevTools.showToolbox(target, "inspector");
  info("inspector opened");

  const onAllWindowsOpened = trackScratchpadWindows();

  info("testing the scratchpad button");
  await testButton(toolbox);
  await onAllWindowsOpened;

  checkResults();

  await gDevTools.closeToolbox(target);
  gBrowser.removeCurrentTab();
});

function trackScratchpadWindows() {
  info("register the window observer to track when scratchpad windows open");

  let numScratchpads = 0;

  return new Promise(resolve => {
    Services.ww.registerNotification(function observer(subject, topic) {
      if (topic == "domwindowopened") {
        const win = subject.QueryInterface(Ci.nsIDOMWindow);
        win.addEventListener("load", function() {
          if (win.Scratchpad) {
            win.Scratchpad.addObserver({
              onReady: function() {
                win.Scratchpad.removeObserver(this);
                numScratchpads++;
                win.close();

                info("another scratchpad was opened and closed, " +
                     `count is now ${numScratchpads}`);

                if (numScratchpads === 4) {
                  Services.ww.unregisterNotification(observer);
                  info("4 scratchpads have been opened and closed, checking results");
                  resolve();
                }
              },
            });
          }
        }, {once: true});
      }
    });
  });
}

async function testButton(toolbox) {
  info("Testing command-button-scratchpad");
  const button = toolbox.doc.querySelector("#command-button-scratchpad");
  ok(button, "Captain, we have the button");

  await delayedClicks(button, 4);
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

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_SCRATCHPAD_")
  // here.
  checkTelemetry("DEVTOOLS_SCRATCHPAD_WINDOW_OPENED_COUNT", "", [4, 0, 0], "array");
}
