/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if a warning is shown in the inspector when debugger is paused.
 */

const TAB_URL = EXAMPLE_URL + "doc_inline-script.html";

var gTab, gPanel, gDebugger;
var gTarget, gToolbox;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gTarget = gPanel.target;
    gToolbox = gPanel._toolbox;

    testPause();
  });
}

function testPause() {
  gDebugger.gThreadClient.addOneTimeListener("paused", () => {
    ok(gDebugger.gThreadClient.paused,
      "threadClient.paused has been updated to true.");

    gToolbox.once("inspector-selected").then(inspector => {
      inspector.once("inspector-updated").then(testNotificationIsUp1);
    });
    gToolbox.selectTool("inspector");
  });

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gDebugger.document.getElementById("resume"),
    gDebugger);

  // Evaluate a script to fully pause the debugger
  once(gDebugger.gClient, "willInterrupt").then(() => {
    evalInTab(gTab, "1+1;");
  });
}

function testNotificationIsUp1() {
  let notificationBox = gToolbox.getNotificationBox();
  let notification = notificationBox.getNotificationWithValue("inspector-script-paused");

  ok(notification,
    "Inspector notification is present (1).");

  gToolbox.once("jsdebugger-selected", testNotificationIsHidden);
  gToolbox.selectTool("jsdebugger");
}

function testNotificationIsHidden() {
  let notificationBox = gToolbox.getNotificationBox();
  let notification = notificationBox.getNotificationWithValue("inspector-script-paused");

  ok(!notification,
    "Inspector notification is hidden (2).");

  gToolbox.once("inspector-selected", testNotificationIsUp2);
  gToolbox.selectTool("inspector");
}

function testNotificationIsUp2() {
  let notificationBox = gToolbox.getNotificationBox();
  let notification = notificationBox.getNotificationWithValue("inspector-script-paused");

  ok(notification,
    "Inspector notification is present again (3).");

  testResume();
}

function testResume() {
  gDebugger.gThreadClient.addOneTimeListener("resumed", () => {
    ok(!gDebugger.gThreadClient.paused,
      "threadClient.paused has been updated to false.");

    let notificationBox = gToolbox.getNotificationBox();
    let notification = notificationBox.getNotificationWithValue("inspector-script-paused");

    ok(!notification,
      "Inspector notification was removed once debugger resumed.");

    closeDebuggerAndFinish(gPanel);
  });

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gDebugger.document.getElementById("resume"),
    gDebugger);
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gTarget = null;
  gToolbox = null;
});
